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

#include "UnmapPBOStage.h"
#include "ProgramOptions.h"

namespace fb = toa::frame_bender;

fb::StageCommand fb::UnmapPBOStage::perform(
    TokenGL& token_in, 
    TokenGL& token_out)
{

    if (token_in.format != token_out.format) {
        FB_LOG_CRITICAL  << "Image format mismatch.";
        return StageCommand::STOP_EXECUTION;
    }

    // Save output upstream token
    TokenGL upstream_token = token_out;    

#ifdef DEBUG_PIPE_FLOW

    FB_LOG_DEBUG << stage_.name() << ": Will unmap PBO '" << token_in.id << "' for downstream, mapping PBO '" << token_out.id << "' for upstream.";

#endif

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    if (!pbo_memory_is_persistently_mapped_) {

        // Unmap downstream token
        bool success = gl::utils::unmap_pbo(token_in.id, pbo_target);
        if (!success) {
            return StageCommand::STOP_EXECUTION;
        }

        token_in.buffer = nullptr;

    }

    token_out.id = token_in.id;
    token_out.buffer = token_in.buffer;
    token_out.composition = token_in.composition;
    token_out.fbo_id = 0;
    token_out.format = token_in.format;
    token_out.gl_fence = 0;
    token_out.time_stamp = token_in.time_stamp;

    if (!pbo_memory_is_persistently_mapped_) {

        // For upstream, map PBO to data
        size_t buffer_size = upstream_token.format.image_byte_size();

        uint8_t* buffer_data = gl::utils::map_pbo(
            upstream_token.id, 
            buffer_size, 
            pbo_target, 
            pbo_access);

        if (buffer_data == nullptr) {
            return StageCommand::STOP_EXECUTION;
        }

        token_in.buffer = buffer_data;

    } else {

        // Make sure that the writes from upstream to the persistently
        // buffer are visible to downstream (from the GL's point of view)
        glMemoryBarrier (GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

        // Then make sure that downstream GL is done with writing to the 
        // buffer that we'd like to give for further writing
        if (glIsSync(upstream_token.gl_fence)) {

            static const std::chrono::nanoseconds timeout(std::chrono::seconds(9999));

            GLenum wait_state = glClientWaitSync(
                upstream_token.gl_fence, 
                GL_SYNC_FLUSH_COMMANDS_BIT, 
                static_cast<GLuint64>(timeout.count()));

            if (wait_state == GL_ALREADY_SIGNALED)
                gl_client_sync_already_signalled_count_++;
            else if (wait_state == GL_TIMEOUT_EXPIRED)
                gl_client_sync_timeout_count_++;
            else if (wait_state == GL_CONDITION_SATISFIED)
                gl_client_sync_condition_satifsied_count_++;

            glDeleteSync(upstream_token.gl_fence);
            upstream_token.gl_fence = 0;

        } else {

            // The sync object is not valid, that means no syncing needs
            // to be done.
            gl_client_sync_already_deleted_++;

        }

        token_in.buffer = upstream_token.buffer;

    }

    if (gl_sampler_)
        gl_sampler_->sample_end();

    // Set upstream values
    token_in.composition = nullptr;
    token_in.format = upstream_token.format;
    token_in.id = upstream_token.id;
    token_in.time_stamp = Time(0, 1);

    return StageCommand::NO_CHANGE;

}

fb::UnmapPBOStage::~UnmapPBOStage() {

    if (gl_sampler_)
        gl_sampler_->flush();

    stage_.flush([](TokenGL& el){
        if (el.gl_fence && glIsSync(el.gl_fence)) {
          glDeleteSync(el.gl_fence);
          el.gl_fence = 0;
        }
    });

    if (ProgramOptions::global().sample_stages()) {
        auto stats = sampler_.collect_statistics();
        for (const auto& stat : stats) {
            FB_LOG_INFO << stage_.name() << ": " << stat;
        }
    }

    if (pbo_memory_is_persistently_mapped_) {
        FB_LOG_INFO << stage_.name() << ": " << "GL client sync already-signalled-count: " << gl_client_sync_already_signalled_count_ << ".";
        FB_LOG_INFO << stage_.name() << ": " << "GL client sync condition-satisfied-count: " << gl_client_sync_condition_satifsied_count_ << ".";
        FB_LOG_INFO << stage_.name() << ": " << "GL client sync already-deleted-count: " << gl_client_sync_already_deleted_ << ".";
        if (gl_client_sync_timeout_count_ != 0) {
            FB_LOG_ERROR << stage_.name() << ": " << "GL client sync timeout-count: " << gl_client_sync_timeout_count_ << ".";
        } else {
            FB_LOG_INFO << stage_.name() << ": No timeouts occurred in GL client sync.";
        }
    }

}
