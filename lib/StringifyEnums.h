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
#ifndef TOA_FRAME_BENDER_GL_STRINGIFY_ENUMS_H
#define TOA_FRAME_BENDER_GL_STRINGIFY_ENUMS_H

#include <glad/glad.h>

#include <string>
#include <unordered_map>

namespace toa {
    namespace frame_bender {
        namespace gl {

            class EnumHelper {

            public:

                static const std::string& pretty_print_enum(GLenum e);

            private:

                static const EnumHelper& get() {
                    static const EnumHelper instance;
                    return instance;
                }

                EnumHelper();

                void init_table();

                void lookup_insert(uint64_t, std::string&& val);

                std::unordered_map<uint64_t, std::string> table_;

            };

        }

    }
}

#endif //TOA_FRAME_BENDER_GL_STRINGIFY_ENUMS_H
