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
#ifndef TOA_FRAME_BENDER_GL_WINDOW_H
#define TOA_FRAME_BENDER_GL_WINDOW_H

#include <memory>

#include "Utils.h"
#include "Context.h"

struct GLFWwindow;

namespace toa {
    namespace frame_bender {
        namespace gl {

            // GLFW (our current cross-platform implementation) only
            // supports one single window at a time, therefore Window-class
            // is currently a singleton.
            // Class represents a double-buffered window.
            class Init;
            class Window final : public ::toa::frame_bender::utils::NoCopyingOrMoving {
            public:

				typedef struct GLFWwindow* Handle;

                // A bit hacky, should be moved to its own input handling class

                enum class PressedKey {

                    none,
                    escape,
                    count

                };

                //typedef std::function<void(Key)> KeyCallback;

                ~Window();

                // Multiple calls will replace the single instance.
                // Note that the context of this window is current context
                // after creating the window
                static void create_window(
                    uint32_t width, 
                    uint32_t height,
                    bool use_debug_context, 
                    bool is_fullscreen = false,
                    bool use_vsync = false);

                static const std::unique_ptr<Window>& get() {
                    return instance;
                }

                const std::unique_ptr<Context>& context() const;
                void swap_buffers() const;

                // convenience method. Equal to wndw->context().create_shared_context(wndw->context())
                std::unique_ptr<Context> create_shared_context() const;

                // TODO: add hide/show functionality? Should be supported in 
                // GLFW at some point...

                const uint32_t width;
                const uint32_t height;
                const bool use_debug_context;

                //void set_key_callback(KeyCallback clbk);

                friend class Init;

                //friend void ::glfw_key_clbk(int, int);

                PressedKey poll_event();

            private:

                Window(
                    uint32_t width, 
                    uint32_t height,
                    bool use_debug_context, 
                    bool is_fullscreen,
                    bool use_vsync);

                static std::unique_ptr<Window> instance;

                std::unique_ptr<Context> context_;

				Handle window_;

                //KeyCallback key_callback_;

            };
        }
    }
}

#endif // TOA_FRAME_BENDER_GL_WINDOW_H