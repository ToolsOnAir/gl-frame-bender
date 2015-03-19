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
#include "TestCommon.h"
#include <stdexcept>

namespace fb = toa::frame_bender;

fb::ProgramOptions fb::test::get_global_options_with_overrides(
    const std::multimap<std::string, std::string>& opt_overrides)
{

    std::vector<std::string> m;
    // Add the "-" for cmd-line override
    for (const auto& el : opt_overrides) {
        m.push_back("--" + el.first + "=" + el.second);
    }

    std::vector<const char*> args;
    size_t sanity_limit = 1000;
    size_t cnt = 0;
    for (; cnt<sanity_limit; ++cnt) {
        if (TestGlobalFixture::options_args()[cnt] == nullptr)
            break;
    }

    if (cnt == sanity_limit) {
        throw std::logic_error("Broken options_args, couldn't find nullptr end.");
    }

    // Copy the global default options
    std::copy_n(&TestGlobalFixture::options_args()[0], cnt, std::back_inserter(args));

    // Add our overrides
    for (const auto& el : m) {
        args.push_back(el.c_str());
    }

    // Add null-terminator
    args.push_back(nullptr);

    return fb::ProgramOptions(static_cast<int>(args.size()-1), args.data());

}