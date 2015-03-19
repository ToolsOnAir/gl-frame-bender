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
#include <iostream>
#include <sstream>
#include <iomanip>

#include "boost/filesystem.hpp"

#include "FrameBender.h"
#include "Init.h"
#include "Window.h"
#include "DemoStreamRenderer.h"

namespace fb = toa::frame_bender;
namespace bf = boost::filesystem;

using namespace fb;

int main(int argc, const char* argv[]) {

    try {

        ProgramOptions::initialize_global(argc, argv);

        const ProgramOptions& opts = ProgramOptions::global();

        if (opts.show_help()) {
            std::cout << opts.help() << std::endl;
            return EXIT_SUCCESS;
        }

        logging::intialize_global_logger(
            opts.log_file(), 
            opts.log_to_stdout(), 
            opts.min_logging_severity());

        if (!opts.config_output_file().empty()) {
            opts.write_config_to_file(opts.config_output_file());
        }

        gl::Init::require();

        // BEGIN RENDER SCENARIO WITH PERFORMANCE METRICS

        gl::Window::get()->context()->attach();

        ImageFormat input_sequence_format(
            static_cast<uint32_t>(ProgramOptions::global().input_sequence_width()),
            static_cast<uint32_t>(ProgramOptions::global().input_sequence_height()),
            ImageFormat::Transfer::BT_709, 
            ImageFormat::Chromaticity::BT_709,
            ProgramOptions::global().input_sequence_pixel_format(),
            ProgramOptions::global().input_sequence_origin()
            );

        ImageFormat::PixelFormat user_render_format = ProgramOptions::global().render_pixel_format();

        FB_LOG_INFO 
            << "User requested pixel format '" 
            << user_render_format 
            << "' for rendering.";

        ImageFormat render_format(
            input_sequence_format.width(),
            input_sequence_format.height(),
            ProgramOptions::global().render_transfer(),
            ImageFormat::Chromaticity::BT_709,
            user_render_format,
            ImageFormat::Origin::LOWER_LEFT
            );

        gl::Window::get()->context()->detach();

        FB_LOG_INFO << "Input sequence format is set to [" << input_sequence_format << "].";
        FB_LOG_INFO << "Assuming BT_709 for chromaticity by default.";

        auto input_sequence = std::make_shared<PrefetchedImageSequence>(
            ProgramOptions::global().input_sequence_name(),
            ProgramOptions::global().input_sequence_pattern(),
            input_sequence_format,
            ProgramOptions::global().input_sequence_frame_duration(),
            ProgramOptions::global().input_sequence_loop_count()
            );

        {

            // Note that we must initialize this before creting the dispatch
            // object, as we need exclusive access to the render (main context)
            gl::Window::get()->context()->attach();
            std::unique_ptr<StreamRenderer> renderer;

            // TODO: for multiple renderer implementations, pass a renderer
            // id as a user option. This would allow more than only one 
            // renderer implementation for demoing.

            if (ProgramOptions::global().force_passthrough_renderer()) {
                renderer = utils::make_unique<PassThroughRenderer>();
                FB_LOG_INFO << "Forcing pass-through rendering.";
            } else {
                renderer = utils::make_unique<DemoStreamRenderer>();
            }

            gl::Window::get()->context()->detach();

            // Get the optimization flags from the user option
            
            StreamDispatch::FlagContainer flags = ProgramOptions::global().optimization_flags();

			{

				// Initialize Stream Dispatch
				StreamDispatch dispatch(
					"FrameBenderDispatch",
					gl::Window::get()->context().get(),
					input_sequence_format,
					render_format,
					ProgramOptions::global().enable_output_stages(),
					ProgramOptions::global().enable_input_stages(),
					ProgramOptions::global().enable_render_stages(),
					flags
					);

				std::mutex mtx;
				bool composition_is_finished = false;
				std::condition_variable is_end_of_sequence;

				StreamComposition::OutputCallback output_callback;

				size_t frame_output_counter = 0;
				bf::path output_path(ProgramOptions::global().write_output_folder());

				if (ProgramOptions::global().enable_write_output()) {

					// Make sure output folder exists

					if (bf::exists(output_path) && !bf::is_directory(output_path)) {
						FB_LOG_ERROR << "Directory '" << output_path.string() << "' already exists, but is not a directory.";
						throw std::invalid_argument("Invalid output directory.");
					}

					if (!bf::exists(output_path)) {
						FB_LOG_INFO << "Directory '" << output_path.string() << "' did not exist, creating directory.";
						bf::create_directories(output_path);
					}

					output_callback = [&](const Frame& f){

						std::ostringstream oss;
						oss << std::setfill('0') << std::setw(8) << frame_output_counter << ".raw";
						bf::path file_path(oss.str());

						bf::path write_path = output_path / file_path;

						f.dump_to_file(
							write_path.string(),
							ProgramOptions::global().write_output_swap_endianness(),
							ProgramOptions::global().write_output_swap_endianness_word_size());

						frame_output_counter++;
					};
				}

				auto composition_id = dispatch.create_composition(
					"V210SingleStreamPassThrough",
					std::vector<std::shared_ptr<StreamSource>>(1, input_sequence),
					std::move(renderer),
					std::move(output_callback)
					);

				// Let's wait a little so all threads are really up for running
				std::this_thread::sleep_for(std::chrono::seconds(1));

				// Start composition now.
				dispatch.start_composition(
					composition_id,
					[&](){
						std::lock_guard<std::mutex> guard(mtx);
						composition_is_finished = true;
						is_end_of_sequence.notify_one();
				});

				// TODO: maybe rewrite when there is a more generic way of waiting for
				// a change in transport state.

				if (ProgramOptions::global().enable_window_user_input()) {

					// Poll events
					while (!composition_is_finished) {

						auto key = gl::Window::get()->poll_event();
						if (key == gl::Window::PressedKey::escape) {

							FB_LOG_DEBUG << "Encountered escape, canceling dispatch.";
							dispatch.stop_composition(composition_id);

						}

						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}

				} else {

					// Wait until we are signaled to stop
					std::unique_lock<std::mutex> lock(mtx);
					is_end_of_sequence.wait(lock, [&]{return composition_is_finished;});

				}

				if (ProgramOptions::global().enable_write_output()) {

					FB_LOG_INFO 
						<< "Wrote " << frame_output_counter 
						<< " frames into '" << output_path.string() << "'.";

				}

				if (ProgramOptions::global().sample_stages() && 
					!ProgramOptions::global().trace_output_file().empty())
				{

					// TODO: add configuration name in here.
					dispatch.write_trace_file(
						ProgramOptions::global().trace_output_file(), 
						"SomeConfiguration"
						);

				}

			}

			// Destroy the demo stream render (with context attached)
			gl::Window::get()->context()->attach();

			renderer.reset();

			gl::Window::get()->context()->detach();

        } 


        // TODO: does it make sense to have some sort of golden test here in 
        // the main program.

        // TODO: add an output option in here?

        // END RENDER SCENARIO WITH PERFORMANCE METRICS

        std::cout << "Done. Good bye.\n";

    } catch (const fb::InvalidInput& e) {

        std::cout << "Invalid input : " << e.what() << std::endl;
        return EXIT_FAILURE;

    } catch (const std::exception& e) {

        std::cout << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}