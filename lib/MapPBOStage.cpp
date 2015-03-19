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

#include "MapPBOStage.h"
#include "ProgramOptions.h"

namespace fb = toa::frame_bender;

fb::StageCommand fb::MapPBOStage::perform(
    TokenGL& token_in, 
    TokenGL& token_out)
{

    // Save output upstream token
    TokenGL upstream_token = token_out;    

#ifdef DEBUG_PIPE_FLOW

    FB_LOG_DEBUG << stage_.name() << ": Will unmap PBO '" << upstream_token.id << "' for upstream, mapping PBO '" << token_in.id << "' for downstream.";

#endif

    // For downstream, map PBO to data
    size_t buffer_size = token_out.format.image_byte_size();

    if (token_in.format != token_out.format) {
        FB_LOG_CRITICAL << "Image format mismatch.";
        return StageCommand::STOP_EXECUTION;
    }

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    if (!pbo_memory_is_persistently_mapped_) {

        uint8_t* buffer_data = gl::utils::map_pbo(
            token_in.id, 
            buffer_size, 
            pbo_target, 
            pbo_access);

        if (buffer_data == nullptr) {
            return StageCommand::STOP_EXECUTION;
        }

        token_out.buffer = buffer_data;

    } else {

        // Need to sync manually (what we provide upstream)

        if (glIsSync(token_in.gl_fence)) {

            static const std::chrono::nanoseconds timeout(std::chrono::seconds(9999));

            GLenum wait_state = glClientWaitSync(
                token_in.gl_fence, 
                GL_SYNC_FLUSH_COMMANDS_BIT, 
                static_cast<GLuint64>(timeout.count()));

            if (wait_state == GL_ALREADY_SIGNALED)
                gl_client_sync_already_signalled_count_++;
            else if (wait_state == GL_TIMEOUT_EXPIRED)
                gl_client_sync_timeout_count_++;
            else if (wait_state == GL_CONDITION_SATISFIED)
                gl_client_sync_condition_satifsied_count_++;

            glDeleteSync(token_in.gl_fence);

        } else {

            // The sync object is not valid, that means no syncing needs
            // to be done.
            gl_client_sync_already_deleted_++;

        }

        token_out.buffer = token_in.buffer;
    }

    token_out.composition = token_in.composition;
    token_out.time_stamp = token_in.time_stamp;
    token_out.id = token_in.id;

    if (!pbo_memory_is_persistently_mapped_) {

        // Unmap upstream token from output, and send upstream to input
        bool success = gl::utils::unmap_pbo(upstream_token.id, pbo_target);
        if (!success) {
            return StageCommand::STOP_EXECUTION;
        }

    }

    if (gl_sampler_)
        gl_sampler_->sample_end();

    token_in.composition = nullptr;
    token_in.time_stamp = Time(0, 1);
    token_in.id = upstream_token.id;
    token_in.gl_fence = 0;
    token_in.fbo_id = 0;
    token_in.buffer = upstream_token.buffer;

    return StageCommand::NO_CHANGE;
}

fb::MapPBOStage::~MapPBOStage() {

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
