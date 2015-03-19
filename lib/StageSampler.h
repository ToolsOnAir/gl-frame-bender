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
#ifndef TOA_FRAME_BENDER_STAGE_SAMPLER_H
#define TOA_FRAME_BENDER_STAGE_SAMPLER_H

#include <array>
#include <vector>

#include "Utils.h"
#include "ChronoUtils.h"
#include <iosfwd>
#include "Stage.h"

namespace toa {
    namespace frame_bender {

        // TODO: think about using thread-clock?
        class TraceFormatWriter;

        class StageSampler : public utils::NoCopyingOrMoving {

        public:

            typedef clock::time_point TraceEvent;

            struct Statistic {
                std::string name;
                clock::duration average;
                clock::duration minimum;
                clock::duration maximum;
                clock::duration std_deviation;
                clock::duration median;
                size_t num_samples;
            };

            StageSampler(
                const std::map<StageExecutionState, std::string>& name_overrides = std::map<StageExecutionState, std::string>());

            void sample(StageExecutionState state);
            void enter_sample(StageExecutionState state, const clock::time_point& time);

            std::vector<Statistic> collect_statistics() const;

            static const size_t kNumMaxTraceEvents = 10000;

            // throws if out of bounds
            const TraceEvent& get_trace_event(StageExecutionState state, size_t idx) const;

            size_t number_of_sampled_trace_events(StageExecutionState state) const;

            void set_name_overrides(std::map<StageExecutionState, std::string> overrides);

            std::string get_name(StageExecutionState state) const;

            friend class TraceFormatWriter;

            static Statistic build_delta_statistic(
                const StageSampler& begin_sampler,
                StageExecutionState begin_state,
                const StageSampler& end_sampler,
                StageExecutionState end_state);

            size_t sample_overflow(StageExecutionState state) const { return sample_overflow_[static_cast<size_t>(state)]; }

        private:

            typedef std::array<TraceEvent, kNumMaxTraceEvents> EventTrace;

            typedef std::array<EventTrace, static_cast<size_t>(StageExecutionState::NUMBER_OF_STATES)> TraceTimes;

            typedef std::array<EventTrace::iterator, static_cast<size_t>(StageExecutionState::NUMBER_OF_STATES)> TraceIterators;

            TraceTimes trace_times_;
            TraceIterators trace_iterators_;
            TraceIterators trace_end_iterators_;
            std::array<size_t, static_cast<size_t>(StageExecutionState::NUMBER_OF_STATES)> sample_overflow_;

            std::map<StageExecutionState, std::string> name_overrides_;

        };

        // Prints a readable report
        std::ostream& operator<< (std::ostream& out, const StageSampler::Statistic& v);

    }
}

#endif // TOA_FRAME_BENDER_STAGE_SAMPLER_H