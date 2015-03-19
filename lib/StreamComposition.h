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
#ifndef TOA_FRAME_BENDER_STREAM_COMPOSITION_H
#define TOA_FRAME_BENDER_STREAM_COMPOSITION_H

#include "FrameTime.h"
#include "Utils.h"
#include "Frame.h"

#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace toa {
    namespace frame_bender {

        class StreamRenderer;
        class StreamSource;
        class StreamDispatch;
        class LocklessQueue; 
        class FrameCompositionOutputStage;

        /**
         * A stream composition is defined as the rendered output of N inputs.
         */
        class StreamComposition final : public utils::NoCopyingOrMoving {

        public:

            // TODO: could we make this cleaner?
            friend class FrameCompositionOutputStage;

            typedef std::string ID;
            typedef std::function<void (const Frame& f)> OutputCallback;

            const std::string& name() const;

            // Hacky intermediate solution, we just use the first source for
            // now.
            StreamSource& first_source();

            const StreamRenderer* renderer() const;
            StreamRenderer* renderer();

            // TODO: should be private, but we use "emplace" in StreamDispatch
            // which requires use to make the ctor public... is there another
            // workaround?
            // Only the StreamDispatcher can register StreamCompositions
            StreamComposition(
                std::string name,
                // TODO-DEF: for a more real usecase, we should add in-time, duration and probably looping in here...
                // At the moment, all streams are considered to start at 0, i.e.
                // at the same time. Meaning, you always provide input frames in lockstep.
                std::vector<std::shared_ptr<StreamSource>> input_sources,
                std::unique_ptr<StreamRenderer> renderer,
                OutputCallback output_callback
                );

        private:

            std::string name_;
            std::vector<std::shared_ptr<StreamSource>> input_sources_;
            std::unique_ptr<StreamRenderer> renderer_;
            OutputCallback output_callback_;
            // Note that the output queue size might be determined internally
            // by the scheduling policy

        };

    }
}

#endif // TOA_FRAME_BENDER_STREAM_COMPOSITION_H