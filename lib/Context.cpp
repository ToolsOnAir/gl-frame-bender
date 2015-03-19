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

#include <vector>
#include <thread>

#include "Context.h"
#include "Init.h"
#include "ProgramOptions.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace fb = toa::frame_bender;

bool fb::gl::Context::this_thread_has_context() {

	return glfwGetCurrentContext() != nullptr;
}

std::unique_ptr<fb::gl::Context> fb::gl::Context::create_shared(std::string name) const
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Init::targeted_gl_version().first);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Init::targeted_gl_version().second);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if (has_debug_enabled_) {
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	}
	else {
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
	}

	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

	GLFWwindow *window = glfwCreateWindow(32, 32, "", nullptr, handle_);

	if (window == nullptr) {
		FB_LOG_ERROR << "Could not create shared context.";
		throw std::runtime_error("Could not create shared context.");
	}

	std::unique_ptr<Context> new_context(new Context(
		std::move(name),
		window,
		has_debug_enabled_,
		true));

	return new_context;
}

fb::gl::Context::Context(
	std::string name,
	GLFWwindow * handle,
	bool has_debug_enabled,
	bool owns_handle)
	: handle_(handle),
	has_debug_enabled_(has_debug_enabled),
	owns_handle_(owns_handle),
	name_(std::move(name)),
	debug_logging_is_enabled_(false)
{
	GLFWwindow *current = glfwGetCurrentContext();

	if (current == handle) {
		attached_thread_ = std::this_thread::get_id();
	}
}

fb::gl::Context::~Context() {

    try {
        if (debug_logging_is_enabled()) {
            force_detach();
            attach();
            disable_debug_logging();
            detach();
        }
    } catch (const std::exception&) {
        FB_LOG_WARNING << "Could not cleanly detach debug-logger from context.";
    }

    if (owns_handle_)
		glfwDestroyWindow(handle_);

}

void fb::gl::Context::attach() {

    if (attached_thread_ != std::thread::id()) {
        FB_LOG_ERROR << "Can't attach context '" << name_ << "' to thread '"
            << std::this_thread::get_id() << "' because it is already attached to thread '" 
            << attached_thread_ << "'.";
        throw std::runtime_error("Context '" + name_ + "' is already attached to a thread.");
    }

	glfwMakeContextCurrent(handle_);

    FB_LOG_DEBUG 
        << "Attached context '" << name_ << "' (" << handle_ << ") "
        << " to thread '" << std::this_thread::get_id() << "'.";
    attached_thread_ = std::this_thread::get_id();

    if (has_debug_enabled_ && !debug_logging_is_enabled_) {
        enable_debug_logging();
    }
}

bool fb::gl::Context::is_attached_to_any_thread() const {

    return (attached_thread_ != std::thread::id());
}

bool fb::gl::Context::is_attached_to_this_thread() const {

    return (std::this_thread::get_id() == attached_thread_);
}

void fb::gl::Context::detach() {

    // Detach must only be called by the attached thread.

    if (std::this_thread::get_id() != attached_thread_) {
        FB_LOG_ERROR << "Can't detach context '" << name_ << "' from calling thread (" << std::this_thread::get_id() << "), because it is not attached to id.";
        throw std::runtime_error("Context '" + name_ + "' is not attached to the calling thread.");
    }

    if (has_debug_enabled_ && debug_logging_is_enabled_) {
        disable_debug_logging();
    }

	glfwMakeContextCurrent(NULL);
    FB_LOG_DEBUG 
        << "Detached context '" << name_ << "' (" << handle_ << ") "
        << " from thread '" << std::this_thread::get_id() << "'.";
    attached_thread_ = std::thread::id();
}

void fb::gl::Context::force_detach() {

	glfwMakeContextCurrent(NULL);
}

void fb::gl::Context::enable_debug_logging() {

    if (debug_logging_is_enabled_) {
        FB_LOG_WARNING << "Debug logging is already enabled, can't enable. Doing nothing";
        return;
    }

    // Make sure the API is bound (probably to some other context, which is 
    // not hundred percent correct, but mostly ok)
    Init::require();

    if (ProgramOptions::global().arb_debug_synchronous_mode()) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
    }

    std::vector<DebugSeverity> severities;
    severities.push_back(DebugSeverity::LOW);
    severities.push_back(DebugSeverity::MEDIUM);
    severities.push_back(DebugSeverity::HIGH);

    DebugSeverity min_gl_severity = ProgramOptions::global().min_debug_gl_context_severity();

    auto itr = std::find(std::begin(severities), std::end(severities), min_gl_severity);

    if (itr == std::end(severities)) {
        FB_LOG_CRITICAL << "Invalid min severity '" << min_gl_severity << "'.";
        throw std::invalid_argument("Invalid severity.");
    }

    FB_LOG_DEBUG << "Using '" << min_gl_severity << "' as minimum GL debug severity.";

    // Disable all

    glDebugMessageControlARB(
        GL_DONT_CARE,
        GL_DONT_CARE,
        GL_DONT_CARE,
        0, 
        nullptr, 
        GL_FALSE);

    // enable only those we want

    for (;itr != std::end(severities);++itr) {

        glDebugMessageControlARB(
            GL_DONT_CARE,
            GL_DONT_CARE,
            static_cast<GLenum>(*itr),
            0, 
            nullptr, 
            GL_TRUE);

    }

    glDebugMessageCallbackARB(&Context::gl_arb_debug_callback, this);
    FB_LOG_DEBUG << "Enabled debug output logging for context '" << name_ << "'.";
    if (ProgramOptions::global().arb_debug_synchronous_mode()) {
        FB_LOG_DEBUG << "Logging is synchronous to the GL.";
    } else {
        FB_LOG_DEBUG << "Logging is asynchronous to the GL.";
    }
    // TODO: add glDebugMessageControl parameters, maybe also add source-application
    // -level messages.

    debug_logging_is_enabled_ = true;
}

void fb::gl::Context::disable_debug_logging() {

    if (!debug_logging_is_enabled_) {
        FB_LOG_WARNING << "Debug logging is not enabled, can't disable. Doing nothing";
        return;
    }

    Init::require();
    glDebugMessageCallbackARB(nullptr, nullptr);
    FB_LOG_DEBUG << "Disabled debug output logging for context '" << name_ << "'.";
    debug_logging_is_enabled_ = false;
}

const fb::gl::Context::Handle fb::gl::Context::handle() const {
    return handle_;
}

bool fb::gl::Context::has_debug_enabled() const {
    return has_debug_enabled_;
}

bool fb::gl::Context::debug_logging_is_enabled() const {
    return debug_logging_is_enabled_;
}

void fb::gl::Context::gl_arb_debug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const GLvoid *userParam)
{
    
    logging::Severity log_severity = get_severity_from_gl_severity(severity);
    const Context* thiss = static_cast<const Context*>(userParam);
    FB_LOG(log_severity) 
        << "OpenGL debug for ctx '" << thiss->name_ << "' :"
        << "source = '" << get_string_from_gl_debug_source(source) << "', "
        << "type = '" << get_string_from_gl_debug_type(type) << "', "
        << "id = '" << id << "', "
        << "severity = '" << get_string_from_gl_severity(severity) << "', "
        << "message = '" << std::string(message, static_cast<size_t>(length)) << "'.";

}

std::string fb::gl::Context::get_string_from_gl_debug_source(GLenum source) {

    std::string result;

    switch (source) {

    case GL_DEBUG_SOURCE_API_ARB:
        result = "GL API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
        result = "Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
        result = "Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
        result = "Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:
        result = "Application";
        break;
    case GL_DEBUG_SOURCE_OTHER_ARB:
        result = "Other";
        break;
    default:
        result = "<unknown?>";
        break;
    }

    return result;
}

std::string fb::gl::Context::get_string_from_gl_debug_type(GLenum type) {
     std::string result;

     switch (type) {

     case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB :
         result = "Deprecated Behavior";
         break;
     case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB  :
         result = "Undefined Behavior";
         break;
     case GL_DEBUG_TYPE_PORTABILITY_ARB         :
         result = "Portability";
         break;
     case GL_DEBUG_TYPE_PERFORMANCE_ARB         :
         result = "Performance";
         break;
     case GL_DEBUG_TYPE_OTHER_ARB               :
         result = "Other";
         break;
     default:
         result = "<unknown?>";
         break;
     }

    return result;
}

std::string fb::gl::Context::get_string_from_gl_severity(GLenum severity) {
    std::string result;

    switch(severity) {

    case GL_DEBUG_SEVERITY_HIGH_ARB   :
        result = "High";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB :
        result = "Medium";
        break;
    case GL_DEBUG_SEVERITY_LOW_ARB    :
        result = "Low";
        break;
    default:
        result = "<unknown?>";
        break;
    }

    return result;
}

fb::logging::Severity fb::gl::Context::get_severity_from_gl_severity(GLenum severity) {

    fb::logging::Severity result = fb::logging::Severity::info;

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH_ARB   :
        result = fb::logging::Severity::error;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB :
        result = fb::logging::Severity::warning;
        break;
    case GL_DEBUG_SEVERITY_LOW_ARB    :
        result = fb::logging::Severity::debug;
        break;
    default:
        result = fb::logging::Severity::info;
        break;
    }

    return result;

}

std::ostream& fb::gl::operator<< (std::ostream& out, const Context::DebugSeverity& v) {

    switch (v) {
    case Context::DebugSeverity::LOW:
        out << "LOW";
        break;
    case Context::DebugSeverity::MEDIUM:
        out << "MEDIUM";
        break;
    case Context::DebugSeverity::HIGH:
        out << "HIGH";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::gl::operator>>(std::istream& in, Context::DebugSeverity& v) {

    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "LOW")
        v = fb::gl::Context::DebugSeverity::LOW;
    else if (token == "MEDIUM")
        v = fb::gl::Context::DebugSeverity::MEDIUM;
    else if (token == "HIGH")
        v = fb::gl::Context::DebugSeverity::HIGH;
    else {
        in.setstate(std::ios::failbit);
    }

    return in;

}
