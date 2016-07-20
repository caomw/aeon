#include "gtest/gtest.h"
#include "gen_image.hpp"

#include "provider_factory.hpp"

#include "etl_image.hpp"
#include "etl_bbox.hpp"
#include "etl_label_map.hpp"
#include "json.hpp"
#include "cpio.hpp"
#include "util.hpp"
#include "provider_factory.hpp"

extern gen_image image_dataset;

using namespace std;
using namespace nervana;

TEST(provider,image) {
    nlohmann::json js = {{"media","image_label"},
                         {"data_config", {{"type", "image"}, {"config", {{"height",128},{"width",128},{"channel_major",false},{"flip",true}}}}},
                         {"target_config", {{"type", "label"}, {"config", {}}}}};

    auto data_config = nervana::config_factory::create(js["data_config"]);
    size_t dsize = data_config->get_shape_type().get_byte_size();
    size_t tsize = 4;

    auto media = nervana::train_provider_factory::create(js);

    size_t batch_size = 128;

    buffer_out_array outBuf({dsize, tsize}, batch_size);

    auto files = image_dataset.GetFiles();
    ASSERT_NE(0,files.size());

    CPIOFileReader reader;
    EXPECT_EQ(reader.open(files[0]), true);
    buffer_in_array bp({0,0});
    buffer_in& data_p = *bp[0];
    buffer_in& target_p = *bp[1];
    for(int i=0; i<reader.itemCount()/2; i++) {
        reader.read(data_p);
        reader.read(target_p);
    }
    EXPECT_GT(data_p.getItemCount(),batch_size);
    for (int i=0; i<batch_size; i++ ) {
        media->provide(i, bp, outBuf);

//        cv::Mat mat(width,height,CV_8UC3,&dbuffer[0]);
//        string filename = "data" + to_string(i) + ".png";
//        cv::imwrite(filename,mat);
    }
    for (int i=0; i<batch_size; i++ ) {
        int target_value = unpack_le<int>(outBuf[1]->getItem(i));
        EXPECT_EQ(42+i,target_value);
    }
}

TEST(provider, argtype) {

    {
        /* Create extractor with default num channels param */
        string cfgString = "{\"height\":10, \"width\":10}";
        auto js = nlohmann::json::parse(cfgString);
        image::config cfg{js};
        auto ic = make_shared<image::extractor>(cfg);
        EXPECT_EQ(ic->get_channel_count(), 3);
    }


    {
        string cfgString = R"(
            {
                "height": 30,
                "width" : 30,
                "distribution":{
                    "angle" : [-20, 20],
                    "scale" : [0.2, 0.8],
                    "lighting" : [0.0, 0.1],
                    "aspect_ratio" : [0.75, 1.33],
                    "flip" : [false]
                }
            }
        )";


        image::config itpj(nlohmann::json::parse(cfgString));

        // output the fixed parameters
        EXPECT_EQ(30,itpj.height);
        EXPECT_EQ(30,itpj.width);

        // output the random parameters
        default_random_engine r_eng(0);
        image::param_factory img_prm_maker(itpj);
        auto imgt = make_shared<image::transformer>(itpj);

        auto input_img_ptr = make_shared<image::decoded>(cv::Mat(256, 320, CV_8UC3));

        auto its = img_prm_maker.make_params(input_img_ptr);
    }
}
