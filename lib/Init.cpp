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
#include "Precompile.h"
#include "Init.h"
#include "Logging.h"
#include "ProgramOptions.h"

#include "Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace fb = toa::frame_bender;

fb::gl::Init::Init() : is_initialized_(false) {

    // NOTE: This loader code implicitly assumes that the entry points
    // to the GL are the same for all future (possibly shared) contexts that
    // we create in this process. This should be ok, but there is no real 
    // guarantee here.
    // GLEW has support for multiple rendering contexts.
    // http://glew.sourceforge.net/advanced.html (look for GLEW_MX)

    int result = glfwInit();
    if (result != GL_TRUE) {
        FB_LOG_ERROR << "Could not initialize the GLFW library.";
        throw std::runtime_error("GLFW not initialized.");
    }

    FB_LOG_DEBUG << "Initialized GLFW library";

    Window::create_window(
        static_cast<uint32_t>(ProgramOptions::global().player_width()),
        static_cast<uint32_t>(ProgramOptions::global().player_height()),
        ProgramOptions::global().use_debug_gl_context(),
        ProgramOptions::global().player_is_fullscreen(),
        ProgramOptions::global().player_use_vsync());

    FB_LOG_DEBUG << "Successfully created the first GLFW window for extension loading.";

    // Window creation above already gave as a context.
  	int success = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    FB_ASSERT_MESSAGE(success == 1, "Loading of GL bindings.");

    FB_LOG_DEBUG << "Sucessfully loaded (global) OpenGL symbols.";

    GLint val = 0;
    glGetIntegerv(GL_MIN_MAP_BUFFER_ALIGNMENT, &val);
    if (val >= 64) {
        FB_LOG_DEBUG << "GL MIN_MAP_BUFFER_ALIGNMENT is properly set to at-least 64-byte boundaries.";
    } else {
        FB_LOG_WARNING << "GL MIN_MAP_BUFFER_ALIGNMENT is less than 64-byte (value=" << val << "). This can result is suboptimal performance.";
    }

    Window::get()->context()->detach();

    is_initialized_ = true;
}

fb::gl::Init::~Init() {

    // When init is tore down, we must ensure that no Window is present
    // anymore
    Window::instance.reset();

    glfwTerminate();
    FB_LOG_DEBUG << "Terminated GLFW library.";

}
