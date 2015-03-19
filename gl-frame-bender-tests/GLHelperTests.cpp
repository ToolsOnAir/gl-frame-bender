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
#include "PrecompileTest.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <iostream>
#include <thread>
#include <future>

#include "FrameBender.h"
#include "Utils.h"

#include "Init.h"
#include "Window.h"
#include "Logging.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace fb = toa::frame_bender;

using namespace fb;

BOOST_AUTO_TEST_SUITE(GLHelperTests)

BOOST_AUTO_TEST_CASE(InitTest) {

    // This is the first call, ensure that it won't throw
    BOOST_REQUIRE_NO_THROW(gl::Init::require());

    // Second call, now everything should be ok (no initialization necessary anymore)
    BOOST_REQUIRE_NO_THROW(gl::Init::require());

}

BOOST_AUTO_TEST_CASE(WindowCreationTest) {

    // Due to the limitations of the underlying GLFW implementation, we can
    // really only create one window at the time.
    gl::Window::create_window(640, 480, true);

    BOOST_REQUIRE((bool)gl::Window::get());
    BOOST_CHECK_EQUAL(gl::Window::get()->width, 640);
    BOOST_CHECK_EQUAL(gl::Window::get()->height, 480);
    BOOST_CHECK_EQUAL(gl::Window::get()->use_debug_context, true);

    // This should close it and create a new window
    gl::Window::create_window(320, 240, false);

    BOOST_REQUIRE((bool)gl::Window::get());
    BOOST_CHECK_EQUAL(gl::Window::get()->width, 320);
    BOOST_CHECK_EQUAL(gl::Window::get()->height, 240);
    BOOST_CHECK_EQUAL(gl::Window::get()->use_debug_context, false);

}

BOOST_AUTO_TEST_CASE(ContextCreationTest) {

    gl::Init::require();

    gl::Window::create_window(640, 480, false);
    BOOST_REQUIRE(!gl::Window::get()->context()->has_debug_enabled());

    gl::Window::create_window(640, 480, true);
    BOOST_REQUIRE(gl::Window::get()->context()->has_debug_enabled());

    // Must be attached after creation
    BOOST_REQUIRE_EQUAL(gl::Window::get()->context()->handle(), glfwGetCurrentContext());

    std::unique_ptr<gl::Context> share_context;
    BOOST_REQUIRE_NO_THROW(share_context = gl::Window::get()->context()->create_shared("test"));
    BOOST_REQUIRE(share_context->has_debug_enabled());

    share_context->attach();
    BOOST_REQUIRE(share_context->is_attached_to_this_thread());
	BOOST_REQUIRE_EQUAL(share_context->handle(), glfwGetCurrentContext());
    share_context->detach();
    BOOST_REQUIRE(!share_context->is_attached_to_this_thread());
	BOOST_REQUIRE_NE(share_context->handle(), glfwGetCurrentContext());

}

BOOST_AUTO_TEST_CASE(ContextAttachDetachMultiThreaded) {

    // Checks the invariants of the Context class behavior

    gl::Init::require();

    gl::Window::create_window(10, 10, true);
    std::unique_ptr<gl::Context> share_context;
    BOOST_REQUIRE_NO_THROW(share_context = gl::Window::get()->context()->create_shared("test_from_threads"));

    // Attach in current thread.
    share_context->attach();

    BOOST_REQUIRE(share_context->is_attached_to_this_thread());
    BOOST_REQUIRE(share_context->is_attached_to_any_thread());

    // Attaching from another thread should fail
    auto try_from_other_thread = std::async(std::launch::async, 
        [&]() -> bool {

            try {
                share_context->attach();
                return true;
            } catch (const std::exception&) {
                return false;
            }

        });

    bool attach_from_other_did_succeed = try_from_other_thread.get();
    BOOST_CHECK(!attach_from_other_did_succeed);

    // Also, detaching from another thread should fail
    auto try_detach_from_other_thread = std::async(std::launch::async, 
        [&]() -> bool {

            try {
                share_context->detach();
                return true;
            } catch (const std::exception&) {
                return false;
            }

        });

    bool detach_from_other_did_succeed = try_detach_from_other_thread.get();
    BOOST_CHECK(!detach_from_other_did_succeed);

    // Now detach
    share_context->detach();
    BOOST_REQUIRE(!share_context->is_attached_to_any_thread());
    BOOST_REQUIRE(!share_context->is_attached_to_this_thread());

    auto try_again_from_other_thread = std::async(std::launch::async, 
        [&]() -> bool {

            try {

                share_context->attach();
                return true;
            } catch (const std::exception&) {
                return false;
            }

        });

    attach_from_other_did_succeed = try_again_from_other_thread.get();
    BOOST_CHECK(attach_from_other_did_succeed);
    
    BOOST_REQUIRE(share_context->is_attached_to_any_thread());
    BOOST_REQUIRE(!share_context->is_attached_to_this_thread());

}

BOOST_AUTO_TEST_CASE(ARBDebugOutputTest) {

    BOOST_REQUIRE(ProgramOptions::global().use_debug_gl_context());

    gl::Init::require();

    gl::Window::create_window(640, 480, true);

    BOOST_REQUIRE(gl::Window::get()->context()->has_debug_enabled());

    std::string message = "Boost Unit Test Message";

    // TODO: to better test, you should add a custom sink and check against
    // its output.
    glDebugMessageInsertARB(
        GL_DEBUG_SOURCE_APPLICATION_ARB, 
        GL_DEBUG_TYPE_OTHER_ARB, 
        1, 
        GL_DEBUG_SEVERITY_HIGH_ARB, 
        (GLsizei)message.size(), 
        message.c_str());

    gl::Window::get()->context()->detach();
}

BOOST_AUTO_TEST_SUITE_END()
