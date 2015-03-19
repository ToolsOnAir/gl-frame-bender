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
#include "CopyMappedPBOToHostStage.h"

#include "ProgramOptions.h"

#include "StreamComposition.h"
#include "StreamSource.h"
#include "Frame.h"

#ifdef FB_USE_INTEL_IPP_FOR_HOST_COPY
#include <ipp.h>
#endif

namespace fb = toa::frame_bender;

fb::StageCommand fb::CopyMappedPBOToHostStage::perform(
    TokenGL& in_token, 
    TokenFrame& out_token)
{

    if (in_token.format != out_token.frame.image_format()) {
        FB_LOG_CRITICAL << "Image format mismatch.";
        return StageCommand::STOP_EXECUTION;
    }

#ifdef DEBUG_PIPE_FLOW

    FB_LOG_DEBUG << stage_.name() << ": Copying mapped memory of PBO '" << in_token.id << "' to frame '" << out_token.frame.time() << "' (bypass='" << std::boolalpha << dbg_bypass_copy_ << "').";

#endif

    if (!dbg_bypass_copy_) {
#ifndef FB_USE_INTEL_IPP_FOR_HOST_COPY
        memcpy(
            out_token.frame.image_data(), 
            in_token.buffer, 
            out_token.frame.image_format().image_byte_size());
#else

        const ImageFormat& fmt = out_token.frame.image_format();

        int width = fmt.gl_native_format().tex_width;
        int height = fmt.gl_native_format().tex_height;

        IppiSize roi_size = {
            width * fmt.gl_native_format().bytes_per_component,
            height
        };

        IppStatus err = ippiCopyManaged_8u_C1R(
            (const Ipp8u*)in_token.buffer,
            fmt.gl_native_format().row_length * fmt.gl_native_format().bytes_per_component,
            out_token.frame.image_data(),
            fmt.gl_native_format().row_length * fmt.gl_native_format().bytes_per_component,
            roi_size,
            IPP_NONTEMPORAL_STORE);

        FB_ASSERT(err == ippStsNoErr);

#endif
    }

    out_token.composition = in_token.composition;
    out_token.frame.set_time(in_token.time_stamp);
    out_token.frame.set_marks_end_of_sequence(false);
    // TODO: how to set end-of-stream in here?

    // Note that we should send the PBO id back to input upstream, but
    // this is something that the Stage generic implementation already
    // gives us for free...

    return StageCommand::NO_CHANGE;

}

fb::CopyMappedPBOToHostStage::~CopyMappedPBOToHostStage() 
{

    if (ProgramOptions::global().sample_stages()) {
        auto stats = sampler_.collect_statistics();
        for (const auto& stat : stats) {
            FB_LOG_INFO << stage_.name() << ": " << stat;
        }
    }

}