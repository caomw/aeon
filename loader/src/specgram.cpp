/*
 Copyright 2016 Nervana Systems Inc.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#include "specgram.hpp"
using cv::Mat;
using cv::Range;
using namespace std;

// These can all be static
void specgram::wav_to_specgram(shared_ptr<RawMedia> wav,
                               const int frame_length_tn,
                               const int frame_stride_tn,
                               const int max_time_steps,
                               const Mat& window,
                               Mat& specgram)
{
    // TODO: support more sample formats
    int sample_dtype = CV_16SC1;
    int sample_dsize  = wav->bytesPerSample();
    if (sample_dsize != 2) {
        throw std::runtime_error(
                "Unsupported number of bytes per sample: " + std::to_string(sample_dsize));
    }

    // Go from time domain to strided signal
    Mat wav_data(1, wav->numSamples(), sample_dtype, wav->getBuf(0));
    Mat wav_frames;
    {
        int num_frames = ((wav_data.cols - frame_length_tn) / frame_stride_tn) + 1;
        num_frames = std::min(num_frames, max_time_steps);
        // ensure that there is enough data for at least one frame
        assert(num_frames >= 0);

        wav_frames.create(num_frames, frame_length_tn, sample_dtype);
        for (int frame = 0; frame < num_frames; frame++) {
            int start = frame * frame_stride_tn;
            int end   = start + frame_length_tn;
            wav_data.colRange(start, end).copyTo(wav_frames.row(frame));
        }
    }

    // Prepare for DFT by converting to float
    Mat input;
    wav_frames.convertTo(input, CV_32FC1);

    // Apply window if it has been created
    if (window.cols == frame_length_tn) {
        input = input.mul(cv::repeat(window, input.rows, 1));
    }

    Mat planes[] = {input, Mat::zeros(input.size(), CV_32FC1)};
    Mat compx;
    cv::merge(planes, 2, compx);
    cv::dft(compx, compx, cv::DFT_ROWS);
    compx = compx(Range::all(), Range(0, frame_length_tn / 2  + 1));

    cv::split(compx, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);

    // NOTE: at this point we are returning the specgram representation in
    // (time_steps, freq_steps) shape order.

    specgram = planes[0];

    return;
}


void specgram::create_filterbanks(const int num_filters,
                                  const int fftsz,
                                  const int sample_freq_hz,
                                  Mat &fbank)
{
    double min_mel_freq = hz_to_mel(0.0);
    double max_mel_freq = hz_to_mel(sample_freq_hz / 2.0);
    double mel_freq_delta = (max_mel_freq - min_mel_freq) / (num_filters + 1);


    int   num_freqs = fftsz / 2 + 1;
    float fft_freq_delta = (sample_freq_hz / 2) / (num_freqs);

    fbank.create(num_freqs, num_filters, CV_32F);

    for (int mel_idx=0; mel_idx < num_filters; ++mel_idx) {
        float m_l = min_mel_freq + mel_idx * mel_freq_delta;
        float m_c = m_l + mel_freq_delta;
        float m_r = m_c + mel_freq_delta;

        for (int freq_idx=0; freq_idx < num_freqs; ++freq_idx) {
            float mel = hz_to_mel(freq_idx * fft_freq_delta);
            float wt = 0.0f;
            if (mel > m_l && mel < m_r) {
                wt = mel <= m_c ? (mel - m_l) / (m_c - m_l) : (m_r - mel) / (m_r - m_c);
            }
            fbank.at<float>(freq_idx, mel_idx) = wt;
        }
    }
    return;
}

void specgram::specgram_to_cepsgram(const Mat& specgram,
                                    const Mat& filter_bank,
                                    Mat& cepsgram)
{
    cepsgram = (specgram.mul(specgram) / specgram.cols) * filter_bank;
    cv::log(cepsgram, cepsgram);
    return;
}

void specgram::cepsgram_to_mfcc(const Mat& cepsgram,
                                const int num_cepstra,
                                Mat& mfcc)
{
    assert(num_cepstra <= cepsgram.cols);
    Mat padcepsgram;
    if (cepsgram.cols % 2 == 0) {
        padcepsgram = cepsgram;
    } else {
        cv::copyMakeBorder(cepsgram, padcepsgram, 0, 0, 0, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
    }
    cv::dct(padcepsgram, padcepsgram, cv::DCT_ROWS);
    mfcc = padcepsgram(Range::all(), Range(0, num_cepstra));
    return;
}


void specgram::create_window(const std::string& window_type, const int n, Mat& win)
{
    if (window_type == "none") {
        return;
    }

    win.create(1, n, CV_32FC1);

    float twopi_by_n = 2.0 * CV_PI / (float) (n-1);
    for (int i = 0; i < n; i++) {
        if (window_type == "hann") {
            win.at<float>(0, i) = 0.5 - 0.5 * cos(twopi_by_n*i);
        } else if (window_type == "blackman") {
            win.at<float>(0, i) = 0.42 - 0.5 * cos(twopi_by_n*i) + 0.08 * cos(2 * twopi_by_n*i);
        } else if (window_type == "hamming") {
            win.at<float>(0, i) = 0.54 - 0.46 * cos(twopi_by_n*i);
        } else if (window_type == "bartlett") {
            win.at<float>(0, i) = 1.0 - 2.0 * fabs(i - n / 2.0) / n;
        } else {
            throw std::runtime_error("Unsupported window function");
        }
    }
}

