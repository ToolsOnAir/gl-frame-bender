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
#ifndef TOA_FRAME_BENDER_FRAME_H
#define TOA_FRAME_BENDER_FRAME_H

#include <memory>

#include "CircularFifo.h"
#include "CircularFifoHelpers.h"
#include "ImageFormat.h"
#include "FrameTime.h"
#include "Utils.h"

#include <boost/align/aligned_alloc.hpp>

namespace toa {
    namespace frame_bender {

        class Frame final {
        public:

            // Create an explicit copy of a frame. We don't want to have
            // this as part of our copy constructor to avoid unintended 
            // copying in user code.
            static Frame create_copy(const Frame& f);

            Frame();
            ~Frame();

            // Creates a frame with the specified dimensions and properties
            // allocates its memory, copies the data and sets the other properties
            Frame(
                ImageFormat format,
                Time time,
                bool end_of_sequence
                );

            // Default move and copy operators are fine, since we are
            // using std::unique_ptr for data storage

            Frame& operator=(Frame&&);
            Frame(Frame&&);

            // TODO: add move, copy construction, etc..

            // timestamp
            // special flag like end-of-stream token
            // validity and bool operator (in this case)

            const Time& time() const;
            void set_time(Time t) { time_ = std::move(t); }

            // When this is true, this frame marks the end of the sequence
            // Note that unlike for other streaming concepts, a frame holding
            // this property to be true, is actually still a valid frame.
            bool marks_end_of_sequence() const;
            void set_marks_end_of_sequence(bool b) { marks_end_of_sequence_ = b; }

            const ImageFormat& image_format() const;

            bool is_valid() const;

            // Same as is_valid()
            // TODO-C++11: make 'explicit'
            operator bool() const { return is_valid(); }

            // TODO: this is currently mainly for testing
            // Actually, exposing the internal data that way really isn't
            // best design... 
            const uint8_t* image_data() const;
            uint8_t* image_data();

            // returns size of image data in bytes.
            size_t image_data_size() const;

            // Currently only supports raw-file dumping
            // Note that byte swapping during writeout is not well optimized
            // and might take much more time than without swapping.
            void dump_to_file(
                const std::string& file_path,
                bool swap_endianness = false,
                size_t word_size = 2
                ) const;

        private:

            using FrameMemoryPtr = std::unique_ptr<uint8_t, decltype(&boost::alignment::aligned_free)>;

            // TODO-C++11: use default and delted functions here!
            Frame& operator=(const Frame&);
            Frame(const Frame&);

            bool is_valid_;
            Time time_;
            ImageFormat image_format_;
            bool marks_end_of_sequence_;
            FrameMemoryPtr image_data_;
            size_t image_data_size_;
        };

    }
}

#endif // TOA_FRAME_BENDER_FRAME_H
