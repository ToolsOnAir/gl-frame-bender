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
#ifndef TOA_FRAME_BENDER_STREAM_SOURCE_H
#define TOA_FRAME_BENDER_STREAM_SOURCE_H

#include "Utils.h"
#include "FrameTime.h"
#include "Frame.h"

#include <memory>
#include <string>
#include <queue>
#include <atomic>
#include <iosfwd>

namespace toa {
    namespace frame_bender {

        /**
         * Base class for all stream sources.
         * Make note that is the the subclasses responsiblity to set the state to READY_TO_READ
         */
        class StreamSource : utils::NoCopyingOrMoving {
        
        public:

            // TODO: think about further states
            enum class State {

                INITIALIZED, // the source has been initialized, but no frames are ready to be read yet
                READY_TO_READ, // frames are available to be read
                END_OF_STREAM // the source has been fully consumed and the buffer queue is also already empty. Nothin is to come anymore

            };

            StreamSource(std::string name);
            virtual ~StreamSource();

            /**
             * @return Returns the point in time until which data has been
             * uploaded to the input queue (i.e. until which time the queue can be
             * expected to hold video data for this source). The time is 
             * expressed in units local to the source.
             */
            //Time buffered_time() const;

            //const IntegerRational& frame_interval() const;

            // This pops from the queue, and notifies the
            // implementation that a frame has been taken on its callback
            // Will throw if state == END_OF_STREAM or state == INITIALIZED
            bool pop_frame(Frame& out_frame);
            const std::string& name() const { return name_; }
            
            // TODO: this should be replaced by proper pool handling in the future
            virtual void invalidate_frame(Frame&& f);

            // Returns the current state. This call is thread-safe
            // Note that end-of-stream (END_OF_STREAM) is only marked AFTER
            // the last frame has been popped.
            State state() const;

        protected:    

            // Implementor is responsible for transitioning the states correctly
            virtual void frame_has_been_used(const Frame& frame) = 0;
            std::string name_;
            std::unique_ptr<CircularFifo<Frame>> input_queue_;
            std::atomic<State> state_;
            // TODO: make the time_buffered value somehow atomic...
        };

        std::ostream& operator<< (std::ostream& out, const StreamSource::State& v);

        /**
         * Special class which basically provides in memory-frames for low
         * latency testing case scenarios.
         */
        class PrefetchedImageSequence final : public StreamSource {
        public:

            PrefetchedImageSequence(
                const std::string& frame_folder, // Note that this is relative to the global program.sequences_location
                const std::string& regex_pattern,
                const ImageFormat& image_format,
                const Time& frame_duration,
                size_t loop_count = 1);

            size_t num_frames() const { return num_frames_prefetched_ * loop_count_; }
            uintmax_t total_data_size() const { return total_data_size_ * loop_count_; }

            virtual void invalidate_frame(Frame&& f) override;

        private:

            virtual void frame_has_been_used(const Frame& frame);

            std::queue<Frame> frames_repository_;
            size_t num_frames_prefetched_;
            uintmax_t total_data_size_;
            Time frame_duration_;
            size_t loop_count_;
            Time end_time_stamp_;
        };

        // TODO-DEF: create a container-framesource?

    }
}

#endif // TOA_FRAME_BENDER_STREAM_SOURCE_H