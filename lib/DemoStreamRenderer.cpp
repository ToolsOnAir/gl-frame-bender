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
#include "DemoStreamRenderer.h"
#include "Logging.h"
#include "ProgramOptions.h"
#include "UtilsGL.h"

#include "roots.h"

#include <mutex>
#include <algorithm>
#include <cmath>

#include <IL/il.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ImageFormat.h"

namespace fb = toa::frame_bender;

template <typename T>
void animate_in_out(
    const fb::Time& t, 
    const T& start_value,
    const T& end_value,
    T& out);

std::once_flag devil_initialized_flag_;

void initialize_devil() {

    ilInit();
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);    

}

fb::ImageFormat::PixelFormat get_pixel_format(
    ILint il_image_format,
    ILint il_image_type,
    ILint il_bits_per_pixel, 
    ILint il_num_channels)
{

    // image_format == e.g. IL_RGBA
    // image_type == e.g. IL_UNSIGNED_SHORT
    // bytes_per_pixel == e.g. 8 (!?)

    auto div= std::div(il_bits_per_pixel, il_num_channels);
    FB_ASSERT(div.rem == 0);

    int bits_per_components = div.quot;

    fb::ImageFormat::PixelFormat answer;

    if (il_image_format == IL_RGBA &&
        il_image_type == IL_UNSIGNED_SHORT &&
        bits_per_components == 16) 
    {

        answer = fb::ImageFormat::PixelFormat::RGBA_16BIT;

    } else if (il_image_format == IL_RGBA &&
               il_image_type == IL_UNSIGNED_BYTE &&
               bits_per_components == 8) 
    {

        answer = fb::ImageFormat::PixelFormat::RGBA_8BIT;

    } else {

        FB_LOG_ERROR << "Unsupported format.";
        throw std::invalid_argument("Unsupported image format in demo stream rendere.");

    }

    return answer;
}

GLuint load_texture_from_file(
    const std::string& file,
    fb::ImageFormat& image_format_out) 
{

	FB_LOG_DEBUG << "Loading texture file '" << file << "'.";

    GLuint texture_id = 0;

    // We use DevIL to load the images from the disk
    ILuint il_image = 0;

    ilGenImages(1, &il_image);
    ilBindImage(il_image);

    ILboolean success = ilLoadImage(file.c_str());

    if (!success) {

        FB_LOG_ERROR << "Could not load image from file '" << file << "'.";
        throw std::runtime_error("Could not load image file.");

    }

    fb::ImageFormat::PixelFormat pf;

    ILint il_image_type = ilGetInteger(IL_IMAGE_TYPE);
    ILint il_image_format = ilGetInteger(IL_IMAGE_FORMAT);
    ILint width = ilGetInteger(IL_IMAGE_WIDTH);
    ILint height = ilGetInteger(IL_IMAGE_HEIGHT);

    ILint il_bits_per_component = ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    ILint il_num_of_channels = ilGetInteger(IL_IMAGE_CHANNELS);

    pf = get_pixel_format(
        il_image_format, 
        il_image_type, 
        il_bits_per_component,
        il_num_of_channels);

    fb::ImageFormat image_format(
        static_cast<uint32_t>(width), 
        static_cast<uint32_t>(height), 
        fb::ImageFormat::Transfer::SRGB, 
        fb::ImageFormat::Chromaticity::SRGB, 
        pf,
        // This shoudl be corrected already by DeVIL (see init above)
        fb::ImageFormat::Origin::LOWER_LEFT);

    std::unique_ptr<uint8_t[]> image_data = std::unique_ptr<uint8_t[]>(
        new uint8_t[image_format.image_byte_size()]);

    ilCopyPixels(
        0, 
        0, 
        0, 
        width, 
        height, 
        1, 
        // Use same type as source
        // TODO: is there any case where we would need something different?
        ilGetInteger(IL_IMAGE_FORMAT), 
        il_image_type, 
        image_data.get());

    ilBindImage(0);
    ilDeleteImages(1, &il_image);

    GLsizei num_levels = 8;

    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, num_levels);

    glTexStorage2D(
        GL_TEXTURE_2D, 
        num_levels, 
        image_format.gl_native_format().tex_internal_format,
        image_format.gl_native_format().tex_width,
        image_format.gl_native_format().tex_height);

    // Load data to base level
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        image_format.gl_native_format().tex_width,
        image_format.gl_native_format().tex_height,
        image_format.gl_native_format().data_format,
        image_format.gl_native_format().data_type,
        image_data.get());

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    image_format_out = image_format;

    return texture_id;
}

fb::DemoStreamRenderer::DemoStreamRenderer() : 
    StreamRenderer("DemoStreamRenderer"),
    quad_(gl::Quad::UV_ORIGIN::LOWER_LEFT),
    background_program_id_(0),
    background_pipeline_id_(0),
    overlay_program_id_(0),
    overlay_pipeline_id_(0),
    lower_third_texture_(0),
    logo_texture_(0),
    transform_uniform_buffer_animated_(0),
    transform_uniform_buffer_static_(0),
    blend_uniform_buffer_animated_(0),
    blend_uniform_buffer_static_(0)
{

    // Load images to textures
    std::call_once(devil_initialized_flag_, initialize_devil);

    lower_third_texture_ = load_texture_from_file(
        ProgramOptions::global().textures_folder() + "/" + ProgramOptions::global().demo_renderer_lower_third_image(),
        lower_third_texture_image_format_);

    logo_texture_ = load_texture_from_file(
        ProgramOptions::global().textures_folder() + "/" + ProgramOptions::global().demo_renderer_logo_image(),
        logo_texture_image_format_);

    overlay_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
        "overlay.vert", 
        "overlay.frag");

    glGenProgramPipelines(1, &overlay_pipeline_id_);

    glUseProgramStages(
        overlay_pipeline_id_, 
        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
        overlay_program_id_);


    background_program_id_ = gl::utils::create_glsl_program_with_fs_and_vs(
        "passthrough.vert", 
        "passthrough.frag");

    glGenProgramPipelines(1, &background_pipeline_id_);

    glUseProgramStages(
        background_pipeline_id_, 
        GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
        background_program_id_);

    GLint uniform_buffer_offset = 0;
    glGetIntegerv(
        GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        &uniform_buffer_offset);


    glGenBuffers(1, &transform_uniform_buffer_animated_);
    GLint uniform_block_size = glm::max(GLint(sizeof(glm::mat4)), uniform_buffer_offset);
    glBindBuffer(GL_UNIFORM_BUFFER, transform_uniform_buffer_animated_);
    glBufferData(GL_UNIFORM_BUFFER, uniform_block_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &transform_uniform_buffer_static_);
    glBindBuffer(GL_UNIFORM_BUFFER, transform_uniform_buffer_static_);
    glm::mat4 identity = glm::mat4(1.0f);
    glBufferData(
        GL_UNIFORM_BUFFER, 
        uniform_block_size, 
        &identity, 
        GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &blend_uniform_buffer_animated_);
    uniform_block_size = glm::max(GLint(sizeof(GLfloat)), uniform_buffer_offset);
    glBindBuffer(GL_UNIFORM_BUFFER, blend_uniform_buffer_animated_);
    glBufferData(GL_UNIFORM_BUFFER, uniform_block_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &blend_uniform_buffer_static_);
    glBindBuffer(GL_UNIFORM_BUFFER, blend_uniform_buffer_static_);
    GLfloat opaque_opacity = 1.0f;
    glBufferData(
        GL_UNIFORM_BUFFER, 
        uniform_block_size, 
        &opaque_opacity, 
        GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

fb::DemoStreamRenderer::~DemoStreamRenderer() {

    // Free textures
    if (lower_third_texture_ != 0) {
        glDeleteTextures(1, &lower_third_texture_);
        lower_third_texture_ = 0;
    }

    if (logo_texture_ != 0) {
        glDeleteTextures(1, &logo_texture_);
        logo_texture_ = 0;
    }

    if (overlay_program_id_ != 0) {
        glDeleteProgram(overlay_program_id_);
        overlay_program_id_ = 0;
    }

    if (overlay_pipeline_id_ != 0) {
        glDeleteProgramPipelines(1, &overlay_pipeline_id_);
        overlay_pipeline_id_ = 0;
    }

    if (background_program_id_ != 0) {
        glDeleteProgram(background_program_id_);
        background_program_id_ = 0;
    }

    if (background_pipeline_id_ != 0) {
        glDeleteProgramPipelines(1, &background_pipeline_id_);
        background_pipeline_id_ = 0;
    }

    if (transform_uniform_buffer_animated_ != 0) {
        glDeleteBuffers(1, &transform_uniform_buffer_animated_);
        transform_uniform_buffer_animated_ = 0;
    }

    if (transform_uniform_buffer_static_ != 0) {
        glDeleteBuffers(1, &transform_uniform_buffer_static_);
        transform_uniform_buffer_static_ = 0;
    }

    if (blend_uniform_buffer_animated_ != 0) {
        glDeleteBuffers(1, &blend_uniform_buffer_animated_);
        blend_uniform_buffer_animated_ = 0;
    }

    if (blend_uniform_buffer_static_ != 0) {
        glDeleteBuffers(1, &blend_uniform_buffer_static_);
        blend_uniform_buffer_static_ = 0;
    }

}

void fb::DemoStreamRenderer::render(
    const Time& frame_time,
    const std::vector<Texture>& source_frames,
    const Fbo& target_fbo) const
{

    // TODO: shoudl the framebuffer already be bound before method entry?
    // TODO: should the background already be set?

    // Bind target frame (via FBO)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fbo.id);

    /////
    // DRAW BACKGROUND (master frame)
    /////

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

    glBindProgramPipeline(background_pipeline_id_);

    // Draw the quad
    quad_.draw();

    glBindProgramPipeline(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    

    ////
    // DRAW OVERLAYS (lower third and logo)
    ////

    /////
    // UPDATE POSITION (lower third)
    ////

    // Update transform matrix

    static const glm::vec3 transpose_end = glm::vec3(.0f);
    static const glm::vec3 transpose_start = glm::vec3(-2.0, .0f, .0f);

    glm::vec3 transpose_animated;

    animate_in_out(
        frame_time, 
        transpose_start, 
        transpose_end, 
        transpose_animated);

    glm::mat4 mat(1.0f);
    mat = glm::translate(mat, transpose_animated);

    glBindBuffer(GL_UNIFORM_BUFFER, transform_uniform_buffer_animated_);
    uint8_t* mat_buffer = reinterpret_cast<uint8_t*>( glMapBufferRange(
        GL_UNIFORM_BUFFER, 
        0,
        sizeof(glm::mat4),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
	memcpy(mat_buffer, &mat, sizeof(glm::mat4));
    glUnmapBuffer(GL_UNIFORM_BUFFER);

    // Update opacity blend value (for logo)

    ////
    // Update opacity
    ////

    static const GLfloat opacity_end = 1.0f;
    static const GLfloat opacity_start = .0f;

    GLfloat opacity_animated = .0f;

    animate_in_out(
        frame_time,
        opacity_start,
        opacity_end,
        opacity_animated);

    glBindBuffer(GL_UNIFORM_BUFFER, blend_uniform_buffer_animated_);
    GLfloat* blend_buffer = (GLfloat*)glMapBufferRange(
        GL_UNIFORM_BUFFER, 
        0,
        sizeof(GLfloat),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *blend_buffer = opacity_animated;
    glUnmapBuffer(GL_UNIFORM_BUFFER);

    glDisable(GL_DEPTH_TEST);

    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glEnable(GL_BLEND);

    // Simply overlay lower third and logo

    glViewportIndexedf(
        0, 
        0, 
        0, 
        static_cast<GLfloat>(lower_third_texture_image_format_.gl_native_format().tex_width), 
        static_cast<GLfloat>(lower_third_texture_image_format_.gl_native_format().tex_height));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lower_third_texture_);
    glBindProgramPipeline(overlay_pipeline_id_);

    static const GLint transform_block_binding = 1;
    static const GLint blend_block_binding = 2;

    glBindBufferBase(
        GL_UNIFORM_BUFFER, 
        transform_block_binding, 
        transform_uniform_buffer_animated_);

    glBindBufferBase(
        GL_UNIFORM_BUFFER, 
        blend_block_binding, 
        blend_uniform_buffer_static_);

    quad_.draw();

    glBindTexture(GL_TEXTURE_2D, logo_texture_);

    glViewportIndexedf(
        0, 
        static_cast<GLfloat>(target_fbo.width) - static_cast<GLfloat>(logo_texture_image_format_.gl_native_format().tex_width), 
        static_cast<GLfloat>(target_fbo.height) - static_cast<GLfloat>(logo_texture_image_format_.gl_native_format().tex_height), 
        static_cast<GLfloat>(logo_texture_image_format_.gl_native_format().tex_width),
        static_cast<GLfloat>(logo_texture_image_format_.gl_native_format().tex_height));

    glBindBufferBase(
        GL_UNIFORM_BUFFER, 
        transform_block_binding, 
        transform_uniform_buffer_static_);

    glBindBufferBase(
        GL_UNIFORM_BUFFER, 
        blend_block_binding, 
        blend_uniform_buffer_animated_);

    quad_.draw();


    glBindProgramPipeline(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

}

std::size_t fb::DemoStreamRenderer::number_of_input_slots() const {

    return 1;

}

bool find_zero (float X1, float X2, float X3, float X4, float x, float& t)
{
    float c0,c1,c2,c3;
    float ts[3];

    // Dealing with border cases. We have to be a little more tolerant here
    // due to possible numerical issues
    static const float EPSILON = 0.0001f;
    if ( std::abs(x - X1) < EPSILON ) {
        t = 0.0f;
        return true;
    } else if (std::abs(x - X4) < EPSILON) {
        t = 1.0f;
        return true;
    }

    c0= X1 - x;
    c1= 3.0f * (X2 - X1);
    c2= 3.0f * (X1 - 2.0f*X2 + X3);
    c3= X4 - X1 + 3.0f * (X2 - X3);
    
    int solutions = solveCubic(c3,c2,c1,c0,ts);

    if        (solutions > 2 && ts[2] >= 0.0f && ts[2] <= 1.0f) {
        t = ts[2];
        return true;
    } else if (solutions > 1 && ts[1] >= 0.0f && ts[1] <= 1.0f) {
        t = ts[1];
        return true;
    } else if (solutions > 0 && ts[0] >= 0.0f && ts[0] <= 1.0f) {
        t = ts[0];
        return true;
    }

    return false;
}

float eval_bezier (float P1, float P2, float P3, float P4, float t)
{
    float c0, c1, c2, c3;

    c0= P1;
    c1= 3.0f * (P2 - P1);
    c2= 3.0f * (P1 - 2.0f*P2 + P3);
    c3= P4 - P1 + 3.0f * (P2 - P3);
    
    return c0 + t*c1 + t*t*c2 + t*t*t*c3;
}

// Hard-coded animation helpers

typedef std::array<std::array<float, 2>, 4> ControlPointsArray;

float evaluate_animation(
    const ControlPointsArray& control_points,
    const float& time_secs) 
{

    // Find t for our current time
    float X1 = control_points[0][0];
    float X2 = control_points[1][0];
    float X3 = control_points[2][0];
    float X4 = control_points[3][0];

    float local_time = time_secs;

    float t;
    bool success = find_zero(
        X1, 
        X2, 
        X3, 
        X4, 
        local_time,
        t);

    if (!success) {
        FB_LOG_ERROR << "Failed to invert time sampler spline";
        FB_LOG_ERROR << "x = " << local_time
             << ", X1 = " << X1
             << ", X2 = " << X2
             << ", X3 = " << X3
             << ", X4 = " << X4;
        return .0f;
    }

    // Evaluate
    //for (int i = 0; i < _components; ++i) {
    //int offset = (_data_sampler->segment_count()*3+1)*i;
    //offset += _current_segment * 3;
    float Y1 = control_points[0][1];
    float Y2 = control_points[1][1];
    float Y3 = control_points[2][1];
    float Y4 = control_points[3][1];

    float interpolated_result = eval_bezier(Y1, Y2, Y3, Y4, t);
    //}

    return interpolated_result;

}

static ControlPointsArray invert_control_points(
    float start_time,
    const ControlPointsArray& arr) 
{

    ControlPointsArray inverted;

    std::reverse_copy(
        std::begin(arr), 
        std::end(arr), 
        std::begin(inverted));

    float target_time = arr[3][0];

    for (auto& cps : inverted) {

        cps[0] = (target_time - cps[0]) + start_time;

    }

    return inverted;
}

template <typename T>
void animate_in_out(
    const fb::Time& t, 
    const T& start_value,
    const T& end_value,
    T& out)
{

    static const float in_start_time_sec = .0f;
    static const float in_duration_sec = 1.f;
    static const float out_start_time_sec = 3.0f;
    static const float out_duration_sec = 1.0f;
    static const float total_duration = out_start_time_sec + out_duration_sec - in_start_time_sec;

    float t_sec = std::fmod(fb::to_seconds(t), total_duration);

    if (out_start_time_sec < (in_start_time_sec + in_duration_sec)) {
        FB_LOG_ERROR << "Bogus animation parameters.";
        throw std::runtime_error("Bogus animation.");
    }

    static const ControlPointsArray in_control_points = {{
        {{.0f, .0f}},
        {{.27f, .69f}},
        {{.22f, .92f}},
        {{1.0f, 1.0f}}
    }};

    static const ControlPointsArray out_control_points = invert_control_points(3.0f, in_control_points);

    float interpolator = 1.0f;

    if (t_sec >= in_start_time_sec && t_sec <= (in_start_time_sec + in_duration_sec)) 
    {

        interpolator = evaluate_animation(
            in_control_points,
            t_sec);

    } else if (t_sec >= out_start_time_sec && t_sec <= (out_start_time_sec + out_duration_sec)) {

        interpolator = evaluate_animation(
            out_control_points,
            t_sec);

    }

    out = start_value + (end_value - start_value)*interpolator;

}
