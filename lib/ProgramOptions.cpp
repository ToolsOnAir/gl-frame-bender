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
#include "Precompile.h"
#include "ProgramOptions.h"
#include "Utils.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "FormatConverterStage.h"

namespace fb = toa::frame_bender;
namespace po = boost::program_options;
namespace bf = boost::filesystem;

fb::InvalidInput::InvalidInput(const char* what) {
    if (what != nullptr) {
        msg_ = std::string(what);
    }
}

const char* fb::InvalidInput::what() const NOEXCEPT{
    return msg_.c_str();
}

std::unique_ptr<const fb::ProgramOptions> fb::ProgramOptions::global_options = nullptr;

const fb::ProgramOptions& fb::ProgramOptions::global() {
    if (global_options) {
        return *global_options;
    } else {
        throw std::runtime_error("Global options are not initialized yet.");
    }
}

void fb::ProgramOptions::initialize_global(int argc, const char* argv[]) {
    fb::ProgramOptions::global_options = utils::make_unique<fb::ProgramOptions>(argc, argv);
}

void fb::ProgramOptions::set_global(const ProgramOptions& opts) {
    fb::ProgramOptions::global_options = utils::make_unique<fb::ProgramOptions>(opts);
}

namespace boost {
    namespace program_options {
        template <>
        void validate<fb::StreamDispatch::FlagContainer, char> (boost::any& v,
            const std::vector<std::string>& values,
            fb::StreamDispatch::FlagContainer* target_type, long) 
        {

            fb::StreamDispatch::FlagContainer combined_flags;

            // Check if we have assigned something previously
            if (!v.empty()) {
                combined_flags = boost::any_cast<fb::StreamDispatch::FlagContainer>(v);
            } 

            for(const auto& token : values) 
            {
                fb::StreamDispatch::Flags new_token;
                std::istringstream ss(token);
                ss >> new_token;

                if (ss.fail()) {
                    throw po::validation_error(po::validation_error::invalid_option_value);
                }

                combined_flags.set(static_cast<size_t>(new_token));

            }

            v = boost::any(combined_flags);
        }
    }
}

fb::ProgramOptions::ProgramOptions(int argc, const char* argv[]) {

    try {
        // These are the options which can only be set via the command line 
        po::options_description generic("Generic options");
        generic.add_options()
            ("help", "Print out help message")
            ("config,c", 
            po::value<std::string>(&config_file_)->default_value(""),
            "Set the path to a configuration file to use. Leave empty to "
            "disable file-based configuration loading.");

        po::options_description config("Configuration");
        config.add_options()
            ("opengl.context.debug", 
            po::value<bool>(&use_debug_gl_context_)->default_value(false), 
            "If set to true, a debug OpenGL context is created, and OpenGL debugging is forwarded to the logger.")
            ("logging.output_file", 
            po::value<std::string>(&log_file_)->default_value("FrameBender.log"), 
            "Set the file which should contain logging of this program. "
            "Set to empty in order to disable file-based logging.h")
            ("logging.write_to_stdout", 
            po::value<bool>(&log_to_stdout_)->default_value(false), 
            "If set to true, logging to stdout will be enabled. "
            "Set to false in order to disable console-output of the logger.")
            ("logging.min_severity", 
            po::value<logging::Severity>(&min_logging_severity_)->default_value(logging::Severity::warning), 
            "Sets the minimum logging level that should be reported. Can be "
            "debug,info,warning,error,critical."
            )
            ("opengl.context.debug.is_synchronous", 
            po::value<bool>(&arb_debug_synchronous_mode_)->default_value(false), 
            "If set to true, and if a debug context is requested, the ARB GL "
            "debug output will be delivered synchronously to the driver, i.e. "
            "breakpoints can be set.")
            ("pipeline.upload.gl_pbo_count", 
            po::value<size_t>(&upload_pbo_rr_count_)->default_value(3), 
            "Sets the size of the round-robin queue for upload PBO buffers.")
            ("pipeline.upload.copy_to_unmap_queue_token_count", 
            po::value<size_t>(&upload_copy_to_unmap_pipeline_size_)->default_value(2), 
            "TODO: write documentation")
            ("pipeline.upload.unmap_to_unpack_queue_token_count", 
            po::value<size_t>(&upload_unmap_to_unpack_pipeline_size_)->default_value(1), 
            "TODO: write documentation")
            ("pipeline.download.gl_pbo_count", 
            po::value<size_t>(&download_pbo_rr_count_)->default_value(3), 
            "Sets the size of the round-robin queue for download PBO buffers.")
            ("pipeline.upload.gl_texture_count", 
            po::value<size_t>(&source_texture_rr_count_)->default_value(3), 
            "Sets the size of the round-robin queue for source textures.")
            ("pipeline.download.gl_texture_count", 
            po::value<size_t>(&destination_texture_rr_count_)->default_value(3), 
            "Sets the size of the round-robin queue for destination textures.")
            ("pipeline.output.token_count", 
            po::value<size_t>(&frame_output_cache_count_)->default_value(3), 
            "Sets the size of the frame output cache, i.e. the number of "
            "pre-allocated output frames.")
            ("pipeline.input.token_count", 
            po::value<size_t>(&frame_input_pipeline_size_)->default_value(3), 
            "TODO: write doc.")
            ("pipeline.download.map_to_copy_queue_token_count", 
            po::value<size_t>(&download_map_to_copy_pipeline_size_)->default_value(2), 
            "Sets the size of the pipeline from download 'map' stage to async 'copy'"
            " stage. This value added by 'pipeline.download.pack_to_map_queue_token_count' "
            "has to be the sum of 'pipeline.download.gl_pbo_count'.")
            ("pipeline.download.pack_to_map_queue_token_count", 
            po::value<size_t>(&download_pack_to_map_pipeline_size_)->default_value(1), 
            "Sets the size of the pipeline from download 'pack' stage to 'map'"
            " stage. This value added by 'pipeline.download.map_to_copy_queue_token_count' "
            "has to be the sum of 'pipeline.download.gl_pbo_count'.")
            ("player.width",
            po::value<size_t>(&player_width_)->default_value(640),
            "Width of the player screen.")
            ("player.height",
            po::value<size_t>(&player_height_)->default_value(360),
            "Height of the player screen.")
            ("player.is_fullscreen",
            po::value<bool>(&player_is_fullscreen_)->default_value(false),
            "If set to true, the player will start in fullscreen mode.")
            ("player.sync_to_vertical_refresh",
            po::value<bool>(&player_use_vsync_)->default_value(true),
            "If set to true, the player will synchronize with the VSync interval.")
            ("program.glsl_sources_location", 
            po::value<std::string>(&glsl_folder_)->default_value("./glsl"), 
            "Sets the folder in which GLSL source files should be searched for.")
            ("player.is_enabled", 
            po::value<bool>(&blit_to_player_)->default_value(true), 
            "If set to true, each rendered video frame will be blit to "
            "player's framebuffer and the player will use double-buffering.")
            ("render.format_conversion.v210.bitextraction_in_shader_is_enabled", 
            po::value<bool>(&v210_use_shader_for_component_access_)->default_value(true), 
            "If set to true, shaders will be used for unpacking the V210 "
            "compressed word into three 10-bit components. If false, a "
            "GL_UNSIGNED_INT_2_10_10_10_REV will be used for unpacking. ")
            ("input.sequence.pixel_format",
            po::value<fb::ImageFormat::PixelFormat>(&input_sequence_pixel_format_)->default_value(ImageFormat::PixelFormat::YUV_10BIT_V210),
            "The pixel format of the input sequence. Use -help for a list of supported pixel format.")
            ("program.sequences_location",
            po::value<std::string>(&input_sequences_folder_)->default_value("test_data"),
            "The folder in which input sequences are stored. Everything sequence_name + pattern is relative to this.")
            ("input.sequence.id",
            po::value<std::string>(&input_sequence_name_)->default_value("horse_v210_1920_1080p_short"),
            "The name (i.e. folder) of the sequence within the input sequence folder.")
            ("input.sequence.file_pattern",
            po::value<std::string>(&input_sequence_pattern_)->default_value(".*\\.v210"),
            "The pattern of the sequence frames to load.")
            ("input.sequence.width",
            po::value<size_t>(&input_sequence_width_)->default_value(1920),
            "Image width of the input sequence's frames.")
            ("input.sequence.height",
            po::value<size_t>(&input_sequence_height_)->default_value(1080),
            "Image height of the input sequence's frames.")
            ("input.sequence.loop_count",
            po::value<size_t>(&input_sequence_loop_count_)->default_value(10),
            "Number of times to replay the input sequence.")
            ("input.sequence.image_origin",
            po::value<fb::ImageFormat::Origin>(&input_sequence_origin_)->default_value(fb::ImageFormat::Origin::UPPER_LEFT),
            "Image origin of the input sequence's frames.")
            ("input.sequence.frame_duration",
            po::value<fb::Time>(&input_sequence_frame_duration_)->default_value(fb::Time(1, 25)),
            "Time duration of one input sequence's frame (inverse of "
            "framerate, e.g. 1/25 for 25fps).")
            ("input.sequence.image_transfer",
            po::value<fb::ImageFormat::Transfer>(&input_sequence_transfer_)->default_value(fb::ImageFormat::Transfer::BT_709),
            "Image transfer of the input sequence's frames.")
            ("program.textures_location",
            po::value<std::string>(&textures_folder_)->default_value("textures"),
            "The folder from which textures are loaded.")
            ("render.pixel_format",
            po::value<fb::ImageFormat::PixelFormat>(&render_pixel_format_)->default_value(ImageFormat::PixelFormat::RGBA_8BIT),
            "The internal pixel format of the renderer. Use -help for a list of supported pixel format.")
            ("render.image_transfer",
            po::value<fb::ImageFormat::Transfer>(&render_transfer_)->default_value(ImageFormat::Transfer::LINEAR),
            "The internal transfer of the renderer. Use -help for a list of supported transfers.")
            ("profiling.stage_sampling_is_enabled",
            po::value<bool>(&sample_stages_)->default_value(false),
            "If enable, then the stages of the pipeline will be sampled, and a statistics summary report will be added to the log output.")
            ("debug.output_is_enabled",
            po::value<bool>(&enable_output_stages_)->default_value(true),
            "Enables/disables the execution of the output pipeline stages, i.e. downloading of video frames.")
            ("debug.input_is_enabled",
            po::value<bool>(&enable_input_stages_)->default_value(true),
            "Enables/disables the execution of the input pipeline stages, i.e. uploading to video frames.")
            ("debug.rendering_is_enabled",
            po::value<bool>(&enable_render_stages_)->default_value(true),
            "Enables/disables the execution of the render pipeline stages, i.e. rendering onto video frames.")
            ("debug.passthrough_renderer_is_forced",
            po::value<bool>(&force_passthrough_renderer_)->default_value(false),
            "When enabled, the renderer used is the passthrough renderer.")
            ("profiling.statistics.first_frames_skipped_count",
            po::value<size_t>(&statistics_skip_first_num_frames_)->default_value(20),
            "Skip the first N frames for calculating the statistics summary. "
            "Use this value 'warmup' of the pipeline execution in order to "
            "retrieve more accurate statistics.")
            ("debug.host_input_copy_is_enabled",
            po::value<bool>(&enable_host_copy_upload_)->default_value(true),
            "Enable/Disable host-side copy operation into mapped GPU memory "
            "for the upload phase.")
            ("debug.host_output_copy_is_enabled",
            po::value<bool>(&enable_host_copy_download_)->default_value(true),
            "Enable/Disable host-side copy operation into mapped GPU memory "
            "for the download phase.")
            ("profiling.upload_gl_timer_queries_are_enabled",
            po::value<bool>(&enable_upload_gl_timer_queries_)->default_value(false),
            "Enables the use of GL timer queries to report on GL-side timings of the upload operations.")
            ("profiling.render_gl_timer_queries_are_enabled",
            po::value<bool>(&enable_render_gl_timer_queries_)->default_value(false),
            "Enables the use of GL timer queries to report on GL-side timings of the render operations.")
            ("profiling.download_gl_timer_queries_are_enabled",
            po::value<bool>(&enable_download_gl_timer_queries_)->default_value(false),
            "Enables the use of GL timer queries to report on GL-side timings of the download operations.")
            ("render.format_conversion_mode",
            po::value<Mode>(&format_conversion_mode_)->default_value(Mode::glsl_420_no_buffer_attachment_ext),
            "Sets the mode to use for the format conversion. Note that the hardware is required to support it. Otherwise the program will exit with an error.")
            ("render.format_conversion.v210.decode.glsl_430_work_group_size",
            po::value<size_t>(&v210_decode_glsl_430_work_group_size_)->default_value(16),
            "Sets the local work group size for the GLSL 430 compute shader "
            "decoding of V210 images. This option is only effective if "
            "render.format_conversion_mode is set to glsl_430_compute.")
            ("render.format_conversion.v210.encode.glsl_430_work_group_size",
            po::value<size_t>(&v210_encode_glsl_430_work_group_size_)->default_value(16),
            "Sets the local work group size for the GLSL 430 compute shader "
            "encoding of V210 images. This option is only effective if "
            "render.format_conversion_mode is set to glsl_430_compute.")
            ("render.demo.lower_third_image",
            po::value<std::string>(&demo_renderer_lower_third_image_)->default_value("fine_trans_lower_third_1080_8bit.png"),
            "Sets the lower third image for the demo renderer.")
            ("render.demo.logo_image",
            po::value<std::string>(&demo_renderer_logo_image_)->default_value("fine_trans_logo_1080_8bit.png"),
            "Sets the logo image for the demo renderer.")
            ("render.format_conversion.v210.decode.chroma_filter_type",
            po::value<fb::ChromaFilter>(&v210_decode_glsl_chroma_filter_)->default_value(ChromaFilter::basic),
            "Sets the complexity of the filter for YCbCr 422 decoding (upscaling filter).")
            ("render.format_conversion.v210.encode.chroma_filter_type",
            po::value<fb::ChromaFilter>(&v210_encode_glsl_chroma_filter_)->default_value(ChromaFilter::basic),
            "Sets the complexity of the filter for YCbCr 422 encoding (decimation filter).")
            ("player.user_input_is_enabled",
            po::value<bool>(&enable_window_user_input_)->default_value(true),
            "Must be enabled in order to react to user input evenst (mouse/keyboard).")
            ("output.is_enabled",
            po::value<bool>(&enable_write_output_)->default_value(false),
            "If enabled, all renderer frames will be written to 'output.location'.")
            ("output.location",
            po::value<std::string>(&write_output_folder_)->default_value("./render_output"),
            "Destination folder for rendered output frames.")
            ("write_output_swap_endianness",
            po::value<bool>(&write_output_swap_endianness_)->default_value(false),
            "If enabled, the output frame's raw image data will be byte-swapped.")
            ("write_output_swap_endianness_word_size",
            po::value<size_t>(&write_output_swap_endianness_word_size_)->default_value(2),
            "Sets the word size for any byte-swapping.")
            ("enable_linear_space_rendering",
            po::value<bool>(&enable_linear_space_rendering_)->default_value(true),
            "If enabled, rendering is performed in linear-space RGB. This "
            "option is likely to be removed in the future, as this should be "
            "derived automatically from the input/output vs. render image "
            "formats.")
            // TODO: these should be validated with a custom validator
            ("pipeline.optimization_flags",
            po::value<StreamDispatch::FlagContainer>(&optimization_flags_)->multitoken(),
            "Can hold multiple optimizations flags. Use --help to print out possible flag values.")
            ("opengl.context.debug.min_severity",
            po::value<gl::Context::DebugSeverity>(&min_debug_gl_context_severity_)->default_value(gl::Context::DebugSeverity::MEDIUM),
            "Sets the reported debug GL context severity. This option has no effect unless opengl.context.debug is set to true.")
            ("pipeline.download.pack_to_map_load_constraint_count",
            po::value<size_t>(&pipeline_download_pack_to_map_load_constraint_count_)->default_value(0),
            "Sets how many tokens are required for the download's map PBO "
            "stage to be in the input queue. In other words, how many times "   
            "must the pack PBO phase run before the map PBO phase is executed.")
            ("pipeline.upload.unmap_to_unpack_load_constraint_count",
            po::value<size_t>(&pipeline_upload_unmap_to_unpack_load_constraint_count_)->default_value(0),
            "See above.")
            ("pipeline.download.format_converter_to_pack_load_constraint_count",
            po::value<size_t>(&pipeline_download_format_converter_to_pack_load_constraint_count_)->default_value(0),
            "See above.")
            ("pipeline.upload.unpack_to_format_converter_load_constraint_count",
            po::value<size_t>(&pipeline_upload_unpack_to_format_converter_load_constraint_count_)->default_value(0),
            "See above.")
            ("profiling.trace_output_file",
            po::value<std::string>(&trace_output_file_)->default_value(""),
            "If profiling.stage_sampling_is_enabled is active, and if the "
            "value of this option is not the empty string (default value), "
            "then a trace file is written to the given location.")
            ("program.config_output_file",
            po::value<std::string>(&config_output_file_)->default_value(""),
            "If this path is not the empty string, then a config file with "
            "the parsed configuration will be written to this file.")
            ("profiling.trace_name",
            po::value<std::string>(&profiling_trace_name_)->default_value(""),
            "The name of the tracing can optionally be overriden by this parameter. "
            "If not set, the name of the configuration file will be used instead.")
            ;
        
        // TODO: add any eventual validation methods for folders validation

        po::options_description cmdline_options;
        cmdline_options.add(generic).add(config);

        // First parse only the generic options
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);

        // Save the help string, in case we need to show help.
        std::ostringstream oss;
        oss << cmdline_options;

        oss << "Supported pixel formats: \n";
        // Also the available pixel formats
        for (int32_t i = 0; i<static_cast<int32_t>(ImageFormat::PixelFormat::COUNT); ++i) {
            ImageFormat::PixelFormat pf = ImageFormat::PixelFormat(i);
            if (pf == ImageFormat::PixelFormat::INVALID)
                continue;
            oss << "\t'" << pf << "'\n";

        }

        oss << "Supported transfers: \n";
        // Also the available pixel formats
        for (int32_t i = 0; i<static_cast<int32_t>(ImageFormat::Transfer::COUNT); ++i) {
            ImageFormat::Transfer pf = ImageFormat::Transfer(i);
            oss << "\t'" << pf << "'\n";

        }

        oss << "Supported conversion modes (FormatConverter): \n";
        
        for (int32_t i = 0; i<static_cast<int32_t>(Mode::count); ++i) {
            Mode pf = Mode(i);
            oss << "\t'" << pf << "'\n";
        }

        oss << "Supported chroma filter complexities (FormatConverter): \n";
        
        for (int32_t i = 0; i<static_cast<int32_t>(ChromaFilter::count); ++i) {
            ChromaFilter pf = ChromaFilter(i);
            oss << "\t'" << pf << "'\n";
        }

        oss << "Possible optimization flags (StreamDispatch implementation): \n";
        
        for (int32_t i = 0; i<static_cast<int32_t>(StreamDispatch::Flags::COUNT); ++i) {
            StreamDispatch::Flags pf = StreamDispatch::Flags(i);
            oss << "\t'" << pf << "'\n";
        }

        help_ = oss.str();

        if (vm.count("help")) {
            show_help_ = true;
        } else {
            show_help_ = false;
        }

        if (!config_file_.empty()) {
            std::ifstream ifs(config_file_.c_str());
            if (ifs)
            {
                po::store(po::parse_config_file(ifs, config), vm);
                notify(vm);		
            }
            else
            {
                std::cerr
                    << "Configuration file '" << config_file_ 
                    << "' does not exist. Not loading options from there." 
                    << std::endl;
            }

        }

        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);

        std::ostringstream oss_config;

        for (const auto& entry : vm) {
            
            if (entry.first == "config" || entry.first == "program.config_output_file")
                continue;

            oss_config << entry.first << "=";

            const boost::any * value = &entry.second.value();

            // I know, this is ugly, but works

            const bool* const bool_val = boost::any_cast<const bool>(value);
            const std::string* const string_val = boost::any_cast<const std::string>(value);
            const logging::Severity* const log_sev_val = boost::any_cast<const logging::Severity>(value);
            const size_t* const size_t_val = boost::any_cast<const size_t>(value);
            const ImageFormat::PixelFormat* const pix_fmt_val = boost::any_cast<const ImageFormat::PixelFormat>(value);
            const ImageFormat::Origin* const img_origin_val = boost::any_cast<const ImageFormat::Origin>(value);
            const ImageFormat::Transfer* const img_transfer_val = boost::any_cast<const ImageFormat::Transfer>(value);
            const Time* const time_val = boost::any_cast<const Time>(value);
            const Mode * const fmt_conv_stage_mode_val = boost::any_cast<const Mode >(value);
            const ChromaFilter* const chroma_filter_val = boost::any_cast<const ChromaFilter>(value);
            const StreamDispatch::FlagContainer* const opt_flag_val = boost::any_cast<const StreamDispatch::FlagContainer>(value);
            const gl::Context::DebugSeverity* const gl_debug_sev_val = boost::any_cast<const gl::Context::DebugSeverity>(value);

            if (bool_val != nullptr) {
                oss_config << std::boolalpha << *bool_val;
            } else if (string_val != nullptr) {
                oss_config << *string_val;
            } else if (log_sev_val != nullptr) {
                oss_config << *log_sev_val;
            } else if (size_t_val != nullptr) {
                oss_config << *size_t_val;
            } else if (pix_fmt_val != nullptr) {
                oss_config << *pix_fmt_val;
            } else if (img_origin_val != nullptr) {
                oss_config << *img_origin_val;
            } else if (img_transfer_val != nullptr) {
                oss_config << *img_transfer_val;
            } else if (time_val != nullptr) {
                oss_config << *time_val;
            } else if (fmt_conv_stage_mode_val != nullptr) {
                oss_config << *fmt_conv_stage_mode_val;
            } else if (chroma_filter_val != nullptr) {
                oss_config << *chroma_filter_val;
            } else if (opt_flag_val != nullptr) {

                for (size_t i = 0; i<static_cast<size_t>(StreamDispatch::Flags::COUNT); ++i) {
                    StreamDispatch::Flags pf = StreamDispatch::Flags(i);
                    if ((*opt_flag_val)[i]) {
                        if (i != 0) {
                            oss_config << "\n";
                            oss_config << entry.first << "=";
                        }
                        oss_config << pf;
                    }
                }

            } else if (gl_debug_sev_val != nullptr) {
                oss_config << *gl_debug_sev_val;
            } else {
                throw std::runtime_error("Missing a type in options print-out.");
            }

            oss_config << "\n";

        }

        config_output_file_contents_ = oss_config.str();

    } catch (const po::error& e) {
        throw InvalidInput(e.what());
    }

}

const std::string& fb::ProgramOptions::config_file() const {
    return config_file_;
}

bool fb::ProgramOptions::use_debug_gl_context() const { 
    return use_debug_gl_context_; 
}

bool fb::ProgramOptions::show_help() const { 
    return show_help_; 
}

const std::string& fb::ProgramOptions::help() const { 
    return help_; 
}

const std::string& fb::ProgramOptions::log_file() const { 
    return log_file_; 
}

bool fb::ProgramOptions::log_to_stdout() const { 
    return log_to_stdout_; 
}

fb::logging::Severity fb::ProgramOptions::min_logging_severity() const {

    return min_logging_severity_;

}

bool fb::ProgramOptions::arb_debug_synchronous_mode() const { 
    return arb_debug_synchronous_mode_; 
}

size_t fb::ProgramOptions::upload_pbo_rr_count() const {
    return upload_pbo_rr_count_;
}

size_t fb::ProgramOptions::upload_copy_to_unmap_pipeline_size() const {
    return upload_copy_to_unmap_pipeline_size_;
}

size_t fb::ProgramOptions::upload_unmap_to_unpack_pipeline_size() const {
    return upload_unmap_to_unpack_pipeline_size_;
}

size_t fb::ProgramOptions::download_pbo_rr_count() const {
    return download_pbo_rr_count_;
}

size_t fb::ProgramOptions::source_texture_rr_count() const {
    return source_texture_rr_count_;
}

size_t fb::ProgramOptions::destination_texture_rr_count() const {
    return destination_texture_rr_count_;
}

size_t fb::ProgramOptions::frame_input_pipeline_size() const {
    return frame_input_pipeline_size_;
}

size_t fb::ProgramOptions::frame_output_cache_count() const {
    return frame_output_cache_count_;
}

size_t fb::ProgramOptions::download_map_to_copy_pipeline_size() const {
    return download_map_to_copy_pipeline_size_;
}

size_t fb::ProgramOptions::download_pack_to_map_pipeline_size() const {
    return download_pack_to_map_pipeline_size_;
}

size_t fb::ProgramOptions::player_width() const {
    return player_width_;
}

size_t fb::ProgramOptions::player_height() const {
    return player_height_;
}

bool fb::ProgramOptions::player_is_fullscreen() const {
    return player_is_fullscreen_;
}

bool fb::ProgramOptions::player_use_vsync() const {
    return player_use_vsync_;
}

const std::string& fb::ProgramOptions::glsl_folder() const { 
    return glsl_folder_; 
}

bool fb::ProgramOptions::blit_to_player() const {
    return blit_to_player_;
}

bool fb::ProgramOptions::v210_use_shader_for_component_access() const {
    return v210_use_shader_for_component_access_;
}

fb::ImageFormat::PixelFormat fb::ProgramOptions::input_sequence_pixel_format() const {
    return input_sequence_pixel_format_;
}

const std::string& fb::ProgramOptions::input_sequences_folder() const {
    return input_sequences_folder_;
}

const std::string& fb::ProgramOptions::input_sequence_name() const {
    return input_sequence_name_;
}

const std::string& fb::ProgramOptions::input_sequence_pattern() const {
    return input_sequence_pattern_;
}

size_t fb::ProgramOptions::input_sequence_width() const {
    return input_sequence_width_;
}

size_t fb::ProgramOptions::input_sequence_height() const {
    return input_sequence_height_;
}

size_t fb::ProgramOptions::input_sequence_loop_count() const {
    return input_sequence_loop_count_;
}

fb::ImageFormat::Origin fb::ProgramOptions::input_sequence_origin() const {
    return input_sequence_origin_;
}

const fb::Time& fb::ProgramOptions::input_sequence_frame_duration() const {
    return input_sequence_frame_duration_;
}

const std::string& fb::ProgramOptions::textures_folder() const {
    return textures_folder_;
}

fb::ImageFormat::PixelFormat fb::ProgramOptions::render_pixel_format() const {
    return render_pixel_format_;
}

fb::ImageFormat::Transfer fb::ProgramOptions::render_transfer() const {
    return render_transfer_;
}

fb::ImageFormat::Transfer fb::ProgramOptions::input_sequence_transfer() const {
    return input_sequence_transfer_;
}

bool fb::ProgramOptions::sample_stages() const {
    return sample_stages_;
}

bool fb::ProgramOptions::enable_output_stages() const {
    return enable_output_stages_;
}

bool fb::ProgramOptions::enable_input_stages() const {
    return enable_input_stages_;
}

bool fb::ProgramOptions::enable_render_stages() const {
    return enable_render_stages_;
}

bool fb::ProgramOptions::force_passthrough_renderer() const {
    return force_passthrough_renderer_;
}

size_t fb::ProgramOptions::statistics_skip_first_num_frames() const {
    return statistics_skip_first_num_frames_;
}

bool fb::ProgramOptions::enable_host_copy_upload() const {
    return enable_host_copy_upload_;
}

bool fb::ProgramOptions::enable_host_copy_download() const {
    return enable_host_copy_download_;
}

bool fb::ProgramOptions::enable_upload_gl_timer_queries() const {
    return enable_upload_gl_timer_queries_;
}

bool fb::ProgramOptions::enable_render_gl_timer_queries() const {
    return enable_render_gl_timer_queries_;
}

bool fb::ProgramOptions::enable_download_gl_timer_queries() const {
    return enable_download_gl_timer_queries_;
}

fb::Mode fb::ProgramOptions::format_conversion_mode() const {
    return format_conversion_mode_;
}

size_t fb::ProgramOptions::v210_decode_glsl_430_work_group_size() const {
    return v210_decode_glsl_430_work_group_size_;
}

size_t fb::ProgramOptions::v210_encode_glsl_430_work_group_size() const {
    return v210_encode_glsl_430_work_group_size_;
}

const std::string& fb::ProgramOptions::demo_renderer_lower_third_image() const {
    return demo_renderer_lower_third_image_;
}

const std::string& fb::ProgramOptions::demo_renderer_logo_image() const {
    return demo_renderer_logo_image_;
}

fb::ChromaFilter fb::ProgramOptions::v210_decode_glsl_chroma_filter() const {
    return v210_decode_glsl_chroma_filter_;
}

fb::ChromaFilter fb::ProgramOptions::v210_encode_glsl_chroma_filter() const {
    return v210_encode_glsl_chroma_filter_;
}

bool fb::ProgramOptions::enable_window_user_input() const {
    return enable_window_user_input_;
}

const std::string& fb::ProgramOptions::write_output_folder() const
{
    return write_output_folder_;
}

bool fb::ProgramOptions::enable_write_output() const {
    return enable_write_output_;
}

bool fb::ProgramOptions::write_output_swap_endianness() const
{
    return write_output_swap_endianness_;
}

size_t fb::ProgramOptions::write_output_swap_endianness_word_size() const
{
    return write_output_swap_endianness_word_size_;
}

bool fb::ProgramOptions::enable_linear_space_rendering() const
{
    return enable_linear_space_rendering_;
}

const fb::StreamDispatch::FlagContainer& fb::ProgramOptions::optimization_flags() const
{
    return optimization_flags_;
}

fb::gl::Context::DebugSeverity fb::ProgramOptions::min_debug_gl_context_severity() const
{
    return min_debug_gl_context_severity_;
}

size_t fb::ProgramOptions::pipeline_download_pack_to_map_load_constraint_count() const
{
    return pipeline_download_pack_to_map_load_constraint_count_;
}

size_t fb::ProgramOptions::pipeline_upload_unmap_to_unpack_load_constraint_count() const
{
    return pipeline_upload_unmap_to_unpack_load_constraint_count_;
}

size_t fb::ProgramOptions::pipeline_download_format_converter_to_pack_load_constraint_count() const
{
    return pipeline_download_format_converter_to_pack_load_constraint_count_;
}

size_t fb::ProgramOptions::pipeline_upload_unpack_to_format_converter_load_constraint_count() const
{
    return pipeline_upload_unpack_to_format_converter_load_constraint_count_;
}

const std::string& fb::ProgramOptions::trace_output_file() const
{

    return trace_output_file_;

}

const std::string& fb::ProgramOptions::config_output_file() const
{

    return config_output_file_;

}

void fb::ProgramOptions::write_config_to_file(const std::string& file_path) const
{
    bf::path out_path(file_path);

    bf::ofstream writer(out_path, std::ios::out);

    if (!writer.good())
        throw std::runtime_error("Could not open writer for config out file.");

    writer << config_output_file_contents_;

    writer.close();

    FB_LOG_INFO << "Wrote config file to '" << file_path << "'.";

}

const std::string& fb::ProgramOptions::profiling_trace_name() const
{

    return profiling_trace_name_;

}
