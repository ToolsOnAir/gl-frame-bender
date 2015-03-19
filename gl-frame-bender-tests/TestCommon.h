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
#ifndef TOA_FRAME_BENDER_TEST_COMMON_H
#define TOA_FRAME_BENDER_TEST_COMMON_H

#include "ProgramOptions.h"
#include "ImageFormat.h"
#include <map>

#ifndef _MSC_VER
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace toa {
    namespace frame_bender {
        namespace test {
            // Note: must be called only with valid OGL context.
            static ImageFormat horse_seq_v210_1080p_format() {

                return ImageFormat(
                    1920, 
                    1080, 
                    ImageFormat::Transfer::BT_709,
                    ImageFormat::Chromaticity::BT_709,
                    ImageFormat::PixelFormat::YUV_10BIT_V210,
                    ImageFormat::Origin::UPPER_LEFT);

            }

            static ImageFormat ducks_seq_rgb24_1080p_format() {

                return ImageFormat(
                    1920, 
                    1080, 
                    ImageFormat::Transfer::BT_709,
                    ImageFormat::Chromaticity::BT_709,
                    ImageFormat::PixelFormat::RGB_8BIT,
                    ImageFormat::Origin::LOWER_LEFT);

            }

            ProgramOptions get_global_options_with_overrides(const std::multimap<std::string, std::string>& opt_overrides);

        }
    }
}

struct TestGlobalFixture {

    static const std::string& config_file() {
        static const std::string f = "./FrameBenderTest.cfg";
        return f;
    }

    static const char** options_args() {
        static const char * args[] = {
            "./dummy.exe", 
            "-c", 
            config_file().c_str(),
            nullptr};
        return args;
    }

    TestGlobalFixture()   {
        // Uncomment when VS debugger needs to be attached.
        // std::cin.get();
        toa::frame_bender::ProgramOptions::initialize_global(3, options_args());
        toa::frame_bender::logging::intialize_global_logger(
            toa::frame_bender::ProgramOptions::global().log_file(),
            toa::frame_bender::ProgramOptions::global().log_to_stdout(),
            toa::frame_bender::ProgramOptions::global().min_logging_severity());
    }
    ~TestGlobalFixture()  {}
};

#ifndef _MSC_VER
#pragma clang diagnostic pop
#endif

#endif //TOA_FRAME_BENDER_TEST_COMMON_H
