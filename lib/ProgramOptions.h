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
#ifndef TOA_FRAME_BENDER_PROGRAM_OPTIONS_H
#define TOA_FRAME_BENDER_PROGRAM_OPTIONS_H

#include <string>
#include <stdexcept>

#include "Logging.h"
#include "ImageFormat.h"
#include "Quad.h"
#include "FrameTime.h"
#include "FormatOptions.h"
#include "StreamDispatch.h"
#include "Context.h"

#ifndef _MSC_VER
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif

namespace toa {

    namespace frame_bender {

        /**
         * A simple class statically wrapping program-wide options which 
         * are read in via the command line;
         */
        class ProgramOptions final {

        public:

            static const ProgramOptions& global();

            static void initialize_global(int argc, const char* argv[]);
            static void set_global(const ProgramOptions& opts);

            /**
             * Throws InvalidInput if input is not valid.
             */
            ProgramOptions(int argc, const char* argv[]);

            bool use_debug_gl_context() const;
            const std::string& config_file() const;
            
            bool show_help() const;

            const std::string& help() const;

            const std::string& log_file() const;
            bool log_to_stdout() const;
            logging::Severity min_logging_severity() const;
            
            bool arb_debug_synchronous_mode() const;

            size_t upload_pbo_rr_count() const;
            size_t upload_copy_to_unmap_pipeline_size() const;
            size_t upload_unmap_to_unpack_pipeline_size() const;

            size_t download_pbo_rr_count() const;
            size_t source_texture_rr_count() const;
            size_t destination_texture_rr_count() const;
            size_t frame_output_cache_count() const;
            size_t frame_input_pipeline_size() const;
            size_t download_map_to_copy_pipeline_size() const;
            size_t download_pack_to_map_pipeline_size() const;

            size_t player_width() const;
            size_t player_height() const; 
            bool player_is_fullscreen() const;
            bool player_use_vsync() const;

            const std::string& glsl_folder() const;
            bool blit_to_player() const;
            bool v210_use_shader_for_component_access() const;

            ImageFormat::PixelFormat input_sequence_pixel_format() const;

            const std::string& input_sequences_folder() const;
            const std::string& input_sequence_name() const;
            const std::string& input_sequence_pattern() const; 
            size_t input_sequence_width() const;
            size_t input_sequence_height() const;
            ImageFormat::Origin input_sequence_origin() const;
            const Time& input_sequence_frame_duration() const;
            ImageFormat::Transfer input_sequence_transfer() const;
            size_t input_sequence_loop_count() const;
            const std::string& textures_folder() const; 

            ImageFormat::PixelFormat render_pixel_format() const;
            ImageFormat::Transfer render_transfer() const;

            bool sample_stages() const;
            bool enable_output_stages() const;
            bool enable_input_stages() const;
            bool enable_render_stages() const;

            bool enable_host_copy_upload() const;
            bool enable_host_copy_download() const;

            bool force_passthrough_renderer() const;

            size_t statistics_skip_first_num_frames() const;

            bool enable_upload_gl_timer_queries() const;
            bool enable_render_gl_timer_queries() const;
            bool enable_download_gl_timer_queries() const;

            Mode format_conversion_mode() const;

            size_t v210_decode_glsl_430_work_group_size() const;
            size_t v210_encode_glsl_430_work_group_size() const;

            const std::string& demo_renderer_lower_third_image() const;
            const std::string& demo_renderer_logo_image() const;

            ChromaFilter v210_decode_glsl_chroma_filter() const;
            ChromaFilter v210_encode_glsl_chroma_filter() const;

            bool enable_window_user_input() const;

            const std::string& write_output_folder() const;
            bool enable_write_output() const;

            bool write_output_swap_endianness() const;
            size_t write_output_swap_endianness_word_size() const;
            bool enable_linear_space_rendering() const;

            const StreamDispatch::FlagContainer& optimization_flags() const;

            gl::Context::DebugSeverity min_debug_gl_context_severity() const;

            size_t pipeline_download_pack_to_map_load_constraint_count() const;
            size_t pipeline_upload_unmap_to_unpack_load_constraint_count() const;

            size_t pipeline_download_format_converter_to_pack_load_constraint_count() const;
            size_t pipeline_upload_unpack_to_format_converter_load_constraint_count() const;

            const std::string& trace_output_file() const;

            const std::string& config_output_file() const;

            void write_config_to_file(const std::string& file_path) const;

            const std::string& profiling_trace_name() const;

        private:

            static std::unique_ptr<const ProgramOptions> global_options;

            bool use_debug_gl_context_;
            std::string config_file_;
            bool show_help_;

            std::string help_;
            std::string log_file_;
            bool log_to_stdout_;
            logging::Severity min_logging_severity_;
            bool arb_debug_synchronous_mode_;
            size_t upload_pbo_rr_count_;
            size_t download_pbo_rr_count_;
            size_t upload_copy_to_unmap_pipeline_size_;
            size_t upload_unmap_to_unpack_pipeline_size_;
            size_t source_texture_rr_count_;
            size_t destination_texture_rr_count_;
            size_t frame_input_pipeline_size_;
            size_t frame_output_cache_count_;
            size_t download_map_to_copy_pipeline_size_;
            size_t download_pack_to_map_pipeline_size_;
            size_t player_width_;
            size_t player_height_;
            bool player_is_fullscreen_;
            bool player_use_vsync_;
            std::string glsl_folder_;
            bool blit_to_player_;
            bool v210_use_shader_for_component_access_;
            ImageFormat::PixelFormat input_sequence_pixel_format_;
            std::string input_sequences_folder_;
            std::string input_sequence_name_;
            std::string input_sequence_pattern_;
            size_t input_sequence_width_;
            size_t input_sequence_height_;
            ImageFormat::Origin input_sequence_origin_;
            std::string textures_folder_;
            ImageFormat::PixelFormat render_pixel_format_;
            ImageFormat::Transfer render_transfer_;
            ImageFormat::Transfer input_sequence_transfer_;
            Time input_sequence_frame_duration_;
            bool sample_stages_;
            size_t input_sequence_loop_count_;
            bool enable_output_stages_;
            bool enable_input_stages_;
            bool enable_render_stages_;
            bool force_passthrough_renderer_;
            bool enable_host_copy_upload_;
            bool enable_host_copy_download_;
            size_t statistics_skip_first_num_frames_;
            bool enable_upload_gl_timer_queries_;
            bool enable_render_gl_timer_queries_;
            bool enable_download_gl_timer_queries_;
            Mode format_conversion_mode_;
            size_t v210_decode_glsl_430_work_group_size_;
            size_t v210_encode_glsl_430_work_group_size_;
            std::string demo_renderer_lower_third_image_;
            std::string demo_renderer_logo_image_;

            ChromaFilter v210_decode_glsl_chroma_filter_;
            ChromaFilter v210_encode_glsl_chroma_filter_;

            bool enable_window_user_input_;

            std::string write_output_folder_;
            bool enable_write_output_;

            bool write_output_swap_endianness_;
            size_t write_output_swap_endianness_word_size_;

            bool enable_linear_space_rendering_;

            StreamDispatch::FlagContainer optimization_flags_;

            gl::Context::DebugSeverity min_debug_gl_context_severity_;

            size_t pipeline_download_pack_to_map_load_constraint_count_;
            size_t pipeline_upload_unmap_to_unpack_load_constraint_count_;
            size_t pipeline_download_format_converter_to_pack_load_constraint_count_;
            size_t pipeline_upload_unpack_to_format_converter_load_constraint_count_;

            std::string trace_output_file_;

            std::string config_output_file_contents_;

            std::string config_output_file_;

            std::string profiling_trace_name_;
        };

        class InvalidInput : public std::exception {
        public:
			const char* what() const NOEXCEPT override;

            friend class ProgramOptions;
        private:
            InvalidInput(const char* msg);
            std::string msg_;
        };

    }

}

#endif // TOA_FRAME_BENDER_PROGRAM_OPTIONS_H
