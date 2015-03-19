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
#ifndef TOA_FRAME_BENDER_UNMAP_PBO_STAGE_H
#define TOA_FRAME_BENDER_UNMAP_PBO_STAGE_H

#include "Stage.h"
#include "Frame.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "StringifyEnums.h"
#include "PipelineStage.h"

#include "Utils.h"
#include "UtilsGL.h"
#include "TimeSampler.h"
#include "ProgramOptions.h"

namespace toa {

    namespace frame_bender {

        class StreamComposition;
        class UnmapPBOStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            static const GLenum pbo_target = GL_PIXEL_UNPACK_BUFFER;
            static const GLbitfield pbo_access = GL_MAP_WRITE_BIT;

            typedef Stage<TokenGL, TokenGL> StageType;

            template <typename InputStage>
            UnmapPBOStage(
                // These are expected to be "unmapped" and also define
                // the pipeline size of this stage. They will be mapped in 
                // the constructor in here... 
                // This is considered a shared resource, and the creator
                // of this class instance is responsible for holding 
                // the pbo array valid as long as this stage exists
                const PboMemVector& startup_pipeline_pbos,
                ImageFormat pbo_image_format,
                const InputStage& input_stage,
                bool enable_gl_timer_queries,
                bool pbo_memory_is_persistently_mapped);

            ~UnmapPBOStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            StageCommand perform(TokenGL& token_in, TokenGL& token_out);

            StageType stage_;

            StageSampler sampler_;
            std::unique_ptr<gl::TimeSampler> gl_sampler_;
            bool pbo_memory_is_persistently_mapped_;

            uint64_t gl_client_sync_already_signalled_count_;
            uint64_t gl_client_sync_condition_satifsied_count_;
            uint64_t gl_client_sync_timeout_count_;
            uint64_t gl_client_sync_already_deleted_;
        };        

        template <typename InputStage>
        UnmapPBOStage::UnmapPBOStage(
            const PboMemVector& startup_pipeline_pbos,
            ImageFormat pbo_image_format,
            const InputStage& input_stage,
            bool enable_gl_timer_queries,
            bool pbo_memory_is_persistently_mapped) :
                pbo_memory_is_persistently_mapped_(pbo_memory_is_persistently_mapped),
                gl_client_sync_already_signalled_count_(0),
                gl_client_sync_condition_satifsied_count_(0),
                gl_client_sync_timeout_count_(0),
                gl_client_sync_already_deleted_(0)
        {
            if (pbo_memory_is_persistently_mapped_) {
                FB_LOG_INFO << "Unmap stage uses persistently-mapped memory, and "
                    "will explicitly synchronize with the GL.";
            }

            if (enable_gl_timer_queries) {

                gl_sampler_ = utils::make_unique<gl::TimeSampler>(
                    &sampler_, 
                    StageExecutionState::GL_TASK_BEGIN, 
                    StageExecutionState::GL_TASK_END);

            }

            std::vector<TokenGL> init;

            size_t buffer_size = pbo_image_format.image_byte_size();

            // These need to be pre-mapped
            for (auto& pbo : startup_pipeline_pbos) {

                uint8_t* buffer_data = nullptr;

                // If we use persistently mapped, then we need to get
                // and set the client ptr in here
                if (pbo_memory_is_persistently_mapped_) {

                    GLbitfield access = UnmapPBOStage::pbo_access;

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
                fnc = std::bind(
                    &StageSampler::sample, 
                    &sampler_, 
                    std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "UnmapPBO",
                std::bind(
                    &UnmapPBOStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                init,
                input_stage.stage(),
                std::move(fnc));

        }

    }

}

#endif // TOA_FRAME_BENDER_UNMAP_PBO_STAGE_H
