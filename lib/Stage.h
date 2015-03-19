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
#ifndef TOA_FRAME_BENDER_STAGE_H
#define TOA_FRAME_BENDER_STAGE_H

#include <array>
#include <functional>
#include <type_traits>
#include <memory>

#include "Utils.h"
#include "CircularFifo.h"
#include "CircularFifoHelpers.h"
#include "Logging.h"

#ifndef _MSC_VER
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace toa {

    namespace frame_bender {

        // TODO: C++11 : add defaulted move constructors and move assignment
        // operators and delete only the copy and copy assignment. It seems
        // that if we use NoCopying, then the compiler doesn't  generate
        // the move stuff for us (!)

        typedef void* NO_OUTPUT;
        typedef void* NO_INPUT;

        enum class PipelineStatus {
            INITIALIZING,
            READY_TO_EXECUTE,
            HAS_BEEN_STOPPED

            // TODO: maybe add exception was encountered?
        };

        enum class StageCommand {
            NO_CHANGE,
            STOP_EXECUTION
        };

        // TODO: make a check for nullptr when input_stage != NO_INPUT
        // TODO: can we add the wait policy here?
        // TODO: maybe add an exception ptr as well?
        // TODO: rename to be more generic?
        template <typename OutputElement>
        struct OutputToken {
            typedef OutputElement TokenElementType;
            TokenElementType element;
            StageCommand command; // TODO-C++11: add class member initialization with a sensible value.

            OutputToken() : command(StageCommand::NO_CHANGE) {}
            ~OutputToken() {}
            OutputToken& operator=(const OutputToken& other) { this->element = other.element; this->command = other.command; return *this; }

            OutputToken(const OutputToken& other) : element(other.element), command(other.command) { }
            OutputToken& operator=(OutputToken&& other) { this->element = std::move(other.element); this->command = other.command; return *this; }
            OutputToken(OutputToken&& other) : element(std::move(other.element)), command(other.command) {}

        };

        typedef OutputToken<NO_OUTPUT> OutputTokenNoOutput;
        typedef OutputToken<NO_INPUT> OutputTokenNoInput;

        namespace detail {

            template <typename InputStageType>
            static void initialize_input_fifos_references(
                const InputStageType& input_stage,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_upstream,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_downstream);
        }

        static const WaitingPolicy kDefaultWaitingPolicy = WaitingPolicy::SPIN;

        // Might have to get more in the future
        // TODO: make note that EXECUTE_BEGIN/EXECUTE_END is also sampled for
        // "stop execution" commands
        enum class StageExecutionState : size_t {
            EXECUTE_BEGIN,
            INPUT_TOKEN_AVAILABLE,
            OUTPUT_TOKEN_AVAILABLE,
            TASK_BEGIN,
            TASK_END,
            EXECUTE_END,
            GL_TASK_BEGIN,
            GL_TASK_END,
            NUMBER_OF_STATES
        };

        static std::ostream& operator<< (std::ostream& out, const StageExecutionState& v) {

            switch (v) {

            case StageExecutionState::EXECUTE_BEGIN:
                out << "EXECUTE_BEGIN";
                break;
            case StageExecutionState::INPUT_TOKEN_AVAILABLE:
                out << "INPUT_TOKEN_AVAILABLE";
                break;
            case StageExecutionState::OUTPUT_TOKEN_AVAILABLE:
                out << "OUTPUT_TOKEN_AVAILABLE";
                break;
            case StageExecutionState::TASK_BEGIN:
                out << "TASK_BEGIN";
                break;
            case StageExecutionState::TASK_END:
                out << "TASK_END";
                break;
            case StageExecutionState::EXECUTE_END:
                out << "EXECUTE_END";
                break;
            case StageExecutionState::GL_TASK_BEGIN:
                out << "GL_TASK_BEGIN";
                break;
            case StageExecutionState::GL_TASK_END:
                out << "GL_TASK_END";
                break;

            default:
                out << "<unknown>";
                break;
            }

            return out;
        }

        typedef std::function<void(StageExecutionState)> SampleFunction;

        // TODO: make sure that you forbid circles or multiple connections...
        //      do this by find a friend way, and setting a flag in the constructor/destructor 
        // TODO: document that void* is forbidden as a type
        // TODO: parameterizer the waiting fifo type...
        // TODO-C++11: need template aliases for FIFOType definition inhere..
        // TODO: Maybe remove the use of shared_ptr?
        // TODO: maube create a safety check where using the same stage as
        // an input to more than one other stage is checked and should be reported.
        // TODO: document that the token that contains STOP_EXECUTION is not considered anymore.
        // TODO: Make the waiting police an additional template argument?
        // TODO: add generically collected performance data?
        template <typename InputElement, typename OutputElement>
        class Stage {

        public:

            typedef OutputElement OutputType;
            typedef InputElement InputType;
            typedef OutputToken<InputElement> InputTokenType;
            typedef OutputToken<OutputElement> OutputTokenType;
            typedef WaitingCircularFifo<OutputTokenType, kDefaultWaitingPolicy> OutputFifoType;
            typedef WaitingCircularFifo<InputTokenType, kDefaultWaitingPolicy> InputFifoType;

            static_assert(
                ! (std::is_same<InputElement, NO_INPUT>::value && 
                   std::is_same<OutputElement, NO_OUTPUT>::value), 
                "Stage must have at least one input or output.");

            // TODO: document that the InputType is ALSO a reference and 
            // should be able to read, because we have a backchannel as well...
            // TODO: docuent that this will not be executed when cancel token has been encountered. 
            // TODO: consider installing a cancel callback lambda
            // TODO: create a helper function where you could use auto... 
            typedef std::function<StageCommand(InputType&, OutputType&)> Task;

            // TODO: the queue feeder should take care of setting the current state...

            // TODO: create a speclized constrcutor for accepting no-input 
            // tasks... 
            // TODO: shall throw if size of output_queue_init is zero (empty)
            // TODO: maybe be more specific in the name of output_Queue_initizliation
            //      -> add upstream to it?
            // TODO: could think about being fail safe in the sense that the destructor
            // send a last HAS_BEEN_KILLED token into one of the queues... 
            template <typename InputInputType>
            Stage(
                std::string name,
                Task task, 
                std::vector<OutputType> output_queue_initialization,
                const Stage<InputInputType, InputType>& input_stage,
                SampleFunction sampler = SampleFunction()
                );

            Stage() : is_valid_(false) {}

            // TODO: how should execute behave if state is not ready to run?
            void execute();

            // TODO: document as thread-safe
            PipelineStatus status() const { return status_.load(); }

            Stage<InputElement, OutputElement>& operator=(Stage<InputElement, OutputElement>&& other);
            Stage<InputElement, OutputElement>(Stage<InputElement, OutputElement>&& other);

            const std::string& name() const { return name_; }

            bool is_valid() const { return is_valid_; }
            operator bool() const { return is_valid(); }

            template <typename InputStageType>
            friend void detail::initialize_input_fifos_references(
                const InputStageType& input_stage,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_upstream,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_downstream);

            size_t input_queue_num_elements() const { if (input_downstream_.lock()) { return input_downstream_.lock()->had_num_elements(); } return 0; }

            size_t output_queue_size() const { if (output_downstream_) { return output_downstream_->size(); } return 0;}

            typedef std::function<void(OutputType&)> FlushTask;

            void flush(FlushTask task);

        private:

            Stage<InputElement, OutputElement>& operator=(const Stage<InputElement, OutputElement>&);
            Stage<InputElement, OutputElement>(const Stage<InputElement, OutputElement>&);

            std::string name_;

            std::weak_ptr<InputFifoType> input_downstream_;
            std::weak_ptr<InputFifoType> input_upstream_;

            std::shared_ptr<OutputFifoType> output_downstream_;
            std::shared_ptr<OutputFifoType> output_upstream_;

            Task task_;

            std::atomic<PipelineStatus> status_;

            bool is_valid_;

            SampleFunction sampler_;
        };

        namespace utils {

            // Convenience factory methods

            template <typename OutputElement>
            Stage<NO_INPUT, OutputElement> 
                create_producer_stage(
                    std::string name,
                    std::function<StageCommand(OutputElement&)> task,
                    std::vector<OutputElement> output_queue_initialization,
                    SampleFunction sampler = SampleFunction());

            template <typename InputStage, typename OutputElement>
            Stage<typename InputStage::OutputType, OutputElement>
                create_stage(
                    std::string name,
                    typename Stage<typename InputStage::OutputType, OutputElement>::Task task,
                    std::vector<OutputElement> output_queue_initialization,
                    const InputStage& input_stage,
                    SampleFunction sampler = SampleFunction());

            template <typename InputStage>
            Stage<typename InputStage::OutputType, NO_OUTPUT>
                create_consumer_stage(
                    std::string name,
                    std::function<StageCommand(typename InputStage::OutputType&)> task,
                    const InputStage& input_stage,
                    SampleFunction sampler = SampleFunction());


        }

        template <>
        class Stage<NO_INPUT, NO_OUTPUT> {
        public:

            static const Stage<NO_INPUT, NO_OUTPUT>& get() {
                static const Stage<void*, NO_OUTPUT> dummy;
                return dummy;
            }

            typedef void* OutputType;
            typedef void* InputStageType;
            typedef void* InputType;
            typedef void* InputTokenType;
            struct OutputToken {
                StageCommand command;
                void* element;
                OutputToken() : command(StageCommand::NO_CHANGE), element(nullptr) {}
            };
            typedef OutputTokenNoInput OutputTokenType;
            typedef WaitingCircularFifo<OutputTokenNoOutput, kDefaultWaitingPolicy> OutputFifoType;
            typedef WaitingCircularFifo<OutputTokenNoInput, kDefaultWaitingPolicy> InputFifoType;

            const std::string& name() const { static const std::string empty; return empty; }
            //Stage<void*, NO_OUTPUT>& operator=(const Stage<void*, NO_OUTPUT>&&);
            //Stage<void*, NO_OUTPUT>(const Stage<void*, NO_OUTPUT>&&);

            Stage<NO_INPUT, NO_OUTPUT>() {}
        private:
            //Stage<void*, NO_OUTPUT>& operator=(const Stage<void*, NO_OUTPUT>&);
            //Stage<void*, NO_OUTPUT>(const Stage<void*, NO_OUTPUT>&);


        };

        typedef Stage<NO_INPUT, NO_OUTPUT> NO_INPUT_STAGE;

    }

}

#include "Stage.inl.h"

#ifndef _MSC_VER
#pragma clang diagnostic pop
#endif

#endif // TOA_FRAME_BENDER_STAGE_H
