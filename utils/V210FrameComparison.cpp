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

#include "FrameBender.h"
#include "TestUtils.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bf = boost::filesystem;
namespace fb = toa::frame_bender;

fb::Frame read_from_file(const bf::path& path) {

    fb::ImageFormat input_sequence_format(
		1920,
		1080,
        fb::ImageFormat::Transfer::BT_709, 
        fb::ImageFormat::Chromaticity::BT_709,
		fb::ImageFormat::PixelFormat::YUV_10BIT_V210,
		fb::ImageFormat::Origin::UPPER_LEFT);

    fb::Frame f(
        input_sequence_format,
        fb::Time(0, 1),
        false
        );

    auto size = bf::file_size(path);
    if (f.image_data_size() != size) {
        throw std::invalid_argument("Unexpected byte size: Got '" + std::to_string(size) + "', but need '" + std::to_string(f.image_data_size()) + "'.");
    }

    bf::ifstream frame_reader(
        path,
        std::ios::in | std::ios::binary);

    if (!frame_reader.good())
        throw std::runtime_error("Can't read.");

    frame_reader.seekg(0, std::ios::beg);

    frame_reader.read(
        reinterpret_cast<char*>(f.image_data()), 
        f.image_data_size());

    return f;
}

int main(int argc, const char* argv[]) {

    try {

        if (argc < 3) {
            std::cout << "Arguments required: [reference] [candidate].\n";
            return EXIT_SUCCESS;
        }

        fb::ProgramOptions::initialize_global(argc, argv);

        std::cout << "Assuming a resolution of 1080p.\n";

        std::string arg_reference_str = std::string(argv[1]);
        std::string arg_candidate_str = std::string(argv[2]);

        bf::path reference_path(arg_reference_str);
        bf::path candidate_path(arg_candidate_str);

        if (!bf::is_regular_file(reference_path) || !bf::exists(reference_path)) {
            throw std::invalid_argument("Invalid path: '" + arg_reference_str + "'.");
        }

        if (!bf::is_regular_file(candidate_path) || !bf::exists(candidate_path)) {
            throw std::invalid_argument("Invalid path: '" + arg_candidate_str + "'.");
        }

        fb::Frame reference = read_from_file(reference_path);
        fb::Frame candidate = read_from_file(candidate_path);

        std::array<fb::test::ChannelComparisonStatistics, 3> stats;

        // Don't care whether frames are within tolerance, just need the stats
        fb::test::compare_v210_frames(reference, candidate, 1000, &stats);

        std::cout << "Comparison statistics: \n";
        for (auto& el : stats) {
            std::cout << el << "\n";
        }

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}