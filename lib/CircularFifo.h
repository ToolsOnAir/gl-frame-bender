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
// This CircularFifo implementation is a single-producer, single-consumer
// thread-safe bounded queue. Its implementation is based on 
// Kjell Hedström's implementation as given on 
// http://www.codeproject.com/Articles/43510/Lock-Free-Single-Producer-Single-Consumer-Circular

#ifndef TOA_FRAME_BENDER_CIRCULARFIFO_H
#define TOA_FRAME_BENDER_CIRCULARFIFO_H

#include <atomic>
#include <cstddef>

namespace toa {

    namespace frame_bender {

        template<typename Element> 
        class CircularFifo {
        public:

            CircularFifo(size_t size);
            virtual ~CircularFifo();

            bool push(Element&& item);
            bool push(const Element& item);

            // TODO: add a unique_ptr or shared_ptr return?
            // Although, this would have the same implications mentioned
            // in C++ concurrency: allocation COULD fail in pop and throw.
            bool pop(Element& item);

            bool was_empty() const;
            bool was_full() const;
            size_t had_num_elements() const;
            bool is_lock_free() const;
            
            size_t size() const;

        private:

            template <typename InsertElement> 
            bool push_element(InsertElement&& item);

            size_t increment(size_t idx) const; 

            const size_t _size;
            const size_t _capacity;

            std::atomic<size_t> _tail;
            std::unique_ptr<Element[]> _array;
            std::atomic<size_t> _head;
        };

        template<typename Element>
        CircularFifo<Element>::CircularFifo(size_t size) : _size(size), _capacity(size+1), _tail(0), _head(0) {
            _array = std::unique_ptr<Element[]>(new Element[_capacity]);
        }

        template<typename Element>
        CircularFifo<Element>::~CircularFifo(){}

        template<typename Element>
        bool CircularFifo<Element>::push(Element&& item) {
            return push_element(std::forward<Element>(item));
        }

        template<typename Element>
        bool CircularFifo<Element>::push(const Element& item) {
            return push_element(item);
        }

        template<typename Element>
        template <typename InsertElement>
        bool CircularFifo<Element>::push_element(InsertElement&& item)
        {	
            const auto current_tail = _tail.load(std::memory_order_relaxed); 
            const auto next_tail = increment(current_tail); 
            if(next_tail != _head.load(std::memory_order_acquire))                           
            {	
                _array[current_tail] = std::forward<InsertElement>(item);
                _tail.store(next_tail, std::memory_order_release); 
                return true;
            }

            return false; // full queue

        }

        template<typename Element>
        bool CircularFifo<Element>::pop(Element& item)
        {
            const auto current_head = _head.load(std::memory_order_relaxed);  
            if(current_head == _tail.load(std::memory_order_acquire)) 
                return false; // empty queue

            item = std::move(_array[current_head]); 
            _head.store(increment(current_head), std::memory_order_release); 
            return true;
        }

        template<typename Element>
        bool CircularFifo<Element>::was_empty() const
        {
            // snapshot with acceptance of that this comparison operation is not atomic
            return (_head.load() == _tail.load()); 
        }


        // snapshot with acceptance that this comparison is not atomic
        template<typename Element>
        bool CircularFifo<Element>::was_full() const
        {
            const auto next_tail = increment(_tail.load()); // aquire, we dont know who call
            return (next_tail == _head.load());
        }

        // snapshot with acceptance that this comparison is not atomic
        template<typename Element>
        size_t CircularFifo<Element>::had_num_elements() const
        {
            const auto tail = _tail.load();
            const auto head = _head.load();
            
            size_t num_elements = 0;

            if (tail < head)
                num_elements = (tail + _capacity) - head;
            else
                num_elements = tail - head;

            return num_elements;
        }

        template<typename Element>
        bool CircularFifo<Element>::is_lock_free() const
        {
            return (_tail.is_lock_free() && _head.is_lock_free());
        }

        template<typename Element>
        size_t CircularFifo<Element>::increment(size_t idx) const
        {
            return (idx + 1) % _capacity;
        }

        template<typename Element>
        size_t CircularFifo<Element>::size() const {
            return _size;
        }

    }

} // memory_relaxed_aquire_release
#endif // TOA_FRAME_BENDER_CIRCULARFIFO_H
