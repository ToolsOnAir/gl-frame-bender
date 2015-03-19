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
#ifndef TOA_FRAME_BENDER_STREAM_RENDERER_H
#define TOA_FRAME_BENDER_STREAM_RENDERER_H

#include <vector>
#include <string>

#include "FrameTime.h"
#include "Utils.h"
#include "ImageFormat.h"

#include <glad/glad.h>
#include "Quad.h"

namespace toa {
    namespace frame_bender {

        /**
         * Interface for a render operation that will be applied by Dispatch
         * on a per-frame basis. A concrete implementation must implement 
         * a couple of methods as described here.
         */
        class StreamRenderer : public utils::NoCopyingOrMoving {
        public:

            // TODO: At the moment, this is only an information holder, 
            // we _could_ make this is a more complete class, but we should
            // really make sure that this would make sense (in this case, 
            // Streamdispatcher would likely be the owner of the class.
            struct Texture {
                GLuint id;
                GLint width;
                GLint height;
                Texture() : id(0), width(0), height(0) {}
                Texture(
                    GLuint id, 
                    GLint width, 
                    GLint height) : 
                        id(id), 
                        width(width), 
                        height(height) {}
            };

            typedef Texture Fbo;

            StreamRenderer(std::string name);
            virtual ~StreamRenderer();       

            virtual void render(
                const Time& frame_time,
                const std::vector<Texture>& source_frames,
                const Fbo& target_fbo
                ) const = 0;

            virtual std::size_t number_of_input_slots() const = 0;

        private:
            std::string name_;

        };

        // Simply passes through images. Mainly used for testing.
        class PassThroughRenderer final : public StreamRenderer {
        public:

            PassThroughRenderer(bool is_integer_format = false);
            ~PassThroughRenderer();
            void render(
                const Time& frame_time,
                const std::vector<Texture>& source_frames,
                const Fbo& target_fbo
                ) const override;

            std::size_t number_of_input_slots() const override { return 1; }

        private:

            GLuint program_id_;
            GLuint pipeline_id_;

            gl::Quad quad_;

        };

        //class PassThroughV210CodecRenderer final : public StreamRenderer {
        //public:

        //    PassThroughV210CodecRenderer(const ImageFormat& v210_format);
        //    ~PassThroughV210CodecRenderer();
        //    void render(
        //        const Time& frame_time,
        //        const std::vector<Texture>& source_frames,
        //        const Texture& target_texture
        //        ) const override;

        //    std::size_t number_of_input_slots() const override { return 1; }



        //private:

        //    ImageFormat v210_format_;


        //    GLuint src_fbo_id_;
        //    GLuint src_texture_rgbaf_id_;

        //    GLuint dst_fbo_id_;
        //    GLuint dst_texture_rgbaf_id_;

        //    //GLuint encode_program_id_;
        //    //GLuint encode_pipe line_id_;

        //    GLuint output_fbo_id_;

        //    GLuint passthrough_program_id_;
        //    GLuint passthrough_pipeline_id_;

        //    gl::Quad quad_;

        //};

        /*class SimpleImageOverlay final : public StreamRenderer {
        public:

            void render(
                const Time& frame_time,
                const std::vector<Texture>& source_frames,
                const Texture& target_texture
                ) override;

            std::size_t number_of_input_slots() const override { return 1; }
        };*/

    }
}

#endif // TOA_FRAME_BENDER_STREAM_RENDERER_H