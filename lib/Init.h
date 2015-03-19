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
#ifndef TOA_FRAME_BENDER_GL_INIT_H
#define TOA_FRAME_BENDER_GL_INIT_H

#include "Utils.h"

#include <stdexcept>

namespace toa {
    namespace frame_bender {
        namespace gl {

            class Init final : public toa::frame_bender::utils::NoCopyingOrMoving {
            public:

                static void require() {
                    static const Init init;
                    if (!init.is_initialized_)
                        throw std::runtime_error("Application's GL was not properly initialized.");
                }

                ~Init();

                static const std::pair<int, int>& targeted_gl_version() {
                    static const std::pair<int, int> gl_version = std::make_pair(4, 3) ;
                    return gl_version;
                }

            private:
                Init();
                bool is_initialized_;
            };

        }
    } 
}

#endif // TOA_FRAME_BENDER_GL_INIT_H
