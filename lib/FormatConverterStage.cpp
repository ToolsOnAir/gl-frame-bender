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
#include "FormatConverterStage.h"
#include "ProgramOptions.h"
#include "UtilsGL.h"
#include "Quad.h"

#include <regex>

namespace fb = toa::frame_bender;

fb::FormatConverterStage::~FormatConverterStage() {
    
    if (gl_sampler_) {
        gl_sampler_->flush();
    }

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

    glDeleteProgram(program_id_);
    program_id_ = 0;

    glDeleteProgramPipelines(1, &pipeline_id_);
    pipeline_id_ = 0;

    glDeleteTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    if (!fbo_ids_.empty()) {
        glDeleteFramebuffers(
            static_cast<GLsizei>(fbo_ids_.size()), 
            fbo_ids_.data());
    }

}

fb::FormatConverterStage::ConversionShaderMap 
    fb::FormatConverterStage::available_conversion_shaders() 
{

    // TODO-C++11: replace with C++11 static map initialization and make return const static

    ConversionShaderMap available_shaders;

    std::vector<ImageFormat::PixelFormat> rgba_like_formats;
    rgba_like_formats.push_back(ImageFormat::PixelFormat::RGBA_FLOAT_32BIT);
    rgba_like_formats.push_back(ImageFormat::PixelFormat::RGBA_FLOAT_16BIT);
    rgba_like_formats.push_back(ImageFormat::PixelFormat::RGBA_8BIT);
    rgba_like_formats.push_back(ImageFormat::PixelFormat::RGBA_16BIT);

    std::map<Mode, std::string> mode_to_file_name_map;
    mode_to_file_name_map[Mode::glsl_330] = "glsl_330.frag";
    mode_to_file_name_map[Mode::glsl_420] = "glsl_420.frag";
    mode_to_file_name_map[Mode::glsl_420_no_buffer_attachment_ext] = "glsl_420.frag";
    mode_to_file_name_map[Mode::glsl_430_compute] = "glsl_430.glsl";
    mode_to_file_name_map[Mode::glsl_430_compute_no_shared] = "glsl_430_no_shared.glsl";

    // Generate the list of supported V210 <-> RGBA conversions

    for (const auto& rgba_format : rgba_like_formats) {
        for (int32_t i = 0; i<static_cast<int32_t>(Mode::count); ++i) {

            Mode mode = static_cast<Mode>(i);

            // Encoder

            auto key = std::make_tuple(
                rgba_format,
                ImageFormat::PixelFormat::YUV_10BIT_V210,
                mode);

            auto result = available_shaders.insert(std::make_pair(key, "v210_encode_" + mode_to_file_name_map.at(mode)));
            FB_ASSERT(result.second);

            // Decoder

            key = std::make_tuple(
                ImageFormat::PixelFormat::YUV_10BIT_V210,
                rgba_format,
                mode);

            result = available_shaders.insert(std::make_pair(key, "v210_decode_" + mode_to_file_name_map.at(mode)));
            FB_ASSERT(result.second);

        }
    }

    return available_shaders;

}

bool fb::FormatConverterStage::is_passthrough(
    const ImageFormat& input_format, 
    const ImageFormat& output_format)
{

    bool is_passthrough = false;

    // TODO: add more "compatible" passthrough format and better define what
    // that means. The best way to actually implement this would be on the
    // dispatch level, i.e. this stage should be skipped AT ALL. But that
    // would require the stages (or at least the render stage) to be more
    // flexible in terms of requireing GL sync or not (as the format converter
    // stages would be missing as the sync guards between threads).

    std::set<ImageFormat::PixelFormat> passthrough_formats;

    passthrough_formats.insert(ImageFormat::PixelFormat::RGB_8BIT);
    passthrough_formats.insert(ImageFormat::PixelFormat::RGBA_8BIT);
    passthrough_formats.insert(ImageFormat::PixelFormat::RGBA_FLOAT_32BIT);

    bool pixel_format_is_compatible = 
        passthrough_formats.count(input_format.pixel_format()) != 0 &&
        passthrough_formats.count(output_format.pixel_format()) != 0;

    if (pixel_format_is_compatible &&
        input_format.width() == output_format.width() && 
        input_format.height() == output_format.height() && 
        input_format.transfer() == output_format.transfer() &&
        input_format.chromaticity() == output_format.chromaticity() &&
        input_format.origin() == output_format.origin())
    {
        is_passthrough = true;
    }

    return is_passthrough;
}

void fb::FormatConverterStage::initialize_resources(size_t num_of_buffers, const std::string& name) {

    // Validate formats

    // Chromaticity and resolution have to be the same at the moment
    if (input_format_.width() != output_format_.width() || 
        input_format_.height() != output_format_.height())
    {
        FB_LOG_ERROR << "Mismatch in resolutions, can't create format conversion executor.";
        throw std::invalid_argument("FormatConversion does not support different resolutions.");

    }

    if (input_format_.chromaticity() != output_format_.chromaticity()) {
        FB_LOG_ERROR << "Mismatch of Chromaticity parameters, can't convert these. Canceling converter construction.";
        throw std::invalid_argument("Mismatch in chromaticity parameters");
    }

    // Define the size of the viewport. This is somewhat dependend on the
    // actual shader and the image format, for now the choice of viewport
    // size is done here, but this should really go somewhere else
    // -> TODO-DEF: how to generalize this?

    // This is the base case
    viewport_width_ = output_format_.gl_native_format().tex_width;
    viewport_height_ = output_format_.gl_native_format().tex_height;

    if (is_passthrough_) {

        quad_ = utils::make_unique<gl::Quad>(gl::Quad::UV_ORIGIN::LOWER_LEFT);

        program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
            "passthrough.vert", 
            "passthrough.frag");

        glGenProgramPipelines(1, &pipeline_id_);

        glUseProgramStages(
            pipeline_id_, 
            GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
            program_id_);

        FB_LOG_INFO << "FormatConverter '" << name << "' will execute in pass-through-mode only.";

    } else {

        // TODO-DEF: see above for making this more generic
        if (mode_ == Mode::glsl_420 || mode_ == Mode::glsl_420_no_buffer_attachment_ext) {

            viewport_width_ = input_format_.gl_native_format().tex_width / format_group_divisor_;
            viewport_height_ = input_format_.gl_native_format().tex_height;

        }

        // If image origin of source and target format are not the same, then
        // we'll need to flip the image (by rendering a Quad with UV origin at
        // upper left.
        // Note that any format converter implementation can choose to either
        // react to the GLSL preprocessor macro "FB_GLSL_FLIP_ORIGIN" OR by simply
        // using the Quad's UV input coordinates (which will be flipped automatically)
        bool flip_image = input_format_.origin() != output_format_.origin();

        std::vector<std::string> vertex_shader_preprocessor_macros;
        std::vector<std::string> fragment_shader_preprocessor_macros;
        std::vector<std::string> compute_shader_preprocessor_macros;

        if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {
            quad_ = utils::make_unique<gl::Quad>(flip_image ? gl::Quad::UV_ORIGIN::UPPER_LEFT : gl::Quad::UV_ORIGIN::LOWER_LEFT);
        }

        // TODO: this whole GLSL compilation stuff should be cleaned up... 
        // from the compiler's view there really is not much of a difference
        // between preprocessor macro strings and actual shader sources.

        switch(mode_) {
        case Mode::glsl_330: 
            // TODO: make it actually compile under 330... 
            fragment_shader_preprocessor_macros.push_back("#version 420 core \n");
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_MODE_330 1 \n");
            break;
        case Mode::glsl_420:
            fragment_shader_preprocessor_macros.push_back("#version 420 core \n");
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_MODE_420 1 \n");
            break;
        case Mode::glsl_420_no_buffer_attachment_ext:
            fragment_shader_preprocessor_macros.push_back("#version 420 core \n");
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_MODE_420 1 \n");
            break;
        case Mode::glsl_430_compute:
            compute_shader_preprocessor_macros.push_back("#version 430 core \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_MODE_430 1 \n");
            break;
        case Mode::glsl_430_compute_no_shared:
            compute_shader_preprocessor_macros.push_back("#version 430 core \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_MODE_430 1 \n");
            break;
        case Mode::count:
        default:
            FB_LOG_CRITICAL << "Invalid mode value '" << mode_ << "'.";
            throw std::invalid_argument("Invalid Mode enum value");
            break;
        }

        fragment_shader_preprocessor_macros.push_back(
            "#define FB_INPUT_IMAGE_WIDTH " + 
            std::to_string(input_format_.gl_native_format().tex_width) + 
            " \n");

        compute_shader_preprocessor_macros.push_back(
            "#define FB_INPUT_IMAGE_WIDTH " + 
            std::to_string(input_format_.gl_native_format().tex_width) + 
            " \n");

        fragment_shader_preprocessor_macros.push_back(
            "#define FB_INPUT_IMAGE_HEIGHT " + 
            std::to_string(input_format_.gl_native_format().tex_height) + 
            " \n");

        compute_shader_preprocessor_macros.push_back(
            "#define FB_INPUT_IMAGE_HEIGHT " + 
            std::to_string(input_format_.gl_native_format().tex_height) + 
            " \n");

        if (flip_image) {
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_FLIP_ORIGIN 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_FLIP_ORIGIN 1 \n");
            FB_LOG_INFO << "Flipping images during conversion.";
        } 

        switch (chroma_filter_) {

        case ChromaFilter::none:
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_NONE 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_NONE 1 \n");
            break;

        case ChromaFilter::basic:
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_BASIC 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_BASIC 1 \n");            
            break;

        case ChromaFilter::high:
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_HIGH 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_CHROMA_FILTER_HIGH 1 \n");            
            break;

        case ChromaFilter::count:
        default:
            FB_LOG_CRITICAL << "Invalid chroma filter value '" << chroma_filter_ << "'.";
            throw std::invalid_argument("Invalid chroma filter value.");
            break;
        }

        // TODO: not too nice to have this as a global option...
        if (ProgramOptions::global().v210_use_shader_for_component_access()) {
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_V210_EXTRACT_BITFIELDS_IN_SHADER 1 \n");
        }

        if (ProgramOptions::global().enable_linear_space_rendering()) {
            fragment_shader_preprocessor_macros.push_back("#define FB_GLSL_LINEAR_RGB 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_GLSL_LINEAR_RGB 1 \n");
        }

        // Try to get the vendor, so vendor-specific bugs can be worked
        // around

        gl::utils::GLInfo info = gl::utils::get_info();

        std::regex amd_pattern(".*(ATI|AMD)+.*");
        std::regex nvidia_pattern(".*(NVIDIA)+.*");

        if (std::regex_match(info.vendor, amd_pattern)) {

            FB_LOG_INFO << "Adding AMD vendor shader compiler hint.";

            fragment_shader_preprocessor_macros.push_back("#define FB_VENDOR_AMD 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_VENDOR_AMD 1 \n");

        } else if (std::regex_match(info.vendor, nvidia_pattern)) {

            FB_LOG_INFO << "Adding NVIDIA vendor shader compiler hint.";

            fragment_shader_preprocessor_macros.push_back("#define FB_VENDOR_NVIDIA 1 \n");
            compute_shader_preprocessor_macros.push_back("#define FB_VENDOR_NVIDIA 1 \n");

        }

        // Get a program for this setup
        auto shaders = available_conversion_shaders();

        auto key = std::make_tuple(
            input_format_.pixel_format(),
            output_format_.pixel_format(),
            mode_);

        auto converter = shaders.find(key);

        if (converter == std::end(shaders)) {
            FB_LOG_ERROR << "'" << name << "': No format converters for requested configuration is available.";
            throw std::invalid_argument("Incompatible formats requested for converter.");
        }

        if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {

            // Add the common stuff for each version.
            fragment_shader_preprocessor_macros.push_back("#line 1 \n");
            auto common_sources = gl::utils::read_glsl_source_file("./common.glsl");
            fragment_shader_preprocessor_macros.push_back(common_sources);

            std::string vertex_shader_file = "passthrough.vert";

            program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
                vertex_shader_file, 
                converter->second,
                vertex_shader_preprocessor_macros,
                fragment_shader_preprocessor_macros);

            glGenProgramPipelines(1, &pipeline_id_);

            glUseProgramStages(
                pipeline_id_, 
                GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
                program_id_);

            FB_LOG_INFO 
                << "FormatConverter '" << name 
                << "' will use shaders '" << vertex_shader_file 
                << "/" << converter->second << "'.";

        } else {

            // Add the common stuff for each version.
            compute_shader_preprocessor_macros.push_back("#line 1 \n");
            auto common_sources = gl::utils::read_glsl_source_file("./common.glsl");
            compute_shader_preprocessor_macros.push_back(common_sources);

            std::ostringstream oss;
            oss << "#define TILE_V210_WIDTH " << compute_shader_work_group_size_ << " \n";
            compute_shader_preprocessor_macros.push_back(oss.str());

            if (deal_with_excess_threads_) {
                compute_shader_preprocessor_macros.push_back("#define FB_GLSL_DEAL_WITH_EXCESS_THREADS 1 \n");
            }

            program_id_ = gl::utils::create_glsl_program_with_cs(
                converter->second,
                compute_shader_preprocessor_macros);

            glGenProgramPipelines(1, &pipeline_id_);

            glUseProgramStages(
                pipeline_id_, 
                GL_COMPUTE_SHADER_BIT, 
                program_id_);

            FB_LOG_INFO 
                << "FormatConverter '" << name 
                << "' will use compute shader '" << converter->second << "'.";

        }

    }

    // Initialize Textures and FBOs

    texture_ids_ = std::vector<GLuint>(
        num_of_buffers,
        0);

    glGenTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    for (auto texture_id : texture_ids_) {

        glBindTexture(GL_TEXTURE_2D, texture_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            output_format_.gl_native_format().tex_internal_format,
            output_format_.gl_native_format().tex_width,
            output_format_.gl_native_format().tex_height);

        glBindTexture(GL_TEXTURE_2D, 0);

    }

    if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {

        fbo_ids_ = std::vector<GLuint>(
            num_of_buffers,
            0);

        glGenFramebuffers(
            static_cast<GLsizei>(fbo_ids_.size()), 
            fbo_ids_.data());

    }

    if (mode_ == Mode::glsl_420) {

        dummy_fbo_attachments_ = std::vector<GLuint>(
            num_of_buffers,
            0);

        glGenTextures(
            static_cast<GLsizei>(dummy_fbo_attachments_.size()), 
            dummy_fbo_attachments_.data());

        for (auto id : dummy_fbo_attachments_) {

            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

            glTexStorage2D(
                GL_TEXTURE_2D, 
                1, 
                GL_RGBA8,
                static_cast<GLsizei>(viewport_width_),
                static_cast<GLsizei>(viewport_height_));

            glBindTexture(GL_TEXTURE_2D, 0);

        }

    }

    if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {

        FB_ASSERT(fbo_ids_.size() == texture_ids_.size());

        auto itr_fbo = std::begin(fbo_ids_);
        auto itr_fbo_texture_attachment = std::begin(texture_ids_);

        if (mode_ == Mode::glsl_420) {
            // Attach dummy textures instead of real ones (as they'll be used
            // by the imageLoad commands
            itr_fbo_texture_attachment = std::begin(dummy_fbo_attachments_);
            FB_ASSERT(fbo_ids_.size() == dummy_fbo_attachments_.size());
        }


        for (;itr_fbo != std::end(fbo_ids_); ++itr_fbo, ++itr_fbo_texture_attachment)
        {

            GLuint texture_id = *itr_fbo_texture_attachment;
            GLuint fbo_id = *itr_fbo;

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id);

            if (mode_ == Mode::glsl_420_no_buffer_attachment_ext) {

                // Basically what ARB_framebuffer_no_attachment gives us (OGL 4.3 core)
                glFramebufferParameteri(
                    GL_DRAW_FRAMEBUFFER, 
                    GL_FRAMEBUFFER_DEFAULT_WIDTH, 
                    static_cast<GLint>(viewport_width_));

                glFramebufferParameteri(
                    GL_DRAW_FRAMEBUFFER, 
                    GL_FRAMEBUFFER_DEFAULT_HEIGHT, 
                    static_cast<GLint>(viewport_height_));

                glFramebufferParameteri(
                    GL_DRAW_FRAMEBUFFER, 
                    GL_FRAMEBUFFER_DEFAULT_LAYERS, 
                    0);

                glFramebufferParameteri(
                    GL_DRAW_FRAMEBUFFER, 
                    GL_FRAMEBUFFER_DEFAULT_SAMPLES, 
                    0);

            } else {

                glFramebufferTexture(
                    GL_DRAW_FRAMEBUFFER, 
                    GL_COLOR_ATTACHMENT0, 
                    texture_id,
                    0);

            }

            bool complete = gl::utils::is_frame_buffer_complete(GL_DRAW_FRAMEBUFFER);
            if (!complete) {
                FB_LOG_ERROR << "Framebuffer '" << fbo_id << "' is incomplete.";
                throw std::runtime_error("Framebuffer is not complete.");
            }

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        }

    }

}

fb::StageCommand fb::FormatConverterStage::perform(
    TokenGL& tex_in_token, 
    TokenGL& tex_out_token)
{

    if (enable_gl_upstream_synchronization_) {
        if (glIsSync(tex_in_token.gl_fence)) {
            glWaitSync(tex_in_token.gl_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(tex_in_token.gl_fence);
            tex_in_token.gl_fence = 0;
        }
    }

    if (enable_gl_downstream_synchronization_) {
        if (glIsSync(tex_out_token.gl_fence)) {
            glWaitSync(tex_out_token.gl_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(tex_out_token.gl_fence);
            tex_out_token.gl_fence = 0;
        }
    }

    if (gl_sampler_)
        gl_sampler_->sample_begin();

    // Assert that the formats fit as required
    if (tex_in_token.format.chromaticity() != input_format_.chromaticity() ||
        tex_in_token.format.transfer() != input_format_.transfer() ||
        tex_in_token.format.pixel_format() != input_format_.pixel_format()) {
        FB_LOG_ERROR << "Unexpected input texture format in converter executor.";
        throw std::runtime_error("Unexpected input texture format.");
    }

    if (tex_out_token.format.chromaticity() != output_format_.chromaticity() ||
        tex_out_token.format.transfer() != output_format_.transfer() ||
        tex_out_token.format.pixel_format() != output_format_.pixel_format()) {
        FB_LOG_ERROR << "Unexpected output texture format in converter executor.";
        throw std::runtime_error("Unexpected output texture format.");
    }

#ifdef DEBUG_PIPE_FLOW
    FB_LOG_DEBUG 
        << stage_.name() << ": Executing format conversion from tex '" 
        << tex_in_token.id << "' to tex '" 
        << tex_out_token.id << "'.";
#endif

    // TODO: maybe we should create three separate blocks of implementations
    // THis interleaved if/else fashion is a bit hard to read.

    // Bind source frame
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_in_token.id);

    if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tex_out_token.fbo_id);
        glViewportIndexedf(
            0, 
            0, 
            0, 
            static_cast<GLfloat>(viewport_width_), 
            static_cast<GLfloat>(viewport_height_));

    }

    if (mode_ == Mode::glsl_420 || 
        mode_ == Mode::glsl_420_no_buffer_attachment_ext || 
        mode_ == Mode::glsl_430_compute || 
        mode_ == Mode::glsl_430_compute_no_shared) 
    {

        // Binding the target image

        // TODO: think about the binding ID
        // TODO: target image is always bound to ID 1!
        glBindImageTexture(
            1, 
            tex_out_token.id, 
            0, 
            GL_FALSE, 
            0, 
            GL_WRITE_ONLY, 
            tex_out_token.format.gl_native_format().tex_internal_format);

    }

    glBindProgramPipeline(pipeline_id_);

    if (mode_ == Mode::glsl_430_compute || mode_ == Mode::glsl_430_compute_no_shared) {

        
        glDispatchCompute(
            num_work_groups_x_, 
            num_work_groups_y_,
            1
            );

    } else {

        // Draw the quad
        quad_->draw();

    }

    if (mode_ == Mode::glsl_420 || 
        mode_ == Mode::glsl_420_no_buffer_attachment_ext || 
        mode_ == Mode::glsl_430_compute ||
        mode_ == Mode::glsl_430_compute_no_shared)
    {
        glMemoryBarrier(gl_barrier_bitfield_);
    }

    glBindProgramPipeline(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (mode_ != Mode::glsl_430_compute && mode_ != Mode::glsl_430_compute_no_shared) {    
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    if (gl_sampler_)
        gl_sampler_->sample_end();

    // TODO: should we allow resizing here as well?

    tex_out_token.format = output_format_;

    // pass through active composition
    tex_out_token.composition = tex_in_token.composition;

    // pass through timestamp
    tex_out_token.time_stamp = tex_in_token.time_stamp;

    if (enable_gl_upstream_synchronization_)
        tex_in_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (enable_gl_downstream_synchronization_)
        tex_out_token.gl_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    return StageCommand::NO_CHANGE;
}

std::ostream& fb::operator<< (std::ostream& out, const Mode& v) {

    switch (v) {
    case Mode::glsl_330:
        out << "glsl_330";
        break;
    case Mode::glsl_420:
        out << "glsl_420";
        break;
    case Mode::glsl_420_no_buffer_attachment_ext:
        out << "glsl_420_no_buffer_attachment_ext";
        break;
    case Mode::glsl_430_compute:
        out << "glsl_430_compute";
        break;
    case Mode::glsl_430_compute_no_shared:
        out << "glsl_430_compute_no_shared";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::operator>>(std::istream& in, Mode& v) {

    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::tolower);

    if (token == "glsl_330")
        v = fb::Mode::glsl_330;
    else if (token == "glsl_420")
        v = fb::Mode::glsl_420;    
    else if (token == "glsl_420_no_buffer_attachment_ext")
        v = fb::Mode::glsl_420_no_buffer_attachment_ext;    
    else if (token == "glsl_430_compute")
        v = fb::Mode::glsl_430_compute;    
    else if (token == "glsl_430_compute_no_shared")
        v = fb::Mode::glsl_430_compute_no_shared;    
    else {
        in.setstate(std::ios::failbit);
    }

    return in;

}

std::ostream& fb::operator<< (std::ostream& out, const ChromaFilter& v) {

    switch (v) {
    case ChromaFilter::none:
        out << "none";
        break;
    case ChromaFilter::basic:
        out << "basic";
        break;
    case ChromaFilter::high:
        out << "high";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::operator>>(std::istream& in, ChromaFilter& v) {

    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::tolower);

    if (token == "none")
        v = ChromaFilter::none;
    else if (token == "basic")
        v = ChromaFilter::basic;
    else if (token == "high")
        v = ChromaFilter::high;    
    else {
        in.setstate(std::ios::failbit);
    }

    return in;

}
