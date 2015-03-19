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
#ifndef TOA_FRAME_BENDER_TEST_UTILS_H
#define TOA_FRAME_BENDER_TEST_UTILS_H

#include <array>
#include <limits>

#include "Frame.h"
#include "Logging.h"

#ifndef _MSC_VER
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace toa {

    namespace frame_bender {

        namespace test {

            template <typename T>
            bool is_close(T a, T b, T tolerance) {

                T left = b > std::numeric_limits<T>::min() + tolerance ? b - tolerance : std::numeric_limits<T>::min();
                T right = b < (std::numeric_limits<T>::max() - tolerance) ? b + tolerance : std::numeric_limits<T>::max();

                return (left <= a) && (a <= right);
            }

            inline static bool compare_rgb24_frames(const Frame& left, const Frame& right) {

                ImageFormat test_format(
                    1920, 
                    1080, 
                    ImageFormat::Transfer::BT_709,
                    ImageFormat::Chromaticity::BT_709,
                    ImageFormat::PixelFormat::RGB_8BIT,
                    ImageFormat::Origin::LOWER_LEFT);

                if (left.image_format() != test_format || 
                    right.image_format() != test_format)
                    throw std::runtime_error("Comparison requires rgb24 raw format.");

                if (!left.is_valid() || !right.is_valid()) {
                    throw std::runtime_error("Comparison requires both frames to be valid.");
                }

                if (left.image_data_size() != right.image_data_size()) {
                    throw std::runtime_error(
                        "Image size doesn't match: left='" + 
                        std::to_string(left.image_data_size()) + 
                        "', right='" + std::to_string(right.image_data_size()) +
                        "'.");
                }

                const uint8_t* data_left = left.image_data();
                const uint8_t* data_right = right.image_data();

                bool data_are_close = true;
                // TODO: make multithreaded using async to speed things up.
                for (size_t i_h = 0; i_h < test_format.height() && data_are_close; ++i_h) {
                    for (size_t i_w = 0; i_w < test_format.width() && data_are_close; ++i_w) {

                        size_t group_addr = (test_format.width() * i_h + i_w)*3;
                        FB_ASSERT(group_addr < left.image_data_size());
                        uint8_t r_left = data_left[group_addr];
                        uint8_t g_left = data_left[group_addr+1];
                        uint8_t b_left = data_left[group_addr+2];
                        uint8_t r_right = data_right[group_addr];
                        uint8_t g_right = data_right[group_addr+1];
                        uint8_t b_right = data_right[group_addr+2];

                        uint8_t tolerance = 10;

                        data_are_close = data_are_close && is_close(r_left, r_right, tolerance);
                        data_are_close = data_are_close && is_close(g_left, g_right, tolerance);
                        data_are_close = data_are_close && is_close(b_left, b_right, tolerance);

                        if (!data_are_close) {

                            FB_LOG_ERROR 
                                << "Pixel diff for coordinates "
                                << "(" << i_w << "/" << i_h << ")"
                                << "exceeded tolerance: "
                                << "left: [" 
                                << static_cast<int>(r_left) << "/" 
                                << static_cast<int>(g_left) << "/" 
                                << static_cast<int>(b_left) << "], "
                                << "right: [" 
                                << static_cast<int>(r_right) << "/" 
                                << static_cast<int>(g_right) << "/" 
                                << static_cast<int>(b_right) << "], "
                                << "with tolerance='" 
                                << static_cast<int>(tolerance) << "'.";

                        }

                    }
                }

                return data_are_close;
            }

            // |               V210                |
            // | BYTE_0 | BYTE_1 | BYTE_2 | BYTE_3 |
            // |76543210|54321098|32109876|XX987654|
            // |   Cb   | Y'  |Cb| Cr|  Y'|XX| Cr  |
            inline static std::array<uint16_t, 3> uncompress_v210_group(
                const std::array<uint8_t, 4>& group) 
            {
                std::array<uint16_t, 3> answer;

                uint16_t byte_0 = group[0];
                uint16_t byte_1 = group[1];
                uint16_t byte_2 = group[2];
                uint16_t byte_3 = group[3];

                answer[0] = byte_0 | ((byte_1 & 0x03) << 8);
                answer[1] = (byte_1 >> 2) | ((byte_2 & 0x0F) << 4);
                answer[2] = ((byte_2 & 0xF0) >> 4) | ((byte_3 & 0x3F) << 4);

                return answer;
            }

            inline static std::array<std::array<uint8_t, 4>, 4> get_v210_group_from_addr(
                size_t group_addr,
                const uint8_t* data)
            {
                std::array<std::array<uint8_t, 4>, 4> answer;
                
                std::copy(
                    &data[group_addr], 
                    &data[group_addr+4],
                    answer[0].data());

                std::copy(
                    &data[group_addr+4], 
                    &data[group_addr+8],
                    answer[1].data());

                std::copy(
                    &data[group_addr+8], 
                    &data[group_addr+12],
                    answer[2].data());

                std::copy(
                    &data[group_addr+12], 
                    &data[group_addr+16],
                    answer[3].data());

                return answer;

            }

            struct ChannelComparisonStatistics {
                std::string channel_name;
                double min_error;
                double max_error;
                double mean_squared_error;
                double psnr;

                ChannelComparisonStatistics(
                    std::string channel_name, 
                    double min_error, 
                    double max_error, 
                    double mean_squared_error, 
                    double psnr)
                    :   channel_name(std::move(channel_name)),
                        min_error(min_error),
                        max_error(max_error),
                        mean_squared_error(mean_squared_error),
                        psnr(psnr)
                {}

                ChannelComparisonStatistics()
                    :   channel_name("n/a"),
                        min_error(.0),
                        max_error(.0),
                        mean_squared_error(.0),
                        psnr(.0)
                {}


            };

            static std::ostream& operator<< (std::ostream& out, const ChannelComparisonStatistics& v) {

                out << "Channel '" << v.channel_name << "': \n";
                out << "    " << "max_error: " << v.max_error << ", \n";
                out << "    " << "min_error: " << v.min_error << ", \n";
                out << "    " << "mean_sq_error: " << v.mean_squared_error << ", \n";
                out << "    " << "PSNR: " << v.psnr << ".\n";

                return out;
            }

            inline static bool compare_v210_frames(
                const Frame& reference, 
                const Frame& candidate,
                uint16_t tolerance,
                std::array<ChannelComparisonStatistics, 3>* statistics_out = nullptr) 
            {

                static_assert(
                    std::numeric_limits<double>::has_infinity == true, 
                    "Requiring infinity to be defined for double.");

                const ImageFormat& test_format = reference.image_format();

                if (test_format.pixel_format() != ImageFormat::PixelFormat::YUV_10BIT_V210) {
                    throw std::invalid_argument("V210 format is required.");
                }

                if (reference.image_format() != test_format || 
                    candidate.image_format() != test_format)
                    throw std::runtime_error("Comparison requires V210 raw format.");

                if (!reference.is_valid() || !candidate.is_valid()) {
                    throw std::runtime_error("Comparison requires both frames to be valid.");
                }

                if (reference.image_data_size() != candidate.image_data_size()) {
                    throw std::runtime_error(
                        "Image size doesn't match: reference='" + 
                        std::to_string(reference.image_data_size()) + 
                        "', candidate='" + std::to_string(candidate.image_data_size()) +
                        "'.");
                }

                const uint8_t* data_reference = reference.image_data();
                const uint8_t* data_candidate = candidate.image_data();

                bool data_are_close = true;

                div_t div_result = std::div(test_format.width(), 6);

                FB_ASSERT(div_result.rem == 0);

                size_t num_horiz_groups = div_result.quot;

                // ranges are [a, b]
                std::pair<int16_t, int16_t> chroma(64, 960);
                std::pair<int16_t, int16_t> luma(64, 940);
                std::pair<int16_t, int16_t> valid_range(4, 1019);

                // Range of values, depending on which element in the v210 group
                std::array<std::array<std::pair<int16_t, int16_t>, 3>, 4> ranges;
                ranges[0][0] = chroma;
                ranges[0][1] = luma;
                ranges[0][2] = chroma;
                ranges[1][0] = luma;
                ranges[1][1] = chroma;
                ranges[1][2] = luma;
                ranges[2][0] = chroma;
                ranges[2][1] = luma;
                ranges[2][2] = chroma;
                ranges[3][0] = luma;
                ranges[3][1] = chroma;
                ranges[3][2] = luma;

                std::array<std::array<std::string, 4>, 4> labels;
                labels[0][0] = "Cb_0";
                labels[0][1] = "Y'_0";
                labels[0][2] = "Cr_0";
                labels[1][0] = "Y'_1";
                labels[1][1] = "Cb_2";
                labels[1][2] = "Y'_2";
                labels[2][0] = "Cr_2";
                labels[2][1] = "Y'_3";
                labels[2][2] = "Cb_4";
                labels[3][0] = "Y'_4";
                labels[3][1] = "Cr_4";
                labels[3][2] = "Y'5";

                std::array<int32_t, 3> max_error = {{0, 0, 0}};
                std::array<std::array<int32_t*, 4>, 4> max_error_ptrs;
                max_error_ptrs[0][0] = &max_error[1];
                max_error_ptrs[0][1] = &max_error[0];
                max_error_ptrs[0][2] = &max_error[2];
                max_error_ptrs[1][0] = &max_error[0];
                max_error_ptrs[1][1] = &max_error[1];
                max_error_ptrs[1][2] = &max_error[0];
                max_error_ptrs[2][0] = &max_error[2];
                max_error_ptrs[2][1] = &max_error[0];
                max_error_ptrs[2][2] = &max_error[1];
                max_error_ptrs[3][0] = &max_error[0];
                max_error_ptrs[3][1] = &max_error[2];
                max_error_ptrs[3][2] = &max_error[0];

                std::array<int32_t, 3> min_error;
                min_error.fill(std::numeric_limits<int32_t>::max());
                std::array<std::array<int32_t*, 4>, 4> min_error_ptrs;
                min_error_ptrs[0][0] = &min_error[1];
                min_error_ptrs[0][1] = &min_error[0];
                min_error_ptrs[0][2] = &min_error[2];
                min_error_ptrs[1][0] = &min_error[0];
                min_error_ptrs[1][1] = &min_error[1];
                min_error_ptrs[1][2] = &min_error[0];
                min_error_ptrs[2][0] = &min_error[2];
                min_error_ptrs[2][1] = &min_error[0];
                min_error_ptrs[2][2] = &min_error[1];
                min_error_ptrs[3][0] = &min_error[0];
                min_error_ptrs[3][1] = &min_error[2];
                min_error_ptrs[3][2] = &min_error[0];

                std::array<double, 3> sum_squared_error = {{.0, .0, .0}};
                std::array<std::array<double*, 4>, 4> sum_squared_error_ptrs;
                sum_squared_error_ptrs[0][0] = &sum_squared_error[1];
                sum_squared_error_ptrs[0][1] = &sum_squared_error[0];
                sum_squared_error_ptrs[0][2] = &sum_squared_error[2];
                sum_squared_error_ptrs[1][0] = &sum_squared_error[0];
                sum_squared_error_ptrs[1][1] = &sum_squared_error[1];
                sum_squared_error_ptrs[1][2] = &sum_squared_error[0];
                sum_squared_error_ptrs[2][0] = &sum_squared_error[2];
                sum_squared_error_ptrs[2][1] = &sum_squared_error[0];
                sum_squared_error_ptrs[2][2] = &sum_squared_error[1];
                sum_squared_error_ptrs[3][0] = &sum_squared_error[0];
                sum_squared_error_ptrs[3][1] = &sum_squared_error[2];
                sum_squared_error_ptrs[3][2] = &sum_squared_error[0];

                
                bool first_encounter_out_of_range_low_reported = false;
                bool first_encounter_out_of_range_high_reported = false;
                bool first_encounter_reported = false;

                for (size_t i_h = 0; i_h < test_format.height(); ++i_h) {
                    for (size_t i_w_group = 0; i_w_group < num_horiz_groups; ++i_w_group) {

                        //// TODO: add row padding to the calculation?
                        //long pixel_addr = (test_format.width() * i_h + i_w);
                        //
                        //// Calculate the block nr, each block is 16 bytes and 
                        //// carries 6 pixels
                        //ldiv_t div_result = std::div(pixel_addr, 6l);

                        //size_t pixel_group_num = div_result.quot;
                        //size_t pixel_group_offset = div_result.rem;

                        //FB_ASSERT(pixel_group_num < (test_format.width() * test_format.height() / 6));
                        //FB_ASSERT(pixel_group_offset < 6);

                        // Get the byte address for this group
                        // Each group is 16 bytes long (4 32 bit LE words)
                        size_t group_addr = (num_horiz_groups * i_h + i_w_group) * 16;

                        // TODO: decode YUV properly? for now we are just
                        // dumping the components (which are shuffled)

                        auto group_reference = get_v210_group_from_addr(
                            group_addr,
                            data_reference);

                        auto group_candidate = get_v210_group_from_addr(
                            group_addr,
                            data_candidate);

                        for (size_t group_el = 0; group_el < 4; ++group_el) {

                            auto components_reference = uncompress_v210_group(group_reference[group_el]);
                            auto components_candidate = uncompress_v210_group(group_candidate[group_el]);

                            for (size_t i_component = 0; i_component < 3; ++i_component) {

                                uint16_t component_reference = components_reference[i_component];
                                uint16_t component_candidate = components_candidate[i_component];
                                
                                int32_t error = static_cast<int32_t>(component_reference) - static_cast<int32_t>(component_candidate);

                                *max_error_ptrs[group_el][i_component] = std::max(abs(error), *max_error_ptrs[group_el][i_component]);
                                *min_error_ptrs[group_el][i_component] = std::min(abs(error), *min_error_ptrs[group_el][i_component]);

                                double squared_error = pow(static_cast<double>(error), 2);

                                *sum_squared_error_ptrs[group_el][i_component] += squared_error;

                                // This is a must (!)
                                if ( (!first_encounter_out_of_range_low_reported) 
                                    && (component_reference < valid_range.first || component_reference > valid_range.second)) {
                                    FB_LOG_ERROR << "Encountered out-of-valid-range value in reference image: '" << component_reference << "' (showing only first encounter).";
                                    data_are_close = false;
                                    first_encounter_out_of_range_low_reported = true;
                                }

                                if (!first_encounter_out_of_range_high_reported && 
                                    (component_candidate < valid_range.first || component_candidate > valid_range.second)) {
                                    FB_LOG_ERROR << "Encountered out-of-valid-range value in candidate imgae: '" << component_candidate << "' (showing only first encounter).";
                                    data_are_close = false;
                                    first_encounter_out_of_range_high_reported = true;
                                }

                                bool component_is_close = is_close(component_reference, component_candidate, tolerance);
                                data_are_close = data_are_close && component_is_close;

                                if (!first_encounter_reported && !component_is_close) {

                                    auto& label = labels[group_el][i_component];

                                    FB_LOG_INFO 
                                        << "Pixel diff for coordinates "
                                        << "(" << i_w_group << " (4-byte group)/" << i_component << "(offset)/" << i_h << ") with label '"
                                        << label << "', "
                                        << "uncompressed pixel coords (circa): " << ((i_w_group * 4 + i_component)/4) * 6 << "/" << i_h << ", "
                                        << "exceeded tolerance: "
                                        << "reference = " 
                                        << static_cast<int>(component_reference) << " vs " 
                                        << "candidate = " 
                                        << static_cast<int>(component_candidate) << " " 
                                        << "with tolerance='" 
                                        << static_cast<int>(tolerance) << "'.";

                                    first_encounter_reported = true;

                                }
                            }
                            

                        }

                    }
                }

                // Finish up stats

                if (statistics_out != nullptr) {

                    std::array<ChannelComparisonStatistics, 3> stat;

                    stat[0].channel_name = "Y'";
                    stat[1].channel_name = "Cb";
                    stat[2].channel_name = "Cr";

                    stat[0].max_error = max_error[0];
                    stat[1].max_error = max_error[1];
                    stat[2].max_error = max_error[2];

                    stat[0].min_error = min_error[0];
                    stat[1].min_error = min_error[1];
                    stat[2].min_error = min_error[2];

                    // Calculate mean squared error
					stat[0].mean_squared_error = sum_squared_error[0] / static_cast<double>((test_format.width() * test_format.height()));
                    // Notice that Cb and Cr have half the sampling rate in the horizontal direction for V210 (!)
                    stat[1].mean_squared_error = sum_squared_error[1] / static_cast<double>(((test_format.width()/2) * test_format.height()));
                    stat[2].mean_squared_error = sum_squared_error[2] / static_cast<double>(((test_format.width()/2) * test_format.height()));

                    // TODO: should we substract the forbidden code value?
                    // Calculate PSNR
                    for (size_t i = 0; i<3; ++i) {
                        if (stat[i].mean_squared_error == 0)
                            stat[i].psnr = std::numeric_limits<double>::infinity();
                        else
                            stat[i].psnr = 20*log10(1024) - 10*log10(stat[i].mean_squared_error);
                    }

                    *statistics_out = stat;

                }

                return data_are_close;
            }
        }

    }

}

#ifndef _MSC_VER
#pragma clang diagnostic pop
#endif

#endif // TOA_FRAME_BENDER_TEST_UTILS_H
