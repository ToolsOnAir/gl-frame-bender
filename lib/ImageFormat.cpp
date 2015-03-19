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

#include <algorithm>

#include "ImageFormat.h"
#include "Logging.h"
#include "ProgramOptions.h"
#include "StringifyEnums.h"
#include "Context.h"

namespace fb = toa::frame_bender;

fb::ImageFormat::ImageFormat(uint32_t width,
                             uint32_t height,
                             Transfer transfer,
                             Chromaticity chromaticity,
                             PixelFormat pixel_format,
                             Origin origin) :
    width_(width), 
    height_(height), 
    transfer_(transfer),
    chromaticity_(chromaticity),
    pixel_format_(pixel_format),
    origin_(origin)
{

    // Calculate byte size and store it
    if (pixel_format_ != PixelFormat::INVALID) {
        image_byte_size_ = calculate_byte_size(
            pixel_format_,
            width_,
            height_);

        set_gl_internal_format();
    }

} 

fb::ImageFormat::PixelFormat fb::ImageFormat::pixel_format() const {
    return pixel_format_;
}

bool fb::ImageFormat::GLFormatInfo::operator==(const ImageFormat::GLFormatInfo& other) const
{    
    return (this->tex_width == other.tex_width && 
            this->tex_height == other.tex_height &&
            this->tex_internal_format == other.tex_internal_format &&          
            this->data_format == other.data_format &&
            this->data_type == other.data_type &&
            this->byte_alignment == other.byte_alignment && 
            this->row_length == other.row_length);
}            

bool fb::ImageFormat::GLFormatInfo::operator!=(const ImageFormat::GLFormatInfo& other) const
{
    return !(*this == other);
}      

bool fb::ImageFormat::operator==(const ImageFormat& other) const
{    
    return (this->width_ == other.width_ && 
            this->height_ == other.height_ &&
            this->transfer_ == other.transfer_ &&          
            this->chromaticity_ == other.chromaticity_ &&
            this->pixel_format_ == other.pixel_format_ &&
            this->image_byte_size_ == other.image_byte_size_ && 
            this->gl_format_info_ == other.gl_format_info_ &&
            this->origin_ == other.origin_);
}            

bool fb::ImageFormat::operator!=(const ImageFormat& other) const
{
    return !(*this == other);
}      


std::ostream& fb::operator<< (std::ostream& out, const fb::ImageFormat::PixelFormat& v) {

    switch (v) {
    case fb::ImageFormat::PixelFormat::RGB_8BIT:
        out << "RGB_8BIT";
        break;
    case fb::ImageFormat::PixelFormat::RGBA_8BIT:
        out << "RGBA_8BIT";
        break;
    case fb::ImageFormat::PixelFormat::RGBA_16BIT:
        out << "RGBA_16BIT";
        break;
    case fb::ImageFormat::PixelFormat::YUV_10BIT_V210:
        out << "YUV_10BIT_V210";
        break;
    case fb::ImageFormat::PixelFormat::RGBA_FLOAT_32BIT:
        out << "RGBA_FLOAT_32BIT";
        break;
    case fb::ImageFormat::PixelFormat::RGBA_FLOAT_16BIT:
        out << "RGBA_FLOAT_16BIT";
        break;
    case fb::ImageFormat::PixelFormat::INVALID:
        out << "INVALID";
        break;
    default:
        out << "<unknown>";
        break;

    }

    return out;
}

std::istream& fb::operator>>(std::istream& in, ImageFormat::PixelFormat& v) {

    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "RGB_8BIT")
        v = fb::ImageFormat::PixelFormat::RGB_8BIT;
    else if (token == "RGBA_16BIT")
        v = fb::ImageFormat::PixelFormat::RGBA_16BIT;
    else if (token == "RGBA_8BIT")
        v = fb::ImageFormat::PixelFormat::RGBA_8BIT;
    else if (token == "YUV_10BIT_V210")
        v = fb::ImageFormat::PixelFormat::YUV_10BIT_V210;
    else if (token == "RGBA_FLOAT_32BIT")
        v = fb::ImageFormat::PixelFormat::RGBA_FLOAT_32BIT;
    else if (token == "RGBA_FLOAT_16BIT")
        v = fb::ImageFormat::PixelFormat::RGBA_FLOAT_16BIT;
    //else throw boost::program_options::validation_error("Invalid pixel format '" + token + "'");
    else {
        in.setstate(std::ios::failbit);
    }

    return in;

}

std::ostream& fb::operator<< (std::ostream& out, const fb::ImageFormat::Origin& v) {

    switch (v) {
    case ImageFormat::Origin::LOWER_LEFT:
        out << "LOWER_LEFT";
        break;
    case ImageFormat::Origin::UPPER_LEFT:
        out << "UPPER_LEFT";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::operator>>(std::istream& in, ImageFormat::Origin& v) {
    
    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "LOWER_LEFT")
        v = fb::ImageFormat::Origin::LOWER_LEFT;
    else if (token == "UPPER_LEFT")
        v = fb::ImageFormat::Origin::UPPER_LEFT;
    else {
        in.setstate(std::ios::failbit);
    }

    return in;
}

std::ostream& fb::operator<< (std::ostream& out, const fb::ImageFormat::Transfer& v) {

    switch (v) {
    case ImageFormat::Transfer::BT_601:
        out << "BT_601";
        break;
    case ImageFormat::Transfer::BT_709:
        out << "BT_709";
        break;
    case ImageFormat::Transfer::LINEAR:
        out << "LINEAR";
        break;
    case ImageFormat::Transfer::SRGB:
        out << "SRGB";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::operator>>(std::istream& in, ImageFormat::Transfer& v) {
    
    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "BT_601")
        v = fb::ImageFormat::Transfer::BT_601;
    else if (token == "BT_709")
        v = fb::ImageFormat::Transfer::BT_709;
    else if (token == "LINEAR")
        v = fb::ImageFormat::Transfer::LINEAR;
    else if (token == "SRGB")
        v = fb::ImageFormat::Transfer::SRGB;
    else {
        in.setstate(std::ios::failbit);
    }

    return in;
}

std::ostream& fb::operator<< (std::ostream& out, const fb::ImageFormat& v) {

    // TODO: complete printout
    out << "PixelFormat: '" << v.pixel_format() << "', width: '" 
        << v.width() << "', height: '" << v.height() 
        << "', origin: '" << v.origin() << "'.";
    return out;

}

size_t fb::ImageFormat::calculate_byte_size(
    PixelFormat pixel_format,
    uint32_t width, 
    uint32_t height)
{

    size_t answer = 0;

    switch (pixel_format) {
    case PixelFormat::RGB_8BIT:
        answer = 3 * width * height;
        break;
    case PixelFormat::RGBA_8BIT:
        answer = 4 * width * height;
        break;
    case PixelFormat::RGBA_16BIT:
        answer = 8 * width * height;
        break;
    case PixelFormat::RGBA_FLOAT_32BIT:
        answer = 16 * width * height;
        break;
    case PixelFormat::RGBA_FLOAT_16BIT:
        answer = 8 * width * height;
        break;
    case PixelFormat::YUV_10BIT_V210:
        // TODO-DEF: add row alignment, e.g. for resolutions like 720 or lower
        FB_ASSERT_MESSAGE(width % 48 == 0, "Doesn't support V210 row padding as of now.");
        answer = width * height * 8/3;
        break;
    case PixelFormat::INVALID:
        answer = 0;
        break;
    default:
        throw std::invalid_argument("Unsupported pixel format encountered.");
        break;
    }

    return answer;

}

const fb::ImageFormat::GLFormatInfo& fb::ImageFormat::gl_native_format() const {
    return gl_format_info_;
}

fb::ImageFormat::FormatQuerySupport::FormatQuerySupport() : is_supported_(false) {

    is_supported_ = GLAD_GL_ARB_internalformat_query2 != 0;

    if(is_supported_) {

        FB_LOG_INFO << "ARB_internalformat_query2 is supported, will check if optimal texture formats are used.";

    } else {

        FB_LOG_INFO << "ARB_internalformat_query2 is not supported, won't report on optimal texture format settings.";

    }

}

void fb::ImageFormat::set_gl_internal_format() {
   
    switch (pixel_format_) {
    case PixelFormat::RGB_8BIT:
        gl_format_info_ = GLFormatInfo(
            static_cast<GLsizei>(width_),
            static_cast<GLsizei>(height_),
            GL_RGB8,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            1,
            static_cast<GLint>(width_),
            3);
        break;
    case PixelFormat::RGBA_8BIT:
        gl_format_info_ = GLFormatInfo(
            static_cast<GLsizei>(width_),
            static_cast<GLsizei>(height_),
            GL_RGBA8,
            GL_RGBA,
            GL_UNSIGNED_INT_8_8_8_8_REV,
            4,
            static_cast<GLint>(width_),
            4);
        break;
    case PixelFormat::RGBA_16BIT:
        gl_format_info_ = GLFormatInfo(
            static_cast<GLsizei>(width_),
            static_cast<GLsizei>(height_),
            GL_RGBA16,
            GL_RGBA,
            GL_UNSIGNED_SHORT,
            4,
            static_cast<GLint>(width_),
            8);
        break;
    case PixelFormat::RGBA_FLOAT_16BIT:
        gl_format_info_ = GLFormatInfo(
            static_cast<GLsizei>(width_),
            static_cast<GLsizei>(height_),
            GL_RGBA16F,
            GL_RGBA,
            GL_HALF_FLOAT,
            4,
            static_cast<GLint>(width_),
            8);
        break;
    case PixelFormat::RGBA_FLOAT_32BIT:
        gl_format_info_ = GLFormatInfo(
            static_cast<GLsizei>(width_),
            static_cast<GLsizei>(height_),
            GL_RGBA32F,
            GL_RGBA,
            GL_FLOAT,
            4,
            static_cast<GLint>(width_),
            16);
        break;
    case PixelFormat::YUV_10BIT_V210: {

        auto result = std::div(width_ * 2, 3);

        if (result.rem != 0) {
            FB_LOG_ERROR << "Width '" << width_ << "' is not a multiple of 2/3.";
            throw std::invalid_argument("Invalid width.");
        }

        GLsizei tex_width = static_cast<GLsizei>(result.quot);

        const bool unpack_by_shader = ProgramOptions::global().v210_use_shader_for_component_access();

        gl_format_info_ = GLFormatInfo(
            tex_width,
            static_cast<GLsizei>(height_),
            unpack_by_shader ? GL_R32UI : GL_RGB10_A2UI,
            unpack_by_shader ? GL_RED_INTEGER : GL_RGBA_INTEGER,
            unpack_by_shader ? GL_UNSIGNED_INT : GL_UNSIGNED_INT_2_10_10_10_REV,
            4,
            // TODO-DEF: for resolutions like 720p we need a different
            // row length (a mutliple of 48, I think)
            tex_width,
            4);
        }
        break;
    default:
        FB_LOG_ERROR << "Unknown pixel format: '" << static_cast<int32_t>(pixel_format_) << "'.";
        throw std::invalid_argument("Unknown pixel format encountered.");
    }

    // Do some checks if extension query format ARB_internalformat_query2
    // is available and log info.
    if (gl::Context::this_thread_has_context() && format_query_is_supported()) {
 
        // TODO: is it safe to assume TEXTURE_2D as the target?

        // see http://www.opengl.org/wiki/GLAPI/glGetInternalFormat

        GLint preferred_format = 0;
        GLint preferred_type = 0;
        GLint preferred_format_get = 0;
        GLint preferred_type_get = 0;

        glGetInternalformativ(
            GL_TEXTURE_2D, 
            gl_format_info_.tex_internal_format,
            GL_TEXTURE_IMAGE_FORMAT,
            1,
            &preferred_format);

        glGetInternalformativ(
            GL_TEXTURE_2D, 
            gl_format_info_.tex_internal_format,
            GL_TEXTURE_IMAGE_TYPE,
            1,
            &preferred_type);

        glGetInternalformativ(
            GL_TEXTURE_2D, 
            gl_format_info_.tex_internal_format,
            GL_GET_TEXTURE_IMAGE_FORMAT,
            1,
            &preferred_format_get);

        glGetInternalformativ(
            GL_TEXTURE_2D, 
            gl_format_info_.tex_internal_format,
            GL_GET_TEXTURE_IMAGE_TYPE,
            1,
            &preferred_type_get);

        GLenum preferred_format_enum = static_cast<GLenum>(preferred_format);
        GLenum preferred_type_enum = static_cast<GLenum>(preferred_type);
        GLenum preferred_format_get_enum = static_cast<GLenum>(preferred_format_get);
        GLenum preferred_type_get_enum = static_cast<GLenum>(preferred_type_get);

        if (preferred_format_enum == 0 &&
            preferred_type_enum == 0 && 
            preferred_format_get_enum == 0 &&
            preferred_type_get_enum == 0)
        {

            FB_LOG_INFO << "PixelFormat '" << pixel_format_ << "': Did not receive feedback from implementation about data types and formats compatibility.";

        } else {

            if (gl_format_info_.data_format != preferred_format_enum) {

                FB_LOG_WARNING 
                    << "PixelFormat '" << pixel_format_ << "': Using suboptimal data format for glTexImage2D and similar."
                    << "Current format: '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_format) << "', "
                    << "optimal format: '" << gl::EnumHelper::pretty_print_enum(preferred_format_enum) << "'.";

            } else {
                FB_LOG_INFO << "PixelFormat '" << pixel_format_ << "': Using optimal data format '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_format) << "' for glTexImage2D.";
            }

            if (gl_format_info_.data_type != preferred_type_enum) {

                FB_LOG_WARNING 
                    << "PixelFormat '" << pixel_format_ << "': Using suboptimal data type for glTexImage2D and similar."
                    << "Current type: '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_type) << "', "
                    << "optimal type: '" << gl::EnumHelper::pretty_print_enum(preferred_type_enum) << "'.";

            } else {
                FB_LOG_INFO << "PixelFormat '" << pixel_format_ << "': Using optimal data type '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_type) << "' for glTexImage2D.";
            }

            if (gl_format_info_.data_format != preferred_format_get_enum) {

                FB_LOG_WARNING 
                    << "PixelFormat '" << pixel_format_ << "': Using suboptimal data format for glGetTexImage and similar."
                    << "Current format: '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_format) << "', "
                    << "optimal format: '" << gl::EnumHelper::pretty_print_enum(preferred_format_get_enum) << "'.";

            } else {
                FB_LOG_INFO << "PixelFormat '" << pixel_format_ << "': Using optimal data format '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_format) << "' for glGetTexImage.";
            }

            if (gl_format_info_.data_type != preferred_type_get_enum) {

                FB_LOG_WARNING 
                    << "PixelFormat '" << pixel_format_ << "': Using suboptimal data type for glGetTexImage and similar."
                    << "Current type: '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_type) << "', "
                    << "optimal type: '" << gl::EnumHelper::pretty_print_enum(preferred_type_get_enum) << "'.";

            } else {
                FB_LOG_INFO << "PixelFormat '" << pixel_format_ << "': Using optimal data type '" << gl::EnumHelper::pretty_print_enum(gl_format_info_.data_type) << "' for glGetTexImage.";
            }
        }
    }
   

}
