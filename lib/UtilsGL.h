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
#ifndef TOA_FRAME_BENDER_UTILS_GL_H
#define TOA_FRAME_BENDER_UTILS_GL_H

#include "Logging.h"
#include <glad/glad.h>

#define FB_GL_BUFFER_OFFSET(i) ((char *)NULL + (i))

namespace toa {
    namespace frame_bender {
        namespace gl {
            namespace utils {

                struct GLInfo {
                    int32_t major;
                    int32_t minor;
                    std::string version_string;
                    std::string vendor;
                    std::string renderer;

                };

                GLInfo get_info();

				std::ostream& operator<< (std::ostream& out, const GLInfo& v);

                inline static std::string get_string_for_framebuffer_status(GLenum status) {
                    std::string answer;
                    switch(status)
                    {
                    case GL_FRAMEBUFFER_COMPLETE:
                        answer = "GL_FRAMEBUFFER_COMPLETE";
                        break;
                    case GL_FRAMEBUFFER_UNDEFINED :
                        answer = "GL_FRAMEBUFFER_UNDEFINED ";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                        break;
                    case GL_FRAMEBUFFER_UNSUPPORTED:
                        answer = "GL_FRAMEBUFFER_UNSUPPORTED";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                        answer = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                        break;
                    default:
                        answer = "<undefined>";
                    }

                    return answer;
                }

                inline static bool is_frame_buffer_complete(GLenum target)
                {
                    GLenum status = glCheckFramebufferStatus(target);
                    if (status != GL_FRAMEBUFFER_COMPLETE) {
                        FB_LOG_ERROR 
                            << "OpenGL framebuffer is not complete: '" 
                            << get_string_for_framebuffer_status(status) 
                            << "'.";
                    }
                    return status == GL_FRAMEBUFFER_COMPLETE;
                }

                std::string read_glsl_source_file(
                    const std::string& shader_source_location,
                    std::set<std::string> already_included_files = std::set<std::string>()
                    );

                GLuint compile_shader(
                    const std::string& shader_source_location,
                    GLenum shader_type,
                    const std::vector<std::string>& preprocessor_macros = std::vector<std::string>());

                /**
                 * Convenience methods which create, compiles, links
                 * a vertex and fragment shader and ties them into one
                 * pipeline object.
                 */
                GLuint create_glsl_program_with_fs_and_vs(
                    const std::string& vertex_shader_source_location, 
                    const std::string& fragment_shader_source_location,
                    const std::vector<std::string>& vertex_shader_source_preprocessor_macros = std::vector<std::string>(),
                    const std::vector<std::string>& fragment_shader_source_preprocessor_macros = std::vector<std::string>());

                GLuint create_glsl_program_with_cs(
                    const std::string& compute_shader_source_location, 
                    const std::vector<std::string>& compute_shader_source_location_preprocessor_macros = std::vector<std::string>());

                std::vector<GLuint> create_and_initialize_pbos(
                    size_t num,
                    size_t byte_size,
                    GLenum target,
                    GLenum usage);

                uint8_t* map_pbo(GLuint pbo_id, size_t buffer_size, GLenum target, GLbitfield access);
                bool unmap_pbo(GLuint pbo_id, GLenum target);

                inline static void insert_debug_message(const std::string& msg) {

                    if (GLAD_GL_ARB_debug_output != 0) {
                        glDebugMessageInsertARB(
                            GL_DEBUG_SOURCE_APPLICATION_ARB, 
                            GL_DEBUG_TYPE_OTHER_ARB, 
                            1, 
                            GL_DEBUG_SEVERITY_LOW_ARB,
                            GLsizei(msg.size()), msg.c_str());
                    } else {
                        FB_LOG_WARNING 
                            << "ARB_debug_output not available, can't print "
                            << "application-level OGL message '" 
                            << msg 
                            << "'.";
                    }

                }

            }
        }

    }
}

#endif // TOA_FRAME_BENDER_UTILS_GL_H