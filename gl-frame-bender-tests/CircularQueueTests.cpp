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
#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "CircularFifo.h"
#include "ChronoUtils.h"
#include "CircularFifoHelpers.h"

// Note that std::thread leaks 44 bytes of memory, which should have been
// fixed in some future VS2012 library release: 
// http://connect.microsoft.com/VisualStudio/feedback/details/773429/memory-leak-for-std-thread

namespace fb = toa::frame_bender;

BOOST_AUTO_TEST_SUITE(CircularQueueTests)

BOOST_AUTO_TEST_CASE(IsLockFree) {

    fb::CircularFifo<std::string> fifo(5);

    BOOST_CHECK(fifo.is_lock_free());

}

BOOST_AUTO_TEST_CASE(ZeroSizeBuffer) {

    fb::CircularFifo<std::string> fifo(0);

    BOOST_CHECK_EQUAL(fifo.size(), 0);

    std::string element;

    BOOST_CHECK(!fifo.pop(element));
    BOOST_CHECK(!fifo.push(element));

}

BOOST_AUTO_TEST_CASE(ReportingSize) {

    fb::CircularFifo<std::string> fifo(5);

    BOOST_CHECK_EQUAL(fifo.size(), 5);

}

BOOST_AUTO_TEST_CASE(ReportingNumElements) {

    fb::CircularFifo<int> fifo(5);

    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 0);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 1);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 2);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 3);

    int val = 0;
    fifo.pop(val);
    fifo.pop(val);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 1);
    fifo.pop(val);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 0);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 1);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 2);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 3);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 4);

    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 5);

    // full, should still be 5
    fifo.push(0);
    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 5);
    BOOST_CHECK_EQUAL(fifo.was_full(), true);

    fifo.pop(val);
    fifo.pop(val);
    fifo.pop(val);
    fifo.pop(val);
    fifo.pop(val);

    BOOST_CHECK_EQUAL(fifo.had_num_elements(), 0);
    BOOST_CHECK_EQUAL(fifo.was_empty(), true);

}

BOOST_AUTO_TEST_CASE(SingleThreadedInsertionTest) {

    fb::CircularFifo<std::string> fifo(4);
    BOOST_CHECK(fifo.push("one"));
    BOOST_CHECK(fifo.push("two"));
    BOOST_CHECK(fifo.push("three"));
    BOOST_CHECK(fifo.push("four"));

    // this one shouldn't make it
    BOOST_CHECK(!fifo.push("five"));

    std::string reader;

    BOOST_CHECK(fifo.pop(reader));
    BOOST_CHECK_EQUAL(reader, "one");
    BOOST_CHECK(fifo.pop(reader));
    BOOST_CHECK_EQUAL(reader, "two");
    BOOST_CHECK(fifo.pop(reader));
    BOOST_CHECK_EQUAL(reader, "three");
    BOOST_CHECK(fifo.pop(reader));
    BOOST_CHECK_EQUAL(reader, "four");

    // Can't read, because it's empty.
    BOOST_CHECK(!fifo.pop(reader));

}

BOOST_AUTO_TEST_CASE(MultiThreadedInsertionTest) {

    fb::CircularFifo<std::string> fifo(128);

    const size_t messages_to_generate = 10000;

    std::atomic<size_t> number_items_produced{0};
    std::condition_variable start_consumption;

    auto verify_messages = std::async(std::launch::async, [&]{

        bool sequence_is_valid = true;

        // wait until the producer has added enough to the queue
        std::mutex mutex;
        std::unique_lock<std::mutex> lk(mutex);
        size_t cnt_queue_was_empty = 0;

        start_consumption.wait(lk, [&]{
            return number_items_produced.load() >= 64;
        });

        std::string read_value;
        for (size_t cnt = 0; cnt != messages_to_generate && sequence_is_valid;) {
            bool success = fifo.pop(read_value);
            if (!success) {
                // queue empty, wait a bit longer and then try again
                cnt_queue_was_empty++;
            } else {
                sequence_is_valid = sequence_is_valid & (read_value == std::to_string(cnt));
                ++cnt;
            }
            fb::short_sleep(fb::microseconds(10));
        }

        std::cout << "Verification was successfull : " << std::boolalpha << sequence_is_valid << "." << std::endl;
        std::cout << "Queue was '" << cnt_queue_was_empty << "' times empty." << std::endl;

        return sequence_is_valid;

    });

    auto produce_messages = std::async(std::launch::async, [&]{

        size_t cnt_queue_was_full = 0;

        for (size_t cnt = 0; cnt != messages_to_generate;) {
            
            bool success = fifo.push(std::to_string(cnt));

            if (!success) {
                // queue full, wait a bit longer and then try again
                cnt_queue_was_full++;
            } else {
                // We only have one waiting consumer thread, so notify_one 
                // is sufficient.
                number_items_produced++;
                start_consumption.notify_one();
                ++cnt;
            }
            fb::short_sleep(fb::microseconds(10));

        }

        std::cout << "Production of messages is complete." << std::endl;
        std::cout << "Queue was '" << cnt_queue_was_full << "' times full." << std::endl;

    });

    bool verification_did_succeed = verify_messages.get();

    BOOST_CHECK(verification_did_succeed);

    // Note that this is actually not necessary, because verification can't
    // complete until production has completed, and also, std::future as 
    // launched from std::async would wait in their destructor (unfortunately)
    produce_messages.wait();
}

class DetectMove {

public:
    DetectMove() : move_count(0) { std::cout << "Constructor" << std::endl; }
    ~DetectMove() { std::cout << "Destructor" << std::endl; }
    DetectMove(const DetectMove& other) : move_count(other.move_count) { std::cout << "Copy Constructor" << std::endl; }
    DetectMove& operator=(const DetectMove& other) { std::cout << "Copy assignment operator" << std::endl; move_count = other.move_count; return *this;}
    DetectMove(DetectMove&& other) : move_count(1) { std::cout << "Move Constructor" << std::endl; }
    DetectMove& operator=(DetectMove&& other) { std::cout << "Move assignment operator" << std::endl; move_count = other.move_count+1; return *this; }            

    size_t move_count;

};

typedef boost::mpl::list<fb::CircularFifo<DetectMove>,fb::WaitingCircularFifo<DetectMove, fb::WaitingPolicy::SPIN>,fb::WaitingCircularFifo<DetectMove, fb::WaitingPolicy::BLOCK>> test_types;

// We also test this for the WaitingFifo wrappers
BOOST_AUTO_TEST_CASE_TEMPLATE(MoveTest, T, test_types) {

    T fifo(2);
    DetectMove a;
    const DetectMove& a_ref = a;
    std::cout << "Pushing R-Value ref" << std::endl;
    fifo.push(DetectMove());
    DetectMove a_out;
    fifo.pop(a_out);
    // One move was on push, the second on pop.
    BOOST_CHECK_EQUAL(a_out.move_count, 2);
    std::cout << "Pushing L-Value ref" << std::endl;
    fifo.push(a_ref);
    fifo.pop(a_out);
    // Move count is only 1, since we haven't moved on input in this case
    // (since it was an lref)
    BOOST_CHECK_EQUAL(a_out.move_count, 1);
    
}

BOOST_AUTO_TEST_CASE(EmptyAfterInitialization) {

    fb::CircularFifo<int> fifo(1);

    BOOST_REQUIRE(fifo.was_empty());

}

// TODO: make over SPIN and BLOCK types

template <fb::WaitingPolicy P>
void run_waiting_fifo_validation_test() {
    
    fb::WaitingCircularFifo<int, P> fifo{4};
    
    static const int max_count = 100000;
    
    std::thread producer_thread{[&]{
        
        int counter = 0;
        
        while (counter <= max_count)
        {
            
            if (fifo.push(counter))
                counter++;
            else
                std::this_thread::yield();
            
            // Somewhere in the middle wait to produce stuff
            if (counter == max_count/2) {
                
                std::this_thread::sleep_for(std::chrono::seconds{5});
            }
        }
        
    }};
    
    std::exception_ptr consumer_error;
    
    std::thread consumer_thread{[&]{
        
        try {
            
            int last_fetch = -1;
            int fetch = -1;
            
            while (fetch < max_count) {
                
                bool s = fifo.pop(fetch);
                
                // Always has to be true because we are waiting
                if (!s)
                    throw std::runtime_error("Return value of pop was false.");
                
                // Numbers should be montonically increasing
                if (fetch != last_fetch+1)
                    throw std::runtime_error("Numbers not monotonically increasing: last_fetch=" + std::to_string(last_fetch) + ", fetch=" + std::to_string(fetch) + ".");
                
                last_fetch = fetch;
            }
            
            
        } catch (...) {
            
            consumer_error = std::current_exception();
        }
        
    }};
    
    // Let the boost test runner deal with this.
    if (consumer_error != nullptr)
        std::rethrow_exception(consumer_error);
    
    producer_thread.join();
    consumer_thread.join();
    
}

BOOST_AUTO_TEST_CASE(WaitingFifoValidation_Spin) {
    
    BOOST_CHECK_NO_THROW(run_waiting_fifo_validation_test<fb::WaitingPolicy::SPIN>());
}

BOOST_AUTO_TEST_CASE(WaitingFifoValidation_Block) {
    
    BOOST_CHECK_NO_THROW(run_waiting_fifo_validation_test<fb::WaitingPolicy::BLOCK>());
}

template <fb::WaitingPolicy P>
void run_fifo_cancelation_test()
{
    
    fb::WaitingCircularFifo<int, P> fifo{4};
    
    BOOST_REQUIRE(!fifo.has_been_canceled());
    
    // queue is empty, popping will cause wait until element is there
    
    std::thread cancel_asynchronously{[&]{
        
        // Canceling the FIFO after one second
        std::this_thread::sleep_for(std::chrono::seconds{5});
        fifo.cancel();
    }};
    
    int element = 0;
    
    // This will block until we've canceled the FIFO
    BOOST_CHECK_THROW(fifo.pop(element), fb::CanceledFifoError);
    
    BOOST_REQUIRE(fifo.has_been_canceled());
    
    cancel_asynchronously.join();
    
}

BOOST_AUTO_TEST_CASE(WaitingFifoCancelation_Spin) {
    
    run_fifo_cancelation_test<fb::WaitingPolicy::SPIN>();
}

BOOST_AUTO_TEST_CASE(WaitingFifoCancelation_Block) {
    
    run_fifo_cancelation_test<fb::WaitingPolicy::BLOCK>();
}

template <fb::WaitingPolicy P>
void run_fifo_optional_wait_test()
{
    fb::WaitingCircularFifo<int, P> fifo{4};
    
    std::mutex cv_mutex;
    std::condition_variable cv;
    
    bool produce_third_element = false;
    bool produce_fourth_element = false;
    
    // At first only produces three elements
    std::thread producer_thread{[&]{
        
        bool& do_produce_third = produce_third_element;
        bool& do_produce_fourth = produce_fourth_element;
        
        fifo.push(0);
        
        std::this_thread::sleep_for(std::chrono::seconds{5});
        
        fifo.push(1);

        {
            std::unique_lock<std::mutex> lk{cv_mutex};
            cv.wait(lk, [&]{
                
                return do_produce_third;
                
            });
            
            fifo.push(2);
        }
        
        {
            std::unique_lock<std::mutex> lk{cv_mutex};
            cv.wait(lk, [&]{
                
                return do_produce_fourth;
                
            });
            
            fifo.push(3);
        }
        
    }};
    
    int el = 0;
    
    bool s = fifo.pop(el, true);
    BOOST_REQUIRE(s);
    BOOST_REQUIRE_EQUAL(el, 0);
    
    // This should now block until we have it
    s = fifo.pop(el, true);
    BOOST_REQUIRE(s);
    BOOST_REQUIRE_EQUAL(el, 1);
    
    // At this point the other thread waits for me to notify it, so let's check
    // non-blocking style
    s = fifo.pop(el, false);
    BOOST_REQUIRE_EQUAL(s, false);
    BOOST_REQUIRE_EQUAL(el, 1);
    
    // Ok, now notify the thread
    {
        std::lock_guard<std::mutex> lk{cv_mutex};
        produce_third_element = true;
        cv.notify_one();
    }
    
    // Now let's check again and again, non-blocking style. Should eventually
    // get it.
    while (!(s = fifo.pop(el, false))) {
        std::this_thread::yield();
    }
    
    BOOST_REQUIRE_EQUAL(s, true);
    BOOST_REQUIRE_EQUAL(el, 2);
    
    
    // Now, the thread is again in a waiting condition, waiting for me to notify
    // it to produce the fourth element
    s = fifo.pop(el, false);
    BOOST_REQUIRE_EQUAL(s, false);
    BOOST_REQUIRE_EQUAL(el, 2);
    
    // Ok, now notify the thread
    {
        std::lock_guard<std::mutex> lk{cv_mutex};
        produce_fourth_element = true;
        cv.notify_one();
    }
    
    // Now let's wait blocking style again, should get it now "immediately"
    s = fifo.pop(el, true);
    BOOST_REQUIRE_EQUAL(s, true);
    BOOST_REQUIRE_EQUAL(el, 3);
    
    // Ok, all done.
    
    producer_thread.join();
    
}

BOOST_AUTO_TEST_CASE(WaitingFifoBlock_OptionalWait_Spin) {
    
    run_fifo_optional_wait_test<fb::WaitingPolicy::SPIN>();
}

BOOST_AUTO_TEST_CASE(WaitingFifoBlock_OptionalWait_Block) {
    
    run_fifo_optional_wait_test<fb::WaitingPolicy::BLOCK>();
}

BOOST_AUTO_TEST_SUITE_END()