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
#ifndef TOA_FRAME_BENDER_MATH_CONSTANTS_H
#define TOA_FRAME_BENDER_MATH_CONSTANTS_H

#include "glm/glm.hpp"

namespace toa {
    namespace frame_bender {
        namespace math_constants {

            // Should we use double in here?
            /**
             * @return Returns the matrix that converts normalized R'G'B'
             * tristimulus (range [0...1]) to binary offset form of Y'CbCr, 
             * as conforming to BT.709 standard (extent = 219 for luma, 224
             * for chroma differences). 
             * Note that this relates to the matrix in Eq. 30.6 of Poynton
             */
            static inline const glm::mat3x3& bt709_rgbnorm_to_ycbcr_offset_binary_8bit() {
                static const glm::mat3x3 m(
                    46.559400000000004f,-25.664151756844149f,112.0f,
                    156.62879999999998f,-86.335848243155851f,-101.73025146050293f,
                    15.8118f,111.99999999999999f,-10.26974853949708f);
                return m;
            }

            /**
             * @return The inverse of @see bt709_rgbnorm_to_ycbcr_offset_binary_8bit.
             */
            static inline const glm::mat3x3& bt709_rgbnorm_to_ycbcr_offset_binary_8bit_inverse() {
                static const glm::mat3x3 m(
                   0.0045662100456621011f,0.0045662100456621002f,0.0045662100456621019f,
                    -1.1496260292937727e-19f,-0.00083626907558325358f,0.0082839285714285723f,
                    0.007030357142857142f,-0.0020898405041546823f,-6.5052130349130266e-19f);
                return m;
            }

            static inline const glm::mat3x3& bt709_rgbnorm_to_ycbcr_offset_binary_10bit() {
                static const glm::mat3x3 m = bt709_rgbnorm_to_ycbcr_offset_binary_8bit() * 4.0f;
                return m;
            }

            static inline const glm::mat3x3& bt709_rgbnorm_to_ycbcr_offset_binary_10bit_inverse() {
                static const glm::mat3x3 m = bt709_rgbnorm_to_ycbcr_offset_binary_8bit_inverse() * 0.25f;
                return m;
            }

        }
    }
}

#endif // TOA_FRAME_BENDER_MATH_CONSTANTS_H
