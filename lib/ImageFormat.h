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
#ifndef TOA_FRAME_BENDER_IMAGE_FORMAT_H
#define TOA_FRAME_BENDER_IMAGE_FORMAT_H

#include <glad/glad.h>

#include <vector>
#include <cstdint>

namespace toa {
    namespace frame_bender {

        // TODO: create a cleaner separation between uncompressed representation
        // and possibly compressed internal storage.
        
        class ImageFormat final {

        public:

            enum class Origin {
                LOWER_LEFT,
                UPPER_LEFT
            };

            enum class Transfer {
                BT_709,
                BT_601,
                SRGB,
                LINEAR,
                COUNT
                // TODO: add studio RGB? (sRGB is full-swing)
            };

            enum class Chromaticity {
                BT_709,
                BT_601,
                SRGB
            };

            // TODO: add RGB_8BIT_SRGB in here...
            enum class PixelFormat : int32_t {
                RGB_8BIT,
                RGBA_8BIT,
                RGBA_16BIT,
                YUV_10BIT_V210,
                RGBA_FLOAT_16BIT,
                RGBA_FLOAT_32BIT,
                INVALID,
                COUNT
            };

            /**
             * @return The width of the uncompressed image size.
             */
            uint32_t width() const { return width_; }

            /**
             * @return The height of the uncompressed image size.
             */
            uint32_t height() const { return height_; }

            Transfer transfer() const { return transfer_; }
            Chromaticity chromaticity() const { return chromaticity_; }

            /**
             * @return The number of bytes it takes to store one image caryying
             * this format description.
             */
            size_t image_byte_size() const { return image_byte_size_; }

            Origin origin() const { return origin_; }

            // TODO: define nice printing functions
            bool operator==(const ImageFormat& other) const;
            bool operator!=(const ImageFormat& other) const;

            // TODO: do we need to specify row alignment explicitly?
            // TODO: do we need to add column major vs. row major?
            // TODO: add interlaced/non-interlaced?
            ImageFormat(
                uint32_t width,
                uint32_t height,
                Transfer transfer,
                Chromaticity chromaticity,
                PixelFormat pixel_format,
                Origin origin);

            static const ImageFormat& kInvalid() {

                static const ImageFormat invalid(
                    0, 
                    0, 
                    Transfer::BT_601,
                    Chromaticity::BT_601,
                    PixelFormat::INVALID,
                    Origin::LOWER_LEFT
                    );

                return invalid;
            }

            ImageFormat() {
                *this = kInvalid();
            }

            PixelFormat pixel_format() const;

            // TODO-DEF: At the moment, we are coupling OpenGL-side image
            // definition with our host-side memory layout. This is convenient
            // but might have to get more flexible in the future.
            struct GLFormatInfo {
                // Note that for a special format like V210 or 2VUY, 
                // the width or height != uncompressed width/height
                // e.g. 1920 V210 -> tex_width 1280
                GLsizei tex_width;
                GLsizei tex_height;
                // That's how OpenGL-clients will access the image
                GLenum tex_internal_format;
                // That's how we store the host-side memory
                GLenum data_format;
                // That's the data type of how we store it
                GLenum data_type;
               
                GLint byte_alignment;
                GLint row_length;

                GLint bytes_per_component;

                GLFormatInfo() : 
                    tex_width(0),
                    tex_height(0),
                    tex_internal_format(0), 
                    data_format(0), 
                    data_type(0),
                    byte_alignment(0),
                    row_length(0),
                    bytes_per_component(0) {}

                GLFormatInfo(
                    GLsizei tex_width,
                    GLsizei tex_height,
                    GLenum tex_internal_format, 
                    GLenum data_format, 
                    GLenum data_type,
                    GLint byte_alignment,
                    GLint row_length,
                    GLint bytes_per_component) : 
                        tex_width(tex_width),
                        tex_height(tex_height),
                        tex_internal_format(tex_internal_format), 
                        data_format(data_format), 
                        data_type(data_type),
                        byte_alignment(byte_alignment),
                        row_length(row_length),
                        bytes_per_component(bytes_per_component) {}

                    bool operator==(const GLFormatInfo& other) const;
                    bool operator!=(const GLFormatInfo& other) const;

            };

            const GLFormatInfo& gl_native_format() const;

        private:

            class FormatQuerySupport {

            public:
                FormatQuerySupport();
                bool is_supported() const { return is_supported_; }

            private:
                bool is_supported_;

            };

            static bool format_query_is_supported() {
                static const FormatQuerySupport s;
                return s.is_supported();
            }

            static size_t calculate_byte_size(
                PixelFormat pixel_format,
                uint32_t width, 
                uint32_t height);

            void set_gl_internal_format();

            uint32_t width_;
            uint32_t height_;
            Transfer transfer_;
            Chromaticity chromaticity_;
            PixelFormat pixel_format_;
            size_t image_byte_size_;
            
            GLFormatInfo gl_format_info_;
            Origin origin_;
            // TODO: define comparison operators by simply using std::tie. That way
            // we can use this class as key in std::map
        };

        std::ostream& operator<< (std::ostream& out, const ImageFormat& v);

        std::ostream& operator<< (std::ostream& out, const ImageFormat::PixelFormat& v);
        std::istream& operator>>(std::istream& in, ImageFormat::PixelFormat& v); 

        std::ostream& operator<< (std::ostream& out, const ImageFormat::Origin& v);
        std::istream& operator>>(std::istream& in, ImageFormat::Origin& v); 

        std::ostream& operator<< (std::ostream& out, const ImageFormat::Transfer& v);
        std::istream& operator>>(std::istream& in, ImageFormat::Transfer& v); 

    }
}

#endif // TOA_FRAME_BENDER_IMAGE_FORMAT_H