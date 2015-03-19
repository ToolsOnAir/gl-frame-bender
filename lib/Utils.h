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
#ifndef TOA_FRAME_BENDER_UTILITIES_H
#define TOA_FRAME_BENDER_UTILITIES_H

#include <memory>
#include <string>
#include <iterator>

#define MEM_IS_ALIGNED(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

namespace toa {
    namespace frame_bender {
        namespace utils {

            static std::string strip_path(const std::string& file, const char sep) {
                auto last_sep = file.rfind(sep);
                if (last_sep != std::string::npos && last_sep+1 < file.size()) {
                    return file.substr(last_sep+1, std::string::npos);
                } else {
                    return file;
                }
            }

            // Returns the following element, or the first element, if already
            // at the end. Only requires forward iterators.
            template <typename T, typename Itr>
            inline Itr next_wrap(const Itr& itr, const T& container) {

                Itr next_itr = itr != std::end(container) ? std::next(itr) : std::begin(container);
                return (next_itr != std::end(container) ? next_itr : std::begin(container));

            }

            class NoCopyingOrMoving {
            public:
                NoCopyingOrMoving() {}
                virtual ~NoCopyingOrMoving() {}
            
            private:
                NoCopyingOrMoving& operator=(const NoCopyingOrMoving&);
                NoCopyingOrMoving(const NoCopyingOrMoving&);

                NoCopyingOrMoving& operator=(NoCopyingOrMoving&&);
                NoCopyingOrMoving(NoCopyingOrMoving&&);
            };

            class NoCopying {
            public:
                NoCopying() {}
                virtual ~NoCopying() {}
            
            private:
                NoCopying& operator=(const NoCopying&);
                NoCopying(const NoCopying&);

            };

            // Helper to create a unique-ptr allocated C++ object. Described here
            // http://herbsutter.com/gotw/_102/
            template<typename T, typename ...Args>
            std::unique_ptr<T> make_unique(Args&& ...args ) {
                return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) ); 
            }    

        }
    }
}

#endif // TOA_FRAME_BENDER_UTILITIES_H
