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
#ifndef TOA_FRAME_BENDER_CIRCULAR_FIFO_HELPERS_H
#define TOA_FRAME_BENDER_CIRCULAR_FIFO_HELPERS_H

#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <thread>

#include "CircularFifo.h"
#include "ChronoUtils.h"

#ifndef _MSC_VER
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#ifndef _MSC_VER
#define STATIC_TSPL
#else
#define STATIC_TSPL static
#endif

namespace toa {
    namespace frame_bender {

        enum class WaitingPolicy {
            SPIN,
            BLOCK
        };

        class CanceledFifoError : public std::runtime_error {
            
        public:
            CanceledFifoError() : std::runtime_error("WaitingCircularFifo was canceled.") {}
            
        };

        
        template <typename Element, WaitingPolicy policy>
        class WaitingCircularFifo {

        public:

            static const WaitingPolicy kWaitingPolicy = policy;
            
            typedef Element ElementType;

            WaitingCircularFifo(
                size_t size) : 
                    fifo_(size),
                    was_canceled_(false) {}

            bool pop(Element& item, bool wait_for_element = true);
            bool push(Element&& item);
            bool push(const Element& item);
            bool was_full() const;
            bool was_empty() const;
            size_t size() const;
            size_t had_num_elements() const;

            // Can be canceled from any thread externally. This will cause
            // CanceledFifoError to be thrown for a currently waiting pop
            // operation
            void cancel();
            bool has_been_canceled() const;

        private:

            template <typename InsertElement>
            bool push_element(InsertElement&& item);

            CircularFifo<Element> fifo_;
            std::mutex mutex_;
            std::condition_variable condition_;
            
            std::atomic<bool> was_canceled_;
        };

        namespace detail {

            template <WaitingPolicy policy>
            static bool wait_pop(
                const std::function<bool()>& pop_function,
                std::mutex& cv_mutex,
                std::condition_variable& cv) 
            {

                // Making sure that we only hit the specializations
                static_assert(policy == WaitingPolicy::SPIN || policy == WaitingPolicy::BLOCK,
                              "Invalid wait policy during compilation.");
                
                return false;
            }

            template <>
			STATIC_TSPL bool wait_pop<WaitingPolicy::SPIN>(
                const std::function<bool()>& pop_function,
                std::mutex&,
                std::condition_variable&) 
            {
                
                while (!pop_function()) {
                    std::this_thread::yield();
                }
                return true;
            }

            template <>
			STATIC_TSPL bool wait_pop<WaitingPolicy::BLOCK>(
                const std::function<bool()>& pop_function,
                std::mutex& cv_mutex,
                std::condition_variable& cv) 
            {
                
                std::unique_lock<std::mutex> lk(cv_mutex);
                cv.wait(lk,
                        [&]() {
                            return pop_function();
                        });

                return true;
            }
            
            template <WaitingPolicy policy>
            static void cancel(std::atomic<bool>& was_canceled_atomic,
                        std::condition_variable& cv)
            {
                
                // Making sure that we only hit the specializations
                static_assert(policy == WaitingPolicy::SPIN || policy == WaitingPolicy::BLOCK,
                              "Invalid wait policy during compilation.");

            }
            
            template <>
			STATIC_TSPL void cancel<WaitingPolicy::SPIN>(std::atomic<bool>& was_canceled,
                                             std::condition_variable& cv)
            {
                
                was_canceled = true;
            }
            
            template <>
			STATIC_TSPL void cancel<WaitingPolicy::BLOCK>(std::atomic<bool>& was_canceled,
                                              std::condition_variable& cv)
            {
                
                was_canceled = true;
                
                cv.notify_one();
            }

        } 

        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::pop(Element& item, bool wait_for_element) {

            auto pop_function = [&]{
                
                if (was_canceled_.load())
                    throw CanceledFifoError{};
                
                return fifo_.pop(item);
            };
            
            bool item_available = pop_function();

            if(!item_available && wait_for_element) {
                item_available = detail::wait_pop<policy>(pop_function,
                                                          mutex_,
                                                          condition_);
            }

            return item_available;
        }
        
        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::push(Element&& item) {
            return push_element(std::forward<Element>(item));
        }

        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::push(const Element& item) {
            return push_element(item);
        }

        template <typename Element, WaitingPolicy policy>
        template <typename InsertElement>
        bool WaitingCircularFifo<Element, policy>::push_element(InsertElement&& item) {
            // Because we can't bind r-value refs in lambdas, we'll have to use
            // an if here... should be cheap, though.
            static const bool do_notify = (policy == WaitingPolicy::BLOCK);
            if (do_notify) {
                std::lock_guard<std::mutex> lk(mutex_);
                bool result = fifo_.push(std::forward<InsertElement>(item));
                if (result)
                    condition_.notify_one();
                return result;
            } else {
                return fifo_.push(std::forward<InsertElement>(item));
            }
        }

        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::was_empty() const {
            return fifo_.was_empty();
        }

        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::was_full() const {
            return fifo_.was_full();
        }

        template <typename Element, WaitingPolicy policy>
        size_t WaitingCircularFifo<Element, policy>::size() const {
            return fifo_.size();
        }

        template <typename Element, WaitingPolicy policy>
        size_t WaitingCircularFifo<Element, policy>::had_num_elements() const {
            return fifo_.had_num_elements();
        }
        
        template <typename Element, WaitingPolicy policy>
        void WaitingCircularFifo<Element, policy>::cancel() {
            detail::cancel<policy>(was_canceled_, condition_);
        }
        
        template <typename Element, WaitingPolicy policy>
        bool WaitingCircularFifo<Element, policy>::has_been_canceled() const {
            return was_canceled_;
        }

    }
}

#ifndef _MSC_VER
#pragma clang diagnostic pop
#endif

#endif // TOA_FRAME_BENDER_WAITING_FIFO_H
