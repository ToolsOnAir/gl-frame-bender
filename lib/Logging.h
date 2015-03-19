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
#ifndef TOA_FRAME_BENDER_LOGGING_H
#define TOA_FRAME_BENDER_LOGGING_H

// Reduce noise of some non-critical warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#include <boost/log/common.hpp>

#include <thread>
#include <string>
#include <iosfwd>

#include "Utils.h"

// Define macros to use the global logger.

// NOTE: there is a problem with Microsoft's scheduling system, that will cause
// an appearant memory leak for thread-local-storage that boost log uses to 
// cleanup thread-specific resources: 
// This bug is calimed to be fixed, and possible future updates on boost log
// and boost itself (boost::thread) might fix this. In the meantime we just
// ignore the memory leak. Note that this is also not a growing leak, it 
// just leaks at exit, which is ok.
// http://connectppe.microsoft.com/VisualStudio/feedback/details/761209/boost-thread-specific-ptr-leaks-on-vs2012-when-using-std-async
// http://permalink.gmane.org/gmane.comp.lib.boost.devel/233951

namespace toa {

    namespace frame_bender {

        namespace logging {

            enum class Severity : size_t {
                debug       = 0,
                info        = 1,
                warning     = 2,
                error       = 3,
                critical    = 4 
            };

            std::ostream& operator<< (std::ostream& out, const Severity& v);
            std::istream& operator>>(std::istream& in, Severity& v); 

            /**
             * Initializes the global default logger. Initialization needs
             * to be done once. This method is thread-safe.
             * @param log_file This is where log files will be written to
             * Set to empty string if file-based logging should be disabled. Each
             * severity will get its own file.
             * @param log_to_std_out Set to true to enable forwarding log message to
             * stdout.
             * @param verbosity_level Set the smallest severity that should be logged
             * for the stdout output. Severity::DEBUG being the smallest.
             */            
            void intialize_global_logger(
                const std::string& log_file,
                bool log_to_stdout,
                Severity verbosity_level);

        }

    }

}

#ifdef _WIN32
#define FB_FILE_SEPARATOR '\\'
#else
#define FB_FILE_SEPARATOR '/'
#endif

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(fb_global_logger, boost::log::sources::severity_logger_mt<toa::frame_bender::logging::Severity>)
    
#define FB_LOG(sev) \
    BOOST_LOG_SEV(fb_global_logger::get(), sev) << "(" << toa::frame_bender::utils::strip_path(__FILE__, FB_FILE_SEPARATOR) << ":" << __LINE__ << ") "

#define FB_LOG_DEBUG FB_LOG(toa::frame_bender::logging::Severity::debug)
#define FB_LOG_INFO FB_LOG(toa::frame_bender::logging::Severity::info)
#define FB_LOG_WARNING FB_LOG(toa::frame_bender::logging::Severity::warning)
#define FB_LOG_ERROR FB_LOG(toa::frame_bender::logging::Severity::error)
#define FB_LOG_CRITICAL FB_LOG(toa::frame_bender::logging::Severity::critical)

#define FB_ASSERT(cond) \
        assert(cond);

#define FB_ASSERT_MESSAGE(cond, msg) \
        assert(cond && msg);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // TOA_FRAME_BENDER_LOGGING_H

