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
#ifndef TOA_FRAME_BENDER_CHRONO_UTILS_H
#define TOA_FRAME_BENDER_CHRONO_UTILS_H

/**
 * Utilities related to compile-time time-based measures, e.g. as used
 * in thread-scheduling or performance measurement. Note that we favor
 * boost::chrono here instead of std::chrono since VS2012 implementation does
 * not provide a high-resolution accurate clock.
 * https://connect.microsoft.com/VisualStudio/feedback/details/719443/c-chrono-headers-high-resolution-clock-does-not-have-high-resolution
 *
 * Some of the utilties of this header are based on Kjell Hedström's 
 * g2log library implementation.
 */ 

#include "Logging.h"

// TODO: use C++11 chrono, once VS2012 caight up with an accurate timer
#include <boost/chrono.hpp>
#include <boost/chrono/chrono_io.hpp>

namespace toa
{       

    namespace frame_bender {

        typedef boost::chrono::high_resolution_clock clock;
        typedef boost::chrono::microseconds microseconds;
        typedef boost::chrono::milliseconds milliseconds;
        typedef boost::chrono::seconds seconds;

        static inline clock::time_point now(){return clock::now();}

        //microseconds intervalUs(const clock::time_point& t1, const clock::time_point& t0)
        //{return std::boost::duration_cast<microseconds>(t1 - t0);}

        //milliseconds intervalMs(const clock::time_point& t1,const clock::time_point& t0)
        //{return std::boost::duration_cast<milliseconds>(t1 - t0);}

        template<typename Duration>
        inline void short_sleep(Duration d)
        {                            
            clock::time_point const start=clock::now(), stop=start+d;
            do{
                std::this_thread::yield();
            } while(clock::now()<stop);
        }

        inline static void adhoc_diff(bool is_tic) {

            static clock::time_point tic;
            static clock::time_point last;
            static clock::duration accumulator;
            static uintmax_t cnt = 0;

            clock::time_point now = clock::now();

            if (last == clock::time_point())
                last = now;

            if (is_tic) {

                tic = now;

            } else {

                accumulator += (now - tic);
                cnt++;

                if ( (now - last) > seconds(2)) {

                    clock::duration avg = accumulator / cnt;

                    FB_LOG_INFO 
                        << "TIC/TOC: Took " 
                        << boost::chrono::duration_cast<boost::chrono::microseconds>(avg) 
                        << " for " << cnt << " iterations.";

                    cnt = 0;
                    last = now;

                }

            }

        }

        inline static void tic() { adhoc_diff(true); }
        inline static void toc() { adhoc_diff(false); }

        //class StopWatch
        //{
        //    clock::time_point start_;
        //public:
        //    StopWatch() : start_(clock::now()){}
        //    clock::time_point restart()         { start_ = clock::now(); return start_;}
        //    microseconds elapsedUs()            { return intervalUs(now(), start_);}
        //    milliseconds elapsedMs()            {return intervalMs(now(), start_);}
        //};
    }
} // g2

#endif // TOA_FRAME_BENDER_CHRONO_UTILS_H
