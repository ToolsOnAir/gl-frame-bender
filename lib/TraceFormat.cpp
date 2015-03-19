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

#include "TraceFormat.h"

#include "Logging.h"
#include "UtilsGL.h"

#ifdef _MSC_VER
#pragma warning(push)        
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif

#include "fbt_format.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/gzip_stream.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time.hpp>

#include <stdexcept>

namespace bf = boost::filesystem;
namespace bt = boost::posix_time;

namespace fb = toa::frame_bender;
namespace fbf = fbt_format;

fb::TraceFormatWriter::TraceFormatWriter(
    const std::string& file_path, 
    const std::string& session_name, 
    const gl::utils::GLInfo& gl_info) : 
        was_flushed_(false)
{

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    bf::path out_path(file_path);

    if (bf::is_directory(out_path)) {
        FB_LOG_ERROR << "Path '" << out_path << "' is a directory, can't write to it.";
        throw std::invalid_argument("Out file is a directory.");
    }

    if (bf::exists(out_path)) {
        FB_LOG_WARNING << "File '" << out_path << "' will be overwritten.";
    }

    writer_ = std::unique_ptr<std::ofstream>(new bf::ofstream(out_path, std::ios::binary | std::ios::out));

    if (!writer_->good()) {
        FB_LOG_ERROR << "Can't open out stream to file '" << file_path << "'.";
        throw std::invalid_argument("Invalid file argument for trace output.");
    }

    container_ = utils::make_unique<fbf::TraceSession>();

    container_->set_name(session_name);

    container_->set_local_time(bt::to_simple_string(bt::second_clock::local_time()));

    container_->mutable_opengl_info()->set_vendor(gl_info.vendor);
    container_->mutable_opengl_info()->set_renderer(gl_info.renderer);
    container_->mutable_opengl_info()->set_version(gl_info.version_string);

}

fb::TraceFormatWriter::~TraceFormatWriter()
{

    if (!was_flushed())
        flush();

}

fbf::StageTrace::EventType get_event_type(fb::StageExecutionState state) {

    fbf::StageTrace::EventType answer;

    switch (state) {

    case fb::StageExecutionState::EXECUTE_BEGIN:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_EXECUTE_BEGIN;
        break;

    case fb::StageExecutionState::INPUT_TOKEN_AVAILABLE:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_INPUT_TOKEN_AVAILABLE;
        break;

    case fb::StageExecutionState::OUTPUT_TOKEN_AVAILABLE:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_OUTPUT_TOKEN_AVAILABLE;
        break;

    case fb::StageExecutionState::TASK_BEGIN:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_TASK_BEGIN;
        break;

    case fb::StageExecutionState::TASK_END:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_TASK_END;
        break;

    case fb::StageExecutionState::EXECUTE_END:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_EXECUTE_END;
        break;

    case fb::StageExecutionState::GL_TASK_BEGIN:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_GL_TASK_BEGIN;
        break;

    case fb::StageExecutionState::GL_TASK_END:
        answer = fbf::StageTrace::EventType::StageTrace_EventType_GL_TASK_END;
        break;

    default:
        throw std::runtime_error("Unexpected stage execution stage.");
        break;
    }

    return answer;

}

void fb::TraceFormatWriter::add_sampler_statistic(
    fbt_format::StageTrace* trace_container,
    const StageExecutionState& begin_event,
    const StageExecutionState& end_event,
    const StageSampler::Statistic& statistic)
{

    auto delta_statistic = trace_container->add_delta_statistics();

    delta_statistic->set_begin_event(get_event_type(begin_event));
    delta_statistic->set_end_event(get_event_type(end_event));
    
    static_assert(clock::period::num == 1, "Expecting a period numerator of 1.");
    static_assert(clock::period::den == 1000000000, "Expecting nanoseconds as the period of time.");
    
    delta_statistic->set_name(statistic.name);
    delta_statistic->set_average_ns(statistic.average.count());
    delta_statistic->set_minimum_ns(statistic.minimum.count());
    delta_statistic->set_maximum_ns(statistic.maximum.count());
    delta_statistic->set_std_deviation_ns(statistic.std_deviation.count());
    delta_statistic->set_median_ns(statistic.median.count());
    delta_statistic->set_num_samples(statistic.num_samples);

}

void fb::TraceFormatWriter::add_stage_sampler(const std::string& name, const StageSampler& sampler)
{

    auto stage_trace = container_->add_stage_traces();

    stage_trace->set_name(name);

    // Get all name overrides and convert it to FBF format.

    for (auto& name_override : sampler.name_overrides_) {

        auto fbf_name_override = stage_trace->add_name_overrides();
        fbf_name_override->set_type(get_event_type(name_override.first));
        fbf_name_override->set_name(name_override.second);

    }

    // For each event type, we get all events and write to our format
    for (size_t i = 0; i<static_cast<size_t>(StageExecutionState::NUMBER_OF_STATES); ++i)
    {

        StageExecutionState state = StageExecutionState(i);

        size_t number_of_trace_events = sampler.number_of_sampled_trace_events(state);

        if (number_of_trace_events > 0) {

            auto event_trace = stage_trace->add_event_traces();
            event_trace->set_type(get_event_type(state));

            for (size_t j = 0; j<number_of_trace_events; ++j) {

                auto trace_event = sampler.get_trace_event(state, j);

                event_trace->add_trace_times_ns(static_cast<google::protobuf::int64>(trace_event.time_since_epoch().count()));

                static_assert(clock::period::num == 1, "Expecting a period numerator of 1.");
                static_assert(clock::period::den == 1000000000, "Expecting nanoseconds as the period of time.");



            }

        }

    }

    // Also add sample statistics that might be interesting
    std::vector<fb::StageSampler::Statistic> answer;

    StageExecutionState begin_event = StageExecutionState::TASK_BEGIN;
    StageExecutionState end_event = StageExecutionState::TASK_END;
    if (sampler.number_of_sampled_trace_events(begin_event) != 0) {

        add_sampler_statistic(
            stage_trace,
            begin_event,
            end_event,
            StageSampler::build_delta_statistic(sampler, begin_event,sampler,end_event));

    }

    begin_event = StageExecutionState::EXECUTE_BEGIN;
    end_event = StageExecutionState::EXECUTE_END;
    if (sampler.number_of_sampled_trace_events(begin_event) != 0) {

        add_sampler_statistic(
            stage_trace,
            begin_event,
            end_event,
            StageSampler::build_delta_statistic(sampler, begin_event,sampler,end_event));

    }

    begin_event = StageExecutionState::EXECUTE_BEGIN;
    end_event = StageExecutionState::INPUT_TOKEN_AVAILABLE;
    if (sampler.number_of_sampled_trace_events(begin_event) != 0) {

        add_sampler_statistic(
            stage_trace,
            begin_event,
            end_event,
            StageSampler::build_delta_statistic(sampler, begin_event,sampler,end_event));

    }

    begin_event = StageExecutionState::EXECUTE_BEGIN;
    end_event = StageExecutionState::OUTPUT_TOKEN_AVAILABLE;
    if (sampler.number_of_sampled_trace_events(begin_event) != 0) {

        add_sampler_statistic(
            stage_trace,
            begin_event,
            end_event,
            StageSampler::build_delta_statistic(sampler, begin_event,sampler,end_event));

    }

    begin_event = StageExecutionState::GL_TASK_BEGIN;
    end_event = StageExecutionState::GL_TASK_END;
    if (sampler.number_of_sampled_trace_events(begin_event) != 0) {

        add_sampler_statistic(
            stage_trace,
            begin_event,
            end_event,
            StageSampler::build_delta_statistic(sampler, begin_event,sampler,end_event));

    }

}

void fb::TraceFormatWriter::set_session_statistics(
    uint64_t number_of_frames_processed,
    float avg_throughput_mb_per_sec,
    int64_t med_frame_processing_time_per_frame_ns,
	float avg_millisecs_per_frame)
{

    container_->mutable_session_statistic()->set_number_of_frames_processed(number_of_frames_processed);
    container_->mutable_session_statistic()->set_avg_throughput_mb_per_sec(avg_throughput_mb_per_sec);
    container_->mutable_session_statistic()->set_med_frame_processing_time_per_frame_ns(med_frame_processing_time_per_frame_ns);
	container_->mutable_session_statistic()->set_avg_millisecs_per_frame(avg_millisecs_per_frame);
}

bool fb::TraceFormatWriter::was_flushed() {

    return was_flushed_;

}

void fb::TraceFormatWriter::flush() {

    if (was_flushed())
        throw std::runtime_error("Writer has already been flushed.");

    using google::protobuf::io::OstreamOutputStream;
    using google::protobuf::io::GzipOutputStream;

    {

        OstreamOutputStream out_stream(writer_.get());
        // Note that this writes out a gzipped-file
        GzipOutputStream gzip_out_stream(&out_stream);

        container_->SerializeToZeroCopyStream(&gzip_out_stream);
    }
    //gzip_out_stream.Close();

    writer_->close();
    was_flushed_ = true;

}
