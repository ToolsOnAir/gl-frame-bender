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
#ifndef TOA_FRAME_BENDER_COPY_HOST_TO_MAPPED_PBO_STAGE_H
#define TOA_FRAME_BENDER_COPY_HOST_TO_MAPPED_PBO_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "UnmapPBOStage.h"
#include "PipelineStage.h"

#include "Utils.h"

namespace toa { 

    namespace frame_bender {

        class CopyHostToMappedPBOStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenFrame, TokenGL> StageType;

            template <typename InputStage>
            CopyHostToMappedPBOStage(
                // These are expected to be "unmapped" and also define
                // the pipeline size of this stage
                // This is considered a shared resource, and the creator
                // of this class instance is responsible for holding 
                // the pbo array valid as long as this stage exists
                const PboMemVector& startup_pipeline_pbos,
                ImageFormat pbo_image_format,
                const InputStage& input_stage, 
                bool dbg_bypass_copy,
                bool pbo_memory_is_persistently_mapped);

            ~CopyHostToMappedPBOStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageCommand perform(TokenFrame& in_token, TokenGL& out_token);

            StageType stage_;

            StageSampler sampler_;

            const bool dbg_bypass_copy_;

            bool pbo_memory_is_persistently_mapped_;
        };        

        template <typename InputStage>
        CopyHostToMappedPBOStage::CopyHostToMappedPBOStage(
            const PboMemVector& startup_pipeline_pbos,
            ImageFormat pbo_image_format,
            const InputStage& input_stage,
            bool dbg_bypass_copy,
            bool pbo_memory_is_persistently_mapped) : dbg_bypass_copy_(dbg_bypass_copy),
              pbo_memory_is_persistently_mapped_(pbo_memory_is_persistently_mapped)
        {
            std::vector<TokenGL> init;

            size_t buffer_size = pbo_image_format.image_byte_size();

            // These need to be pre-mapped
            for (auto& pbo : startup_pipeline_pbos) {

                uint8_t* buffer_data = nullptr;

                GLbitfield access = UnmapPBOStage::pbo_access;

                if (pbo_memory_is_persistently_mapped_)
                    access |= GL_MAP_PERSISTENT_BIT;

                // Pre-map
                buffer_data = gl::utils::map_pbo(
                    pbo.pbo_id, 
                    buffer_size, 
                    UnmapPBOStage::pbo_target, 
                    access);

                if (buffer_data == nullptr) {
                    FB_LOG_CRITICAL 
                        << "Couldn't map pbo '" << pbo.pbo_id 
                        << "' during construction of Map stage.";
                    throw std::runtime_error("Mapped buffer was null.");
                }

                TokenGL new_token(
                    0,
                    pbo.pbo_id,
                    pbo_image_format,
                    Time(0, 1),
                    0,
                    nullptr,
                    buffer_data
                    );

                init.push_back(new_token);
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "CopyHostToPBO",
                std::bind(
                    &CopyHostToMappedPBOStage::perform, 
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

#endif // TOA_FRAME_BENDER_COPY_HOST_TO_MAPPED_PBO_STAGE_H
