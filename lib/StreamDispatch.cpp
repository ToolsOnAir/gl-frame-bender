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
#include <algorithm>
#include <functional>
#include <iostream>

#include <boost/filesystem.hpp>

#include "StreamDispatch.h"
#include "StreamSource.h"
#include "StreamRenderer.h"
#include "Logging.h"
#include "ProgramOptions.h"
#include "TimeSampler.h"
#include "TraceFormat.h"

#include "MapPBOStage.h"
#include "CopyMappedPBOToHostStage.h"
#include "FormatConverterStage.h"
#include "PackTextureToPBOStage.h"
#include "RenderStage.h"
#include "UnpackPBOToTextureStage.h"
#include "ByPassDownloadStage.h"
#include "ByPassUploadStage.h"
#include "FrameCompositionOutputStage.h"
#include "FrameCompositionInputStage.h"
#include "CopyHostToMappedPBOStage.h"
#include "UnmapPBOStage.h"

namespace fb = toa::frame_bender;
namespace bf = boost::filesystem;

fb::StreamDispatch::StreamDispatch(
    std::string name,
    gl::Context* main_context,
    const ImageFormat& origin_format,
    const ImageFormat& render_format,
    bool enable_output_stages,
    bool enable_input_stages,
    bool enable_render_stages,
    const FlagContainer& flags) :
        name_(std::move(name)), 
        origin_format_(origin_format),
        render_format_(render_format),
        stop_threads_(false),
        gl_context_master_(main_context),
        active_composition_(nullptr),
        enable_output_stages_(enable_output_stages),
        enable_input_stages_(enable_input_stages),
        enable_render_stages_(enable_render_stages),
        impl_use_multiple_gl_contexts_(flags[static_cast<size_t>(Flags::MULTIPLE_GL_CONTEXTS)]),
        impl_async_input_(flags[static_cast<size_t>(Flags::ASYNC_INPUT)]),
        impl_async_output_(flags[static_cast<size_t>(Flags::ASYNC_OUTPUT)]),
        impl_use_arb_persistent_mapping_(flags[static_cast<size_t>(Flags::ARB_PERSISTENT_MAPPING)]),
        impl_copy_pbo_before_download_(flags[static_cast<size_t>(Flags::COPY_PBO_BEFORE_DOWNLOAD)])
{

    if (main_context == nullptr)
        throw std::invalid_argument("GL Main Context is nullptr.");

    if (main_context->is_attached_to_any_thread()) {
        FB_LOG_ERROR << "Can't create StreamDispatcher when main context is still attached to some thread.";
        throw std::invalid_argument("Main Context is still attached to some thread.");
    }

    if (!enable_input_stages) {
        FB_LOG_INFO << "Input stages are disabled for benchmarking, render output won't be correct.";
    }

    if (!enable_output_stages) {
        FB_LOG_INFO << "Output stages are disabled for benchmarking, render output won't be correct.";
    }

    if (!enable_render_stages) {
        FB_LOG_INFO << "Render stages are disabled for benchmarking, render output won't be correct.";
    }

    gl_context_master_->attach();
    gl_info_ = gl::utils::get_info();
    gl_context_master_->detach();

    FB_LOG_INFO << "GL Info: " << gl_info_ << ".";

    enable_upload_gl_timer_queries_ = ProgramOptions::global().enable_upload_gl_timer_queries();
    if (enable_upload_gl_timer_queries_) {
        FB_LOG_INFO << "GL timer queries are enabled for upload.";
    }

    enable_render_gl_timer_queries_ = ProgramOptions::global().enable_render_gl_timer_queries();
    if (enable_render_gl_timer_queries_) {
        FB_LOG_INFO << "GL timer queries are enabled for render.";
    }

    enable_download_gl_timer_queries_ = ProgramOptions::global().enable_download_gl_timer_queries();
    if (enable_download_gl_timer_queries_) {
        FB_LOG_INFO << "GL timer queries are enabled for download.";
    }

    if (impl_use_arb_persistent_mapping_)
    {
        if (GLAD_GL_ARB_buffer_storage != 0) {
            FB_LOG_INFO <<  "Using persistently-mapped buffers with ARB_buffer_storage";
        } else {
            FB_LOG_INFO << "Can't use persistently-mapped buffers, extension is not available.";
            impl_use_arb_persistent_mapping_ = false;
        }
    }

    if (impl_use_multiple_gl_contexts_) {
    
        FB_LOG_INFO << "Using multiple GL contexts for upload/render/download.";

        // Create the shared contexts for dispatch
        gl_shared_context_upload_async_ = gl_context_master_->create_shared("Dispatch: upload context");

        gl_shared_context_download_async_ = gl_context_master_->create_shared("Dispatch: download context");

    } else {
        FB_LOG_INFO << "Using a single GL context for upload/render/download.";
    }

    define_pipeline();
    
    if (impl_use_multiple_gl_contexts_) {

        gl_upload_async_thread_ = std::thread(&StreamDispatch::execute_gl_upload_async, this);
        gl_download_async_thread_ = std::thread(&StreamDispatch::execute_gl_download_async, this);

    }

    if (impl_async_input_) {

        FB_LOG_INFO << "Input frame handling and copying is asynchronous to OpenGL upload.";

        host_copy_input_async_thread_ = std::thread(&StreamDispatch::execute_host_copy_input_async, this);

    } else {

        FB_LOG_INFO << "Input frame handling and copying is synchronous to OpenGL upload.";

    }

    if (impl_async_output_) {

        FB_LOG_INFO << "Output frame handling and copying is asynchronous to OpenGL download.";

        host_copy_output_async_thread_ = std::thread(&StreamDispatch::execute_host_copy_output_async, this);

    } else {

        FB_LOG_INFO << "Output frame handling and copying is synchronous to OpenGL download.";

    }

    gl_master_thread_ = std::thread(&StreamDispatch::execute_gl_master, this);

    if ((ProgramOptions::global().download_map_to_copy_pipeline_size() + 
        ProgramOptions::global().download_pack_to_map_pipeline_size()) !=
        ProgramOptions::global().download_pbo_rr_count()) 
    {
        FB_LOG_CRITICAL << "Pack-to-map and Map-to-copy have to add up to Download PBO count! Please check you parameters.";
        throw std::invalid_argument("Invalid pipeline size configuration");
    }

    if ((ProgramOptions::global().upload_copy_to_unmap_pipeline_size() + 
        ProgramOptions::global().upload_unmap_to_unpack_pipeline_size()) !=
        ProgramOptions::global().upload_pbo_rr_count()) 
    {
        FB_LOG_CRITICAL << "Copy-to-unmap and unmap-to-unpack pipeline sizes must be equal to pipeline.upload.gl_pbo_count! Check your parameters.";
        throw std::invalid_argument("Invalid pipeline size configuration");
    }

    FB_LOG_DEBUG
        << "Round-Robin-Size configuration: "
        << "PBO-upload: " << ProgramOptions::global().upload_pbo_rr_count() << ", "
        << "PBO-upload (copy to unmap): " << ProgramOptions::global().upload_copy_to_unmap_pipeline_size() << ", "
        << "PBO-upload (unmap to unpack): " << ProgramOptions::global().upload_unmap_to_unpack_pipeline_size() << ", "
        << "PBO-download: " << ProgramOptions::global().download_pbo_rr_count() << ", "
        << "PBO-download (pack to map): " << ProgramOptions::global().download_pack_to_map_pipeline_size() << ", "
        << "PBO-download (map to copy): " << ProgramOptions::global().download_map_to_copy_pipeline_size() << ", "
        << "Source-texture: " << ProgramOptions::global().source_texture_rr_count() << ", "
        << "Destination-texture: " << ProgramOptions::global().destination_texture_rr_count() << ".";

    FB_LOG_DEBUG 
        << "Launched dispatch threads: "
        << "upload='" << gl_upload_async_thread_.get_id() 
        << "', upload-memcpy='" << host_copy_input_async_thread_.get_id()
        << "', render='" << gl_master_thread_.get_id()
        << "', download-memcpy='" << host_copy_output_async_thread_.get_id()
        << "', download='" << gl_download_async_thread_.get_id() << "'.";

}

fb::StreamDispatch::~StreamDispatch() {

    stop_threads_ = true;

    if (host_copy_input_async_thread_.joinable())
        host_copy_input_async_thread_.join();

    if (gl_upload_async_thread_.joinable())
        gl_upload_async_thread_.join();

    if (gl_master_thread_.joinable())
        gl_master_thread_.join();

    if (gl_download_async_thread_.joinable())
        gl_download_async_thread_.join();

    if (host_copy_output_async_thread_.joinable())
        host_copy_output_async_thread_.join();

    FB_LOG_DEBUG << "Joined dispatch threads.";

    // Print summarizing statistics, if we have sampled.
    if (ProgramOptions::global().sample_stages()) {

        // TODO: make user option
        const size_t in_point = ProgramOptions::global().statistics_skip_first_num_frames();
        
        const StageSampler& head_sampler = enable_input_stages_ ? frame_composition_input_stage_->sampler() : by_pass_upload_stage_->sampler();
        const StageSampler& output_sampler = enable_output_stages_ ? frame_composition_output_stage_->sampler() : by_pass_download_stage_->sampler();
        size_t num_samples_available = head_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_BEGIN);
        if (num_samples_available > 200) {

            // Total throughput
            StageSampler::TraceEvent first_event_begin = head_sampler.get_trace_event(
                StageExecutionState::TASK_BEGIN,
                in_point);

            size_t output_trace_count = output_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_END);

            FB_ASSERT(in_point < output_trace_count);

            StageSampler::TraceEvent last_event_stop = output_sampler.get_trace_event(
                StageExecutionState::TASK_END,
                output_trace_count - 1);

            auto throughput_time = last_event_stop - first_event_begin;
                        
            microseconds duration_microseconds = boost::chrono::duration_cast<microseconds>(throughput_time);

            size_t sample_count = output_trace_count - in_point;

            FB_LOG_INFO << " ========================== ";
            FB_LOG_INFO << " === Statistics Summary === ";
            FB_LOG_INFO << " ========================== ";

            FB_LOG_INFO << "Skipped the first " << in_point << " frames in order to accomodate pipeline stabilization.";
            FB_LOG_INFO << "Num of sampled frames: " << sample_count << ".";
            FB_LOG_INFO << "Throughput time: " << boost::chrono::duration_cast<boost::chrono::microseconds>(throughput_time) << ".";
            FB_LOG_INFO << "Avg. duration per frame: " << boost::chrono::duration_cast<boost::chrono::microseconds>(throughput_time / sample_count) << ".";
            double fps = (static_cast<double>(sample_count) / duration_microseconds.count()) * 1e6;
            FB_LOG_INFO << "Avg. frames per second: " << fps << ".";
            FB_LOG_INFO << "For frames using format [" << origin_format_ << "], this amount to a throughput of ";
            
            uintmax_t total_num_bytes = origin_format_.image_byte_size() * sample_count;
            double mb_total = total_num_bytes / 1e6;

            double mb_per_sec = (mb_total / duration_microseconds.count()) * 1e6;

            FB_LOG_INFO << mb_per_sec << " mb/sec";

            FB_LOG_INFO << " ========================== ";
            FB_LOG_INFO << " ===== End of Summary ===== ";
            FB_LOG_INFO << " ========================== ";

            std::cout << "Avg. throughput: " << mb_per_sec << " mb/sec\n";

            FB_LOG_INFO << " ========================== ";
            FB_LOG_INFO << " ======== Trace Cut ======= ";
            FB_LOG_INFO << " ========================== ";

            // TODO: make these user-configurable?

            // Print time slices for three consecutive frames, starting somewhere in the middle
            static const size_t num_frames_to_show = 6;
            size_t first_frame = in_point + ((num_samples_available - in_point) / 2);
            size_t last_frame = first_frame + num_frames_to_show;

            if (first_frame != last_frame) {

                FB_LOG_INFO << "Showing trace of frame nr. " << first_frame << " to " << last_frame << ".";
                FB_LOG_INFO  << "Using relative times.";

                auto relative_start_time = head_sampler.get_trace_event(StageExecutionState::TASK_BEGIN, first_frame);

                auto make_relative_micros = [&](const clock::time_point& t) -> microseconds {
                    return boost::chrono::duration_cast<boost::chrono::microseconds>(t - relative_start_time);
                };

                auto format_trace = [&](
                    const std::string& name,
                    size_t frame_nr, 
                    const microseconds& begin, 
                    const microseconds& end) {

                        std::string pad = "        ";
                        std::copy_n(
                            std::begin(name), 
                            std::min(name.size(), pad.size()), 
                            std::begin(pad)); 
                        FB_LOG_INFO << pad << ": \t[" << begin << ", " << end << "], dur=" << end - begin << ", frame_nr=" << frame_nr << ".";
                };

                if (enable_input_stages_) {

                    FB_LOG_INFO << " ======== Input Stages (host time) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_frame_input = make_relative_micros(frame_composition_input_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_frame_input = make_relative_micros(frame_composition_input_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_copy= make_relative_micros(copy_host_to_mapped_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_copy = make_relative_micros(copy_host_to_mapped_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_unmap = make_relative_micros(unmap_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_unmap = make_relative_micros(unmap_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_unpack = make_relative_micros(unpack_pbo_to_texture_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_unpack = make_relative_micros(unpack_pbo_to_texture_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        format_trace("In", i, begin_frame_input, end_frame_input);
                        format_trace("Copy", i, begin_copy, end_copy);
                        format_trace("Unmap", i, begin_unmap, end_unmap);
                        format_trace("Unpack", i, begin_unpack, end_unpack);

                        FB_LOG_INFO << "-----";
                    }
                }

                if (enable_input_stages_ && enable_upload_gl_timer_queries_) {

                    FB_LOG_INFO << " ======== Input Stages (GL time) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_unmap = make_relative_micros(unmap_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_unmap = make_relative_micros(unmap_pbo_up_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        auto begin_unpack = make_relative_micros(unpack_pbo_to_texture_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_unpack = make_relative_micros(unpack_pbo_to_texture_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        format_trace("GL Unmap", i, begin_unmap, end_unmap);
                        format_trace("GL Unpack", i, begin_unpack, end_unpack);

                        FB_LOG_INFO << "-----";
                    }
                }

                if (enable_render_stages_) {

                    FB_LOG_INFO << " ======== Render Stages (host time) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_fmt_input_to_render = make_relative_micros(format_converter_stage_input_to_render_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_fmt_input_to_render = make_relative_micros(format_converter_stage_input_to_render_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_render= make_relative_micros(render_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_render = make_relative_micros(render_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_fmt_render_to_output = make_relative_micros(format_converter_stage_render_to_output_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_fmt_render_to_output = make_relative_micros(format_converter_stage_render_to_output_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        format_trace("FMTIN", i, begin_fmt_input_to_render, end_fmt_input_to_render);
                        format_trace("Render", i, begin_render, end_render);
                        format_trace("FMTOUT", i, begin_fmt_render_to_output, end_fmt_render_to_output);

                        FB_LOG_INFO << "-----";
                    }
                }

                if (enable_render_stages_ && enable_render_gl_timer_queries_) {

                    FB_LOG_INFO << " ======== Render Stages (GL times) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "Reported times do not include wait-times.";

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_fmt_input_to_render = make_relative_micros(format_converter_stage_input_to_render_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_fmt_input_to_render = make_relative_micros(format_converter_stage_input_to_render_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        auto begin_render= make_relative_micros(render_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_render = make_relative_micros(render_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        auto begin_fmt_render_to_output = make_relative_micros(format_converter_stage_render_to_output_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_fmt_render_to_output = make_relative_micros(format_converter_stage_render_to_output_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        format_trace("GL FMTIN", i, begin_fmt_input_to_render, end_fmt_input_to_render);
                        format_trace("GL Render", i, begin_render, end_render);
                        format_trace("GL FMTOUT", i, begin_fmt_render_to_output, end_fmt_render_to_output);

                        FB_LOG_INFO << "-----";
                    }
                }

                if (enable_output_stages_) {

                    FB_LOG_INFO << " ======== Output Stages (host time) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_pack = make_relative_micros(pack_texture_to_pbo_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_pack = make_relative_micros(pack_texture_to_pbo_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_map = make_relative_micros(map_pbo_down_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_map = make_relative_micros(map_pbo_down_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_copy = make_relative_micros(copy_mapped_pbo_down_stage->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_copy = make_relative_micros(copy_mapped_pbo_down_stage->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        auto begin_deliver = make_relative_micros(frame_composition_output_stage_->sampler().get_trace_event(StageExecutionState::TASK_BEGIN, i));
                        auto end_deliver = make_relative_micros(frame_composition_output_stage_->sampler().get_trace_event(StageExecutionState::TASK_END, i));

                        format_trace("Pack", i, begin_pack, end_pack);
                        format_trace("Map", i, begin_map, end_map);
                        format_trace("Copy", i, begin_copy, end_copy);
                        format_trace("Out", i, begin_deliver, end_deliver);

                        FB_LOG_INFO << "-----";
                    }
                }

                if (enable_output_stages_ && enable_download_gl_timer_queries_) {

                    FB_LOG_INFO << " ======== Output Stages (GL time) ======= ";

                    // we make all time stamps relative to first reported frame.
                    // we report times in microseconds

                    FB_LOG_INFO << "-----";
                    for (size_t i = first_frame; i<last_frame; ++i) {

                        auto begin_pack = make_relative_micros(pack_texture_to_pbo_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_pack = make_relative_micros(pack_texture_to_pbo_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        auto begin_map = make_relative_micros(map_pbo_down_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_BEGIN, i));
                        auto end_map = make_relative_micros(map_pbo_down_stage_->sampler().get_trace_event(StageExecutionState::GL_TASK_END, i));

                        format_trace("GL Pack", i, begin_pack, end_pack);
                        format_trace("GL Map", i, begin_map, end_map);

                        FB_LOG_INFO << "-----";
                    }
                }

            } else {
                FB_LOG_ERROR << "First and last frame for trace analysis is equal.";
            }

            FB_LOG_INFO << " ========================== ";
            FB_LOG_INFO << " ====== End of Trace ====== ";
            FB_LOG_INFO << " ========================== ";

        } else {
            FB_LOG_WARNING 
                << "Won't summarize throughput, number of samples is too little (" 
                << head_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_BEGIN) 
                << "). Increase your input sequence's number of frames to at least 200.";
        }

    } else {
        FB_LOG_INFO << "Stage sampling was turned off, can't print throughput summary.";
    }

    // Cleanup OGL stuff, and make sure the right context is attached
    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_upload_async_->attach();
    else
        gl_context_master_->attach();

    if (by_pass_upload_stage_)
        by_pass_upload_stage_.reset();

    if (copy_host_to_mapped_pbo_up_stage_)
        copy_host_to_mapped_pbo_up_stage_.reset();

    if (unmap_pbo_up_stage_)
        unmap_pbo_up_stage_.reset();

    if (unpack_pbo_to_texture_stage_)
        unpack_pbo_to_texture_stage_.reset();

    if (!upload_pbo_ids_.empty()) {
        glDeleteBuffers(
            static_cast<GLsizei>(upload_pbo_ids_.size()), 
            upload_pbo_ids_.data());
    }

    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_upload_async_->detach();
    else
        gl_context_master_->detach();

    gl_context_master_->attach();

    if (format_converter_stage_input_to_render_)
        format_converter_stage_input_to_render_.reset();    

    if (render_stage_)
        render_stage_.reset();    

    if (format_converter_stage_render_to_output_)
        format_converter_stage_render_to_output_.reset();    

    gl_context_master_->detach();

    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_download_async_->attach();
    else
        gl_context_master_->attach();
    
    if (pack_texture_to_pbo_stage_)
        pack_texture_to_pbo_stage_.reset();    

    if (map_pbo_down_stage_)
        map_pbo_down_stage_.reset();    

    if (copy_mapped_pbo_down_stage)
        copy_mapped_pbo_down_stage.reset();    

    // Cleanup shared OGL buffers
    if (!download_pbo_ids_.empty()) {
        glDeleteBuffers(
            static_cast<GLsizei>(download_pbo_ids_.size()), 
            download_pbo_ids_.data());
    }

    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_download_async_->detach();
    else
        gl_context_master_->detach();

}

void fb::StreamDispatch::write_trace_file(
    const std::string& file_path, 
    const std::string& configuration_name_prefix)
{

    const ProgramOptions& po = ProgramOptions::global();

    bf::path config_file_path = po.config_file();

    std::string session_name = po.profiling_trace_name().empty() ? config_file_path.stem().string() : po.profiling_trace_name();

    // TODO: define session name
    // TODO: add program option string?

    TraceFormatWriter format_writer(file_path, session_name, gl_info_);

    // TODO-DEF: should really be based on a common interface !

    // Note that the order in which the stages are added here is important
    // and should reflect the chrononical order of the pipeline

    if (by_pass_upload_stage_) {
        format_writer.add_stage_sampler(
            by_pass_upload_stage_->name(), 
            by_pass_upload_stage_->sampler());
    }

    if (frame_composition_input_stage_) {
        format_writer.add_stage_sampler(
            frame_composition_input_stage_->name(), 
            frame_composition_input_stage_->sampler());
    }

    if (copy_host_to_mapped_pbo_up_stage_) {
        format_writer.add_stage_sampler(
            copy_host_to_mapped_pbo_up_stage_->name(), 
            copy_host_to_mapped_pbo_up_stage_->sampler());
    }

    if (unmap_pbo_up_stage_) {
        format_writer.add_stage_sampler(
            unmap_pbo_up_stage_->name(), 
            unmap_pbo_up_stage_->sampler());
    }

    if (unpack_pbo_to_texture_stage_) {
        format_writer.add_stage_sampler(
            unpack_pbo_to_texture_stage_->name(), 
            unpack_pbo_to_texture_stage_->sampler());
    }

    if (format_converter_stage_input_to_render_) {
        format_writer.add_stage_sampler(
            format_converter_stage_input_to_render_->name(), 
            format_converter_stage_input_to_render_->sampler());
    }

    if (render_stage_) {
        format_writer.add_stage_sampler(
            render_stage_->name(), 
            render_stage_->sampler());
    }

    if (format_converter_stage_render_to_output_) {
        format_writer.add_stage_sampler(
            format_converter_stage_render_to_output_->name(), 
            format_converter_stage_render_to_output_->sampler());
    }

    if (pack_texture_to_pbo_stage_) {
        format_writer.add_stage_sampler(
            pack_texture_to_pbo_stage_->name(), 
            pack_texture_to_pbo_stage_->sampler());
    }

    if (map_pbo_down_stage_) {
        format_writer.add_stage_sampler(
            map_pbo_down_stage_->name(), 
            map_pbo_down_stage_->sampler());
    }

    if (copy_mapped_pbo_down_stage) {
        format_writer.add_stage_sampler(
            copy_mapped_pbo_down_stage->name(), 
            copy_mapped_pbo_down_stage->sampler());
    }

    if (frame_composition_output_stage_) {
        format_writer.add_stage_sampler(
            frame_composition_output_stage_->name(), 
            frame_composition_output_stage_->sampler());
    }

    if (by_pass_download_stage_) {
        format_writer.add_stage_sampler(
            by_pass_download_stage_->name(), 
            by_pass_download_stage_->sampler());
    }

    const size_t in_point = ProgramOptions::global().statistics_skip_first_num_frames();

    const StageSampler& head_sampler = enable_input_stages_ ? frame_composition_input_stage_->sampler() : by_pass_upload_stage_->sampler();
    const StageSampler& output_sampler = enable_output_stages_ ? frame_composition_output_stage_->sampler() : by_pass_download_stage_->sampler();

    size_t num_samples_available = head_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_BEGIN);

    if (num_samples_available > 200 && num_samples_available > in_point) {

        // Total throughput
        StageSampler::TraceEvent first_event_begin = head_sampler.get_trace_event(
            StageExecutionState::TASK_BEGIN,
            in_point);

        size_t output_trace_count = output_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_END);

        if(in_point >= output_trace_count) {
            FB_LOG_ERROR << "Not enough frames in output trace: " << in_point << " vs. " << output_trace_count;
            throw std::runtime_error("Invalid trace count.");
        }

        StageSampler::TraceEvent last_event_stop = output_sampler.get_trace_event(
            StageExecutionState::TASK_END,
            output_trace_count - 1);

        auto throughput_time = last_event_stop - first_event_begin;
                        
        microseconds duration_microseconds = boost::chrono::duration_cast<microseconds>(throughput_time);

        size_t sample_count = output_trace_count - in_point;
            
        uintmax_t total_num_bytes = origin_format_.image_byte_size() * sample_count;
        double mb_total = static_cast<double>(total_num_bytes) / 1e6;

        double mb_per_sec = (mb_total / duration_microseconds.count()) * 1e6;

		double millisecs_per_frame = (static_cast<double>(duration_microseconds.count()) / sample_count) * 1e-3;

        auto latency_stat = StageSampler::build_delta_statistic(head_sampler, StageExecutionState::TASK_BEGIN, output_sampler, StageExecutionState::TASK_END);

        static_assert(clock::period::num == 1, "Expecting a period numerator of 1.");
        static_assert(clock::period::den == 1000000000, "Expecting nanoseconds as the period of time.");

        format_writer.set_session_statistics(
            static_cast<uint64_t>(sample_count),
            static_cast<float>(mb_per_sec),
            static_cast<int64_t>(latency_stat.median.count()),
			static_cast<float>(millisecs_per_frame));

    } else {
        FB_LOG_WARNING 
            << "Won't write session statistics to trace file, number of samples is too little (" 
            << head_sampler.number_of_sampled_trace_events(StageExecutionState::TASK_BEGIN) 
            << "). Increase your input sequence's number of frames to at least 200.";
    }


    format_writer.flush();
}

fb::StreamComposition::ID fb::StreamDispatch::get_unique_id_for_name(const std::string& name) const
{
    fb::StreamComposition::ID unique_id = name;
    size_t cnt = 1;
    while (compositions_.count(unique_id) != 0) {
        unique_id = name + std::to_string(cnt);
        cnt++;
    }

    return unique_id;
}

fb::StreamComposition::ID fb::StreamDispatch::create_composition(
    std::string name,
    std::vector<std::shared_ptr<StreamSource>> input_sources,
    std::unique_ptr<StreamRenderer> renderer, 
    StreamComposition::OutputCallback output_callback)
{
    fb::StreamComposition::ID new_id = get_unique_id_for_name(name);
    compositions_.emplace(std::piecewise_construct, 
        std::forward_as_tuple(new_id),
        std::forward_as_tuple(
            std::move(name),
            std::move(input_sources), 
            std::move(renderer), 
            std::move(output_callback)));

    FB_LOG_INFO << "Created composition new composition with ID '" << new_id << "'.";

    return new_id;
}

bool fb::StreamDispatch::is_composition(const StreamComposition::ID& id) {
    return compositions_.find(id) != std::end(compositions_);
}

void fb::StreamDispatch::remove_composition(const StreamComposition::ID& id) {
    auto itr = compositions_.find(id);
    if (itr == std::end(compositions_))
        throw std::invalid_argument("Composition to remove has invalid id '" + id + "'.");

    if (is_composition_running(id)) {
        FB_LOG_INFO << "Composition '" << id << "' is stopped before it is removed.";
        stop_composition(id);
    }

    compositions_.erase(itr);

    FB_LOG_INFO << "Removed and destroyed composition with id '" << id << "'.";
}

// TODO: the communication between the thrads and the "active" compositions
// info should be optimized. This can be done with FIFOs btw, too (an "add"
// FIFO, and a "remove" FIFO?

void fb::StreamDispatch::start_composition(
    const StreamComposition::ID& id, 
    CompletionHandler completion_handler,
    clock::time_point when)
{

    // TODO: take care of when parameter
    FB_LOG_WARNING << "'when' parameter for start_composition is ignored at the moment and always assumed to be 'now'.";

    auto itr = compositions_.find(id);
    if (itr == std::end(compositions_))
        throw std::invalid_argument("Composition with id '" + id + "' doesn't exist.");

    //std::lock_guard<std::mutex> lock(running_compositions_lock_);

    //auto res = running_compositions_.insert(id);
    //if (!res.second)
    //    throw std::runtime_error("Composition '" + id + "' seems to be already running. Can't start it.");

    //FB_LOG_INFO << "Started composition with name " << itr->second.name() << " ('" << id << "').";

    // Note that this is just hacky (though non-blocking) way of only allowing
    // one stream to be dealt with at a time.
    if (enable_input_stages_)
        frame_composition_input_stage_->set_composition(&itr->second);
    else
        by_pass_upload_stage_->set_composition(&itr->second);

    active_composition_completion_handler_ = std::move(completion_handler);

    // TODO: this should be done better, especially, when active_composition
    // is null the first stage must not execute (!)
    active_composition_.store(&itr->second);
}

void fb::StreamDispatch::stop_composition(const StreamComposition::ID& id) {
    
    auto itr = compositions_.find(id);
    if (itr == std::end(compositions_))
        throw std::invalid_argument("Composition with id '" + id + "' doesn't exist.");

    //std::lock_guard<std::mutex> lock(running_compositions_lock_);

    //auto res = running_compositions_.find(id);
    //if (res == std::end(running_compositions_))
    //    throw std::logic_error("Composition '" + id + "' does not seem to be running. Can't stop it.");

    //running_compositions_.erase(res);
    
    if (enable_input_stages_)
        frame_composition_input_stage_->set_composition(nullptr);
    else
        by_pass_upload_stage_->set_composition(nullptr);

    FB_LOG_INFO << "Stopped composition with name " << itr->second.name() << " ('" << id << "').";

}

bool fb::StreamDispatch::is_composition_running(const StreamComposition::ID& id) {

    throw std::runtime_error("Not implemented.");

    //auto itr = compositions_.find(id);
    //if (itr == std::end(compositions_))
    //    throw std::invalid_argument("Composition with id '" + id + "' doesn't exist.");

    //return running_composition_ == &itr->second;

    //std::lock_guard<std::mutex> lock(running_compositions_lock_);
    //return running_compositions_.find(id) != std::end(running_compositions_);
}

void fb::StreamDispatch::define_pipeline() {

    // TODO: do the same for upload IDs

    // TODO: the Stage classes should probably be derived from a templated
    // base class. That way you won't need a templated constructor. Alternatively
    // we could simply use inheritance from the Stage basis template... 
    // Needs to be thought about... 
    // The only benefit would be to have less if/else here in the initialization

    ////
    // CREATE INPUT STAGES
    ///

    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_upload_async_->attach();
    else
        gl_context_master_->attach();


    // Based on the implementation hints, different stage go will to 
    // different execution data collections

    std::vector<PipelineStageExecution>* host_upload_executor = nullptr;
    std::vector<PipelineStageExecution>* gl_upload_executor = nullptr;
    std::vector<PipelineStageExecution>* gl_render_executor = nullptr;
    std::vector<PipelineStageExecution>* gl_download_executor = nullptr;
    std::vector<PipelineStageExecution>* host_download_executor = nullptr;

    // That's the only one that always stays the same
    gl_render_executor = &gl_master_thread_stages_;

    if (impl_use_multiple_gl_contexts_) {

        // For multiple GL contexts, we deal with upload/download stages on 
        // separate threads.
        gl_upload_executor = &gl_upload_async_thread_stages_;
        gl_download_executor = &gl_download_async_thread_stages_;

    } else {

        // Otherwise they are all dealt in the same collection
        gl_upload_executor = gl_render_executor;
        gl_download_executor = gl_render_executor;

    }

    // If we are doing upload/download host-copies asynchronously, let's
    // point them to the right collection of executions. If not, then 
    // they are the same as on the GL upload/download threads.

    if (impl_async_input_) {
        host_upload_executor = &host_copy_input_async_thread_stages_;
    } else {
        host_upload_executor = gl_upload_executor;
    }

    if (impl_async_output_) {
        host_download_executor = &host_copy_output_async_thread_stages_;
    } else {
        host_download_executor = gl_download_executor;
    }


    if (enable_input_stages_) {

        // TODO: is there any difference if we created the buffers on the 
        // actual upload OGL context?

        PboMemVector upload_pbos = create_and_initialize_pbos(
            ProgramOptions::global().upload_pbo_rr_count(), 
            origin_format_.image_byte_size(), 
            TransferDirection::UPLOAD, 
            impl_use_arb_persistent_mapping_);

        if (((ProgramOptions::global().upload_copy_to_unmap_pipeline_size() + 
            ProgramOptions::global().upload_unmap_to_unpack_pipeline_size()) !=
            ProgramOptions::global().upload_pbo_rr_count()))
        {
            FB_LOG_CRITICAL 
                << "Invalid PBO configuration: Total upload PBO count is " 
                << ProgramOptions::global().upload_pbo_rr_count() 
                << ", copy-to-unmap-count is " 
                << ProgramOptions::global().upload_copy_to_unmap_pipeline_size() 
                << " and unmap-to-unpack PBO count is " 
                << ProgramOptions::global().upload_unmap_to_unpack_pipeline_size() 
                << ". The last two values must be exactly the sum of the upload PBO count.";
            throw std::invalid_argument("Invalid upload PBO pipeline configuration.");
        }

        PboMemVector copy_to_unmap_pbo_ids;
        for (size_t i = 0; i<ProgramOptions::global().upload_copy_to_unmap_pipeline_size(); ++i) {
            copy_to_unmap_pbo_ids.push_back(upload_pbos[i]);
        }

        PboMemVector unmap_to_unpack_pbo_ids;
        for (size_t i = 0; i<ProgramOptions::global().upload_unmap_to_unpack_pipeline_size(); ++i) {
            size_t idx = ProgramOptions::global().upload_copy_to_unmap_pipeline_size() + i;
            unmap_to_unpack_pbo_ids.push_back(upload_pbos[idx]);
        }

        frame_composition_input_stage_ = utils::make_unique<FrameCompositionInputStage>(
            ProgramOptions::global().frame_input_pipeline_size()
            );

        host_upload_executor->push_back(PipelineStageExecution(
            frame_composition_input_stage_.get()
            ));

        copy_host_to_mapped_pbo_up_stage_ = utils::make_unique<CopyHostToMappedPBOStage>(
            copy_to_unmap_pbo_ids,
            origin_format_,
            *frame_composition_input_stage_,
            !ProgramOptions::global().enable_host_copy_upload(),
            impl_use_arb_persistent_mapping_);

        host_upload_executor->push_back(PipelineStageExecution(
            copy_host_to_mapped_pbo_up_stage_.get(),
            frame_composition_input_stage_.get()
            ));

        unmap_pbo_up_stage_ = utils::make_unique<UnmapPBOStage>(
            unmap_to_unpack_pbo_ids,
            origin_format_,
            *copy_host_to_mapped_pbo_up_stage_,
            enable_upload_gl_timer_queries_,
            impl_use_arb_persistent_mapping_);

        gl_upload_executor->push_back(PipelineStageExecution(
            unmap_pbo_up_stage_.get(),
            copy_host_to_mapped_pbo_up_stage_.get()
            ));

        unpack_pbo_to_texture_stage_ = utils::make_unique<UnpackPBOToTextureStage>(
            ProgramOptions::global().source_texture_rr_count(),
            origin_format_, 
            *unmap_pbo_up_stage_,
            enable_upload_gl_timer_queries_,
            impl_use_multiple_gl_contexts_,
            impl_use_arb_persistent_mapping_);

        gl_upload_executor->push_back(PipelineStageExecution(
            unpack_pbo_to_texture_stage_.get(),
            unmap_pbo_up_stage_.get(),
            ProgramOptions::global().pipeline_upload_unmap_to_unpack_load_constraint_count()
            ));

    } else {

        by_pass_upload_stage_ =  utils::make_unique<ByPassUploadStage>(origin_format_);

        host_upload_executor->push_back(PipelineStageExecution(
            by_pass_upload_stage_.get()
            ));

    }

    if (impl_use_multiple_gl_contexts_)
        gl_shared_context_upload_async_->detach();
    else
        gl_context_master_->detach();

    ////
    // CREATE RENDER STAGES
    ////

    if (enable_render_stages_) {

        gl_context_master_->attach();

        if (enable_input_stages_ && enable_render_stages_) {

            // TODO: C++11: replace with make_unique
            format_converter_stage_input_to_render_ = std::unique_ptr<FormatConverterStage>(
                new FormatConverterStage(
                ProgramOptions::global().source_texture_rr_count(),
                origin_format_, 
                render_format_,
                impl_use_multiple_gl_contexts_, 
                false,
                *unpack_pbo_to_texture_stage_,
                enable_render_gl_timer_queries_,
                ProgramOptions::global().format_conversion_mode(),
                GL_TEXTURE_FETCH_BARRIER_BIT,
                ProgramOptions::global().v210_decode_glsl_430_work_group_size(),
                ProgramOptions::global().v210_decode_glsl_chroma_filter()));

            gl_render_executor->push_back(PipelineStageExecution(
                format_converter_stage_input_to_render_.get(),
                unpack_pbo_to_texture_stage_.get(),
                ProgramOptions::global().pipeline_upload_unpack_to_format_converter_load_constraint_count()
                ));

        } else {

            format_converter_stage_input_to_render_ = std::unique_ptr<FormatConverterStage>(
                new FormatConverterStage(
                ProgramOptions::global().source_texture_rr_count(),
                origin_format_, 
                render_format_,
                false,
                false,
                *by_pass_upload_stage_,
                enable_render_gl_timer_queries_,
                ProgramOptions::global().format_conversion_mode(),
                GL_TEXTURE_FETCH_BARRIER_BIT,
                ProgramOptions::global().v210_decode_glsl_430_work_group_size(),
                ProgramOptions::global().v210_decode_glsl_chroma_filter()));

            gl_render_executor->push_back(PipelineStageExecution(
                format_converter_stage_input_to_render_.get(),
                by_pass_upload_stage_.get()
                ));
        }

        render_stage_ = std::unique_ptr<RenderStage>(
            new RenderStage(
                ProgramOptions::global().destination_texture_rr_count(),
                render_format_.width(),
                render_format_.height(),
                *format_converter_stage_input_to_render_,
                render_format_,
                enable_render_gl_timer_queries_));

        gl_render_executor->push_back(PipelineStageExecution(
            render_stage_.get(),
            format_converter_stage_input_to_render_.get()
            ));

        format_converter_stage_render_to_output_= std::unique_ptr<FormatConverterStage>(
            new FormatConverterStage(
                ProgramOptions::global().destination_texture_rr_count(),
                render_format_, 
                origin_format_,
                false, 
                impl_use_multiple_gl_contexts_ && enable_output_stages_,
                *render_stage_,
                enable_render_gl_timer_queries_,
                ProgramOptions::global().format_conversion_mode(),
                GL_PIXEL_BUFFER_BARRIER_BIT,
                ProgramOptions::global().v210_encode_glsl_430_work_group_size(),
                ProgramOptions::global().v210_encode_glsl_chroma_filter()));

        gl_render_executor->push_back(PipelineStageExecution(
            format_converter_stage_render_to_output_.get(),
            render_stage_.get()
            ));

        gl_context_master_->detach();

    }

    ////
    // CREATE OUTPUT STAGES
    ////

    if (enable_output_stages_) {

        if (impl_use_multiple_gl_contexts_)
            gl_shared_context_download_async_->attach();
        else
            gl_context_master_->attach();

        PboMemVector download_pbos = create_and_initialize_pbos(
            ProgramOptions::global().download_pbo_rr_count(), 
            origin_format_.image_byte_size(), 
            TransferDirection::DOWNLOAD, 
            impl_use_arb_persistent_mapping_);

        
        if ((ProgramOptions::global().download_map_to_copy_pipeline_size() + 
            ProgramOptions::global().download_pack_to_map_pipeline_size()) !=
            ProgramOptions::global().download_pbo_rr_count())
        {

            FB_LOG_CRITICAL 
                << "Invalid PBO configuration: Total download PBO count is " 
                << ProgramOptions::global().download_pbo_rr_count() 
                << ", punmap_to_unpack_load_constraint_countack-to-map-count is " 
                << ProgramOptions::global().download_pack_to_map_pipeline_size() 
                << " and map-to-copy PBO count is " 
                << ProgramOptions::global().download_map_to_copy_pipeline_size() 
                << ". The last two values must be exactly the sum of the download PBO count.";
            throw std::invalid_argument("Invalid download PBO pipeline configuration.");

        }

        PboMemVector pack_to_map_pbo_ids;
        for (size_t i = 0; i<ProgramOptions::global().download_pack_to_map_pipeline_size(); ++i) {
            pack_to_map_pbo_ids.push_back(download_pbos[i]);
        }

        PboMemVector map_to_copy_pbo_ids;
        for (size_t i = 0; i<ProgramOptions::global().download_map_to_copy_pipeline_size(); ++i) {
            size_t idx = ProgramOptions::global().download_pack_to_map_pipeline_size() + i;
            map_to_copy_pbo_ids.push_back(download_pbos[idx]);
        }

        if (enable_render_stages_) {

            pack_texture_to_pbo_stage_ = std::unique_ptr<PackTextureToPBOStage>(
                new PackTextureToPBOStage(
                    pack_to_map_pbo_ids,
                    origin_format_,
                    *format_converter_stage_render_to_output_,
                    enable_download_gl_timer_queries_,
                    impl_use_arb_persistent_mapping_,
                    impl_use_multiple_gl_contexts_,
                    impl_use_arb_persistent_mapping_));

            gl_download_executor->push_back(PipelineStageExecution(
                pack_texture_to_pbo_stage_.get(),
                format_converter_stage_render_to_output_.get(),
                ProgramOptions::global().pipeline_download_format_converter_to_pack_load_constraint_count()
                ));

        } else {

            // Directly connect with the input stage's last node

            if (enable_input_stages_) {

                pack_texture_to_pbo_stage_ = std::unique_ptr<PackTextureToPBOStage>(
                    new PackTextureToPBOStage(
                        pack_to_map_pbo_ids,
                        origin_format_,
                        *unpack_pbo_to_texture_stage_,
                        enable_download_gl_timer_queries_,
                        impl_use_arb_persistent_mapping_,
                        impl_use_multiple_gl_contexts_,
                        impl_use_arb_persistent_mapping_));

                gl_download_executor->push_back(PipelineStageExecution(
                    pack_texture_to_pbo_stage_.get(),
                    unpack_pbo_to_texture_stage_.get()
                    ));

            } else {

                pack_texture_to_pbo_stage_ = std::unique_ptr<PackTextureToPBOStage>(
                    new PackTextureToPBOStage(
                        pack_to_map_pbo_ids,
                        origin_format_,
                        *by_pass_upload_stage_,
                        enable_download_gl_timer_queries_,
                        impl_use_arb_persistent_mapping_,
                        impl_use_multiple_gl_contexts_,
                        impl_use_arb_persistent_mapping_));

                gl_download_executor->push_back(PipelineStageExecution(
                    pack_texture_to_pbo_stage_.get(),
                    by_pass_upload_stage_.get()
                    ));

            }

        }

        map_pbo_down_stage_ = std::unique_ptr<MapPBOStage>(
            new MapPBOStage(
                map_to_copy_pbo_ids,
                origin_format_,
                *pack_texture_to_pbo_stage_,
                enable_download_gl_timer_queries_,
                impl_use_arb_persistent_mapping_));

        gl_download_executor->push_back(PipelineStageExecution(
            map_pbo_down_stage_.get(),
            pack_texture_to_pbo_stage_.get(),
            ProgramOptions::global().pipeline_download_pack_to_map_load_constraint_count()
            ));

        copy_mapped_pbo_down_stage = std::unique_ptr<CopyMappedPBOToHostStage>(
            new CopyMappedPBOToHostStage(
                ProgramOptions::global().frame_output_cache_count(),
                origin_format_,
                *map_pbo_down_stage_,
                !ProgramOptions::global().enable_host_copy_download()
            ));

        host_download_executor->push_back(PipelineStageExecution(
            copy_mapped_pbo_down_stage.get(),
            map_pbo_down_stage_.get()
            ));

        frame_composition_output_stage_ = std::unique_ptr<FrameCompositionOutputStage>(
            new FrameCompositionOutputStage(
                *copy_mapped_pbo_down_stage));

        host_download_executor->push_back(PipelineStageExecution(
            frame_composition_output_stage_.get(),
            copy_mapped_pbo_down_stage.get()
            ));

        if (impl_use_multiple_gl_contexts_)
            gl_shared_context_download_async_->detach();
        else
            gl_context_master_->detach();

    } else {

        if (enable_render_stages_) {

            by_pass_download_stage_ = std::unique_ptr<ByPassDownloadStage>(
                new ByPassDownloadStage(*format_converter_stage_render_to_output_)
                );

            host_download_executor->push_back(PipelineStageExecution(
                by_pass_download_stage_.get(),
                format_converter_stage_render_to_output_.get()
                ));

        } else {

            if (enable_input_stages_) {

                by_pass_download_stage_ = std::unique_ptr<ByPassDownloadStage>(
                    new ByPassDownloadStage(*unpack_pbo_to_texture_stage_)
                    );

                host_download_executor->push_back(PipelineStageExecution(
                    by_pass_download_stage_.get(),
                    unpack_pbo_to_texture_stage_.get()
                    ));

            } else {

                by_pass_download_stage_ = std::unique_ptr<ByPassDownloadStage>(
                    new ByPassDownloadStage(*by_pass_upload_stage_)
                    );

                host_download_executor->push_back(PipelineStageExecution(
                    by_pass_download_stage_.get(),
                    by_pass_upload_stage_.get()
                    ));

            }

        }

    }
}

std::string fb::StreamDispatch::executions_to_string(const std::vector<PipelineStageExecution>& executions) {

    std::ostringstream oss;
    oss << "[ BEGIN";
    for (const auto& el : executions) {
        oss << "'" << el.stage->name() << "'";
        if (el.previous_stage != nullptr) {
            oss << " (prev='" << el.previous_stage->name() << "', ";
            oss << "input_load_constraint=" << el.input_load_constraint << ") -> ";
        }
    }

    oss << "END ]";

    return oss.str();

}

void fb::StreamDispatch::run_pipeline_stages(
    const std::vector<PipelineStageExecution>& executions,
    const std::string& name) 
{

    if (executions.empty()) {
        FB_LOG_WARNING << "Phase '" << name << "' was empty, not executing at all.";
        return;
    }

    auto executions_string = executions_to_string(executions);

    FB_LOG_INFO << "Starting phase '" << name << "' with '" << executions_string << "'.";

    try {

        auto itr_begin = std::begin(executions);
        auto itr_end = std::end(executions);

        // Validate the input constraint numbers
        for (auto itr = itr_begin; itr != itr_end; ++itr) 
        {

            const PipelineStageExecution& stage_execution = *itr;

            size_t this_stage_input_load_constraint = stage_execution.input_load_constraint;

            if (this_stage_input_load_constraint != 0) {

                if (stage_execution.previous_stage == nullptr) {
                    FB_LOG_CRITICAL 
                        << "Stage '" << stage_execution.stage->name()
                        << "' requires input constraints of '" 
                        << stage_execution.input_load_constraint 
                        << "', but the previous stage is nullptr.";
                    throw std::runtime_error("Invalid stage executions.");
                }

                // Check if ANY of the previous stages has and output queue size
                // less of the input constraint (in which case we would 
                // deadlock).
                // TODO: really check if we couldn't relax these constraints
                // a bit... but I think they are actually correct.
                for (auto itr_prev = itr_begin; itr_prev != itr; ++itr_prev) {

                    if (itr_prev->stage->output_queue_size() < this_stage_input_load_constraint) {
                        FB_LOG_CRITICAL 
                            << "Stage '" << stage_execution.stage->name()
                            << "' requires input constraints of '" 
                            << stage_execution.input_load_constraint 
                            << "', but one of the previous stages ('" 
                            << itr_prev->stage->name() << "') has only an "
                            << "output queue size of " 
                            << itr_prev->stage->output_queue_size() 
                            << ", which is smaller than the requested input "
                            << "constraint and would cause a deadlock.";

                        throw std::runtime_error("Invalid input constraint configuration.");
                    }

                }

            }

        }

        auto is_done = [&]{
            return std::none_of(
                std::begin(executions), 
                std::end(executions), 
                [](const PipelineStageExecution& e) -> bool { 
                    return (e.stage->status() == PipelineStatus::READY_TO_EXECUTE); 
            });
        };

        while(!is_done()) {

            bool previous_stages_are_still_up = true;

            for (const auto& stage_execution : executions) {

                // TODO: have we validated the input constraint with the input size before?
                // otherwise we might run into a deadlock

                // If this stage has an input load constraint (requiring 
                // that a certain of number of tokens are in the pipe, e.g.
                // to ensure that background processes are well fed, and IF
                // the previous stage is not null, and IF
                // all previous stages are still able to execute, 
                // and IF we haven't meet the input load constraint criterium,
                // THEN we'll break out of the loop and start from the beginning.

                if (stage_execution.input_load_constraint != 0 &&
                    stage_execution.previous_stage != nullptr &&
                    previous_stages_are_still_up &&
                    stage_execution.stage->input_queue_num_elements() < stage_execution.input_load_constraint &&
                    stage_execution.previous_stage->status() == PipelineStatus::READY_TO_EXECUTE) 
                {
                    break;
                }

                if (stage_execution.stage->status() == PipelineStatus::READY_TO_EXECUTE) {
                    stage_execution.stage->execute();
                }

                previous_stages_are_still_up = previous_stages_are_still_up && (stage_execution.stage->status() == PipelineStatus::READY_TO_EXECUTE);

            }

        }

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Phase '" << name << "' caught an exception: " << e.what() << ".";
    }

    FB_LOG_INFO << "Phase '" << name << "' is done executing.";

}

void fb::StreamDispatch::wait_for_composition()
{

    while (active_composition_.load() == nullptr)
        std::this_thread::yield();

}

void fb::StreamDispatch::execute_host_copy_input_async() {

    static const std::string name = "host_copy_input_async";

    try {

        wait_for_composition();

        run_pipeline_stages(host_copy_input_async_thread_stages_, name);

        FB_LOG_DEBUG 
            << "Thread '" << name << "' ('" 
            << std::this_thread::get_id() << "') is done.";

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Thread '" << name << "' caught exception: '" << e.what() << "'.";
    }    

}

void fb::StreamDispatch::execute_gl_upload_async() {

    static const std::string name = "gl_upload_async";

    try {

        gl_shared_context_upload_async_->attach();

        gl::utils::insert_debug_message("Starting gl_upload_async execution.");

        wait_for_composition();

        run_pipeline_stages(gl_upload_async_thread_stages_, name);

        gl_shared_context_upload_async_->detach();

        FB_LOG_DEBUG 
            << "Thread '" << name << "' ('" 
            << std::this_thread::get_id() << "') is done.";

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Thread '" << name << "' caught exception: '" << e.what() << "'.";
    }    

}

void fb::StreamDispatch::execute_gl_master() {

    static const std::string name = "gl_master";

    try {

        gl_context_master_->attach();

        gl::utils::insert_debug_message("Starting gl_master execution.");

        wait_for_composition();

        run_pipeline_stages(gl_master_thread_stages_, name);

        gl_context_master_->detach();

        FB_LOG_DEBUG 
            << "Thread '" << name << "' ('" 
            << std::this_thread::get_id() << "') is done.";

        // TODO: that is not too nice, should be done by stage itself, or otherwise
        if (!impl_use_multiple_gl_contexts_ && !impl_async_output_) {
            FB_LOG_DEBUG << "Calling completion handler from " << name << ".";        
            if (active_composition_completion_handler_)
                active_composition_completion_handler_();
        }

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Thread '" << name << "' caught exception: '" << e.what() << "'.";
    }    
}

void fb::StreamDispatch::execute_gl_download_async() {

    static const std::string name = "gl_download_async";

    try {

        gl_shared_context_download_async_->attach();

        gl::utils::insert_debug_message("Starting gl_download_async execution.");

        wait_for_composition();

        run_pipeline_stages(gl_download_async_thread_stages_, name);

        gl_shared_context_download_async_->detach();

        FB_LOG_DEBUG 
            << "Thread '" << name << "' ('" 
            << std::this_thread::get_id() << "') is done.";

        // TODO: that is not too nice, should be done by stage itself, or otherwise
        if (!impl_async_output_) {
            FB_LOG_DEBUG << "Calling completion handler from " << name << ".";
            if (active_composition_completion_handler_)
                active_composition_completion_handler_();
        }

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Thread '" << name << "' caught exception: '" << e.what() << "'.";
    }    

}

void fb::StreamDispatch::execute_host_copy_output_async() {

    static const std::string name = "host_copy_output_async";

    try {

        wait_for_composition();

        run_pipeline_stages(host_copy_output_async_thread_stages_, name);

        FB_LOG_DEBUG 
            << "Thread '" << name << "' ('" 
            << std::this_thread::get_id() << "') is done.";

        FB_LOG_DEBUG << "Calling completion handler from " << name << ".";
        if (active_composition_completion_handler_)
            active_composition_completion_handler_();

    } catch (const std::exception& e) {
        FB_LOG_CRITICAL << "Thread '" << name << "' caught exception: '" << e.what() << "'.";
    }    

}

fb::PboMemVector fb::StreamDispatch::create_and_initialize_pbos(
    size_t num,
    size_t byte_size,
    TransferDirection transfer_direction,
    bool use_persistent_mapping)
{

    if (num == 0) 
        throw std::invalid_argument("Can't create zero PBOs.");

    if (byte_size == 0)
        throw std::invalid_argument("Can't create zero-sized PBOs.");

    GLenum target = 0;
    GLenum usage = 0;

    // We use client-storage, since we will have to read/write from
    // client memory anyway. On some architectures this might provide us
    // with special memory which can be accessed by the GPU without
    // copying
    GLbitfield buffer_storage_flags = GL_CLIENT_STORAGE_BIT;

    PboMemVector answer;

    std::vector<GLuint>* target_pbo_id_container = nullptr;

    if (transfer_direction == TransferDirection::UPLOAD) {

        target = GL_PIXEL_UNPACK_BUFFER;
        usage = GL_STREAM_DRAW;
        target_pbo_id_container = &upload_pbo_ids_;
        buffer_storage_flags |= GL_MAP_WRITE_BIT;

    } else if (transfer_direction == TransferDirection::DOWNLOAD) {

        target = GL_PIXEL_PACK_BUFFER;
        usage = GL_STREAM_READ;
        target_pbo_id_container = &download_pbo_ids_;
        buffer_storage_flags |= GL_MAP_READ_BIT;

    } else {
        throw std::invalid_argument("Invalid parameters for PBO creation.");
    }

    if (GLAD_GL_ARB_buffer_storage != 0)  {
        FB_LOG_DEBUG << "ARB_buffer_storage is supported, will use immutable buffer storage";
    }

    if (GLAD_GL_ARB_buffer_storage != 0 && use_persistent_mapping)  {
        FB_LOG_DEBUG << "Will use ARB_buffer_storage persistent memory";
        buffer_storage_flags |= GL_MAP_PERSISTENT_BIT;
    }
    
    std::vector<GLuint> pbo_ids = std::vector<GLuint>(num, 0);

    glGenBuffers(
        static_cast<GLsizei>(pbo_ids.size()), 
        pbo_ids.data());

    for (const auto& id : pbo_ids) {

        glBindBuffer(target, id);

        if (GLAD_GL_ARB_buffer_storage != 0) {
          glBufferStorage(
              target,
              static_cast<GLsizeiptr>(byte_size),
              static_cast<const GLvoid*>(nullptr),
              buffer_storage_flags);
        } else {
          glBufferData(
              target, 
              static_cast<GLsizeiptr>(byte_size),
              static_cast<const GLvoid*>(nullptr),
              usage);
        }

        target_pbo_id_container->push_back(id);

        answer.push_back(PboMemory(id));

        glBindBuffer(target, 0);

    }

    return answer;
}

std::ostream& fb::operator<< (std::ostream& out, const fb::StreamDispatch::Flags& v) {

    switch (v) {
    case StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS:
        out << "MULTIPLE_GL_CONTEXTS";
        break;
    case StreamDispatch::Flags::ASYNC_INPUT:
        out << "ASYNC_INPUT";
        break;
    case StreamDispatch::Flags::ASYNC_OUTPUT:
        out << "ASYNC_OUTPUT";
        break;
    case StreamDispatch::Flags::ARB_PERSISTENT_MAPPING:
        out << "ARB_PERSISTENT_MAPPING";
        break;
    case StreamDispatch::Flags::COPY_PBO_BEFORE_DOWNLOAD:
        out << "COPY_PBO_BEFORE_DOWNLOAD";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::operator>>(std::istream& in, StreamDispatch::Flags& v) {
    
    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "MULTIPLE_GL_CONTEXTS")
        v = fb::StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS;
    else if (token == "ASYNC_INPUT")
        v = fb::StreamDispatch::Flags::ASYNC_INPUT;
    else if (token == "ASYNC_OUTPUT")
        v = fb::StreamDispatch::Flags::ASYNC_OUTPUT;
    else if (token == "ARB_PERSISTENT_MAPPING")
        v = fb::StreamDispatch::Flags::ARB_PERSISTENT_MAPPING;
    else if (token == "COPY_PBO_BEFORE_DOWNLOAD")
        v = fb::StreamDispatch::Flags::COPY_PBO_BEFORE_DOWNLOAD;
    else {
        in.setstate(std::ios::failbit);
    }

    return in;
}

void fb::StreamDispatch::print_flags(std::ostream& out, const FlagContainer& v)
{

    out << "[";

    for (size_t i = 0; i<static_cast<size_t>(StreamDispatch::Flags::COUNT); ++i) {

        StreamDispatch::Flags f = StreamDispatch::Flags(i);

        out << f << "=";

        size_t idx = static_cast<size_t>(f);
        out << v[idx];

        if ((i+1) < static_cast<size_t>(StreamDispatch::Flags::COUNT))
            out << ", ";
    }

    out << "]";

}
