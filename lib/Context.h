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
#ifndef TOA_FRAME_BENDER_GL_CONTEXT_H
#define TOA_FRAME_BENDER_GL_CONTEXT_H

#include <string>
#include <thread>

#include "Utils.h"
#include "Logging.h"

#include <glad/glad.h>

struct GLFWwindow;

namespace toa {
    namespace frame_bender {
        namespace gl {

            class Window;

            // TODO: actually, you could implement moving. That makes sense here.
            class Context final : public ::toa::frame_bender::utils::NoCopyingOrMoving {
            public:

				typedef struct GLFWwindow * Handle;

                enum class DebugSeverity : GLenum {

                    LOW = GL_DEBUG_SEVERITY_LOW_ARB,
                    MEDIUM = GL_DEBUG_SEVERITY_MEDIUM_ARB,
                    HIGH = GL_DEBUG_SEVERITY_HIGH_ARB,
                    COUNT = 3

                };

                /**
                 * Returns true if the calling thread has some Context
                 * attached.
                 */
                static bool this_thread_has_context();

                // TODO: document that create_shared also sets the new context
                // as the active context of the calling thread.
                std::unique_ptr<Context> create_shared(std::string name) const;

                ~Context();

                /**
                 * Attaches this context to the calling thread. If the context
                 * is already attached to another thread (is_attached returns
                 * true), then this will throw an exception. A context can
                 * only be attached to a single thread at a time.
                 */
                void attach();

                /**
                 * Detaches this context from the calling thread. If the context
                 * is not attached to the calling thread, this will throw 
                 * an exception. The context can only be detached from the 
                 * thread that was attached to it.
                 */
                void detach();

                /**
                 * Returns true, if this context is attached to the calling
                 * thread.
                 */
                bool is_attached_to_this_thread() const;

                /**
                 * Returns true, if this context is attached to any thread, i.e.
                 * checks whether it can be attached to a new thread.
                 */
                bool is_attached_to_any_thread() const;

                bool debug_logging_is_enabled() const;

                const Handle handle() const;

                bool has_debug_enabled() const;

                friend class Window;

            private:

                // throws if this is not debug context, or ARB_debug_output is
                // not supported.
                // TODO: document that these functions require the context be
                // set as "current" (activated)
                void enable_debug_logging();
                void disable_debug_logging();
                
                void force_detach();

                static std::string get_string_from_gl_debug_source(GLenum source);
                static std::string get_string_from_gl_debug_type(GLenum type);
                static std::string get_string_from_gl_severity(GLenum severity);
                static logging::Severity get_severity_from_gl_severity(GLenum severity);

                static void gl_arb_debug_callback (
                    GLenum source,
                    GLenum type,
                    GLuint id,
                    GLenum severity,
                    GLsizei length,
                    const GLchar *message,
                    const GLvoid *userParam);

				Context(
					std::string name,
					Handle wnd,
					bool has_debug_enabled,
					bool owns_handle
					);

                Handle handle_;
                const bool has_debug_enabled_;
                const bool owns_handle_;
                const std::string name_;
                bool debug_logging_is_enabled_;
                std::thread::id attached_thread_;
            };

            std::ostream& operator<< (std::ostream& out, const Context::DebugSeverity& v);
            std::istream& operator>>(std::istream& in, Context::DebugSeverity& v); 

        }
    }
}

#endif // TOA_FRAME_BENDER_GL_CONTEXT_H
