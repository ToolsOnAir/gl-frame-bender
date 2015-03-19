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
#ifndef TOA_FRAME_BENDER_BY_PASS_DOWNLOAD_STAGE_H
#define TOA_FRAME_BENDER_BY_PASS_DOWNLOAD_STAGE_H

#include "Stage.h"
#include "Frame.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "ProgramOptions.h"
#include "PipelineStage.h"

#include "Utils.h"

namespace toa {

    namespace frame_bender {

        class StreamComposition;

        class ByPassDownloadStage : public utils::NoCopyingOrMoving, public PipelineStage {

        public:

            typedef Stage<TokenGL, NO_OUTPUT> StageType;

            template <typename InputStage>
            ByPassDownloadStage(const InputStage& input_stage);

            ~ByPassDownloadStage() {
                if (ProgramOptions::global().sample_stages()) {
                    auto stats = sampler_.collect_statistics();
                    for (const auto& stat : stats) {
                        FB_LOG_INFO << stage_.name() << ": " << stat;
                    }
                }
            }

            void execute() override { stage_.execute(); }

            PipelineStatus status() const override { return stage_.status(); }

            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }

            const std::string& name() const override { return stage_.name(); }

            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageType stage_;

            StageSampler sampler_;

        };        

        template <typename InputStage>
        ByPassDownloadStage::ByPassDownloadStage(
            const InputStage& input_stage)
        {

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_consumer_stage<typename InputStage::StageType>(
                "ByPassDownloadStage",
                [](typename InputStage::StageType::OutputType&){return StageCommand::NO_CHANGE;},
                input_stage.stage(),
                std::move(fnc));

        }



    }

}

#endif //TOA_FRAME_BENDER_BY_PASS_DOWNLOAD_STAGE_H