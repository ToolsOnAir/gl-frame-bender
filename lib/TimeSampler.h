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
#ifndef TOA_FRAME_BENDER_GL_TIME_SAMPLER_H
#define TOA_FRAME_BENDER_GL_TIME_SAMPLER_H

#include "Utils.h"
#include <glad/glad.h>
#include "ChronoUtils.h"
#include "StageSampler.h"
#include "CircularFifo.h"

namespace toa {
    namespace frame_bender {

        namespace gl {

            class TimeSampler;
            class SyncPoint : public ::toa::frame_bender::utils::NoCopyingOrMoving {

            public:

                bool is_initialized() const { return initialized_; }
                void initialize();
                clock::time_point convert_gl_time_to_host_time(GLuint64 time) const;

                friend class TimeSampler;

            private:
                SyncPoint() : initialized_(false) {}
                bool initialized_;
                clock::duration gl_to_host_diff_;
            };

            // This is a helper class that uses timer queries and feeds 
            // the results into a stage sampler. Note that the results 
            // are fed in a deferred manner in order to avoid block the 
            // GL pipeline.
            class TimeSampler : public ::toa::frame_bender::utils::NoCopyingOrMoving {

            public:

                const SyncPoint& sync_point() const {
                    return sync_point_;
                }

                SyncPoint& sync_point() {
                    return sync_point_;
                }

                static const size_t kDefaultBufferSize = 5;

                // It is the callers responsibility to keep the stage sampler
                // valid over the lifetime of the TimeSampler
                TimeSampler(
                    StageSampler* sampler,
                    StageExecutionState begin_state,
                    StageExecutionState end_state,
                    size_t buffer_size = kDefaultBufferSize);

                ~TimeSampler();

                void sample_begin();
                void sample_end();

                void flush();

            private:

                struct ActiveQuery {
                    GLuint id;
                    StageExecutionState designated_state;

                    ActiveQuery() : id(0) {}

                    ActiveQuery(GLuint id, StageExecutionState designated_state):
                        id(id), designated_state(designated_state) {}

                };

                void resolve_query(const ActiveQuery& query);

                void sample(StageExecutionState designated_state);

                StageSampler* stage_sampler_;
                StageExecutionState begin_state_;
                StageExecutionState end_state_;
                std::vector<GLuint> query_ids_;

                CircularFifo<ActiveQuery> active_queries_;
                CircularFifo<GLuint> inactive_queries_;

                SyncPoint sync_point_;
            };

        }
    }
}

#endif // TOA_FRAME_BENDER_GL_TIME_SAMPLER_H
