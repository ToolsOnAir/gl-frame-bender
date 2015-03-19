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
#ifndef TOA_FRAME_BENDER_FORMAT_CONVERTER_STAGE_H
#define TOA_FRAME_BENDER_FORMAT_CONVERTER_STAGE_H

#include <vector>
#include <map>
#include <sstream>
#include <string>
#include <cmath>

#include "Stage.h"
#include "StageDataTypes.h"
#include "StageSampler.h"
#include "Logging.h"
#include "PipelineStage.h"
#include "FormatOptions.h"

#include "Utils.h"

#include "TimeSampler.h"

#include "ProgramOptions.h"

namespace toa { 

    namespace frame_bender {

        namespace gl {
            class Quad;
        }

        // TODO: this execut should get a little more importance
        // For multiple composition it should interleave or manage 
        // their rendering in a smart way.

        class FormatConverterStage : public utils::NoCopyingOrMoving, public PipelineStage  {

        public:

            typedef Stage<TokenGL, TokenGL> StageType;

            template <typename InputStage>
            FormatConverterStage(
                size_t pipeline_size,
                ImageFormat input_format,
                ImageFormat output_format,
                bool enable_gl_upstream_synchronization,
                bool enable_gl_downstream_synchronization,
                const InputStage& input_stage, 
                bool enable_gl_timer_queries,
                Mode mode,
                // Only used when mode is glsl_420 or higher.
                GLbitfield gl_barrier_bitfield,
                // Only used when mode is glsl_430_compute
                size_t compute_shader_work_group_size,
                ChromaFilter chroma_filter
                );

            ~FormatConverterStage();

            void execute() override { stage_.execute(); }
            PipelineStatus status() const override { return stage_.status(); }
            size_t input_queue_num_elements() const override { return stage_.input_queue_num_elements(); }
            const std::string& name() const override { return stage_.name(); }
            size_t output_queue_size() const override { return stage_.output_queue_size(); } 

            const StageType& stage() const { return stage_; }

            const StageSampler& sampler() const { return sampler_; }

        private:

            // DEBUG: remove me
            bool is_v210_decoder() {
                return (input_format_.pixel_format() == ImageFormat::PixelFormat::YUV_10BIT_V210);
            }

            void initialize_resources(size_t num_of_buffers, const std::string& name);

            StageCommand perform(TokenGL& tex_in_token, TokenGL& tex_out_token);

            StageType stage_;

            // TODO: Extend if other parameters can be part of the conversion
            // process as well.
            typedef ImageFormat::PixelFormat ConversionParameters;
            typedef std::map<std::tuple<ConversionParameters, ConversionParameters, Mode>, std::string> ConversionShaderMap;

            static ConversionShaderMap available_conversion_shaders();

            static bool is_passthrough(
                const ImageFormat& input_format, 
                const ImageFormat& output_format);


            ImageFormat input_format_;
            ImageFormat output_format_;

            GLuint program_id_;
            GLuint pipeline_id_;

            std::unique_ptr<gl::Quad> quad_;

            bool enable_gl_upstream_synchronization_;
            bool enable_gl_downstream_synchronization_;

            std::vector<GLuint> texture_ids_;
            std::vector<GLuint> fbo_ids_;
            StageSampler sampler_;
            std::unique_ptr<gl::TimeSampler> gl_sampler_;

            std::vector<GLuint> dummy_fbo_attachments_;

            Mode mode_;

            bool is_passthrough_;

            // TODO-DEF: this should really be stuck to the image format, 
            // something like "conversion" ratio, or maybe to the 
            // conversion shader definitions... 
            size_t viewport_width_;
            size_t viewport_height_;
            GLbitfield gl_barrier_bitfield_;
            size_t format_group_divisor_;
            size_t compute_shader_work_group_size_;
            GLuint num_work_groups_x_;
            GLuint num_work_groups_y_;
            bool deal_with_excess_threads_;

            ChromaFilter chroma_filter_;
        };   

        std::ostream& operator<< (std::ostream& out, const Mode& v);
        std::istream& operator>>(std::istream& in, Mode& v); 

        std::ostream& operator<< (std::ostream& out, const ChromaFilter& v);
        std::istream& operator>>(std::istream& in, ChromaFilter& v); 

        template <typename InputStage>
        FormatConverterStage::FormatConverterStage(       
                size_t pipeline_size,
                ImageFormat input_format,
                ImageFormat output_format,
                bool enable_gl_upstream_synchronization,
                bool enable_gl_downstream_synchronization,
                const InputStage& input_stage,
                bool enable_gl_timer_queries,
                Mode mode,
                GLbitfield gl_barrier_bitfield,
                size_t compute_shader_work_group_size,
                ChromaFilter chroma_filter) :
                    input_format_(std::move(input_format)),
                    output_format_(std::move(output_format)),
                    program_id_(0),
                    pipeline_id_(0),
                    enable_gl_upstream_synchronization_(enable_gl_upstream_synchronization),
                    enable_gl_downstream_synchronization_(enable_gl_downstream_synchronization),
                    mode_(mode),
                    viewport_width_(0),
                    viewport_height_(0),
                    gl_barrier_bitfield_(gl_barrier_bitfield),
                    format_group_divisor_(0),
                    compute_shader_work_group_size_(compute_shader_work_group_size),
                    num_work_groups_x_(0),
                    num_work_groups_y_(0),
                    deal_with_excess_threads_(false),
                    chroma_filter_(chroma_filter)
        {

            is_passthrough_ = is_passthrough(input_format_, output_format_);

            if (is_passthrough_) {
                FB_LOG_INFO << "Falling back to GLSL 330 renderer for passthrough conversion.";
                mode_ = Mode::glsl_330;
            }

            // TODO-DEF: how could we make this more generic?
            if (input_format_.pixel_format() == ImageFormat::PixelFormat::YUV_10BIT_V210 && 
                output_format_.pixel_format() != ImageFormat::PixelFormat::YUV_10BIT_V210) 
            {

                format_group_divisor_ = 4;

            } else if (output_format_.pixel_format() == ImageFormat::PixelFormat::YUV_10BIT_V210 && 
                input_format_.pixel_format() != ImageFormat::PixelFormat::YUV_10BIT_V210)
            {

                format_group_divisor_ = 6;

            }

            if (mode_ == Mode::glsl_430_compute || mode_ == Mode::glsl_430_compute_no_shared) {

                size_t x_extent = input_format_.gl_native_format().tex_width / format_group_divisor_;
                float work_groups_x = static_cast<float>(x_extent) / compute_shader_work_group_size_;
                num_work_groups_x_ = static_cast<GLuint>(std::ceil(work_groups_x));
                num_work_groups_y_ = static_cast<GLuint>(input_format_.gl_native_format().tex_height);

                if ((x_extent % compute_shader_work_group_size_) != 0) {

                    float integral = .0f;
                    float fraction = std::modf(work_groups_x, &integral);
                    size_t excess_threads = static_cast<size_t>(fraction * compute_shader_work_group_size_);

                    FB_LOG_WARNING 
                        << "The setting of the work group size of '" 
                        << compute_shader_work_group_size_ 
                        << "' will result in some excess computation. "
                        << "X-dim = " << x_extent << ", therefore an "
                        << "excess of '" << excess_threads 
                        << "' threads will be executed and the compute shader "
                        << "will have to additionally deal with that.";

                    deal_with_excess_threads_ = true;

                }

            }

            FB_LOG_DEBUG << 
                "Creating format converter between '" << input_format << 
                "' (input) and '" << output_format << 
                "' (output) using mode '" << mode_ << "' with " 
                << pipeline_size << " number of buffers.";

            FB_LOG_DEBUG << "Using chroma filter '" << chroma_filter_ << "'.";

            if (ProgramOptions::global().v210_use_shader_for_component_access()) {
                FB_LOG_DEBUG << "Using shaders for V210 component access.";
            } else {
                FB_LOG_DEBUG << "Not using shaders for V210 component access, but 10_10_10_2 pixel format.";
            }

            std::ostringstream oss;
            oss << "FormatConverter (" 
                << input_format_.pixel_format() 
                << " -> " 
                << output_format_.pixel_format() 
                << ")";

            std::string name = oss.str();

            initialize_resources(pipeline_size, name);

            if (enable_gl_timer_queries) {

                gl_sampler_ = utils::make_unique<gl::TimeSampler>(
                    &sampler_, 
                    StageExecutionState::GL_TASK_BEGIN, 
                    StageExecutionState::GL_TASK_END);

            }

            std::vector<TokenGL> init;

            auto itr_tex = std::begin(texture_ids_);
            auto itr_fbo = std::begin(fbo_ids_);

            if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {
                FB_ASSERT(texture_ids_.size() == fbo_ids_.size());
            }

            for (;itr_tex != std::end(texture_ids_); ++itr_tex) {

                GLuint fbo_id = 0;
                if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {
                    fbo_id = *itr_fbo;
                }

                TokenGL new_token(
                    0,
                    *itr_tex,
                    output_format_,
                    0,
                    fbo_id,
                    nullptr,
                    nullptr
                    );
                init.push_back(new_token);

                if (itr_fbo != std::end(fbo_ids_))
                    ++itr_fbo;
            }

            SampleFunction fnc;

            if (ProgramOptions::global().sample_stages()) {
                fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
            }

            stage_ = utils::create_stage<typename InputStage::StageType, TokenGL>(
                "ConvertFormat",
                std::bind(
                    &FormatConverterStage::perform, 
                    this, 
                    std::placeholders::_1,
                    std::placeholders::_2),
                init,
                input_stage.stage(),
                std::move(fnc));

        }

    }

}

#endif // TOA_FRAME_BENDER_FORMAT_CONVERTER_STAGE_H
