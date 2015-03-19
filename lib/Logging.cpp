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
#include <mutex>
#include <atomic>
#include <limits>
#include <thread>

#include "Logging.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/support/date_time.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace fb = toa::frame_bender;
namespace bl = boost::log;
namespace bf = boost::filesystem;

void fb::logging::intialize_global_logger(
    const std::string& log_file,
    bool log_to_stdout,
    Severity verbosity_level)
{

    // Make sure for two competing threads, only will intialize our logger.
    static std::atomic<bool> is_initialized{false};
    static std::recursive_mutex mutex;
    std::lock_guard<std::recursive_mutex> guard(mutex);

    if (!is_initialized.load()) {

        // auto severity_filter = blf::attr< Severity >("Severity", std::nothrow) >= verbosity_level;
		auto formatter = bl::expressions::format("%1% @ %2% [%3%] [%4%] - %5%")
			% bl::expressions::attr< unsigned int >("LineID")
			% bl::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			% bl::expressions::attr< bl::attributes::current_thread_id::value_type >("ThreadID")
			% bl::expressions::attr< Severity >("Severity")
			% bl::expressions::smessage;

		auto severity_filter = bl::expressions::attr< Severity >("Severity") >= verbosity_level;

        if (!log_file.empty()) {

			auto flog = bl::add_file_log
				(
				bl::keywords::file_name = log_file
				);
			flog->set_formatter(formatter);
        }

        // Initialize the console logger
        if (log_to_stdout) {

			auto clog = bl::add_console_log();
			clog->set_formatter(formatter);
        }
   
		bl::add_common_attributes();

		bl::core::get()->set_filter(severity_filter);

        is_initialized = true;

    } else {
        FB_LOG_WARNING << "Logger is already initialized.";
    }

}

std::ostream& fb::logging::operator<< (std::ostream& out, const Severity& v) {

    switch (v) {
    case Severity::debug:
        out << "DEBUG";
        break;
    case Severity::info:
        out << "INFO";
        break;
    case Severity::warning:
        out << "WARNING";
        break;
    case Severity::error:
        out << "ERROR";
        break;
    case Severity::critical:
        out << "CRITICAL";
        break;
    default:
        out << "<unknown>";
        break;
    }

    return out;

}

std::istream& fb::logging::operator>>(std::istream& in, Severity& v) {

    std::string token;
    in >> token;

    std::transform(token.begin(), token.end(),token.begin(), ::toupper);

    if (token == "DEBUG")
        v = Severity::debug;
    else if (token == "INFO")
        v = Severity::info;
    else if (token == "WARNING")
        v = Severity::warning;
    else if (token == "ERROR")
        v = Severity::error;
    else if (token == "CRITICAL")
        v = Severity::critical;
    else {
        in.setstate(std::ios::failbit);
    }

    return in;

}
