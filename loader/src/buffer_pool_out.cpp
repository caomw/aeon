/*
 Copyright 2015 Nervana Systems Inc.
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

#include <assert.h>
#if HAS_GPU
#include <cuda.h>
#endif

#include <random>
#include <algorithm>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>

#include "buffer_pool_out.hpp"

using namespace std;

buffer_pool_out::buffer_pool_out(const std::vector<size_t>& writeSizes,
                                 size_t batchSize, bool pinned)
{
    for (int i = 0; i < _count; i++) {
        _bufs.push_back(make_shared<buffer_out_array>(writeSizes, batchSize, pinned));
    }

}

buffer_pool_out::~buffer_pool_out() {
}

buffer_out_array& buffer_pool_out::getForWrite()
{
    return *_bufs[_writePos];
}

buffer_out_array& buffer_pool_out::getForRead() {
    return *_bufs[_readPos];
}

buffer_out_array& buffer_pool_out::getPair(int bufIdx) {
    assert(bufIdx >= 0 && bufIdx < _count);
    return *_bufs[bufIdx];
}

void buffer_pool_out::advanceReadPos() {
    _used--;
    advance(_readPos);
}

void buffer_pool_out::advanceWritePos() {
    _used++;
    advance(_writePos);
}

bool buffer_pool_out::empty() {
    assert(_used >= 0);
    return (_used == 0);
}

bool buffer_pool_out::full() {
    assert(_used <= _count);
    return (_used == _count);
}

std::mutex& buffer_pool_out::getMutex() {
    return _mutex;
}

void buffer_pool_out::waitForNonEmpty(std::unique_lock<std::mutex>& lock) {
    _nonEmpty.wait(lock);
}

void buffer_pool_out::waitForNonFull(std::unique_lock<std::mutex>& lock) {
    _nonFull.wait(lock);
}

void buffer_pool_out::signalNonEmpty() {
    _nonEmpty.notify_all();
}

void buffer_pool_out::signalNonFull() {
    _nonFull.notify_all();
}

void buffer_pool_out::advance(int& index) {
    // increment index and reset to 0 when index hits `_count`
    if (++index == _count) {
        index = 0;
    }
}

