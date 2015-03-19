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
#ifndef TOA_FRAME_BENDER_FRAME_COMPOSITION_INPUT_STAGE_H
#define TOA_FRAME_BENDER_FRAME_COMPOSITION_INPUT_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "PipelineStage.h"

#include "Utils.h"
#include "StageSampler.h"

namespace toa {

    namespace frame_bender {

        // TODO: this execut should get a little more importance
        // For multiple composition it should interleave or manage 
        // their rendering in a smart way.

        class StreamComposition;
        class FrameCompositionInputStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<NO_INPUT, TokenFrame> StageType;

            FrameCompositionInputStage(size_t pipeline_size);

            ~FrameCompositionInputStage();

            // thread safe
            void set_composition(StreamComposition* composition);

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageCommand perform(TokenFrame& token_out);

            StageType stage_;

            // TODO: shall be replaced with *many* compositions
            std::atomic<StreamComposition*> head_composition_;
            StageSampler sampler_;
        };        

    }

}

#endif // TOA_FRAME_BENDER_COPY_FRAME_TO_PBO_STAGE_H