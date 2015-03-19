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
#ifndef TOA_FRAME_BENDER_DEMO_STREAM_RENDERER_H
#define TOA_FRAME_BENDER_DEMO_STREAM_RENDERER_H

#include "StreamRenderer.h"
#include "Quad.h"

#include <array>

namespace toa {

    namespace frame_bender {

        class DemoStreamRenderer : public StreamRenderer {

        public:

            DemoStreamRenderer();
            ~DemoStreamRenderer();

            virtual void render(
                const Time& frame_time,
                const std::vector<Texture>& source_frames,
                const Fbo& target_fbo
                ) const override;

            virtual std::size_t number_of_input_slots() const override;

        private:

            gl::Quad quad_;

            GLuint background_program_id_;
            GLuint background_pipeline_id_;

            GLuint overlay_program_id_;
            GLuint overlay_pipeline_id_;

            GLuint lower_third_texture_;
            ImageFormat lower_third_texture_image_format_;
            GLuint logo_texture_;
            ImageFormat logo_texture_image_format_;

            GLuint transform_uniform_buffer_animated_;
            GLuint transform_uniform_buffer_static_;
            GLuint blend_uniform_buffer_animated_;
            GLuint blend_uniform_buffer_static_;

        };

    }

}

#endif // TOA_FRAME_BENDER_DEMO_STREAM_RENDERER_H