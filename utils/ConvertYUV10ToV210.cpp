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
#include <regex>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bf = boost::filesystem;

int main(int argc, const char* argv[]) {

    try {
        if (argc < 2) {
            std::cout << "Argument required: regex pattern for all YUV10 files of \n";
            std::cout << "current folder.";
            return EXIT_SUCCESS;
        }

        // First argument is pattern to use for YUV10 files.
        // Everything happens in CWD (input and output as well)
        // Files will be overwritten by default

        // iterator of the file system
        auto dir_start = bf::directory_iterator(".");

        if (dir_start == bf::directory_iterator()) {
            std::cout << "Directory is empty, nothing to process.\n";
            return EXIT_SUCCESS;
        }

        std::vector<bf::directory_entry> filtered_files;

        std::string regex_pattern_str(argv[1]);
        std::regex frame_pattern_regex(regex_pattern_str);

        std::cout << "Using regex pattern '" << regex_pattern_str << "'.\n";

        std::copy_if(
            dir_start, 
            bf::directory_iterator(), 
            std::back_inserter(filtered_files),
            [&](const bf::directory_entry& entry) {
                
                bool does_match = std::regex_match(
                    entry.path().filename().string(), 
                    frame_pattern_regex);

                return does_match;
        });

        if (filtered_files.empty()) {
            std::cout 
                << "No files matched the pattern '" 
                << regex_pattern_str << "'. Nothing to process.\n";
            return EXIT_SUCCESS;
        }

        std::sort(std::begin(filtered_files), std::end(filtered_files));

        std::cout << "First frame of selection : '" << filtered_files.front() << "'.\n";
        std::cout << "Last frame of selection: '" << filtered_files.back() << "'.\n";

        for (const auto& frame_dir_entry : filtered_files) {

            bf::ifstream frame_reader(
                frame_dir_entry.path(),
                std::ios::in | std::ios::binary);

            bf::path out_path = frame_dir_entry.path();
            out_path.replace_extension(".v210");

            std::cout << "Writing '" << out_path << "'...";

            bf::ofstream frame_writer(
                out_path,
                std::ios::out | std::ios::binary);
            frame_writer.imbue(std::locale::classic());

            if (!frame_reader.good())
                throw std::runtime_error("Reading failed.");

            if (!frame_writer.good())
                throw std::runtime_error("Writing failed.");

            frame_reader.seekg(0, std::ios::beg);

            // Get four bytes, representing three components
            while (frame_reader.peek() != bf::ifstream::traits_type::eof()) {

                // Need to convert from YUV10 -> V210
                // The following shows the layout in increasing
                // memory addresses
                // Sources: 
                // https://developer.apple.com/quicktime/icefloe/dispatch019.html#v210
                // and
                // http://www.dvs.de/fileadmin/downloads/products/videosystems/clipster/extraweb/support/documentation/archive/software/clipster_edit_ug_v2_4.pdf
                // and
                // http://www.tribler.org/trac/wiki/FfmpegYuv

                // |               YUV10               |
                // | BYTE_0 | BYTE_1 | BYTE_2 | BYTE_3 |
                // |98765432|10987654|32109876|543210XX|
                // |    Cb    |    Y'    |    Cr    |XX|
                //
                // |               V210                |
                // | BYTE_0 | BYTE_1 | BYTE_2 | BYTE_3 |
                // |76543210|54321098|32109876|XX987654|
                // |   Cb   | Y'  |Cb| Cr|  Y'|XX| Cr  |

                int32_t a = frame_reader.get();
                int32_t b = frame_reader.get();
                int32_t c = frame_reader.get();
                int32_t d = frame_reader.get();

                int32_t C_0 = (a << 2) | (b >> 6);
                int32_t C_1 = ((b & 0x3F) << 4) | (c >> 4);
                int32_t C_2 = ((c & 0x0F) << 6) | (d >> 2);

                char k = C_0;
                char l = C_0 >> 8 | C_1 << 2;
                char m = C_1 >> 6 | C_2 << 4;
                char n = C_2 >> 4;

                frame_writer.put(k);
                frame_writer.put(l);
                frame_writer.put(m);
                frame_writer.put(n);

            }

            std::cout << "done!\n";
        }

        std::cout << "Done. Good bye!\n";
    } catch (const std::exception& e) {
        std::cout << "Caugth exception: '" << e.what() << "'.\n";
    }
    return EXIT_SUCCESS;
}