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
#include "StageSampler.h"
#include <algorithm>
#include <limits>
#include <sstream>

namespace fb = toa::frame_bender;
namespace bc = boost::chrono;

fb::StageSampler::StageSampler(const std::map<StageExecutionState, std::string>& name_overrides)
    : name_overrides_(name_overrides)
{

    sample_overflow_.fill(false);

    // Report size
    size_t byte_size = 0;
    for (const auto& e : trace_times_)
        byte_size += e.size() * sizeof(EventTrace::value_type);

    FB_LOG_DEBUG << "StageSampler uses " << byte_size << " bytes for trace data.";

    // Initialize iterators
    std::transform(
        std::begin(trace_times_), 
        std::end(trace_times_), 
        std::begin(trace_iterators_), 
        [](EventTrace&  e) { return std::begin(e); });

    std::transform(
        std::begin(trace_times_), 
        std::end(trace_times_), 
        std::begin(trace_end_iterators_), 
        [](EventTrace&  e) { return std::end(e); });

}

void fb::StageSampler::enter_sample(StageExecutionState state, const clock::time_point& time) {

    auto& itr = trace_iterators_[static_cast<size_t>(state)];
    auto end_itr = trace_end_iterators_[static_cast<size_t>(state)];

    if (itr != end_itr) {
        *itr = time;
        itr++;
    } else {
        sample_overflow_[static_cast<size_t>(state)]++;
    }

}

void fb::StageSampler::sample(StageExecutionState state) 
{

    clock::time_point sample_time = clock::now();
    enter_sample(state, sample_time);

}

void fb::StageSampler::set_name_overrides(std::map<StageExecutionState, std::string> overrides) {

    name_overrides_ = std::move(overrides);

}

std::string fb::StageSampler::get_name(StageExecutionState state) const {

    std::ostringstream oss;

    auto itr = name_overrides_.find(state);

    if (itr != std::end(name_overrides_)) {

        oss << itr->second;

    } else {

        oss << state;

    }

    return oss.str();

}

fb::StageSampler::Statistic fb::StageSampler::build_delta_statistic(
                const StageSampler& begin_sampler,
                StageExecutionState begin_state,
                const StageSampler& end_sampler,
                StageExecutionState end_state)
{
    Statistic s;

    size_t start_idx = static_cast<size_t>(begin_state);
    size_t end_idx = static_cast<size_t>(end_state);

    bool was_overflow = (begin_sampler.sample_overflow(begin_state) != 0) || (end_sampler.sample_overflow(end_state) != 0);

    std::ostringstream oss;
    oss << begin_sampler.get_name(begin_state) << "<->" << end_sampler.get_name(end_state);

    if (was_overflow) {

        FB_LOG_WARNING 
            << "Statistic '" << oss.str() 
            << "' is biased because there were too many samples to hold "
            << "within one trace. State '" << begin_state 
            << "' missed " << begin_sampler.sample_overflow(begin_state)
            << "' samples, state '" << end_state << "' missed " 
            <<  end_sampler.sample_overflow(end_state) << " samples.";

        oss << " (biased, overflow happened)";

    }

    s.name = oss.str();

    EventTrace::const_iterator start_trace = std::begin(begin_sampler.trace_times_[start_idx]);
    EventTrace::const_iterator start_trace_end = begin_sampler.trace_iterators_[start_idx];
    size_t start_num_traces = std::distance(start_trace, start_trace_end);

    EventTrace::const_iterator end_trace = std::begin(end_sampler.trace_times_[end_idx]);
    EventTrace::const_iterator end_trace_end = end_sampler.trace_iterators_[end_idx];
    size_t end_num_traces = std::distance(end_trace, end_trace_end);

    if (start_num_traces < end_num_traces) {
        FB_LOG_ERROR << "Can't calculate statistics with incoherent traces, start_num_traces is less than end_num_traces.";
        throw std::runtime_error("Incoherent number of traces, can't calculate statistics.");
    }

    auto itr_starts = start_trace;
    auto itr_ends = end_trace;

    s.num_samples = start_num_traces;

    clock::duration::rep min_count = std::numeric_limits<clock::duration::rep>::max();
    clock::duration::rep max_count = std::numeric_limits<clock::duration::rep>::min();

    uintmax_t sum = 0;

    std::vector<clock::duration> delta_times;
    delta_times.reserve(start_num_traces);

    // calculate average

    for (;itr_ends != end_trace_end; ++itr_starts, ++itr_ends) {

        auto delta = *itr_ends - *itr_starts;
        sum += delta.count();

        min_count = std::min(min_count, delta.count());
        max_count = std::max(max_count, delta.count());

        delta_times.push_back(delta);
    }

    s.minimum = clock::duration(min_count);
    s.maximum = clock::duration(max_count);

    s.average = clock::duration(sum / s.num_samples);

    // Calculate median

    // Sorts ascending
    std::sort(std::begin(delta_times), std::end(delta_times));

    bool is_odd = ((start_num_traces % 2) == 1);

    if (is_odd) {
        s.median = delta_times.at(start_num_traces/2);
    } else {
        auto a = delta_times.at(start_num_traces/2).count();
        auto b = delta_times.at(start_num_traces/2 -1).count();
        s.median = clock::duration((a + b)/2);
    }

    // Calculate stddev

    double sum_d = static_cast<double>(sum);
    double expected = sum_d / s.num_samples;
    double num_samples_d = static_cast<double>(s.num_samples);
    double sum_squared_expectancies_d = 0.0f;

    itr_starts = start_trace;
    itr_ends = end_trace;

    for (;itr_starts != start_trace_end; ++itr_starts, ++itr_ends) {

        auto diff = *itr_ends - *itr_starts;

        double diff_d = static_cast<double>(diff.count());

        double diff_to_expected = (diff_d - expected);
        sum_squared_expectancies_d += diff_to_expected*diff_to_expected;

    }

    double stddev_d = std::sqrt((sum_squared_expectancies_d / (num_samples_d-1)));
    uintmax_t stddev = static_cast<uintmax_t>(stddev_d);

    s.std_deviation = clock::duration(stddev);

    return s;
}

const fb::StageSampler::TraceEvent& fb::StageSampler::get_trace_event(
    StageExecutionState state, 
    size_t idx) const
{

    size_t num_samples_available = number_of_sampled_trace_events(state);

    if (!(idx < num_samples_available)) {
        FB_LOG_ERROR 
            << "Index " << idx 
            << " is larger than the number of available samples (" 
            << num_samples_available << ").";
        throw std::invalid_argument("Trace index out-of-range.");
    }

    return trace_times_[static_cast<size_t>(state)][idx];

}

size_t fb::StageSampler::number_of_sampled_trace_events(StageExecutionState state) const {

    EventTrace::const_iterator itr = std::begin(trace_times_[static_cast<size_t>(state)]);
    EventTrace::const_iterator itr_end = trace_iterators_[static_cast<size_t>(state)];

    return std::distance(
        itr,
        itr_end);
        
}

std::vector<fb::StageSampler::Statistic> fb::StageSampler::collect_statistics() const
{

    std::vector<fb::StageSampler::Statistic> answer;

    if (number_of_sampled_trace_events(StageExecutionState::TASK_BEGIN) != 0)
        answer.push_back(build_delta_statistic(*this, StageExecutionState::TASK_BEGIN, *this, StageExecutionState::TASK_END));

    if (number_of_sampled_trace_events(StageExecutionState::EXECUTE_BEGIN) != 0)
        answer.push_back(build_delta_statistic(*this, StageExecutionState::EXECUTE_BEGIN, *this, StageExecutionState::EXECUTE_END));

    if (number_of_sampled_trace_events(StageExecutionState::EXECUTE_BEGIN) != 0)
        answer.push_back(build_delta_statistic(*this, StageExecutionState::EXECUTE_BEGIN, *this, StageExecutionState::INPUT_TOKEN_AVAILABLE));

    if (number_of_sampled_trace_events(StageExecutionState::EXECUTE_BEGIN) != 0)
        answer.push_back(build_delta_statistic(*this, StageExecutionState::EXECUTE_BEGIN, *this, StageExecutionState::OUTPUT_TOKEN_AVAILABLE));

    if (number_of_sampled_trace_events(StageExecutionState::GL_TASK_BEGIN) != 0)
        answer.push_back(build_delta_statistic(*this, StageExecutionState::GL_TASK_BEGIN, *this, StageExecutionState::GL_TASK_END));

    return answer;

}

std::ostream& fb::operator<< (std::ostream& out, const StageSampler::Statistic& v) {

    out << "Statistic '" << v.name << "': [";
    out << "average: " << bc::duration_cast<bc::microseconds>(v.average) << ", ";
    out << "median: " << bc::duration_cast<bc::microseconds>(v.median) << ", ";
    out << "minimum: " << bc::duration_cast<bc::microseconds>(v.minimum) << ", ";
    out << "maximum: " << bc::duration_cast<bc::microseconds>(v.maximum) << ", ";
    out << "stddev: " << bc::duration_cast<bc::microseconds>(v.std_deviation) << ", ";
    out << "num_samples: " << v.num_samples << "].";

    return out;
}