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
#ifndef TOA_FRAME_BENDER_COPY_HOST_BUFFER_STAGE_H
#define TOA_FRAME_BENDER_COPY_HOST_BUFFER_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "PipelineStage.h"
#include "ProgramOptions.h"

#include "Utils.h"

namespace toa { 

    namespace frame_bender {

        class CopyMappedPBOToHostStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenGL, TokenFrame> StageType;

            template <typename InputStage>
            CopyMappedPBOToHostStage(
                size_t pipeline_size,
                ImageFormat frame_format,
                const InputStage& input_stage,
                bool dbg_bypass_copy = false);

            ~CopyMappedPBOToHostStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageCommand perform(TokenGL& in_token, TokenFrame& out_token);

            StageType stage_;

            StageSampler sampler_;

            ImageFormat format_;

            bool dbg_bypass_copy_;
        };        

        template <typename InputStage>
        CopyMappedPBOToHostStage::CopyMappedPBOToHostStage(
            size_t pipeline_size,
            ImageFormat frame_format,
            const InputStage& input_stage,
            bool dbg_bypass_copy) :
                format_(frame_format),
                dbg_bypass_copy_(dbg_bypass_copy)
        {

            std::vector<TokenFrame> init;
            for (size_t i = 0; i<pipeline_size; ++i) {
                TokenFrame new_token;

                new_token.frame = Frame(format_, Time(0, 1), false);
                init.push_back(std::move(new_token));
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenFrame>(
                "CopyPBOToHost",
                std::bind(
                    &CopyMappedPBOToHostStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                std::move(init),
                input_stage.stage(),
                std::move(fnc));

#ifdef FB_USE_INTEL_IPP_FOR_HOST_COPY
            FB_LOG_INFO << "Using Intel IPP for host-buffer copying.";
#endif

            if (dbg_bypass_copy_) {
                FB_LOG_INFO << "Copying is disabled for benchmarking, this will result in corrupted image contents.";
            }

        }

    }

}

#endif // TOA_FRAME_BENDER_COPY_HOST_BUFFER_STAGE_H
