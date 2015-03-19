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
#include "FrameCompositionInputStage.h"

#include "Logging.h"
#include "ProgramOptions.h"
#include "StreamComposition.h"
#include "StreamSource.h"

namespace fb = toa::frame_bender;

fb::FrameCompositionInputStage::FrameCompositionInputStage(size_t pipeline_size)
{

    // TODO: using a global option here might not be the best option
    SampleFunction fnc;

    if (ProgramOptions::global().sample_stages()) {
        fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
    }

    stage_ = utils::create_producer_stage<TokenFrame>(
        "FrameInput",
        std::bind(
            &FrameCompositionInputStage::perform, 
            this, 
            std::placeholders::_1),
        std::vector<TokenFrame>(pipeline_size),
        std::move(fnc)
    );

}

fb::FrameCompositionInputStage::~FrameCompositionInputStage() {

    if (ProgramOptions::global().sample_stages()) {
        auto stats = sampler_.collect_statistics();
        for (const auto& stat : stats) {
            FB_LOG_INFO << stage_.name() << ": " << stat;
        }
    }

}

void fb::FrameCompositionInputStage::set_composition(StreamComposition* composition) {

    head_composition_.store(composition);

    if (composition != nullptr) {
        FB_LOG_INFO << "Transitioned '" << composition->name() << "' into running state.";
    } else {
        FB_LOG_INFO << "Canceling stage (composition==nullptr).";
    }

}

fb::StageCommand fb::FrameCompositionInputStage::perform(TokenFrame& token_out) {

    StageCommand status = StageCommand::NO_CHANGE;

    // signal the source that the upstream frame token is not needed anymore
    if (token_out.composition != nullptr && token_out.frame.is_valid()) {
        token_out.composition->first_source().invalidate_frame(std::move(token_out.frame));
    }

    // Get snapshot
    StreamComposition* composition = head_composition_.load();

    if (composition != nullptr) {

        if (composition->first_source().state() != StreamSource::State::END_OF_STREAM) {

            StreamSource& source = composition->first_source();

            // We are assuming that some render context has been attached
            // We are also currently ignoring the GL sync, as we assume that this is dealt from the outside (being called in sequence, e.g.) 

            Frame frame;
            bool frame_available = source.pop_frame(frame);

            FB_ASSERT_MESSAGE(
                frame_available, 
                "Could not retrieve frame from input source during upload.");

            FB_ASSERT_MESSAGE(
                frame.is_valid(), 
                "Invalid frame during upload phase.");

#ifdef DEBUG_PIPE_FLOW

            FB_LOG_DEBUG
                << stage_.name() << ": Got frame '" << frame.time() << ", passing it on.";

#endif

            token_out.frame = std::move(frame);
            token_out.composition = composition;

        } else {
            status = StageCommand::STOP_EXECUTION;
        }

    } else {
        status = StageCommand::STOP_EXECUTION;
    }

    return status;

}