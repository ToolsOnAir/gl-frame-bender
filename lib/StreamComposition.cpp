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
#include "Precompile.h"
#include "StreamComposition.h"
#include "Logging.h"
#include "StreamSource.h"
#include "StreamRenderer.h"

namespace fb = toa::frame_bender;

fb::StreamComposition::StreamComposition(
    std::string name,
    std::vector<std::shared_ptr<StreamSource>> input_sources,
    std::unique_ptr<StreamRenderer> renderer,
    OutputCallback output_callback) : 
        name_(std::move(name)),
        input_sources_(std::move(input_sources)),
        renderer_(std::move(renderer)),
        output_callback_(std::move(output_callback))
{
    FB_LOG_DEBUG << "Initialized StreamComposition '" << name_ << "'.";
}

const std::string& fb::StreamComposition::name() const {
    return name_;
}

fb::StreamSource& fb::StreamComposition::first_source() {
    return *input_sources_.front();
}

const fb::StreamRenderer* fb::StreamComposition::renderer() const {

    return renderer_.get();

}

fb::StreamRenderer* fb::StreamComposition::renderer() {

    return renderer_.get();

}