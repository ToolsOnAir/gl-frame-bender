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
#ifndef TOA_FRAME_BENDER_STAGE_DATA_TYPES_H
#define TOA_FRAME_BENDER_STAGE_DATA_TYPES_H

#include <vector>

#include "ImageFormat.h"
#include <glad/glad.h>
#include "FrameTime.h"
#include "Frame.h"

namespace toa {

    namespace frame_bender {

        class StreamComposition;

        struct PboMemory {
            GLuint pbo_id;
            PboMemory() : pbo_id(0) {}
            PboMemory(GLuint pbo_id):
                pbo_id(pbo_id) {}
        };

        typedef std::vector<PboMemory> PboMemVector;

        struct TokenGL {

            GLsync gl_fence;
            GLuint id; // Could be PBO id or TEX id
            ImageFormat format;
            StreamComposition* composition;
            GLuint fbo_id;
            Time time_stamp;
            uint8_t* buffer;

            TokenGL() : 
                gl_fence(0), 
                id(0), 
                format(ImageFormat::kInvalid()),
                composition(nullptr),
                fbo_id(0),
                time_stamp(0, 1),
                buffer(nullptr) {}

            TokenGL(GLsync gl_fence, 
                    GLuint id,
                    ImageFormat format, 
                    Time time_stamp,
                    GLuint fbo_id,
                    StreamComposition* composition,
                    uint8_t* buffer) 
                    :   gl_fence(gl_fence), 
                        id(id),
                        format(format),
                        composition(composition),
                        fbo_id(fbo_id),
                        time_stamp(time_stamp),
                        buffer(buffer) {}
        };

        // TODO-C++11: replace with =default or =delete

        struct TokenFrame {

            Frame frame;
            StreamComposition* composition;

            TokenFrame() : composition(nullptr) {}
            TokenFrame(Frame f, StreamComposition* c) : 
                frame(std::move(f)), composition(c) {}

            TokenFrame& operator=(TokenFrame&& other) {
                this->frame = std::move(other.frame);
                this->composition = other.composition;
                other.composition = nullptr;
                return *this;
            }

            TokenFrame(TokenFrame&& other) :
                frame(std::move(other.frame)),
                composition(other.composition) {
                    other.composition = nullptr;
            }

        private:
            TokenFrame& operator=(const TokenFrame&);
            TokenFrame(const TokenFrame&);
        };

    }

}

#endif // TOA_FRAME_BENDER_STAGE_DATA_TYPES_H
