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

#include "StreamRenderer.h"
#include "Logging.h"
#include "UtilsGL.h"
#include "ProgramOptions.h"
#include "Window.h"
#include "MathConstants.h"

#include "glm/glm.hpp"

namespace fb = toa::frame_bender;

fb::StreamRenderer::StreamRenderer(std::string name) :
    name_(std::move(name))    
{
}

fb::StreamRenderer::~StreamRenderer() {
}

fb::PassThroughRenderer::PassThroughRenderer(bool is_integer_format) : 
    StreamRenderer("PassThroughRenderer"),
    program_id_(0),
    pipeline_id_(0),
    quad_(gl::Quad::UV_ORIGIN::LOWER_LEFT)
{

    // Can we assume that the context is set???


    program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
    "passthrough.vert", 
    is_integer_format ? "passthrough_integer.frag" : "passthrough.frag");

    glGenProgramPipelines(1, &pipeline_id_);

    glUseProgramStages(
        pipeline_id_, 
        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
        program_id_);

}

fb::PassThroughRenderer::~PassThroughRenderer() {
    
    glDeleteProgram(program_id_);
    program_id_ = 0;

    glDeleteProgramPipelines(1, &pipeline_id_);
    pipeline_id_ = 0;

}

void fb::PassThroughRenderer::render(
    const Time& frame_time,
    const std::vector<Texture>& source_frames,
    const Fbo& target_fbo) const 
{

    FB_ASSERT(source_frames.size() == 1);

    // Note that TEXTURE0 is already bound via layout qualifiers
    // in the shader.

    // TODO: is this really necessary?
    glDisable(GL_DEPTH_TEST);

    // Bind source frame
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_frames[0].id);

    glViewportIndexedf(
        0, 
        0, 
        0, 
        static_cast<GLfloat>(target_fbo.width), 
        static_cast<GLfloat>(target_fbo.height));

    // Bind target frame (via FBO)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fbo.id);

    // TODO: might have to 'clear' here... 
    // TODO: Do I have to set a viewport here?

    glBindProgramPipeline(pipeline_id_);

    // Draw the quad
    quad_.draw();

    glBindProgramPipeline(0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // TODO: un-attach texture from framebuffer?

    // TODO: this is somewhat hacky but very efficient. This renderer should
    // probably be called FBO renderer, and then we should have other
    // subclasses doing the actual rendering leg work.
    // This also implicitly assume that we are in the Windows' OGL context.
    // TODO: when rendering into float, you'll have to convert to INT before!
    //  OR use a float backing buffer, not sure if that's possible.

}
//
//fb::PassThroughV210CodecRenderer::PassThroughV210CodecRenderer(const ImageFormat& v210_format) : 
//    StreamRenderer("PassThroughV210CodecRenderer"),
//    v210_format_(v210_format),
//    decode_program_id_(0),
//    decode_pipeline_id_(0),
//    src_fbo_id_(0),
//    src_texture_rgbaf_id_(0),
//    dst_fbo_id_(0),
//    dst_texture_rgbaf_id_(0),
//    encode_program_id_(0),
//    encode_pipeline_id_(0),
//    output_fbo_id_(0),
//    passthrough_program_id_(0),
//    passthrough_pipeline_id_(0)
//{
//
//    // We assume we have a right context set.
//
//    // Creating the FrameBuffers
//    glGenFramebuffers(1, &output_fbo_id_);
//    glGenFramebuffers(1, &src_fbo_id_);
//    glGenFramebuffers(1, &dst_fbo_id_);
//
//    // Compile shaders and create pipeline objects via programs.
//
//    //decode_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
//    //    "passthrough.vert", 
//    //    "v210_decode.frag");
//
//    //glGenProgramPipelines(1, &decode_pipeline_id_);
//
//    glUseProgramStages(
//        decode_pipeline_id_, 
//        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
//        decode_program_id_);
//
//    encode_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
//        "passthrough.vert", 
//        "v210_encode.frag");
//
//    glGenProgramPipelines(1, &encode_pipeline_id_);
//
//    glUseProgramStages(
//        encode_pipeline_id_, 
//        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
//        encode_program_id_);
//
//    passthrough_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
//        "passthrough.vert", 
//        "passthrough.frag");
//
//    glGenProgramPipelines(1, &passthrough_pipeline_id_);
//
//    glUseProgramStages(
//        passthrough_pipeline_id_, 
//        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
//        passthrough_program_id_);
//
//
//    // Create the intermediate RGBA 32F textures
//    // TODO: not sure if it's good that we render into FBO and then directly
//    // bind it as a texture again.
//
//    // Source texture
//
//    glGenTextures(1, &src_texture_rgbaf_id_);
//    glBindTexture(GL_TEXTURE_2D, src_texture_rgbaf_id_);
//
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
//
//    glTexStorage2D(
//        GL_TEXTURE_2D, 
//        1, 
//        GL_RGBA32F, // TODO: check if RGBA16F would be sufficient ... 
//        v210_format_.width(),
//        v210_format_.height());
//
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    // Destination texture
//
//    glGenTextures(1, &dst_texture_rgbaf_id_);
//    glBindTexture(GL_TEXTURE_2D, dst_texture_rgbaf_id_);
//
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
//
//    glTexStorage2D(
//        GL_TEXTURE_2D, 
//        1, 
//        GL_RGBA32F, // TODO: check if RGBA16F would be sufficient ... 
//        v210_format_.width(),
//        v210_format_.height());
//
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    // Create an FBO and already bind it to its respective texture (no re-bind necessary)
//
//    // Source FBO
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, src_fbo_id_);
//    glFramebufferTexture(
//        GL_DRAW_FRAMEBUFFER, 
//        GL_COLOR_ATTACHMENT0, 
//        src_texture_rgbaf_id_,
//        0);
//
//    bool complete = gl::utils::is_frame_buffer_complete();
//    if (!complete) {
//        FB_LOG_ERROR << "Framebuffer '" << dst_fbo_id_ << "' is incomplete.";
//        throw std::runtime_error("Framebuffer is not complete.");
//    }
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//
//    // Destination FBO
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo_id_);
//    glFramebufferTexture(
//        GL_DRAW_FRAMEBUFFER, 
//        GL_COLOR_ATTACHMENT0, 
//        dst_texture_rgbaf_id_,
//        0);
//
//    complete = gl::utils::is_frame_buffer_complete();
//    if (!complete) {
//        FB_LOG_ERROR << "Framebuffer '" << dst_fbo_id_ << "' is incomplete.";
//        throw std::runtime_error("Framebuffer is not complete.");
//    }
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//
//}
//
//fb::PassThroughV210CodecRenderer::~PassThroughV210CodecRenderer() {
//    
//    glDeleteProgram(passthrough_program_id_);
//    passthrough_program_id_ = 0;
//
//    glDeleteProgramPipelines(1, &passthrough_pipeline_id_);
//    passthrough_pipeline_id_ = 0;
//
//    //glDeleteProgram(decode_program_id_);
//    //decode_program_id_ = 0;
//
//    //glDeleteProgramPipelines(1, &decode_pipeline_id_);
//    //decode_pipeline_id_ = 0;
//
//    glDeleteProgram(encode_program_id_);
//    encode_program_id_ = 0;
//
//    glDeleteProgramPipelines(1, &encode_pipeline_id_);
//    encode_pipeline_id_ = 0;
//
//    glDeleteTextures(1, &dst_texture_rgbaf_id_);
//    dst_texture_rgbaf_id_ = 0;
//
//    glDeleteTextures(1, &src_texture_rgbaf_id_);
//    src_texture_rgbaf_id_ = 0;
//
//    glDeleteFramebuffers(1, &output_fbo_id_);
//    output_fbo_id_= 0;
//
//    glDeleteFramebuffers(1, &dst_fbo_id_);
//    dst_fbo_id_= 0;
//
//    glDeleteFramebuffers(1, &src_fbo_id_);
//    src_fbo_id_= 0;
//
//}
//
//void fb::PassThroughV210CodecRenderer::render(
//    const Time& frame_time,
//    const std::vector<Texture>& source_frames,
//    const Texture& target_texture) const
//{
//
//    FB_ASSERT(source_frames.size() == 1);
//
//    // Note that TEXTURE0 is already bound via layout qualifiers
//    // in the shader.
//
//    // TODO: is this really necessary?
//    glDisable(GL_DEPTH_TEST);
//
//
//
//
//    ///////////
//    // RENDER FROM RGBA SRC INTO RGBA DST (passthrough, or in future 
//    // implementation-dependent)
//    //////////
//
//    glBindTexture(GL_TEXTURE_2D, src_texture_rgbaf_id_);
//
//    // viewport is still set from previous pass
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo_id_);
//
//    glBindProgramPipeline(passthrough_pipeline_id_);
//
//    // Draw the quad
//    quad_.draw();
//
//    glBindProgramPipeline(0);
//
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//
//
//
//    // TODO: fix this for the RGBAF case... not sure if this is possible at all...
//
//    // TODO: this is somewhat hacky but very efficient. This renderer should
//    // probably be called FBO renderer, and then we should have other
//    // subclasses doing the actual rendering leg work.
//    // This also implicitly assume that we are in the Windows' OGL context.
//    // TODO: when rendering into float, you'll have to convert to INT before!
//    //  OR use a float backing buffer, not sure if that's possible.
//    if (ProgramOptions::global().blit_to_player()) {
//
//        //glEnable(GL_FRAMEBUFFER_SRGB);
//        //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); //Bind the default framebuffer.
//        //GLint encoding;
//        //glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
//        //assert(encoding == GL_SRGB);
//        //glDisable(GL_FRAMEBUFFER_SRGB);
//
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, dst_fbo_id_);
//        glDrawBuffer(GL_BACK);            
//        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//        glReadBuffer(GL_COLOR_ATTACHMENT0);
//
//
//        glBlitFramebuffer(
//            0, 
//            0, 
//            static_cast<GLint>(v210_format_.width()), 
//            static_cast<GLint>(v210_format_.height()), 
//            0,
//            static_cast<GLint>(ProgramOptions::global().player_height()), 
//            static_cast<GLint>(ProgramOptions::global().player_width()), 
//            0,
//            GL_COLOR_BUFFER_BIT, 
//            GL_NEAREST);
//
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
//        gl::Window::get()->swap_buffers();
//
//    }
//
//}
