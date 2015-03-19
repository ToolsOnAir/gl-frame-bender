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
#include <stdexcept>

#include "Logging.h"
#include <glad/glad.h>

#include "Window.h"
#include "Init.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace fb = toa::frame_bender;

// FIXME: Remove, this we can do better now with glfw as backend
std::unique_ptr<fb::gl::Window> fb::gl::Window::instance;

void fb::gl::Window::create_window(
    uint32_t width, 
    uint32_t height,
    bool use_debug_context, 
    bool is_fullscreen,
    bool use_vsync)
{

    if (fb::gl::Window::instance) {
        FB_LOG_WARNING << "A window has already been created. Need to destroy "
            "this instance before creating a new one.";
        fb::gl::Window::instance.reset();
    }

    // Why is make_unique not working here?
    fb::gl::Window::instance = std::unique_ptr<Window>(
        new Window(
            width, 
            height, 
            use_debug_context,
            is_fullscreen,
            use_vsync));

    FB_LOG_DEBUG << "Created a new window.";
}

fb::gl::Window::Window(
                    uint32_t width, 
                    uint32_t height,
                    bool use_debug_context,
                    bool is_fullscreen,
                    bool use_vsync) : 
    width(width), height(height), use_debug_context(use_debug_context)
{

    // Note that you can assume that glfw is initialized already, but nothing
    // else at this point.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Init::targeted_gl_version().first);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Init::targeted_gl_version().second);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    if (use_debug_context) {
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    } else {
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
    }

    GLFWmonitor * fs_monitor = is_fullscreen ? glfwGetPrimaryMonitor() : nullptr;

	GLFWwindow *window = glfwCreateWindow(width, height, "gl-frame-bender", fs_monitor, nullptr);
    
    if (window == nullptr) {
        FB_LOG_ERROR << "Could not create master window.";
        throw std::runtime_error("Could not create master window.");
    }

	// By default, we bind the context after creation
	glfwMakeContextCurrent(window);

    glfwSwapInterval(use_vsync ? 1 : 0);

	context_ = std::unique_ptr<Context>(new Context("MainWindowContext",
		window,
		use_debug_context,
		false));

	window_ = window;

}

fb::gl::Window::~Window() {

    // Must destroy the context before window is tore down.
    context_.reset();

	glfwDestroyWindow(window_);
}

const std::unique_ptr<fb::gl::Context>& fb::gl::Window::context() const {
    return context_;
}

void fb::gl::Window::swap_buffers() const {

    glfwSwapBuffers(window_);

}

fb::gl::Window::PressedKey fb::gl::Window::poll_event() {

    glfwPollEvents();

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        return PressedKey::escape;
    }

    return PressedKey::none;
}
