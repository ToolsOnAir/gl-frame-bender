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
#ifndef TOA_FRAME_BENDER_UNPACK_PBO_TO_TEXTURE_STAGE_H
#define TOA_FRAME_BENDER_UNPACK_PBO_TO_TEXTURE_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "TimeSampler.h"
#include "PipelineStage.h"
#include "ProgramOptions.h"

#include "Utils.h"

namespace toa { 

    namespace frame_bender {

        // TODO: this execut should get a little more importance
        // For multiple composition it should interleave or manage 
        // their rendering in a smart way.

        class UnpackPBOToTextureStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenGL, TokenGL> StageType;

            template <typename InputStage>
            UnpackPBOToTextureStage(
                size_t pipeline_size,
                ImageFormat image_format,
                const InputStage& input_stage,
                bool enable_gl_timer_queries,
                bool enable_downstream_gl_synchronization,
                bool enable_upstream_gl_synchronization);

            ~UnpackPBOToTextureStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            // TODO: can we find something more elegant than this?
            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            void initialize_resources(size_t num_of_buffers);

            StageCommand perform(TokenGL& pbo_in_token, TokenGL& tex_out_token);

            StageType stage_;
            StageSampler sampler_;
            std::unique_ptr<gl::TimeSampler> gl_sampler_;

            std::vector<GLuint> texture_ids_;

            ImageFormat image_format_;

            bool enable_downstream_gl_synchronization_;
            bool enable_upstream_gl_synchronization_;
        };        

        template <typename InputStage>
        UnpackPBOToTextureStage::UnpackPBOToTextureStage(
            size_t pipeline_size,
            ImageFormat image_format,
            const InputStage& input_stage,
            bool enable_gl_timer_queries,
            bool enable_downstream_gl_synchronization,
            bool enable_upstream_gl_synchronization) :
                image_format_(std::move(image_format)),
                enable_downstream_gl_synchronization_(enable_downstream_gl_synchronization),
                enable_upstream_gl_synchronization_(enable_upstream_gl_synchronization)
        {

            initialize_resources(pipeline_size);

            if (enable_gl_timer_queries) {

                gl_sampler_ = utils::make_unique<gl::TimeSampler>(
                    &sampler_, 
                    StageExecutionState::GL_TASK_BEGIN, 
                    StageExecutionState::GL_TASK_END);

            }

            std::vector<TokenGL> tex_init;

            for (auto& tex_id : texture_ids_) {
                TokenGL new_token(
                    0,
                    tex_id,
                    image_format_, 
                    0, 
                    0, 
                    nullptr,
                    nullptr);
                tex_init.push_back(new_token);
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "UnpackPBO",
                std::bind(
                    &UnpackPBOToTextureStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                tex_init,
                input_stage.stage(),
                std::move(fnc));

        }

    }

}

#endif // TOA_FRAME_BENDER_UNPACK_PBO_TO_TEXTURE_STAGE_H
