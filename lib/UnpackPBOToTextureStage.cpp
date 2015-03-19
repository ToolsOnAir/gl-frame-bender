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
#include "UnpackPBOToTextureStage.h"
#include "ProgramOptions.h"

namespace fb = toa::frame_bender;

fb::UnpackPBOToTextureStage::~UnpackPBOToTextureStage()
{

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

    glDeleteTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());
}

void fb::UnpackPBOToTextureStage::initialize_resources(size_t num_of_buffers) {

    texture_ids_ = std::vector<GLuint>(
        num_of_buffers,
        0);

    glGenTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    std::for_each(
        std::begin(texture_ids_),
        std::end(texture_ids_),
        [this](const GLuint& texture_id) {
        glBindTexture(GL_TEXTURE_2D, texture_id);

        // TODO: are these necessary?
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            image_format_.gl_native_format().tex_internal_format,
            image_format_.gl_native_format().tex_width,
            image_format_.gl_native_format().tex_height);

        glBindTexture(GL_TEXTURE_2D, 0);

    });

}

fb::StageCommand fb::UnpackPBOToTextureStage::perform(
    TokenGL& pbo_in_token, 
    TokenGL& tex_out_token) 
{

    StageCommand answer = StageCommand::NO_CHANGE;

    if (pbo_in_token.format != tex_out_token.format) {
        FB_LOG_CRITICAL << "Image format mismatch.";
        return StageCommand::STOP_EXECUTION;
    }

    // TODO: in future, we could also sync with the input token for PBOs
    // at the moment we accept the implicit synchronization enforcement

    // Sync, as the output token crosses the context/threading boundaries
    if (enable_downstream_gl_synchronization_) {
        if (glIsSync(tex_out_token.gl_fence)) {
            glWaitSync(tex_out_token.gl_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(tex_out_token.gl_fence);
            tex_out_token.gl_fence = 0;
        }
    }

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    glBindTexture(GL_TEXTURE_2D, tex_out_token.id);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_in_token.id);

#ifdef DEBUG_PIPE_FLOW
    FB_LOG_DEBUG 
        << "Commanding upload from PBO '" << pbo_in_token.id 
        << "' to texture '" << tex_out_token.id << "'.";
#endif

    glPixelStorei(
        GL_UNPACK_ALIGNMENT, 
        pbo_in_token.format.gl_native_format().byte_alignment);

    glTexSubImage2D(
        GL_TEXTURE_2D, 
        0, 
        0, 
        0, 
        tex_out_token.format.gl_native_format().tex_width,
        tex_out_token.format.gl_native_format().tex_height,
        tex_out_token.format.gl_native_format().data_format,
        tex_out_token.format.gl_native_format().data_type,
        0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (gl_sampler_)
        gl_sampler_->sample_end();

    // Create a fence for downstream (crossing the context boundaries...)
    if (enable_downstream_gl_synchronization_) {
        tex_out_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    if (enable_upstream_gl_synchronization_) {
        // Also send to upstream, e.g. pinned memory needs that, or
        // when using unsynchronized access buffer bit.
        pbo_in_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    // TODO: in future, we should also create a fence with PBOs
    // for the previous stage, if we map unsynchronzied

    // Make sure that formats match (that's what we expect to be configured from the outside)
    FB_ASSERT_MESSAGE(
        tex_out_token.format == pbo_in_token.format,
        "Unexpected format mismatch in Stage");

    // Pass through the renderer for this frame
    tex_out_token.composition = pbo_in_token.composition;

    // Pass through time stamp
    tex_out_token.time_stamp = pbo_in_token.time_stamp;

    return answer;

}
