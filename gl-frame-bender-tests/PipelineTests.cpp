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

#include <string>
#include <numeric>
#include <future>

#include "Stage.h"
#include "StageSampler.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/algorithm/string.hpp>

namespace fb = toa::frame_bender;

using namespace fb;

BOOST_AUTO_TEST_SUITE(PipelineTests)

BOOST_AUTO_TEST_CASE(BasicTests) {

    // Create a pipeline for strings with three stages: 
    // 1] Strip whitespaces left and right
    // 2] Convert to uppercase
    // 4] Count the occurrence of the sequence 'US', return the strings and the number of occurrence

    const size_t pipeline_size = 3;

    std::vector<std::string> input_strings;
    input_strings.push_back("Lorem ipsum dolor sit amet, consectetur ");
    input_strings.push_back(" bi nec urna libero. Integer ve");
    input_strings.push_back("olor sit amet, consectetur adipiscing elit.");
    input_strings.push_back("  faucibus orci luctus et ultrices ");
    input_strings.push_back(" feugiat vel tristique sit amet, accumsan in neque");
    input_strings.push_back(" Nam condimentum porta nisl non dictum");
    input_strings.push_back("  scelerisque faucibUs");
    input_strings.push_back("   Cras cursus dui mollis urna   ");
    input_strings.push_back("Hallo, du");
    
    std::vector<std::pair<std::string, size_t>> expected_output;
    expected_output.push_back(std::make_pair("LOREM IPSUM DOLOR SIT AMET, CONSECTETUR", 0));
    expected_output.push_back(std::make_pair("BI NEC URNA LIBERO. INTEGER VE", 0));
    expected_output.push_back(std::make_pair("OLOR SIT AMET, CONSECTETUR ADIPISCING ELIT.", 0));
    expected_output.push_back(std::make_pair("FAUCIBUS ORCI LUCTUS ET ULTRICES", 2));
    expected_output.push_back(std::make_pair("FEUGIAT VEL TRISTIQUE SIT AMET, ACCUMSAN IN NEQUE", 0));
    expected_output.push_back(std::make_pair("NAM CONDIMENTUM PORTA NISL NON DICTUM", 0));
    expected_output.push_back(std::make_pair("SCELERISQUE FAUCIBUS", 1));
    expected_output.push_back(std::make_pair("CRAS CURSUS DUI MOLLIS URNA", 1));
    expected_output.push_back(std::make_pair("HALLO, DU", 0));

    auto itr = input_strings.begin();
    
    auto first_stage = utils::create_producer_stage<std::string>(
        "ReadSomeStrings",
        [&](std::string& out_element){

            StageCommand command = StageCommand::NO_CHANGE;

            if (itr != input_strings.end()) {

                out_element = *itr;
                itr++;

            } else {

                // Send cancel token
                command = StageCommand::STOP_EXECUTION;

            }

            return command;

        },
        std::vector<std::string>(pipeline_size)
    );

    auto second_stage = utils::create_stage<decltype(first_stage), std::string>(
        "TrimmingStrings",
        [](std::string& in_element, std::string& out_element){

            out_element = boost::algorithm::trim_copy(in_element);

            return StageCommand::NO_CHANGE;
        },
        std::vector<std::string>(pipeline_size),
        first_stage
    );

    auto third_stage = utils::create_stage<decltype(second_stage), std::string>(
        "ConvertToUpperStrings",
        [](std::string& in_element, std::string& out_element){

            out_element = boost::algorithm::to_upper_copy(in_element);

            return StageCommand::NO_CHANGE;
        },
        std::vector<std::string>(pipeline_size),
        second_stage
    );

    std::vector<std::pair<std::string, size_t>> output_elements;

    auto fourth_stage = utils::create_consumer_stage<decltype(third_stage)>(
        "CountUSAndConsume",
        [&](std::string& in_element){

            auto output_pair = std::make_pair(in_element, 0);
            
            for (
                auto itr = in_element.begin(); 
                itr != in_element.end() && std::next(itr) != in_element.end(); 
                ++itr)
            {
                auto nxt_itr = std::next(itr);
                std::string element; 
                element.push_back(*itr);
                element.push_back(*nxt_itr);

                if (element == "US")
                    output_pair.second++;

            }

            output_elements.push_back(output_pair);

            return StageCommand::NO_CHANGE;
        },
        third_stage
    );

    //// Serialized execution

    BOOST_REQUIRE(first_stage.status() == PipelineStatus::READY_TO_EXECUTE);
    BOOST_REQUIRE(second_stage.status() == PipelineStatus::READY_TO_EXECUTE);
    BOOST_REQUIRE(third_stage.status() == PipelineStatus::READY_TO_EXECUTE);
    BOOST_REQUIRE(fourth_stage.status() == PipelineStatus::READY_TO_EXECUTE);

    while(fourth_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
        first_stage.execute();
        second_stage.execute();
        third_stage.execute();
        fourth_stage.execute();
    }

    BOOST_REQUIRE(first_stage.status() == PipelineStatus::HAS_BEEN_STOPPED);
    BOOST_REQUIRE(second_stage.status() == PipelineStatus::HAS_BEEN_STOPPED);
    BOOST_REQUIRE(third_stage.status() == PipelineStatus::HAS_BEEN_STOPPED);
    BOOST_REQUIRE(fourth_stage.status() == PipelineStatus::HAS_BEEN_STOPPED);

    BOOST_REQUIRE_EQUAL(output_elements.size(), input_strings.size());
    
    BOOST_REQUIRE_EQUAL(output_elements.size(), expected_output.size());

    for (size_t i = 0; i<output_elements.size(); ++i) {
        BOOST_REQUIRE(output_elements[i] == expected_output[i]);
    }

}

BOOST_AUTO_TEST_CASE(MultithreadedTest) {

    // sequence of numbers
    const size_t num_elements = 1000000;
    const size_t pipeline_size = 10;
    std::vector<int> input_elements(num_elements);
    // fill with ascending values -N/2 ... N/2
    std::iota(
        std::begin(input_elements),
        std::end(input_elements),
        -static_cast<int>(num_elements)/2);

    auto itr = std::begin(input_elements);
    // first stage increment values by one
    // second stage: convert to float
    // third stage: divide by two
    // fourth stage: apply abs
    int cnt = 0;
    auto first_stage = utils::create_producer_stage<int>(
        "produce_values",
        [&](int& output)
        {
            if (itr != std::end(input_elements)) {
                output = *itr;
                ++itr;
                            cnt++;
                return StageCommand::NO_CHANGE;
            } else {
                return StageCommand::STOP_EXECUTION;
            }
        },
        std::vector<int>(pipeline_size, 0)
    );

    auto second_stage = utils::create_stage<decltype(first_stage), int>(
        "increment_by_one",
        [](int& input, int& output)
        {
            output = input+1;
            return StageCommand::NO_CHANGE;
        },
        std::vector<int>(pipeline_size, 0),
        first_stage
    );

    auto third_stage = utils::create_stage<decltype(second_stage), float>(
        "convert_to_float",
        [](int& input, float& output)
        {
            output = static_cast<float>(input);
            return StageCommand::NO_CHANGE;
        },
        std::vector<float>(pipeline_size, 0),
        second_stage
    );

    auto fourth_stage = utils::create_stage<decltype(third_stage), float>(
        "divide_by_two",
        [](float& input, float& output)
        {
            output = input / 2.0f;
            return StageCommand::NO_CHANGE;
        },
        std::vector<float>(pipeline_size, 0),
        third_stage
    );

    auto fifth_stage = utils::create_stage<decltype(fourth_stage), float>(
        "apply_absolute",
        [](float& input, float& output)
        {
            output = fabs(input);
            return StageCommand::NO_CHANGE;
        },
        std::vector<float>(pipeline_size, 0),
        fourth_stage
    );

    std::vector<float> results;
    // write out values
    auto sixth_stage = utils::create_consumer_stage<decltype(fifth_stage)>(
        "consume_values",
        [&results](float& input)
        {
            results.push_back(input);
            return StageCommand::NO_CHANGE;
        },
        fifth_stage
    );

    // Launch all stages concurrently, we even use a condition variables, 
    // so threads start executing stages very close to each other
    std::atomic<bool> kick_off_stage_execution{false};
    std::mutex mtx;
    std::condition_variable cv;

    std::thread first_thread([&]{
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (first_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            first_stage.execute();
            std::this_thread::yield();
        }
    });

    std::thread second_thread([&]{
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (second_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
             second_stage.execute();
            std::this_thread::yield();
        }
    });

    std::thread third_thread([&]{
        
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (third_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            third_stage.execute();
            std::this_thread::yield();
        }
    });

    std::thread fourth_thread([&]{
        
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (fourth_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            fourth_stage.execute();
            std::this_thread::yield();
        }
    });

    std::thread fifth_thread([&]{

        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (fifth_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            fifth_stage.execute();
            std::this_thread::yield();
        }
    });

    std::thread sixth_thread([&]{
       
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (sixth_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            sixth_stage.execute();
            std::this_thread::yield();
        }
    });

    // Kick off execution
    kick_off_stage_execution.store(true);
    cv.notify_all();
    
    clock::time_point start = clock::now();
    first_thread.join();
    second_thread.join();
    third_thread.join();
    fourth_thread.join();
    fifth_thread.join();
    sixth_thread.join();
    clock::time_point end = clock::now();
    milliseconds diff = boost::chrono::duration_cast<milliseconds>(end - start);
    BOOST_TEST_MESSAGE("Execution took: " + std::to_string(diff.count()) + "ms");

    BOOST_TEST_MESSAGE("Stage executions are done.");

    BOOST_REQUIRE_EQUAL(results.size(), input_elements.size());

    for (size_t i = 0; i<results.size(); ++i) {

        float expected = fabs(static_cast<float>(input_elements[i]+1)/2.0f);
        BOOST_REQUIRE_CLOSE(results[i], expected, 0.00001f);

    }
}

class MoveOnly {

public:
    MoveOnly() : num_(-1) {}
    MoveOnly(int num) : num_(num) {}
    virtual ~MoveOnly() {}

    MoveOnly& operator=(MoveOnly&& other) {
        this->num_ = other.num_;
        other.num_ = -1;
        return *this;
    }

    MoveOnly(MoveOnly&& other) : num_(other.num_) {
        other.num_ = -1;
    }

    int num() const { return num_; }

private:

    MoveOnly& operator=(const MoveOnly&) { throw std::runtime_error("Unexpected.");}
    MoveOnly(const MoveOnly&) { throw std::runtime_error("Unexpected."); }

    int num_;

};

BOOST_AUTO_TEST_CASE(MoveOnlyTest) {
    
    int cnt = 0;
    int last_cnt = 100;
    const int pipeline_size = 10;

    std::vector<MoveOnly> init;
    for (int i = 0; i<pipeline_size; ++i) 
        init.emplace_back(-1);

    auto head_stage = utils::create_producer_stage<MoveOnly>(
        "produce_values",
        [&](MoveOnly& output)
        {

            MoveOnly el(cnt);
            output = std::move(el);
            cnt++;

            if (cnt <= 100) {
                return StageCommand::NO_CHANGE;
            } else {
                return StageCommand::STOP_EXECUTION;
            }
        },
        std::move(init)
    );

    std::vector<MoveOnly> init_mid;
    for (int i = 0; i<pipeline_size; ++i) 
        init_mid.emplace_back(-1);

    auto mid_stage = utils::create_stage<decltype(head_stage), MoveOnly>(
        "process_values",
        [](MoveOnly& input, MoveOnly& output)
        {
            output = std::move(input);

            return StageCommand::NO_CHANGE;
        },
        std::move(init_mid),
        head_stage
    );

    std::vector<MoveOnly> results;

    auto tail_stage = utils::create_consumer_stage<decltype(mid_stage)>(
        "consume_values",
        [&results](MoveOnly& input)
        {
            results.push_back(std::move(input));
            return StageCommand::NO_CHANGE;
        },
        mid_stage
    );

    std::atomic<bool> kick_off_stage_execution{false};
    std::mutex mtx;
    std::condition_variable cv;

    std::thread first_thread([&]{

        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (head_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            head_stage.execute();
            std::this_thread::yield();
        }
    });  

    std::thread second_thread([&]{

        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (mid_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            mid_stage.execute();
            std::this_thread::yield();
        }
    });  

    std::thread third_thread([&]{

        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{return kick_off_stage_execution.load();});
        }

        while (tail_stage.status() == PipelineStatus::READY_TO_EXECUTE) {
            tail_stage.execute();
            std::this_thread::yield();
        }
    });  

    kick_off_stage_execution.store(true);
    cv.notify_all();

    first_thread.join();
    second_thread.join();
    third_thread.join();

    // Check the results
    for (int i = 0; i < last_cnt; ++i) {

        BOOST_CHECK_EQUAL(results[i].num(), i);

    }

}

BOOST_AUTO_TEST_CASE(ImplicitDataDependencyOverOne) {

    // What's the constraint on these?
    static const int32_t first_pipe_size = 3;
    static const int32_t second_pipe_size = 2;

    std::array<std::atomic<int32_t>, first_pipe_size> shared_ids;
    for (auto& el : shared_ids)
        el.store(0);

    typedef std::atomic<int32_t>* TokenType;

    std::vector<std::atomic<int32_t>*> init;
    for (int32_t i = 0; i<first_pipe_size; ++i) 
        init.push_back(&shared_ids[i]);

    int32_t cnt = 0;

    bool race_condition_happened = false;

    std::unique_ptr<StageSampler> pack_sampler = utils::make_unique<StageSampler>();

    // very inaccurate, but should suffice
    clock::time_point begin = clock::now();

    auto pack_stage = utils::create_producer_stage<TokenType>(
        "pack",
        [&](TokenType& output)
        {

            if (output->load() != 0)  {
                race_condition_happened = true;
                return StageCommand::STOP_EXECUTION;
            }

            cnt++;

            if (cnt <= 100) {
                return StageCommand::NO_CHANGE;
            } else {
                return StageCommand::STOP_EXECUTION;
            }
        },
        std::move(init),
        std::bind(&StageSampler::sample, pack_sampler.get(), std::placeholders::_1)
    );    

    std::unique_ptr<StageSampler> map_sampler = utils::make_unique<StageSampler>();

    auto map_stage = utils::create_stage<decltype(pack_stage), TokenType>(
        "map",
        [](TokenType& input, TokenType& output)
        {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            output = input;

            return StageCommand::NO_CHANGE;
        },
        std::vector<TokenType>(second_pipe_size),
        pack_stage,
        std::bind(&StageSampler::sample, map_sampler.get(), std::placeholders::_1)
    );

    std::unique_ptr<StageSampler> copy_sampler = utils::make_unique<StageSampler>();
    auto copy_stage = utils::create_consumer_stage<decltype(map_stage)>(
        "copy",
        [](TokenType& input)
        {
            input->store(1);
            // do something lengthy
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            input->store(0);

            return StageCommand::NO_CHANGE;
        },
        map_stage,
        std::bind(&StageSampler::sample, copy_sampler.get(), std::placeholders::_1)
    );


    std::thread copy_thread([&]{

        while (copy_stage.status() == PipelineStatus::READY_TO_EXECUTE) {

            if (copy_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                copy_stage.execute();

            std::this_thread::yield();
        }

    });  

    std::thread pack_map_thread([&]{

        while (
            pack_stage.status() == PipelineStatus::READY_TO_EXECUTE || 
            map_stage.status() == PipelineStatus::READY_TO_EXECUTE) {

            if (pack_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                pack_stage.execute();

            if (map_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                map_stage.execute();

            std::this_thread::yield();
        }

    });  

    pack_map_thread.join();
    copy_thread.join();
    clock::time_point end = clock::now();

    FB_LOG_INFO << "Test duration: " << boost::chrono::duration_cast<milliseconds>(end-begin) << ".";

    auto stats = pack_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Pack-sampler:" << stat;
    }

    stats = map_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Map-sampler:" << stat;
    }

    stats = copy_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Copy-sampler:" << stat;
    }

    BOOST_REQUIRE(!race_condition_happened);

}

BOOST_AUTO_TEST_CASE(PassingDataOverOne) {

    // Needs to be an even number
    static const int32_t num_shared_resources = 3;
    static const int32_t first_pipe_size = 1;
    static const int32_t second_pipe_size = 2;

    if ( (first_pipe_size + second_pipe_size) != num_shared_resources) {
        throw std::runtime_error("Faulty pipeline size configuration.");
    }

    std::array<std::atomic<int32_t>, num_shared_resources> shared_ids;
    for (auto& el : shared_ids)
        el.store(0);

    typedef std::atomic<int32_t>* TokenType;

    std::vector<std::atomic<int32_t>*> init_first;
    for (int32_t i = 0; i<first_pipe_size; ++i) 
        init_first.push_back(&shared_ids[i]);

    std::vector<std::atomic<int32_t>*> init_second;
    for (int32_t i = 0; i<second_pipe_size; ++i) {
        init_second.push_back(&shared_ids[first_pipe_size+i]);
        // For seocnd stage, the values are all "1"
        init_second.back()->fetch_add(1);
    }

    int32_t cnt = 0;

    bool race_condition_happened = false;

    std::unique_ptr<StageSampler> pack_sampler = utils::make_unique<StageSampler>();

    // very inaccurate, but should suffice
    clock::time_point begin = clock::now();

    auto pack_stage = utils::create_producer_stage<TokenType>(
        "pack",
        [&](TokenType& output)
        {

            if (output->load() != 0)  {
                race_condition_happened = true;
                return StageCommand::STOP_EXECUTION;
            }

            cnt++;

            if (cnt <= 100) {
                return StageCommand::NO_CHANGE;
            } else {
                return StageCommand::STOP_EXECUTION;
            }
        },
        std::move(init_first),
        std::bind(&StageSampler::sample, pack_sampler.get(), std::placeholders::_1)
    );    

    std::unique_ptr<StageSampler> map_sampler = utils::make_unique<StageSampler>();

    auto map_stage = utils::create_stage<decltype(pack_stage), TokenType>(
        "map",
        [](TokenType& input, TokenType& output)
        {
            
            // Save output upstream token
            TokenType output_upstream_tmp = std::move(output);
            
            // increment on downstream
            input->fetch_add(1);

            // Send input downstream to output downstream
            output = input;

            // Send output upstream to input upstream
            // decrement on upstream
            output_upstream_tmp->fetch_sub(1);
            input = std::move(output_upstream_tmp);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            return StageCommand::NO_CHANGE;
        },
        std::move(init_second),
        pack_stage,
        std::bind(&StageSampler::sample, map_sampler.get(), std::placeholders::_1)
    );

    std::unique_ptr<StageSampler> copy_sampler = utils::make_unique<StageSampler>();
    auto copy_stage = utils::create_consumer_stage<decltype(map_stage)>(
        "copy",
        [](TokenType& input)
        {
            // Increment while working on it
            input->fetch_add(1);
            // do something lengthy
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // decrement, we are done with it
            input->fetch_sub(1);

            return StageCommand::NO_CHANGE;
        },
        map_stage,
        std::bind(&StageSampler::sample, copy_sampler.get(), std::placeholders::_1)
    );


    std::thread copy_thread([&]{

        while (copy_stage.status() == PipelineStatus::READY_TO_EXECUTE) {

            if (copy_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                copy_stage.execute();

            std::this_thread::yield();
        }

    });  

    std::thread pack_map_thread([&]{

        while (
            pack_stage.status() == PipelineStatus::READY_TO_EXECUTE || 
            map_stage.status() == PipelineStatus::READY_TO_EXECUTE) {

            if (pack_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                pack_stage.execute();

            if (map_stage.status() == PipelineStatus::READY_TO_EXECUTE )
                map_stage.execute();

            std::this_thread::yield();
        }

    });  

    pack_map_thread.join();
    copy_thread.join();
    clock::time_point end = clock::now();

    FB_LOG_INFO << "Test duration: " << boost::chrono::duration_cast<milliseconds>(end-begin) << ".";

    auto stats = pack_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Pack-sampler:" << stat;
    }

    stats = map_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Map-sampler:" << stat;
    }

    stats = copy_sampler->collect_statistics();
    for (const auto& stat : stats) {
        FB_LOG_INFO << "Copy-sampler:" << stat;
    }

    BOOST_REQUIRE(!race_condition_happened);

}

// TODO: test failure for zero-sized pipe
// TODO: test varying pipeline sizes... 
// TODO: test full-duplex side-effects (feed-back loop algorithms)

// TODO: Test faulty connections (circles, etc...)
// TODO: Test empty stage handle (valid or not)

BOOST_AUTO_TEST_SUITE_END()
