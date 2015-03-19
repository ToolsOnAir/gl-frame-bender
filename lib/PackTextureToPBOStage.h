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
#ifndef TOA_FRAME_BENDER_PACK_TEXTURE_TO_PBO_STAGE_H
#define TOA_FRAME_BENDER_PACK_TEXTURE_TO_PBO_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "PipelineStage.h"
#include "ProgramOptions.h"
#include "MapPBOStage.h"

#include "Utils.h"
#include "TimeSampler.h"

namespace toa { 

    namespace frame_bender {

        // TODO: this execut should get a little more importance
        // For multiple composition it should interleave or manage 
        // their rendering in a smart way.

        class PackTextureToPBOStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenGL, TokenGL> StageType;

            template <typename InputStage>
            PackTextureToPBOStage(
                // These are expected to be "unmapped" and also define
                // the pipeline size of this stage
                // This is considered a shared resource, and the creator
                // of this class instance is responsible for holding 
                // the pbo array valid as long as this stage exists
                const PboMemVector& startup_pipeline_pbos,
                ImageFormat pack_format,
                const InputStage& input_stage,
                bool enable_gl_timer_queries,
                bool enable_downstream_gl_synchronization,
                bool enable_upstream_gl_synchronization,
                bool pbo_memory_is_persistently_mapped);

            ~PackTextureToPBOStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageCommand perform(TokenGL& dst_tex_in_token, TokenGL& pbo_out_token);

            StageType stage_;

            StageSampler sampler_;

            std::unique_ptr<gl::TimeSampler> gl_sampler_;
            bool enable_downstream_gl_synchronization_;
            bool enable_upstream_gl_synchronization_;

            bool pbo_memory_is_persistently_mapped_;
        };        

        template <typename InputStage>
        PackTextureToPBOStage::PackTextureToPBOStage(
            const PboMemVector& startup_pipeline_pbos,
            ImageFormat pack_format,
            const InputStage& input_stage,
            bool enable_gl_timer_queries,
            bool enable_downstream_gl_synchronization,
            bool enable_upstream_gl_synchronization,
            bool pbo_memory_is_persistently_mapped) :
                enable_downstream_gl_synchronization_(enable_downstream_gl_synchronization),
                enable_upstream_gl_synchronization_(enable_upstream_gl_synchronization),
                pbo_memory_is_persistently_mapped_(pbo_memory_is_persistently_mapped)
        {

            if (enable_gl_timer_queries) {

                gl_sampler_ = utils::make_unique<gl::TimeSampler>(
                    &sampler_, 
                    StageExecutionState::GL_TASK_BEGIN, 
                    StageExecutionState::GL_TASK_END);

            }

            std::vector<TokenGL> pbo_down_init;

            size_t buffer_size = pack_format.image_byte_size();

            // These need to be pre-mapped
            for (auto& pbo : startup_pipeline_pbos) {

                uint8_t* buffer_data = nullptr;

                if (pbo_memory_is_persistently_mapped_) {

                    GLbitfield access = MapPBOStage::pbo_access;

                    access |= GL_MAP_PERSISTENT_BIT;

                    // Pre-map
                    buffer_data = gl::utils::map_pbo(
                        pbo.pbo_id, 
                        buffer_size, 
                        MapPBOStage::pbo_target, 
                        access);

                    if (buffer_data == nullptr) {
                        FB_LOG_CRITICAL 
                            << "Couldn't map pbo '" << pbo.pbo_id 
                            << "' during construction of Map stage.";
                        throw std::runtime_error("Mapped buffer was null.");
                    }
                }

                TokenGL new_token(
                    0,
                    pbo.pbo_id,
                    pack_format,
                    Time(0, 1),
                    0,
                    nullptr,
                    buffer_data
                    );

                pbo_down_init.push_back(new_token);
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "PackPBO",
                std::bind(
                    &PackTextureToPBOStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                pbo_down_init,
                input_stage.stage(),
                std::move(fnc));

        }

    }

}

#endif // TOA_FRAME_BENDER_PACK_TEXTURE_TO_PBO_STAGE_H
