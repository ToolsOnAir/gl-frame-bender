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

#include "ByPassUploadStage.h"

#include "Logging.h"

#include "ProgramOptions.h"
#include "StreamComposition.h"
#include "StreamSource.h"

namespace fb = toa::frame_bender;

fb::ByPassUploadStage::ByPassUploadStage(
    ImageFormat image_format) :   
    image_format_(std::move(image_format)),
    head_composition_(nullptr)
{

    texture_ids_ = std::vector<GLuint>(
        ProgramOptions::global().source_texture_rr_count(),
        0);

    glGenTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());

    std::unique_ptr<uint8_t[]> init_data = std::unique_ptr<uint8_t[]>(new uint8_t[image_format_.image_byte_size()]);

    std::fill(&init_data[0], &init_data[image_format_.image_byte_size()], 0);

    std::for_each(
        std::begin(texture_ids_),
        std::end(texture_ids_),
        [&](const GLuint& texture_id) {
        glBindTexture(GL_TEXTURE_2D, texture_id);

        // TODO: are these necessary?
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            image_format_.gl_native_format().tex_internal_format,
            image_format_.gl_native_format().tex_width,
            image_format_.gl_native_format().tex_height);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            image_format_.gl_native_format().tex_width,
            image_format_.gl_native_format().tex_height,
            image_format_.gl_native_format().data_format,
            image_format_.gl_native_format().data_type,
            init_data.get());

        glBindTexture(GL_TEXTURE_2D, 0);

    });

    // Set initial tokens for upload PBO
    std::vector<TokenGL> tex_init;
    for (auto& tex_id : texture_ids_) {
        TokenGL new_token(
            0,
            tex_id,
            image_format_,
            0, 0, nullptr, nullptr);
        tex_init.push_back(new_token);
    }

    // TODO: using a global option here might not be the best option
    SampleFunction fnc;

    if (ProgramOptions::global().sample_stages()) {
        fnc = std::bind(&StageSampler::sample, &sampler_, std::placeholders::_1);
    }

    stage_ = utils::create_producer_stage<TokenGL>(
        "ByPassUpload",
        std::bind(
            &ByPassUploadStage::perform, 
            this, 
            std::placeholders::_1),
        tex_init,
        std::move(fnc)
    );

}

fb::ByPassUploadStage::~ByPassUploadStage() 
{

    if (ProgramOptions::global().sample_stages()) {
        auto stats = sampler_.collect_statistics();
        for (const auto& stat : stats) {
            FB_LOG_INFO << stage_.name() << ": " << stat;
        }
    }

    glDeleteTextures(
        static_cast<GLsizei>(texture_ids_.size()), 
        texture_ids_.data());
}

void fb::ByPassUploadStage::set_composition(StreamComposition* composition) {

    head_composition_.store(composition);

    FB_LOG_INFO << "Transitioned '" << composition->name() << "' into running state.";

}

fb::StageCommand fb::ByPassUploadStage::perform(TokenGL& token_out) {

    StageCommand status = StageCommand::NO_CHANGE;

    // Get snapshot
    StreamComposition* composition = head_composition_.load();

    if (composition != nullptr) {

        if (composition->first_source().state() != StreamSource::State::END_OF_STREAM) {

            StreamSource& source = composition->first_source();

            Frame frame;
            bool frame_available = source.pop_frame(frame);

            FB_ASSERT_MESSAGE(
                frame_available, 
                "Could not retrieve frame from input source during upload.");

            FB_ASSERT_MESSAGE(
                frame.is_valid(), 
                "Invalid frame during upload phase.");

            // Note that the token's tex_id is already set... and that
            // we DON'T require a GL context to be set during perform
            // (but of course during construction we do)

            // Set timestamp
            token_out.time_stamp = frame.time();

            token_out.composition = composition;

            // done with this frame
            source.invalidate_frame(std::move(frame));

        } else {
            status = StageCommand::STOP_EXECUTION;
        }

    } else {
        status = StageCommand::STOP_EXECUTION;
    }

    return status;

}
