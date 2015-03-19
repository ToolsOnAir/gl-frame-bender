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
#include "TimeSampler.h"
#include "Init.h"
#include "Logging.h"
#include <stdexcept>
#include <type_traits>

namespace fb = toa::frame_bender;

fb::gl::TimeSampler::TimeSampler(
    StageSampler* sampler,
    StageExecutionState begin_state,
    StageExecutionState end_state,
    size_t buffer_size) : 
        stage_sampler_(sampler),
        begin_state_(begin_state),
        end_state_(end_state),
        query_ids_(buffer_size*2, 0),
        active_queries_(buffer_size*2),
        inactive_queries_(buffer_size*2)
{

    gl::Init::require();

    glGenQueries(
        static_cast<GLsizei>(query_ids_.size()), 
        &query_ids_[0]);

    for (auto el : query_ids_) {
        bool b = inactive_queries_.push(el);
        FB_ASSERT(b);
    }

    sync_point_.initialize();

}

void fb::gl::TimeSampler::resolve_query(const ActiveQuery& query) {

    // TODO: should we really do that? Does that really flush everything?
    // shouldn't do that... 
//#ifdef _DEBUG
    GLint is_available = 0;
    glGetQueryObjectiv(query.id, GL_QUERY_RESULT_AVAILABLE, &is_available);

    if (is_available == GL_FALSE) {
        FB_LOG_WARNING << "Had to query timestamp too early, revisit your configuration for num_buffered_query_objects.";
    }
//#endif

    GLuint64 time = 0;
    glGetQueryObjectui64v(query.id, GL_QUERY_RESULT, &time);

    auto gl_host_time = sync_point().convert_gl_time_to_host_time(time);

    stage_sampler_->enter_sample(query.designated_state, gl_host_time);

}

void fb::gl::TimeSampler::sample(StageExecutionState designated_state) {

    if (!sync_point().is_initialized()) {
        FB_LOG_ERROR << "Can't sample for GL because sync point is not initialized yet.";
        throw std::runtime_error("GL sync point is not initialized.");
    }

    GLuint new_query_id = 0;

    bool id_available = inactive_queries_.pop(new_query_id);

    if (!id_available) {

        // pop from active, retrieve timestamp and enter into sampler
        ActiveQuery old_query;
        bool b = active_queries_.pop(old_query);

        if (!b) {

            FB_LOG_CRITICAL << "Could not get query unexpectedly.";
            throw std::runtime_error("Could not get query.");

        }

        resolve_query(old_query);

        // old_query is now free-to-use
        new_query_id = old_query.id;
    }

    FB_ASSERT(new_query_id != 0);

    glQueryCounter(new_query_id, GL_TIMESTAMP);
    bool s = active_queries_.push(ActiveQuery(new_query_id, designated_state));
    if (!s) {
        FB_LOG_CRITICAL << "Could not push query unexpectedly.";
        throw std::runtime_error("Could not enter query.");
    }

}

void fb::gl::TimeSampler::sample_begin() {

    sample(begin_state_);

}

void fb::gl::TimeSampler::sample_end() {

    sample(end_state_);

}

void fb::gl::TimeSampler::flush() {

    ActiveQuery outstanding_query;
    while (active_queries_.pop(outstanding_query)) {

        resolve_query(outstanding_query);

    }

}

fb::gl::TimeSampler::~TimeSampler() {

    glDeleteQueries(
        static_cast<GLsizei>(query_ids_.size()), 
        &query_ids_[0]);

}

void fb::gl::SyncPoint::initialize() {

    GLint64 gl_time = 0;
    glGetInteger64v(GL_TIMESTAMP, &gl_time);
    auto host_time = clock::now();

    // Check if our clock really is in nanoseconds units
    static_assert(
        std::is_same<clock::duration, boost::chrono::nanoseconds>::value,
        "Clock is assumed to be in nanoseconds resolution.");

    // Note that the GL time is also in nanoseconds, therefore the explicit
    // check above
    clock::duration gl_time_clk(static_cast<clock::duration::rep>(gl_time));

    gl_to_host_diff_ = host_time.time_since_epoch() - gl_time_clk;

    FB_LOG_DEBUG << "Sync point determined diff time between host and GL: " << gl_to_host_diff_;

    initialized_ = true;
}

fb::clock::time_point fb::gl::SyncPoint::convert_gl_time_to_host_time(GLuint64 gl_time) const {

    static_assert(
        std::is_same<clock::duration, boost::chrono::nanoseconds>::value,
        "Clock is assumed to be in nanoseconds resolution.");

    clock::duration gl_time_clk(static_cast<clock::duration::rep>(gl_time));

    return clock::time_point(gl_time_clk + gl_to_host_diff_);

}
