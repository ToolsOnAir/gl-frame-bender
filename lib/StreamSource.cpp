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
#include <regex>
#include <algorithm>
#include <iomanip>

#include "StreamSource.h"
#include "Logging.h"
#include "ProgramOptions.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bf = boost::filesystem;
namespace fb = toa::frame_bender;

fb::StreamSource::StreamSource(std::string name)
    : name_(std::move(name)), state_(State::INITIALIZED)

{
    // TODO: make this a program option?
    input_queue_ = utils::make_unique<CircularFifo<Frame>>(32);
    FB_ASSERT_MESSAGE(state_.is_lock_free(), "Expecting state ot be lock-free.");
}

fb::StreamSource::~StreamSource() {}

bool fb::StreamSource::pop_frame(Frame& out_frame) {

    if (state_ == State::INITIALIZED || state_ == State::END_OF_STREAM) {
        throw std::runtime_error("Can't pop frame for invalid reader state in '" + name_ + "'.");
    }

    bool success = input_queue_->pop(out_frame);

    if (success) {

        // TODO: think about detaching this to some thread, probably
        // using async?
        frame_has_been_used(out_frame);

    } else {

        FB_LOG_ERROR << "StreamSource exhausted, could not pop frame.";

    }

    return success;
}

fb::StreamSource::State fb::StreamSource::state() const {
    return state_.load();
}

void fb::StreamSource::invalidate_frame(Frame&& f) {
    // does nothing
}

std::ostream& fb::operator<< (std::ostream& out, const fb::StreamSource::State& v)
{

    switch (v) {

        case fb::StreamSource::State::INITIALIZED:
            out << "INITIALIZED";
            break;

        case fb::StreamSource::State::READY_TO_READ:
            out << "READY_TO_READ";
            break;

        case fb::StreamSource::State::END_OF_STREAM:
            out << "END_OF_STREAM";
            break;

        default:
            out << "<unknown>";
            break;

    }

    return out;

}

fb::PrefetchedImageSequence::PrefetchedImageSequence(
    const std::string& frame_folder,
    const std::string& regex_pattern,
    const ImageFormat& image_format,
    const Time& frame_duration,
    size_t loop_count) : 
        StreamSource("Prefetch ('" + frame_folder + "/" + regex_pattern +"')"),
        num_frames_prefetched_(0),
        total_data_size_(0),
        frame_duration_(frame_duration),
        loop_count_(loop_count)

{
    
    bf::path frames_folder_path = bf::path(ProgramOptions::global().input_sequences_folder());
    frames_folder_path /= bf::path(frame_folder);

    if (!bf::exists(frames_folder_path))
        throw std::invalid_argument("Path '" + frames_folder_path.string() +"' does not exist.");

    if (!bf::is_directory(frames_folder_path))
        throw std::invalid_argument("Path '" + frame_folder +"' is not a folder.");

    // iterator of the file system
    auto dir_start = bf::directory_iterator(frames_folder_path);

    std::vector<bf::directory_entry> filtered_files;

    std::regex frame_pattern_regex(regex_pattern);

    std::copy_if(
        dir_start, 
        bf::directory_iterator(), 
        std::back_inserter(filtered_files),
        [&](const bf::directory_entry& entry) {

            bool does_match = std::regex_match(
                entry.path().filename().string(), 
                frame_pattern_regex);

            return does_match;
    });

    if (filtered_files.empty()) {
        FB_LOG_ERROR << "No files in folder '" << frames_folder_path << "' matched with pattern '" << regex_pattern << "'.";
        throw std::runtime_error("Input sequence is empty.");
    }

    std::sort(std::begin(filtered_files), std::end(filtered_files));

    uintmax_t total_size = 0;
    for (const auto& entry : filtered_files)
        total_size += bf::file_size(entry.path());

    FB_LOG_INFO << 
        "Prefetching " << filtered_files.size() << " frames from folder '" 
        << frame_folder << "' with pattern '" << regex_pattern << "'.";

    FB_LOG_INFO << "First frame : '" << filtered_files.front() << "'.";
    FB_LOG_INFO << "Last frame : '" << filtered_files.back() << "'.";

    FB_LOG_INFO << "Frame duration is '" << frame_duration_ << "'.";

    float mb = static_cast<float>(total_size) / 1e6f;

    FB_LOG_INFO << "Reading " << std::setprecision(4) << mb << "mb of data.";

    Time time_stamp(0, 1);
    
    end_time_stamp_ = frame_duration_ * (filtered_files.size() * loop_count_ -1);

    for (const auto& frame_dir_entry : filtered_files) {

        Frame f(
            image_format,
            time_stamp,
            (time_stamp == end_time_stamp_)
            );
        
        time_stamp += frame_duration;

        auto byte_size = bf::file_size(frame_dir_entry.path());

        if (f.image_data_size() != byte_size) {
            FB_LOG_ERROR 
                << "Unexpected byte size of image on disk vs. specified format. "
                << "Disk: " << byte_size << " bytes, format requires " 
                << f.image_data_size() << " bytes. Format: ["
                << f.image_format() << "].";
            throw std::runtime_error("Invalid image size. Check format.");
        }

        bf::ifstream frame_reader(
            frame_dir_entry.path(),
            std::ios::in | std::ios::binary);

        FB_ASSERT(frame_reader.good());

        frame_reader.seekg(0, std::ios::beg);

        frame_reader.read(
            reinterpret_cast<char*>(f.image_data()), 
            f.image_data_size());

        // Move it into the vector
        frames_repository_.push(std::move(f));

    }

    FB_ASSERT(frames_repository_.size() == filtered_files.size());

    FB_LOG_INFO << "Successfully read all frames.";

    num_frames_prefetched_ = frames_repository_.size();
    total_data_size_ = frames_repository_.empty() ? 0 : frames_repository_.front().image_data_size() * num_frames_prefetched_;

    size_t cnt = 0;
    while (!input_queue_->was_full() && !frames_repository_.empty()) {
        // We can use that safely as we are for sure the only thread 
        // working on the queue, i.e. we have a fully consistent view
        bool success = input_queue_->push(std::move(frames_repository_.front()));
        FB_ASSERT(success);
        frames_repository_.pop();
        cnt++;
    }

    FB_LOG_INFO << "Preheated fifo with " << cnt << " frames.";

    state_ = State::READY_TO_READ;
}

void fb::PrefetchedImageSequence::frame_has_been_used(const Frame& frame) {

    // For each consumed frame, we add another (if we have one)
    if (!frames_repository_.empty()) {

        bool success = input_queue_->push(std::move(frames_repository_.front()));
        FB_ASSERT(success);
        frames_repository_.pop();    
    }

    // When the last frame has been used, we mark the stream to be fully 
    // done. TODO: enable looping by default in here
    if (frame.marks_end_of_sequence()) {
        // Sanity check
        FB_ASSERT(state_ == State::READY_TO_READ);
        state_ = State::END_OF_STREAM;
    }

}

void fb::PrefetchedImageSequence::invalidate_frame(Frame&& f) {

    f.set_time(f.time() + num_frames_prefetched_*frame_duration_);
    f.set_marks_end_of_sequence(f.time() == end_time_stamp_);
    frames_repository_.push(std::move(f));

}

