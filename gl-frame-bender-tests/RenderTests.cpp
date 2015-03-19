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
#include <boost/test/parameterized_test.hpp>

#include <iostream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <numeric>
#include <limits>
#include <algorithm>
#include <sstream>

#include "FrameBender.h"
#include "Utils.h"
#include "Window.h"
#include "Context.h"
#include "Init.h"
#include "TestUtils.h"
#include "TestCommon.h"

#include "FormatConverterStage.h"

namespace fb = toa::frame_bender;

using namespace fb;

BOOST_AUTO_TEST_SUITE(RenderTests)

//BOOST_AUTO_TEST_CASE(RGBSingleStreamPassThrough) {
//
//    gl::Init::require();
//
//    gl::Window::get()->context()->attach();
//
//    ImageFormat input_format(
//        1920, 
//        1080, 
//        ImageFormat::Transfer::BT_709,
//        ImageFormat::Chromaticity::BT_709,
//        ImageFormat::PixelFormat::RGB_8BIT,
//        ImageFormat::Origin::LOWER_LEFT);
//
//    ImageFormat render_format(
//        1920, 
//        1080, 
//        ImageFormat::Transfer::BT_709,
//        ImageFormat::Chromaticity::BT_709,
//        ImageFormat::PixelFormat::RGBA_8BIT,
//        ImageFormat::Origin::LOWER_LEFT);
//
//    gl::Window::get()->context()->detach();
//
//    auto input_sequence = std::make_shared<PrefetchedImageSequence>(
//        "ducks_rgb24_1920_1080p",
//        ".*\\.raw",
//        input_format,
//        Time(1, 50)
//        );
//
//    // we need the same here as input_frames.
//    std::vector<Frame> output_frames;
//    
//    // Preallocate
//    output_frames.reserve(input_sequence->num_frames());
//
//    clock::time_point gl_end_time;
//    clock::time_point gl_start_time;
//
//    {
//
//        // Note that we must initialize this before creting the dispatch
//        // object, as we need exclusive access to the render (main context)
//        gl::Window::get()->context()->attach();
//        auto renderer = utils::make_unique<PassThroughRenderer>();
//        gl::Window::get()->context()->detach();
//
//        // Initialize Stream Dispatch
//        StreamDispatch dispatch(
//            "SingleStreamPassThroughDispatch",
//            gl::Window::get()->context().get(),
//            input_format,
//            render_format,
//            true, 
//            true,
//            true,
//            StreamDispatch::FlagContainer()
//            );
//
//        std::mutex mtx;
//        bool composition_is_finished = false;
//        std::condition_variable is_end_of_sequence;
//
//        size_t num_expected_frames = input_sequence->num_frames();
//
//        auto store_all_output_frames = [&](const Frame& f){
//
//            // We save the frames for later comparison.
//            // Need to create copies for later comparison
//            output_frames.push_back(Frame::create_copy(f));
//
//        };
//
//        auto composition_id = dispatch.create_composition(
//            "RGBASingleStreamPassThrough",
//            std::vector<std::shared_ptr<StreamSource>>(1, input_sequence),
//            std::move(renderer),
//            std::move(store_all_output_frames)
//            );
//
//        BOOST_CHECK(dispatch.is_composition(composition_id));
//
//        gl_start_time = now();
//        // Start composition now.
//        dispatch.start_composition(
//            composition_id,
//            [&](){
//                std::lock_guard<std::mutex> guard(mtx);
//                composition_is_finished = true;
//                is_end_of_sequence.notify_one();}
//            );
//
//        // TODO: maybe rewrite when there is a more generic way of waiting for
//        // a change in transport state.
//        std::unique_lock<std::mutex> lock(mtx);
//        is_end_of_sequence.wait(lock, [&]{return composition_is_finished;});
//
//        gl_end_time = now();
//
//    } // This puts dispatch out-of-scope and kills the threads as well.
//
//    milliseconds diff = boost::chrono::duration_cast<milliseconds>(gl_end_time - gl_start_time);
//
//    //// DEBUG
//    //int cntdebg = 0;
//    //for (auto& el : output_frames) {
//    //    el.dump_to_file("./dbg_" + std::to_string(cntdebg) + ".raw");
//    //    cntdebg++;
//    //}
//    //// EOF DEBUG
//
//    BOOST_TEST_MESSAGE("GL download/render/upload time: " + std::to_string(diff.count()) + "ms");
//
//    uintmax_t total_size_transferred = input_sequence->total_data_size();
//    float mb_total = static_cast<float>(total_size_transferred) / 1e6;
//
//    BOOST_TEST_MESSAGE("Extrapolated total throughput (roundtrip):" + std::to_string((mb_total / diff.count())*1e3) + " mb/sec");
//    BOOST_TEST_MESSAGE("Re-loading the input sequence for golden test.");
//    auto reference_sequence = utils::make_unique<PrefetchedImageSequence>(
//        "ducks_rgb24_1920_1080p",
//        ".*\\.raw",
//        input_format,
//        Time(1, 50)
//        );
//
//    BOOST_REQUIRE_EQUAL(
//        reference_sequence->num_frames(), 
//        input_sequence->num_frames());
//
//    BOOST_REQUIRE_EQUAL(
//        reference_sequence->num_frames(), 
//        output_frames.size());
//
//    bool are_equal = true;
//
//    for (size_t i = 0; i<reference_sequence->num_frames() && are_equal; ++i) {
//
//        Frame reference;
//
//        bool success = reference_sequence->pop_frame(reference);
//        BOOST_REQUIRE(success && reference.is_valid());
//
//        are_equal = are_equal && test::compare_rgb24_frames(
//            reference,
//            output_frames[i]);
//
//        if (!are_equal) {
//            BOOST_TEST_MESSAGE("Comparison failed at frame nr. '" + std::to_string(i) + "'.");
//            BOOST_TEST_MESSAGE("Dumping frames for which comparison failed.");
//            reference.dump_to_file("./comparison_reference_rgb8.raw");
//            output_frames[i].dump_to_file("./comparison_candidate_rgb8.raw");
//        }
//
//    }
//
//    BOOST_REQUIRE(are_equal);
//
//}

//BOOST_AUTO_TEST_CASE(V210CodecSingleStreamPassThroughModeTest) {
//
//    // we are looping over a couple for ProgramOption configurations, mainly
//    // concerned about different flavours of V210 encoding/decoding
//
//    ProgramOptions original_opts = ProgramOptions::global();
//
//    std::vector<ProgramOptions> variations;
//
//    for (int32_t mode_i = 0; mode_i < static_cast<int32_t>(Mode::count); ++mode_i)
//    {
//
//        Mode mode = Mode(mode_i);
//
//        std::ostringstream oss;
//        oss << mode;
//
//        std::multimap<std::string, std::string> m;
//        m.insert(std::make_pair("render.format_conversion_mode", oss.str()));
//
//        m.insert(std::make_pair("render.format_conversion.v210.bitextraction_in_shader_is_enabled", "true"));
//        variations.push_back(test::get_global_options_with_overrides(m));
//        
//        m.erase("render.format_conversion.v210.bitextraction_in_shader_is_enabled");
//        m.insert(std::make_pair("render.format_conversion.v210.bitextraction_in_shader_is_enabled", "false"));
//
//        variations.push_back(test::get_global_options_with_overrides(m));
//
//    }
//
//    for (const auto& opt : variations) {
//
//        ProgramOptions::set_global(opt);
//
//        BOOST_TEST_MESSAGE("Running 'Horse' sequence tests.");
//
//        ImageFormat input_format = test::horse_seq_v210_1080p_format();
//
//        perform_v210_pass_through_test(
//            ProgramOptions::global().optimization_flags(),
//            input_format,
//            "horse_v210_1920_1080p_short",
//            "horse_\\d+\\.v210",
//            90
//            );
//
//        BOOST_TEST_MESSAGE("Running 'Black' sequence tests.");
//
//        perform_v210_pass_through_test(
//            ProgramOptions::global().optimization_flags(),
//            input_format,
//            "black_v210_1920_1080p",
//            "black_\\d+\\.v210",
//            0
//            );
//
//    }
//
//    ProgramOptions::set_global(original_opts);
//
//}
//
//BOOST_AUTO_TEST_CASE(V210CodecSingleStreamPassThroughOptimizationsTest) {
//
//    gl::Init::require();
//
//    // we are looping over a couple for ProgramOption configurations, mainly
//    // concerned about different flavours of V210 encoding/decoding
//
//    ProgramOptions original_opts = ProgramOptions::global();
//
//    std::vector<ProgramOptions> variations;
//
//    // These are the sets of optimizations that we want to test
//    std::vector<std::vector<std::string>> optimization_flag_combinations;
//
//    // First one is empty, all single threaded
//    optimization_flag_combinations.push_back(std::vector<std::string>());
//
//    // Single GL context, but async input
//    std::vector<std::string> args;
//    args.push_back("ASYNC_INPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // Single GL context, but async output
//    args.clear();
//    args.push_back("ASYNC_OUTPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // Single GL context, but async output and input
//    args.clear();
//    args.push_back("ASYNC_OUTPUT");
//    args.push_back("ASYNC_INPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // Multi GL context
//    args.clear();
//    args.push_back("MULTIPLE_GL_CONTEXTS");
//    optimization_flag_combinations.push_back(args);
//
//    // Multi GL context async input
//    args.clear();
//    args.push_back("MULTIPLE_GL_CONTEXTS");
//    args.push_back("ASYNC_INPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // Multi GL context async output
//    args.clear();
//    args.push_back("MULTIPLE_GL_CONTEXTS");
//    args.push_back("ASYNC_OUTPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // Multi GL context async input and output
//    args.clear();
//    args.push_back("MULTIPLE_GL_CONTEXTS");
//    args.push_back("ASYNC_OUTPUT");
//    args.push_back("ASYNC_INPUT");
//    optimization_flag_combinations.push_back(args);
//
//    // We always test OpenGL 4.2 as mode here
//    Mode mode = Mode::glsl_420;
//
//    for (const auto& optimization_combo : optimization_flag_combinations) {
//
//        std::ostringstream oss;
//        oss << mode;
//
//        std::multimap<std::string, std::string> m;
//        m.insert(std::make_pair("render.format_conversion_mode", oss.str()));
//        for (const auto& el : optimization_combo) {
//
//            m.insert(std::make_pair("pipeline.optimization_flags", el));                
//
//        }
//
//        m.insert(std::make_pair("render.format_conversion.v210.bitextraction_in_shader_is_enabled", "true"));
//        variations.push_back(test::get_global_options_with_overrides(m));
//
//    }
//
//    for (const auto& opt : variations) {
//
//        ProgramOptions::set_global(opt);
//
//        std::ostringstream oss;
//        StreamDispatch::print_flags(oss, ProgramOptions::global().optimization_flags());
//
//        BOOST_TEST_MESSAGE("Running 'Horse' sequence tests with " + oss.str() + ".");
//
//        ImageFormat input_format = test::horse_seq_v210_1080p_format();
//
//        perform_v210_pass_through_test(
//            ProgramOptions::global().optimization_flags(),
//            input_format,
//            "horse_v210_1920_1080p_short",
//            "horse_\\d+\\.v210",
//            90
//            );
//
//    }
//
//    ProgramOptions::set_global(original_opts);
//
//}

BOOST_AUTO_TEST_SUITE_END()


struct V210PassThroughTestOptions {

    std::string input_seq_name;
    std::string input_seq_pattern;
    uint16_t tolerance;
    Mode format_conv_mode;
    bool use_shader_integer_bitfield_ops;
    StreamDispatch::FlagContainer optimization_flags;
    uint32_t permutation_count;

};

std::ostream& operator<< (std::ostream& out, const V210PassThroughTestOptions& v) {

    out << "[seq name: " << v.input_seq_name << ", ";
    out << "tolerance: " << v.tolerance << ", ";
    out << "mode: " << v.format_conv_mode << ", ";
    out << "use_shader_bitfield_ops: " << v.use_shader_integer_bitfield_ops << ", ";
    out << "optimization_flags: ";
    StreamDispatch::print_flags(out, v.optimization_flags);
    out << ", ";
    out << "permutation_count: " << v.permutation_count;
    out << "]";

    return out;
}

void perform_v210_pass_through_test(const V210PassThroughTestOptions& test_parameters) 
{

    std::ostringstream oss_params;
    oss_params << test_parameters;
    BOOST_TEST_MESSAGE("Running '" + oss_params.str() + "'.");


    // Build a global program options object from test parameters
    std::multimap<std::string, std::string> opts_overrides;

    opts_overrides.insert(std::make_pair("input.sequence.id", test_parameters.input_seq_name));
    opts_overrides.insert(std::make_pair("input.sequence.file_pattern", test_parameters.input_seq_pattern));

    std::ostringstream oss;
    oss << test_parameters.format_conv_mode;
    opts_overrides.insert(std::make_pair("render.format_conversion_mode", oss.str()));

    for (size_t i = 0; i<static_cast<size_t>(StreamDispatch::Flags::COUNT); ++i) {

        std::ostringstream oss;

        StreamDispatch::Flags f = StreamDispatch::Flags(i);

        if (test_parameters.optimization_flags[i]) {

            oss << f;
            opts_overrides.insert(std::make_pair("pipeline.optimization_flags", oss.str()));        

        }

    }

    {
        std::ostringstream oss;

        oss << std::boolalpha << test_parameters.use_shader_integer_bitfield_ops;
        opts_overrides.insert(std::make_pair("render.format_conversion.v210.bitextraction_in_shader_is_enabled", oss.str()));
    }

    fb::ProgramOptions opts = test::get_global_options_with_overrides(opts_overrides);

    fb::ProgramOptions prev_global = fb::ProgramOptions::global();
 
    fb::ProgramOptions::set_global(opts);

    gl::Window::get()->context()->attach();

    ImageFormat render_format(
        1920, 
        1080, 
        ImageFormat::Transfer::LINEAR,
        ImageFormat::Chromaticity::BT_709,
        ImageFormat::PixelFormat::RGBA_FLOAT_32BIT,
        ImageFormat::Origin::LOWER_LEFT);

    gl::Window::get()->context()->detach();

    ImageFormat input_sequence_format(
        static_cast<uint32_t>(ProgramOptions::global().input_sequence_width()),
        static_cast<uint32_t>(ProgramOptions::global().input_sequence_height()),
        ImageFormat::Transfer::BT_709, 
        ImageFormat::Chromaticity::BT_709,
        ProgramOptions::global().input_sequence_pixel_format(),
        ProgramOptions::global().input_sequence_origin()
        );

    auto input_sequence = std::make_shared<PrefetchedImageSequence>(
            ProgramOptions::global().input_sequence_name(),
            ProgramOptions::global().input_sequence_pattern(),
            input_sequence_format,
            ProgramOptions::global().input_sequence_frame_duration(),
            ProgramOptions::global().input_sequence_loop_count()
            );

    // we need the same here as input_frames.
    std::vector<Frame> output_frames;

    // Preallocate
    output_frames.reserve(input_sequence->num_frames());

    clock::time_point gl_end_time;
    clock::time_point gl_start_time;

    {

        // Note that we must initialize this before creting the dispatch
        // object, as we need exclusive access to the render (main context)
        gl::Window::get()->context()->attach();
        auto renderer = utils::make_unique<PassThroughRenderer>();
        gl::Window::get()->context()->detach();

        // Initialize Stream Dispatch
        StreamDispatch dispatch(
            "SingleStreamPassThroughDispatch",
            gl::Window::get()->context().get(),
            input_sequence_format,
            render_format,
            true, 
            true,
            true,
            opts.optimization_flags()
            );

        std::mutex mtx;
        bool composition_is_finished = false;
        std::condition_variable is_end_of_sequence;

        auto store_all_output_frames = [&](const Frame& f){

            // We save the frames for later comparison.
            output_frames.push_back(Frame::create_copy(f));

        };

        auto composition_id = dispatch.create_composition(
            "V210SingleStreamPassThrough",
            std::vector<std::shared_ptr<StreamSource>>(1, input_sequence),
            std::move(renderer),
            std::move(store_all_output_frames)
            );

        BOOST_CHECK(dispatch.is_composition(composition_id));

        gl_start_time = now();
        // Start composition now.
        dispatch.start_composition(
            composition_id,
            [&](){
                std::lock_guard<std::mutex> guard(mtx);
                composition_is_finished = true;
                is_end_of_sequence.notify_one();});

                std::unique_lock<std::mutex> lock(mtx);
                is_end_of_sequence.wait(lock, [&]{return composition_is_finished;});

                gl_end_time = now();

    } // This puts dispatch out-of-scope and kills the threads as well.

    milliseconds diff = boost::chrono::duration_cast<milliseconds>(gl_end_time - gl_start_time);

    BOOST_TEST_MESSAGE("GL download/render/upload time: " + std::to_string(diff.count()) + "ms");

    uintmax_t total_size_transferred = input_sequence->total_data_size();
    float mb_total = static_cast<float>(total_size_transferred) / 1e6f;

    BOOST_TEST_MESSAGE("Extrapolated total throughput (roundtrip):" + std::to_string((mb_total / diff.count())*1e3) + " mb/sec");
    BOOST_TEST_MESSAGE("Re-loading the input sequence for golden test.");
    auto reference_sequence = std::make_shared<PrefetchedImageSequence>(
            ProgramOptions::global().input_sequence_name(),
            ProgramOptions::global().input_sequence_pattern(),
            input_sequence_format,
            ProgramOptions::global().input_sequence_frame_duration(),
            ProgramOptions::global().input_sequence_loop_count()
            );

    BOOST_REQUIRE_EQUAL(
        reference_sequence->num_frames(), 
        input_sequence->num_frames());

    BOOST_REQUIRE_EQUAL(
        reference_sequence->num_frames(), 
        output_frames.size());

    bool are_within_tolerance = true;

    std::array<test::ChannelComparisonStatistics, 3> stats;

    for (size_t i = 0; i<reference_sequence->num_frames() && are_within_tolerance; ++i) {

        Frame reference;

        bool success = reference_sequence->pop_frame(reference);
        BOOST_REQUIRE(success && reference.is_valid());

        are_within_tolerance = are_within_tolerance && test::compare_v210_frames(
            reference,
            output_frames[i],
            static_cast<uint16_t>(test_parameters.tolerance),
            &stats);

        // Printing out statistics for first frame
        if (i == 0) {

            FB_LOG_INFO << "Statistics for comparison of frame nr. 0:";
            for (auto& el : stats) {
                FB_LOG_INFO << el;
            }
        }

        if (!are_within_tolerance) {
            FB_LOG_INFO << "Comparison failed at frame nr. '" << std::to_string(i) << "'.";
            FB_LOG_INFO << "Dumping frames for which comparison failed.";
            reference.dump_to_file("./comparison_reference_v210.raw");
            output_frames[i].dump_to_file("./comparison_candidate_v210.raw");
            FB_LOG_INFO << "Comparison vs. Reference statistics:";
            for (auto& el : stats) {
                FB_LOG_INFO << el;
            }
        }

    }

    BOOST_CHECK(are_within_tolerance);

    fb::ProgramOptions::set_global(prev_global);
}

struct InputSeqDefinition {

    std::string name;
    std::string pattern;
    uint16_t tolerance_expected;
    InputSeqDefinition(const std::string& name, const std::string& pattern, uint16_t tolerance_expected) :
        name(name), pattern(pattern), tolerance_expected(tolerance_expected) {}
};

std::vector<V210PassThroughTestOptions> rolled_out_parameters;

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {

	TestGlobalFixture global_fixture_;

    gl::Init::require();

    // Have two sequences to test, both 1080, but is perfect black
    std::vector<InputSeqDefinition> input_seqs;
    input_seqs.push_back(InputSeqDefinition("horse_v210_1920_1080p_short", "horse_\\d+\\.v210", 90));
    input_seqs.push_back(InputSeqDefinition("black_v210_1920_1080p", "black_\\d+\\.v210", 0));

    std::vector<Mode> format_conversion_modes;

    for (int32_t i = 0; i<static_cast<int32_t>(Mode::count); ++i) {
        Mode pf = Mode(i);

        format_conversion_modes.push_back(pf);
    }
    
    std::vector<StreamDispatch::FlagContainer> optimization_flags_combinations;

    optimization_flags_combinations.push_back(StreamDispatch::FlagContainer());

    StreamDispatch::FlagContainer flags;
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
    optimization_flags_combinations.push_back(flags); flags.reset(); 
    flags.set(static_cast<size_t>(StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    flags.set(static_cast<size_t>(StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS));
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    flags.set(static_cast<size_t>(StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS));
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    flags.set(static_cast<size_t>(StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS));
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
    flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
    optimization_flags_combinations.push_back(flags);
    flags.reset();

    if (GLAD_GL_ARB_buffer_storage != 0) {

        flags.set(static_cast<size_t>(StreamDispatch::Flags::ARB_PERSISTENT_MAPPING));
        optimization_flags_combinations.push_back(flags);
        flags.reset();

        flags.set(static_cast<size_t>(StreamDispatch::Flags::ARB_PERSISTENT_MAPPING));
        flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
        flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
        optimization_flags_combinations.push_back(flags);
        flags.reset();

        flags.set(static_cast<size_t>(StreamDispatch::Flags::ARB_PERSISTENT_MAPPING));
        flags.set(static_cast<size_t>(StreamDispatch::Flags::MULTIPLE_GL_CONTEXTS));
        flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_OUTPUT));
        flags.set(static_cast<size_t>(StreamDispatch::Flags::ASYNC_INPUT));
        optimization_flags_combinations.push_back(flags);
        flags.reset();


    } else {

        BOOST_TEST_MESSAGE("Not generating tests for AMD pinned memory.");

    }

    for (const auto& input_seq : input_seqs) {

        for (const auto& format_conversion_mode : format_conversion_modes) {

            for (const auto& optimization_flags : optimization_flags_combinations) {
            
                // add once with, and once with out shader V210 settings
                V210PassThroughTestOptions param;

                param.input_seq_name = input_seq.name;
                param.input_seq_pattern = input_seq.pattern;
                param.format_conv_mode = format_conversion_mode;
                param.optimization_flags = optimization_flags;
                param.tolerance = input_seq.tolerance_expected;
                param.use_shader_integer_bitfield_ops = true;
                param.permutation_count = rolled_out_parameters.size();

                rolled_out_parameters.push_back(param);

                param.use_shader_integer_bitfield_ops = false;
                param.permutation_count = rolled_out_parameters.size();

                rolled_out_parameters.push_back(param);

            }

        }

    }

    boost::unit_test::framework::master_test_suite().
        add(BOOST_PARAM_TEST_CASE( &perform_v210_pass_through_test, std::begin(rolled_out_parameters), std::end(rolled_out_parameters) ) );

    return nullptr;
}

//int main( int argc, char* argv[] )
//{
//
//    TestGlobalFixture global_fixture_;
//    return ::boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
//}
