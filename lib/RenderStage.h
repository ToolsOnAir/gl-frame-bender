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
#ifndef TOA_FRAME_BENDER_RENDER_STAGE_H
#define TOA_FRAME_BENDER_RENDER_STAGE_H

#include <vector>
#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "PipelineStage.h"
#include "ProgramOptions.h"

#include "Utils.h"
#include "Quad.h"
#include "TimeSampler.h"

namespace toa { 

    namespace frame_bender {

        class RenderStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenGL, TokenGL> StageType;

            template <typename InputStage>
            RenderStage(
                size_t pipeline_size,
                uint32_t render_width,
                uint32_t render_height,
                const InputStage& input_stage,
                const ImageFormat& render_format,
                bool enable_gl_timer_queries);

            ~RenderStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const ImageFormat& render_format() const { return render_format_; }
            
            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            void initialize_resources(size_t num_of_buffers);

            StageCommand perform(
                TokenGL& src_tex_in_token, 
                TokenGL& dst_tex_out_token);

            StageType stage_;
            StageSampler sampler_;
            std::unique_ptr<gl::TimeSampler> gl_sampler_;

            std::vector<GLuint> texture_ids_;
            std::vector<GLuint> fbo_ids_;

            ImageFormat render_format_;

            // Used for blitting
            // TODO: think about whether we should add a "display"
            // stage here? ... probably not.
            GLuint blit_program_id_;
            GLuint blit_pipeline_id_;

            gl::Quad quad_;


        };        

        template <typename InputStage>
        RenderStage::RenderStage(
            size_t pipeline_size,
            uint32_t render_width,
            uint32_t render_height,
            const InputStage& input_stage,
            const ImageFormat& render_format,
            bool enable_gl_timer_queries) :
                render_format_(render_format),
                blit_program_id_(0),
                blit_pipeline_id_(0),
                quad_(gl::Quad::UV_ORIGIN::LOWER_LEFT)
        {

            initialize_resources(pipeline_size);

            if (enable_gl_timer_queries) {

                gl_sampler_ = utils::make_unique<gl::TimeSampler>(
                    &sampler_, 
                    StageExecutionState::GL_TASK_BEGIN, 
                    StageExecutionState::GL_TASK_END);
            }

            std::vector<TokenGL> init;

            auto itr_tex = std::begin(texture_ids_);
            auto itr_fbo = std::begin(fbo_ids_);

            FB_ASSERT(texture_ids_.size() == fbo_ids_.size());

            for (;itr_tex != std::end(texture_ids_); ++itr_tex, ++itr_fbo ) {
                TokenGL new_token(
                    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0),
                    *itr_tex,
                    render_format_,
                    0,
                    *itr_fbo,
                    nullptr,
                    nullptr
                    );
                init.push_back(new_token);
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "Render",
                std::bind(
                    &RenderStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                init,
                input_stage.stage(),
                std::move(fnc));

        }

    }

}

#endif // TOA_FRAME_BENDER_UNPACK_PBO_TO_TEXTURE_STAGE_H
