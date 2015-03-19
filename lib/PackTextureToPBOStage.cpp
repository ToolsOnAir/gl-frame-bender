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

#include "PackTextureToPBOStage.h"
#include "ProgramOptions.h"
#include "UtilsGL.h"

namespace fb = toa::frame_bender;

fb::PackTextureToPBOStage::~PackTextureToPBOStage() {

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

}

fb::StageCommand fb::PackTextureToPBOStage::perform(
    TokenGL& dst_tex_in_token, 
    TokenGL& pbo_out_token) 
{

    StageCommand answer = StageCommand::NO_CHANGE;

    // Sync with the limited resource destination input texture.
    // "Tell GL that the GL should wait until the previous operation
    // of rendering into dst_tex_in_token is complete."
    if (enable_upstream_gl_synchronization_) {
        if (glIsSync(dst_tex_in_token.gl_fence)) {
            glWaitSync(dst_tex_in_token.gl_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(dst_tex_in_token.gl_fence);
            dst_tex_in_token.gl_fence = 0;
        }
    }

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    glPixelStorei(
        GL_PACK_ALIGNMENT, 
        dst_tex_in_token.format.gl_native_format().byte_alignment);

    glBindTexture(GL_TEXTURE_2D, dst_tex_in_token.id);

#ifdef DEBUG_PIPE_FLOW
    FB_LOG_DEBUG 
        << "Commanding download from texture '" 
        << dst_tex_in_token.id << "' to PBO '" 
        << pbo_out_token.id << "'.";
#endif

    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_out_token.id);

    glGetTexImage(
        GL_TEXTURE_2D, 
        0,
        pbo_out_token.format.gl_native_format().data_format,
        pbo_out_token.format.gl_native_format().data_type,
        0);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // If downstream is using persistently-mapped buffers, then we have
    // to insert a GL memory barrier in here
    if (pbo_memory_is_persistently_mapped_)
      glMemoryBarrier (GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

    if (gl_sampler_)
        gl_sampler_->sample_end();

    // Create a fence so the next render operation can tell the
    // GL to sync properly
    if (enable_downstream_gl_synchronization_)
        pbo_out_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (enable_upstream_gl_synchronization_)
        dst_tex_in_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    // TODO: in the future you should sync the PBO down stuff as 
    // well.

    // Pass through active composition for this frame
    pbo_out_token.composition = dst_tex_in_token.composition;

    // Pass through timestamp 
    pbo_out_token.time_stamp = dst_tex_in_token.time_stamp;

    //short_sleep(microseconds(800));

    return answer;

}
