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
#ifndef TOA_FRAME_BENDER_STREAM_DISPATCH_H
#define TOA_FRAME_BENDER_STREAM_DISPATCH_H

#include <memory>
#include <map>
#include <thread>
#include <set>
#include <mutex>
#include <atomic>
#include <bitset>

#include "Utils.h"
#include "UtilsGL.h"
#include "Context.h"
#include "StreamComposition.h"
#include "ChronoUtils.h"
#include "CircularFifo.h"
#include "CircularFifoHelpers.h"
#include <glad/glad.h>
#include "PipelineStage.h"
#include "StageDataTypes.h"

namespace toa {
    namespace frame_bender {

        class StreamRenderer;
        class StreamSource;

        class FrameCompositionInputStage;
        class CopyHostToMappedPBOStage;
        class UnmapPBOStage;
        class MapPBOStage;
        class CopyMappedPBOToHostStage;
        class FormatConverterStage;
        class PackTextureToPBOStage;
        class RenderStage;
        class UnpackPBOToTextureStage;
        class ByPassDownloadStage;
        class ByPassUploadStage;

        class FrameCompositionOutputStage;

        class StreamDispatch final : utils::NoCopyingOrMoving {

        public:

            // TODO-DEF: add required num of pipeline deepness

            enum class Flags : size_t {

                // this basically enables the dual-copy engine approach
                // if this flag is NOT set, then dispatch will only use a
                // single GL context
                MULTIPLE_GL_CONTEXTS,           

                // host-side memcpy for upload and input handling is asynchronouos
                ASYNC_INPUT,

                // host-side memcpy for download and output handling is asynchronous
                ASYNC_OUTPUT,

                // if supported, will use the persistently mapped buffers
                // with the ARB_buffer_storage core extension
                ARB_PERSISTENT_MAPPING,
                
                // Might improve performance on NVIDIA cards
                COPY_PBO_BEFORE_DOWNLOAD,

                COUNT
            };

            typedef std::bitset<static_cast<size_t>(Flags::COUNT)> FlagContainer;

            // NOTE: at the moment, only GL_MULTI_CONTEXT is implemented
            // Note that the constructor will throw if main_context is attached
            // to any thread during construction.
            StreamDispatch(
                std::string name,
                // This is the context that is used for rendering, and which
                // some window should be attached to.
                gl::Context* main_context,
                // TODO: the format shouldn't be fixed, but dependend on the
                // streams and setup of the compositions used.
                // Could move this to create_composition, which must be called
                // on the main render thread in this case.
                const ImageFormat& origin_format,
                // TODO: same goes here as for above
                const ImageFormat& render_format,
                bool enable_output_stages,
                bool enable_input_stages,
                bool enable_render_stages,
                const FlagContainer& flags);

            ~StreamDispatch();

            typedef std::function<void()> CompletionHandler;

            // TODO-C++11: could use perfect forwarding in here... i.e. fwd
            // to StreamComposition constructor?
            // Guess we'd have to wait for initializer lists and variadic
            // templates to be supported by VS2012?
            StreamComposition::ID create_composition(
                std::string name,
                std::vector<std::shared_ptr<StreamSource>> input_sources,
                std::unique_ptr<StreamRenderer> renderer, 
                StreamComposition::OutputCallback output_callback);

            bool is_composition(const StreamComposition::ID& id);
            void remove_composition(const StreamComposition::ID& id);

            void start_composition(
                const StreamComposition::ID& id, 
                CompletionHandler completion_handler,
                clock::time_point when = clock::now());

            void stop_composition(const StreamComposition::ID& id);
            bool is_composition_running(const StreamComposition::ID& id);

            // TODO: maybe add something like a std::promise or shared_future
            // to wait until a composition has been stopped. OR maybe add
            // a callback explicitly to create_composition? That really
            // dependends on further use cases.

            //void start_all();
            //bool is_running() const;
            //void stop();

            static void print_flags(std::ostream& out, const FlagContainer& v);

            void write_trace_file(
                const std::string& file_path, 
                const std::string& configurat_name_prefix);

        private:
             
            enum class TransferDirection {
                UPLOAD,
                DOWNLOAD
            };

            PboMemVector create_and_initialize_pbos(
                size_t num,
                size_t byte_size,
                TransferDirection transfer_direction,
                bool use_persistent_mappings);

            struct PipelineStageExecution;

            static std::string executions_to_string(const std::vector<PipelineStageExecution>& executions);

            static void run_pipeline_stages(const std::vector<PipelineStageExecution>& executions, const std::string& name);

            void execute_gl_upload_async();
            void execute_host_copy_input_async();
            void execute_gl_master();
            void execute_gl_download_async();
            void execute_host_copy_output_async();

            void initialize_gl_buffers();
            void define_pipeline();

            void wait_for_composition();

            StreamComposition::ID get_unique_id_for_name(const std::string& name) const;

            std::string name_;
            ImageFormat origin_format_;
            ImageFormat render_format_;

            std::map<StreamComposition::ID, StreamComposition> compositions_;
            
            // TODO-DEF: as an optimization, you could store the pointer to the composition
            // in here as a map-value already (that way the compositions_ list needn't
            // to be walked just to get the value
            // TODO: think about whether we should put an is_running attribtue
            // to the composition itself...

            //std::set<StreamComposition::ID> running_compositions_;
            //std::mutex running_compositions_lock_;
            std::atomic<bool> stop_threads_;

            // TODO-DEF: this shoud really be refactored into a single Stage
            // class with inheritance. At the moment it's a mix and mash
            // which is a bit confusing at times.

            struct PipelineStageExecution {

                PipelineStage* previous_stage;
                PipelineStage* stage;
                size_t input_load_constraint;

                PipelineStageExecution() : 
                    previous_stage(nullptr), 
                    stage(nullptr), 
                    input_load_constraint(0) {}

                PipelineStageExecution(
                    PipelineStage* stage, 
                    PipelineStage* previous_stage = nullptr,
                    size_t input_load_constraint = 0): 
                        previous_stage(previous_stage),
                        stage(stage), 
                        input_load_constraint(input_load_constraint) {}
            };

            std::thread host_copy_input_async_thread_;
            std::vector<PipelineStageExecution> host_copy_input_async_thread_stages_;

            std::thread gl_upload_async_thread_;
            std::vector<PipelineStageExecution> gl_upload_async_thread_stages_;

            std::thread gl_master_thread_;
            std::vector<PipelineStageExecution> gl_master_thread_stages_;

            std::thread gl_download_async_thread_;
            std::vector<PipelineStageExecution> gl_download_async_thread_stages_;

            std::thread host_copy_output_async_thread_;
            std::vector<PipelineStageExecution> host_copy_output_async_thread_stages_;

            std::unique_ptr<ByPassUploadStage> by_pass_upload_stage_;

            std::unique_ptr<FrameCompositionInputStage> frame_composition_input_stage_;
            std::unique_ptr<CopyHostToMappedPBOStage> copy_host_to_mapped_pbo_up_stage_;
            std::unique_ptr<UnmapPBOStage> unmap_pbo_up_stage_;

            std::unique_ptr<UnpackPBOToTextureStage> unpack_pbo_to_texture_stage_;
            std::unique_ptr<FormatConverterStage> format_converter_stage_input_to_render_;
            std::unique_ptr<RenderStage> render_stage_;
            std::unique_ptr<FormatConverterStage> format_converter_stage_render_to_output_;
            std::unique_ptr<PackTextureToPBOStage> pack_texture_to_pbo_stage_;
            std::unique_ptr<MapPBOStage> map_pbo_down_stage_;
            std::unique_ptr<CopyMappedPBOToHostStage> copy_mapped_pbo_down_stage;
            std::unique_ptr<FrameCompositionOutputStage> frame_composition_output_stage_;
            
            std::unique_ptr<ByPassDownloadStage> by_pass_download_stage_;

            gl::Context * const gl_context_master_;
            std::unique_ptr<gl::Context> gl_shared_context_upload_async_;
            std::unique_ptr<gl::Context> gl_shared_context_download_async_;

            std::atomic<StreamComposition*> active_composition_;
            CompletionHandler active_composition_completion_handler_;

            bool enable_output_stages_;
            bool enable_input_stages_;
            bool enable_render_stages_;

            std::vector<GLuint> download_pbo_ids_;
            // std::vector<std::unique_ptr<uint8_t[]>> download_pbos_external_memory_;

            std::vector<GLuint> upload_pbo_ids_;
            // std::vector<std::unique_ptr<uint8_t[]>> upload_pbos_external_memory_;

            bool enable_upload_gl_timer_queries_;
            bool enable_render_gl_timer_queries_;
            bool enable_download_gl_timer_queries_;

            bool impl_use_multiple_gl_contexts_;
            bool impl_async_input_;
            bool impl_async_output_;
            bool impl_use_arb_persistent_mapping_;
            bool impl_copy_pbo_before_download_;

            gl::utils::GLInfo gl_info_;

        };

        std::ostream& operator<< (std::ostream& out, const StreamDispatch::Flags& v);
        std::istream& operator>>(std::istream& in, StreamDispatch::Flags& v); 
    }
}

#endif // TOA_FRAME_BENDER_STREAM_DISPATCH_H
