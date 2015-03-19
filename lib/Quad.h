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
#ifndef TOA_FRAME_BENDER_GL_QUAD_H
#define TOA_FRAME_BENDER_GL_QUAD_H

#include <glad/glad.h>
#include "Utils.h"
#include "glm/glm.hpp"

namespace toa {
    namespace frame_bender {
        namespace gl {
            
            /**
             * Defines a screen-filling quad, already sets the VBOs and VAOs.
             * Note that a single Quad instance can only be used by the
             * context that created it.
             */
            class Quad final : public toa::frame_bender::utils::NoCopyingOrMoving {
            
            public:

                enum class UV_ORIGIN : int32_t {
                    LOWER_LEFT,
                    UPPER_LEFT,
                    DEFAULT = LOWER_LEFT
                };

                Quad(UV_ORIGIN origin);
                ~Quad();

                void draw() const;

            private:

                UV_ORIGIN uv_origin_;
                GLuint vertex_vbo_id_;
                GLuint element_vbo_id_;
                GLuint vertex_array_id_;
            };

        }
    }
}

#endif // TOA_FRAME_BENDER_GL_QUAD_H