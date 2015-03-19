/* Copyright (c) 2015 Heinrich Fink <hfink@toolsonair.com>
 * Copyright (c) 2014 ToolsOnAir Broadcast Engineering GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "Precompile.h"
#include "Frame.h"
#include "Logging.h"

#include "Utils.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bf = boost::filesystem;
namespace fb = toa::frame_bender;

// TODO :C++11 please use delegatin constructors or default class member
// initialization...

fb::Frame fb::Frame::create_copy(const Frame& f) {

    Frame new_frame(
        f.image_format(),
        f.time(),
        f.marks_end_of_sequence());

    FB_ASSERT(new_frame.image_data_size() == f.image_data_size());

    // TODO: use Intel IPP optimized copy in here?
    memcpy(
        new_frame.image_data(),
        f.image_data(),
        f.image_data_size());

    return new_frame;

}

fb::Frame::Frame() :
    is_valid_(false),
    image_format_(ImageFormat::kInvalid()),
    marks_end_of_sequence_(false),
    image_data_{nullptr, &boost::alignment::aligned_free},
    image_data_size_(0)
{
}

fb::Frame::~Frame() {

}

fb::Frame::Frame(
    ImageFormat format,
    Time time,
    bool end_of_sequence) :
        is_valid_(false),
        time_(time),
        image_format_(format),
        marks_end_of_sequence_(end_of_sequence),
        image_data_{nullptr, &boost::alignment::aligned_free},
        image_data_size_(0)
{

    image_data_size_ = image_format_.image_byte_size();
    image_data_ = FrameMemoryPtr{reinterpret_cast<uint8_t*>(boost::alignment::aligned_alloc(64, image_data_size_)), &boost::alignment::aligned_free};
      
     //  std::unique_ptr<uint8_t[]>(new uint8_t[image_data_size_]);

    if (!MEM_IS_ALIGNED(image_data_.get(), 64)) {
        FB_LOG_WARNING << "Frame data is not aligned to 64-byte boundary. This results in suboptimal performance.";
    }

    is_valid_ = true;
}

//Frame& fb::Frame::operator=(const Frame&) {
//}
//
//fb::Frame::Frame(const Frame&) {
//}
//

// TODO-C++11: replace this with a defaulted implementation
// However, we don't want the defaulted copy constructor, therefore
// for VS2012 we have to go this extra mile

fb::Frame::Frame(Frame&& other) :
    is_valid_(other.is_valid_),
    time_(other.time_),
    image_format_(other.image_format_),
    marks_end_of_sequence_(other.marks_end_of_sequence_),
    image_data_(std::move(other.image_data_)),
    image_data_size_(other.image_data_size_)
{
    other.is_valid_ = false;
    other.time_ = Time(0, 1);
    other.image_format_ = ImageFormat::kInvalid();
    other.marks_end_of_sequence_ = false;
    other.image_data_size_ = 0;
}

fb::Frame& fb::Frame::operator=(Frame&& other) {

    is_valid_ = other.is_valid_;
    time_ = other.time_;
    image_format_ = other.image_format_;
    marks_end_of_sequence_ = other.marks_end_of_sequence_;
    image_data_ = std::move(other.image_data_);
    image_data_size_ = other.image_data_size_;

    other.is_valid_ = false;
    other.time_ = Time(0, 1);
    other.image_format_ = ImageFormat::kInvalid();
    other.marks_end_of_sequence_ = false;
    other.image_data_size_ = 0;

    return *this;
}

const fb::Time& fb::Frame::time() const {
    return time_;
}

bool fb::Frame::marks_end_of_sequence() const {
    return marks_end_of_sequence_;
}

const fb::ImageFormat& fb::Frame::image_format() const {
    return image_format_;
}

bool fb::Frame::is_valid() const {
    return is_valid_;
}

const uint8_t* fb::Frame::image_data() const {
    return image_data_.get();
}

uint8_t* fb::Frame::image_data() {
    return image_data_.get();
}

size_t fb::Frame::image_data_size() const {
    return image_format_.image_byte_size();
}

void fb::Frame::dump_to_file(
    const std::string& file_path,
    bool swap_endianness,
    size_t word_size) const 
{

    bf::path out_path(file_path);

    if (bf::exists(out_path) && bf::is_directory(out_path)) {
        throw std::invalid_argument("Path '" + file_path + "' already exists and is a directory, can't overwrite it.");
    }

    if (bf::exists(out_path)) {
        FB_LOG_WARNING << "Overwriting file path '" << out_path << "'.";
    }

    bf::ofstream out_stream(out_path, std::ios::out | std::ios::binary);

    if (!swap_endianness) {

        out_stream.write(
            reinterpret_cast<const char*>(image_data_.get()), 
            image_data_size_);

    } else {

        if (word_size == 0) {
            FB_LOG_ERROR << "Invalid word size '" << word_size << "'.";
            throw std::invalid_argument("Invalid word size.");
        }

        if ((image_data_size_ % word_size) != 0) {
            
            FB_LOG_ERROR 
                << "Can't swap endiannes for word size " 
                << word_size << "because image_size is not a multiple of that.";

            throw std::invalid_argument("Invalid word size for endian-swap.");
        }

        for (size_t i = 0; i<image_data_size_; i += word_size) {

            for (size_t j = 0; j<word_size; ++j) {

                out_stream.write(
                    reinterpret_cast<const char*>(&image_data_.get()[i + (word_size - j - 1)]),
                    1);

            }

        }

    }

    out_stream.close();

    FB_LOG_INFO << "Successfully dumped frame to '" << out_path << "'.";

}
