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
#include "RenderStage.h"
#include "ProgramOptions.h"
#include "UtilsGL.h"
#include "StreamRenderer.h"
#include "StreamComposition.h"
#include "StageDataTypes.h"
#include "Window.h"

namespace fb = toa::frame_bender;

fb::RenderStage::~RenderStage() {

    if (gl_sampler_)
        gl_sampler_->flush();

    if (ProgramOptions::global().sample_stages()) {
        auto stats = sampler_.collect_statistics();
        for (const auto& stat : stats) {
            FB_LOG_INFO << stage_.name() << ": " << stat;
        }
    }

    glDeleteTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    glDeleteFramebuffers(
        static_cast<GLsizei>(fbo_ids_.size()), 
        fbo_ids_.data());

    if (blit_program_id_ != 0) {
        glDeleteProgram(blit_program_id_);
        blit_program_id_ = 0;
    }

    if (blit_pipeline_id_ != 0) {
        glDeleteProgramPipelines(1, &blit_pipeline_id_);
        blit_pipeline_id_ = 0;
    }

}

void fb::RenderStage::initialize_resources(size_t num_of_buffers) {

    // Initialize Textures and FBOs

    texture_ids_ = std::vector<GLuint>(
        num_of_buffers,
        0);

    fbo_ids_ = std::vector<GLuint>(
        num_of_buffers,
        0);

    glGenTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    glGenFramebuffers(
        static_cast<GLsizei>(fbo_ids_.size()), 
        fbo_ids_.data());

    auto initialize_textures_and_fbos = [this](
            const GLuint& texture_id,
            const GLuint& fbo_id) {
        
        glBindTexture(GL_TEXTURE_2D, texture_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            render_format_.gl_native_format().tex_internal_format,
            render_format_.gl_native_format().tex_width,
            render_format_.gl_native_format().tex_height);

        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id);
        glFramebufferTexture(
            GL_DRAW_FRAMEBUFFER, 
            GL_COLOR_ATTACHMENT0, 
            texture_id,
            0);

        bool complete = gl::utils::is_frame_buffer_complete(GL_DRAW_FRAMEBUFFER);
        if (!complete) {
            FB_LOG_ERROR << "Framebuffer '" << fbo_id << "' is incomplete.";
            throw std::runtime_error("Framebuffer is not complete.");
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        
    };

    FB_ASSERT(fbo_ids_.size() == texture_ids_.size());

    auto itr_tex = std::begin(texture_ids_);
    auto itr_fbo = std::begin(fbo_ids_);

    for (;itr_tex != std::end(texture_ids_); ++itr_tex, ++itr_fbo)
    {
        initialize_textures_and_fbos(*itr_tex, *itr_fbo);
    }

    if (ProgramOptions::global().blit_to_player()) {
        // Stuff for emulating "blitting"
        blit_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
            "passthrough.vert", 
            "passthrough.frag");

        glGenProgramPipelines(1, &blit_pipeline_id_);

        glUseProgramStages(
            blit_pipeline_id_, 
            GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
            blit_program_id_);
    }
    
}

fb::StageCommand fb::RenderStage::perform(
    TokenGL& src_tex_in_token, 
    TokenGL& dst_tex_out_token) 
{

    StageCommand answer = StageCommand::NO_CHANGE;

    // TODO: also pass the time parameter correctly, using [0, 1] currently
    // IDEA: The renderer and its input dependencies should be passed along with 
    // the token the reach true pipelining

    std::vector<StreamRenderer::Texture> input_textures(
        1, 
        StreamRenderer::Texture(
        src_tex_in_token.id,
        src_tex_in_token.format.gl_native_format().tex_width,
        src_tex_in_token.format.gl_native_format().tex_height));

    StreamRenderer::Fbo output_target(
        dst_tex_out_token.fbo_id,
        dst_tex_out_token.format.gl_native_format().tex_width,
        dst_tex_out_token.format.gl_native_format().tex_height);

#ifdef DEBUG_PIPE_FLOW
    FB_LOG_DEBUG 
        << "Rendering with src-texture '" 
        << src_tex_in_token.id << "' and destination texture '" 
        << dst_tex_out_token.id << "'.";
#endif

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    src_tex_in_token.composition->renderer()->render(
        src_tex_in_token.time_stamp, 
        input_textures,
        output_target);

    if (gl_sampler_)
        gl_sampler_->sample_end();

    // Pass through active composition for this token
    dst_tex_out_token.composition = src_tex_in_token.composition;

    // pass through timestamp
    dst_tex_out_token.time_stamp = src_tex_in_token.time_stamp;

    // The format should be passthrough, conversion is either done before or 
    // after.
    dst_tex_out_token.format = src_tex_in_token.format;

    if (ProgramOptions::global().blit_to_player()) {

#if USE_NATIVE_GL_BLIT

        glBindFramebuffer(GL_READ_FRAMEBUFFER, dst_tex_out_token.fbo_id);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);            
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        //glBlitFramebuffer(
        //    0, 
        //    0, 
        //    render_format_.gl_native_format().tex_width, 
        //    render_format_.gl_native_format().tex_height, 
        //    0,
        //    static_cast<GLint>(ProgramOptions::global().player_height()), 
        //    static_cast<GLint>(ProgramOptions::global().player_width()), 
        //    0,
        //    GL_COLOR_BUFFER_BIT, 
        //    GL_NEAREST);

        glEnable(GL_FRAMEBUFFER_SRGB);

        glBlitFramebuffer(
            0, 
            0, 
            render_format_.gl_native_format().tex_width, 
            render_format_.gl_native_format().tex_height, 
            0,
            0,
            static_cast<GLint>(ProgramOptions::global().player_width()), 
            static_cast<GLint>(ProgramOptions::global().player_height()), 
            GL_COLOR_BUFFER_BIT, 
            GL_NEAREST);

        glDisable(GL_FRAMEBUFFER_SRGB);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#else

        // TODO: is worth the effort to generate MIP_MAPS in here? (for smaller preview?)

        glBindFramebuffer(GL_READ_FRAMEBUFFER, dst_tex_out_token.fbo_id);
        GLint tex = 0;
        // Get the attached texture from the FBO
        glGetFramebufferAttachmentParameteriv(
            GL_READ_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
            &tex);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);            

        FB_ASSERT(tex != 0);

        glViewportIndexedf(
            0, 
            0, 
            0, 
            static_cast<GLfloat>(ProgramOptions::global().player_width()), 
            static_cast<GLfloat>(ProgramOptions::global().player_height()));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        glBindProgramPipeline(blit_pipeline_id_);

        // Draw the quad
        quad_.draw();

        glBindProgramPipeline(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_FRAMEBUFFER_SRGB);
#endif

        gl::Window::get()->swap_buffers();

    }

    return answer;

}