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

#include <algorithm>

#include <boost/test/unit_test.hpp>

#include "StreamSource.h"
#include "TestUtils.h"
#include "TestCommon.h"

namespace fb = toa::frame_bender;

using namespace fb;

BOOST_AUTO_TEST_SUITE(StreamSourceTests)

BOOST_AUTO_TEST_CASE(FrameTimeStampTest) {

    PrefetchedImageSequence seq1(
        "horse_v210_1920_1080p_short",
        "horse_\\d+\\.v210",
        test::horse_seq_v210_1080p_format(),
        Time(1, 50)
        );

    BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::READY_TO_READ);

    Frame f;
    size_t cnt = 0;
    while(seq1.state() == StreamSource::State::READY_TO_READ) {

        bool s = seq1.pop_frame(f);
        BOOST_REQUIRE(s);

        BOOST_REQUIRE_EQUAL(f.time(), Time(cnt, 50));

        cnt++;

    }

}

BOOST_AUTO_TEST_CASE(RGBPrefetchedImageTestSequenceTest) {

    PrefetchedImageSequence seq1(
        "ducks_rgb24_1920_1080p_short",
        ".*\\.raw",
        test::ducks_seq_rgb24_1080p_format(),
        Time(1, 50)
        );

    BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::READY_TO_READ);

    BOOST_TEST_MESSAGE("Finish loading images for the first time.");

    // Identity check
    PrefetchedImageSequence seq2(
        "ducks_rgb24_1920_1080p_short",
        ".*\\.raw",
        test::ducks_seq_rgb24_1080p_format(),
        Time(1, 50)
        );

    BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::READY_TO_READ);

    BOOST_TEST_MESSAGE("Finish loading images for the second time.");

    BOOST_REQUIRE_EQUAL(
        seq1.num_frames(), 
        seq2.num_frames());

    bool are_equal = true;

    for (size_t i = 0; i<seq1.num_frames() && are_equal; ++i) {

        Frame left;
        Frame right;

        bool success = seq1.pop_frame(left);
        BOOST_REQUIRE(success && left.is_valid());

        success = seq2.pop_frame(right);
        BOOST_REQUIRE(success && right.is_valid());

        if (i+1 < seq1.num_frames()) {
            BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::READY_TO_READ);
            BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::READY_TO_READ);
        } else {
            BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::END_OF_STREAM);
            BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::END_OF_STREAM);
        }

        are_equal = are_equal && test::compare_rgb24_frames(left, right);
    }

    BOOST_REQUIRE(are_equal);

    Frame some_frame;
    BOOST_REQUIRE_THROW(seq1.pop_frame(some_frame), std::runtime_error);

    BOOST_TEST_MESSAGE("Comparison complete.");
}

BOOST_AUTO_TEST_CASE(V210PrefetchedImageTestSequenceTest) {

    PrefetchedImageSequence seq1(
        "horse_v210_1920_1080p_short",
        "horse_\\d+\\.v210",
        test::horse_seq_v210_1080p_format(),
        Time(1, 50)
        );

    BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::READY_TO_READ);

    BOOST_TEST_MESSAGE("Finish loading images for the first time.");

    // Identity check
    PrefetchedImageSequence seq2(
        "horse_v210_1920_1080p_short",
        "horse_\\d+\\.v210",
        test::horse_seq_v210_1080p_format(),
        Time(1, 50)
        );

    BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::READY_TO_READ);

    BOOST_TEST_MESSAGE("Finish loading images for the second time.");

    BOOST_REQUIRE_EQUAL(
        seq1.num_frames(), 
        seq2.num_frames());

    bool are_equal = true;

    for (size_t i = 0; i<seq1.num_frames() && are_equal; ++i) {

        Frame left;
        Frame right;

        bool success = seq1.pop_frame(left);
        BOOST_REQUIRE(success && left.is_valid());

        success = seq2.pop_frame(right);
        BOOST_REQUIRE(success && right.is_valid());

        if (i+1 < seq1.num_frames()) {
            BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::READY_TO_READ);
            BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::READY_TO_READ);
        } else {
            BOOST_REQUIRE_EQUAL(seq1.state(), StreamSource::State::END_OF_STREAM);
            BOOST_REQUIRE_EQUAL(seq2.state(), StreamSource::State::END_OF_STREAM);
        }

        are_equal = are_equal && test::compare_v210_frames(left, right, 0);
    }

    BOOST_REQUIRE(are_equal);

    Frame some_frame;
    BOOST_REQUIRE_THROW(seq1.pop_frame(some_frame), std::runtime_error);

    BOOST_TEST_MESSAGE("Comparison complete.");
}


BOOST_AUTO_TEST_SUITE_END()