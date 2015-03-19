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
#ifndef TOA_FRAME_BENDER_STAGE_DEFINITION_H
#define TOA_FRAME_BENDER_STAGE_DEFINITION_H

namespace toa {
    namespace frame_bender {

        namespace detail {

            template <typename OutputElement>
            static bool is_consumer() {
                return false;
            }

            template <>
			STATIC_TSPL bool is_consumer<NO_OUTPUT>() {
                return true;
            }

            template <typename InputElement>
            static void log_stage_construction(
                const std::string& input_stage_name,
                const std::string& this_stage)
            {

                FB_LOG_DEBUG 
                    << "Created new stage '" << this_stage 
                    << "' with stage '" << input_stage_name << "' as input stage.";

            }

            template <>
			STATIC_TSPL void log_stage_construction<NO_INPUT>(
                const std::string&,
                const std::string& this_stage)
            {
                FB_LOG_DEBUG 
                    << "Created new stage '" << this_stage 
                    << "' (producer, no input).";
            }

            template <typename OutputFifoType>
            static void initialize_output_fifos(
                std::vector<typename OutputFifoType::ElementType::TokenElementType> upstream_init,
                std::shared_ptr<OutputFifoType>& output_fifo_upstream,
                std::shared_ptr<OutputFifoType>& output_fifo_downstream) 
            {

                // Make sure that we don't get this far for a consumer-like
                // void* output element type.
                static_assert(
                    !std::is_same<typename OutputFifoType::ElementType::TokenElementType, NO_OUTPUT>::value, 
                    "Compilation logic-error: Shouldn't hit this for NO_OUTPUT consumer-like stages.");

                if (upstream_init.empty()) {
                    throw std::invalid_argument("Output initialization for a non-consumer stage must not be empty.");
                }

                output_fifo_upstream = std::make_shared<OutputFifoType>(upstream_init.size());
                output_fifo_downstream = std::make_shared<OutputFifoType>(upstream_init.size());
                
                // Fill upstream with initialization
                for (auto& el : upstream_init) {
                    typename OutputFifoType::ElementType token;
                    token.element = std::move(el);
                    token.command = StageCommand::NO_CHANGE;
                    bool b = output_fifo_upstream->push(std::move(token));
                    FB_ASSERT_MESSAGE(b, "Fatal logic error, there must be space in queue.");
                }

            }

            template <>
			STATIC_TSPL void initialize_output_fifos<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>(
                std::vector<NO_OUTPUT>,
                std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>&,
                std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>&) 
            {
            }

            template <>
			STATIC_TSPL void initialize_output_fifos<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>(
                std::vector<NO_OUTPUT>,
                std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>&,
                std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>&) 
            {
            }

            template <typename InputStageType>
            static void initialize_input_fifos_references(
                const InputStageType& input_stage,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_upstream,
                std::weak_ptr<typename InputStageType::OutputFifoType>& input_fifo_downstream)
            {
                input_fifo_upstream = input_stage.output_upstream_;
                input_fifo_downstream = input_stage.output_downstream_;
            }

            template <>
			STATIC_TSPL void initialize_input_fifos_references<NO_INPUT_STAGE>(
                const NO_INPUT_STAGE&,
                std::weak_ptr<typename NO_INPUT_STAGE::OutputFifoType>& ,
                std::weak_ptr<typename NO_INPUT_STAGE::OutputFifoType>& )
            {
            }

            template <typename InputFifoType>
            static bool get_token_from_input(
                const std::weak_ptr<InputFifoType>& input_fifo, 
                typename InputFifoType::ElementType& token)
            {

                static_assert(!std::is_same<InputFifoType, NO_INPUT_STAGE::InputFifoType>::value, "Sanity check failed.");
                FB_ASSERT(!input_fifo.expired());
                return input_fifo.lock()->pop(token);

            }

            template <>
			STATIC_TSPL bool get_token_from_input<NO_INPUT_STAGE::InputFifoType>(
                const std::weak_ptr<NO_INPUT_STAGE::InputFifoType>&, 
                typename NO_INPUT_STAGE::InputFifoType::ElementType& token)
            {
                return true;
            }

            template <typename OutputFifoType>
            static bool get_token_from_output(
                const std::shared_ptr<OutputFifoType>& output_fifo,
                typename OutputFifoType::ElementType& token,
                bool wait_for_element = true)
            {
                // Make sure that we don't get this far for a consumer-like
                // void* output element type.
                static_assert(
                    !std::is_same<typename OutputFifoType::ElementType::TokenElementType, NO_OUTPUT>::value, 
                    "Compilation logic-error: Shouldn't hit this for NO_OUTPUT consumer-like stages.");

                return output_fifo->pop(token, wait_for_element);
            }

            template <>
			STATIC_TSPL bool get_token_from_output<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>(
                const std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>&,
                typename WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>::ElementType&,
                bool wait_for_element)
            {
                return true;
            }

            template <>
			STATIC_TSPL bool get_token_from_output<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>(
                const std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>&,
                typename WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>::ElementType&,
                bool wait_for_element)
            {
                return true;
            }

            // TODO: use rvalues or perfect forwarding for tokens?

            template <typename InputFifoType>
            static bool put_token_to_input_upstream(
                const std::weak_ptr<InputFifoType>& input_fifo, 
                typename InputFifoType::ElementType token)
            {
                static_assert(
                    !std::is_same<InputFifoType, NO_INPUT_STAGE::OutputFifoType>::value, 
                    "Compilation logic error: Should not have reached this.");

                auto fifo = input_fifo.lock();

                FB_ASSERT_MESSAGE(
                    static_cast<bool>(fifo),
                    "Crticial memory management issue: input-upstream fifo is gone.");
                return fifo->push(std::move(token));
            }

            template <>
			STATIC_TSPL bool put_token_to_input_upstream<NO_INPUT_STAGE::OutputFifoType>(
                const std::weak_ptr<NO_INPUT_STAGE::OutputFifoType>& input_fifo, 
                typename NO_INPUT_STAGE::OutputFifoType::ElementType token)
            {
                return true;
            }

            template <typename OutputFifoType>
            static bool put_token_to_output_downstream(
                const std::shared_ptr<OutputFifoType>& output_fifo, 
                typename OutputFifoType::ElementType token)
            {

                // Make sure that we don't get this far for a consumer-like
                // void* output element type.
                static_assert(
                    !std::is_same<typename OutputFifoType::ElementType::TokenElementType, NO_OUTPUT>::value, 
                    "Compilation logic-error: Shouldn't hit this for NO_OUTPUT consumer-like stages.");

                return output_fifo->push(std::move(token));
            }

            template <>
			STATIC_TSPL bool put_token_to_output_downstream<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>(
                const std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>>& , 
                typename WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::SPIN>::ElementType )
            {

                return true;
            }

            template <>
			STATIC_TSPL bool put_token_to_output_downstream<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>(
                const std::shared_ptr<WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>>& , 
                typename WaitingCircularFifo<OutputTokenNoOutput, WaitingPolicy::BLOCK>::ElementType )
            {

                return true;
            }
        }

        template <typename InputElement, typename OutputElement>
        template <typename InputInputType>
        Stage<InputElement, OutputElement>::Stage(
            std::string name,
            Task task, 
            std::vector<OutputElement> output_queue_initialization,
            const Stage<InputInputType, InputElement>& input_stage,
            SampleFunction sampler) :
                name_(std::move(name)),
                task_(std::move(task)),
                status_(PipelineStatus::INITIALIZING),
                is_valid_(true),
                sampler_(std::move(sampler))
        {
            // Initialize output queues

            detail::initialize_output_fifos(
                std::move(output_queue_initialization),
                output_upstream_,
                output_downstream_);

            detail::initialize_input_fifos_references(
                input_stage,
                input_upstream_,
                input_downstream_
                );

            status_.store(PipelineStatus::READY_TO_EXECUTE);

            detail::log_stage_construction<InputElement>(input_stage.name(), name_);
            
            bool is_consumer = detail::is_consumer<OutputType>();

            if (is_consumer) {
                FB_LOG_DEBUG << "Stage '" << name_ << "' is a consumer (no output).";
            }

            if (input_downstream_.lock()) {
                FB_LOG_DEBUG << "Stage '" << name_ << "': input-queue-size (referenced): " << input_downstream_.lock()->size() << ".";
            }

            if (!is_consumer) {
                FB_LOG_DEBUG << "Stage '" << name_ << "': output-queue-size (local): " << output_downstream_->size() << ".";
            }

        }

        template <typename InputElement, typename OutputElement>
        Stage<InputElement, OutputElement>::Stage(
            Stage<InputElement, OutputElement>&& other) : 
                name_(std::move(other.name_)),
                input_downstream_(std::move(other.input_downstream_)),
                input_upstream_(std::move(other.input_upstream_)),
                output_downstream_(std::move(other.output_downstream_)),
                output_upstream_(std::move(other.output_upstream_)),
                task_(std::move(other.task_)),
                status_(other.status_.load()),
                is_valid_(other.is_valid_),
                sampler_(std::move(other.sampler_))
        {
        }

        template <typename InputElement, typename OutputElement>
        Stage<InputElement, OutputElement>& Stage<InputElement, OutputElement>::operator=(
            Stage<InputElement, OutputElement>&& other)
        {
            name_ = std::move(other.name_);
            input_downstream_ = std::move(other.input_downstream_);
            input_upstream_ = std::move(other.input_upstream_);
            output_downstream_ = std::move(other.output_downstream_);
            output_upstream_ = std::move(other.output_upstream_);
            task_ = std::move(other.task_);
            status_ = other.status_.load();
            is_valid_ = other.is_valid_;
            sampler_ = std::move(other.sampler_);
            return *this;
        }

        template <typename InputElement, typename OutputElement>
        void Stage<InputElement, OutputElement>::execute() {

            if (!is_valid_) {
                FB_LOG_ERROR 
                    << "Invalid stage '" << name_ 
                    << "' was tried to be executed.";
                throw std::runtime_error("Attempt of executing invalid stage.");
            }

            if (status_ != PipelineStatus::READY_TO_EXECUTE) {
                FB_LOG_ERROR 
                    << "Stage '" << name_ 
                    << "' was tried to be executed while not being in stage "
                    << "'ready-to-execute'.";
                throw std::runtime_error("Stage not ready to be executed.");
            }

            if (sampler_)
                sampler_(StageExecutionState::EXECUTE_BEGIN);

            // Get input token from input downstream
            // Note that we usually assume that this will wait/block when
            // until a token is available. This is also the reason that we
            // must initialize the output_downstream queue in order to get
            // started.
            InputTokenType token_input;
            bool token_available = detail::get_token_from_input<InputFifoType>(
                input_downstream_, 
                token_input);           

            // TODO: do something better for timeout, maybe catch an exception?

            FB_ASSERT(token_available);

            if (sampler_)
                sampler_(StageExecutionState::INPUT_TOKEN_AVAILABLE);

            // Get available output token from output upstream
            // Note that for Stages with NO_OUTPUT, this is specialzied to
            // basically do nothing.
            OutputTokenType token_output;
            token_available = detail::get_token_from_output<OutputFifoType>(
                output_upstream_,
                token_output
                );

            FB_ASSERT(token_available);

            if (sampler_)
                sampler_(StageExecutionState::OUTPUT_TOKEN_AVAILABLE);

            // if input token says we are stopped, exit
            if (token_input.command == StageCommand::STOP_EXECUTION) {
                
                FB_LOG_DEBUG 
                    << "Stage '" << name_ 
                    << "' has been stopped by input downstream token.";

                status_ = PipelineStatus::HAS_BEEN_STOPPED;

            }

            // Note that we allow to cancel both ways (the stage which is down
            // stream, might notifiy this stage being upstream to actually stop).
            if (token_output.command == StageCommand::STOP_EXECUTION) {

                FB_LOG_DEBUG 
                    << "Stage '" << name_ 
                    << "' has been stopped by output upstream token.";
                
                status_ = PipelineStatus::HAS_BEEN_STOPPED;
            }

            // Commands are distributed downstream
            token_output.command = token_input.command;

            if (status_ == PipelineStatus::READY_TO_EXECUTE) {
                
                if (sampler_)
                    sampler_(StageExecutionState::TASK_BEGIN);

                // call task with both
                StageCommand command_from_task = task_(
                    token_input.element, 
                    token_output.element);

                if (sampler_)
                    sampler_(StageExecutionState::TASK_END);

                // TODO: think if you need BOTH directions (see above
                // distribution of commands)
                token_input.command = command_from_task;
                token_output.command = command_from_task;

                if (command_from_task == StageCommand::STOP_EXECUTION) {
                    FB_LOG_DEBUG
                        << "Task of Stage '" << name_ 
                        << "' requested to stop execution.";
                    status_ = PipelineStatus::HAS_BEEN_STOPPED;
                    // Note that we still continue to dispatch tokens, so the
                    // word gets around that we should stop.
                }

            }

            // Put input token back to input upstream
            bool put_success = detail::put_token_to_input_upstream<InputFifoType>(
                input_upstream_, 
                std::move(token_input));

            FB_ASSERT(put_success);

            // Put output token to output downstream
            put_success = detail::put_token_to_output_downstream<OutputFifoType>(
                output_downstream_,
                std::move(token_output));

            FB_ASSERT(put_success);

            if (sampler_)
                sampler_(StageExecutionState::EXECUTE_END);
        }

        template <typename InputElement, typename OutputElement>
        void Stage<InputElement, OutputElement>::flush(FlushTask flush_task) {

            if (!is_valid_) {
                FB_LOG_ERROR 
                    << "Invalid stage '" << name_ 
                    << "' was tried to be flushed.";
                throw std::runtime_error("Attempt of flushing invalid stage.");
            }

            if (status_ != PipelineStatus::HAS_BEEN_STOPPED) {
                FB_LOG_ERROR 
                    << "Stage '" << name_ 
                    << "' was tried to be flushed while not being in stage "
                    << "'has-been-stopped'.";
                throw std::runtime_error("Stage not ready to be flushed.");
            }

            OutputTokenType token_output;

            // Flush both up- and downstream
            while(detail::get_token_from_output<OutputFifoType>(
                  output_upstream_,
                  token_output,
                  false))
            {
                flush_task(token_output.element);
            }

            while(detail::get_token_from_output<OutputFifoType>(
                  output_downstream_,
                  token_output,
                  false))
            {
                flush_task(token_output.element);
            }
        }

        namespace utils {

            // Convenience factory methods

            template <typename OutputElement>
            Stage<NO_INPUT, OutputElement> 
                create_producer_stage(
                    std::string name,
                    std::function<StageCommand(OutputElement&)> task,
                    std::vector<OutputElement> output_queue_initialization, 
                    SampleFunction sampler) 
            {

                // Wrap into single argument lambda
                // TODO: find out if this is an issue that we can't move(!)
                typename Stage<NO_INPUT, OutputElement>::Task wrap = 
                    [=](NO_INPUT&, OutputElement& out) {
                        return task(out);
                };

                Stage<NO_INPUT, OutputElement> new_stage(
                    std::move(name),
                    std::move(wrap), 
                    std::move(output_queue_initialization), 
                    NO_INPUT_STAGE::get(),
                    std::move(sampler));

                return new_stage;
            }

            template <typename InputStage, typename OutputElement>
            Stage<typename InputStage::OutputType, OutputElement>
                create_stage(
                    std::string name,
                    typename Stage<typename InputStage::OutputType, OutputElement>::Task task,
                    std::vector<OutputElement> output_queue_initialization,
                    const InputStage& input_stage,
                    SampleFunction sampler)
            {
                
                Stage<typename InputStage::OutputType, OutputElement> new_stage(
                    std::move(name),
                    std::move(task), 
                    std::move(output_queue_initialization),
                    input_stage,
                    std::move(sampler));

                return new_stage;
            }

            template <typename InputStage>
            Stage<typename InputStage::OutputType, NO_OUTPUT>
                create_consumer_stage(
                    std::string name,
                    std::function<StageCommand(typename InputStage::OutputType&)> task,
                    const InputStage& input_stage,
                    SampleFunction sampler)
            {
                
                // TODO: check if no-move here is ok
                typename Stage<typename InputStage::OutputType, NO_OUTPUT>::Task wrap = 
                    [=](typename InputStage::OutputType& in, NO_OUTPUT& out) {
                        return task(in);
                };

                Stage<typename InputStage::OutputType, NO_OUTPUT> new_stage(
                    std::move(name),
                    std::move(wrap), 
                    std::vector<NO_OUTPUT>(),
                    input_stage,
                    std::move(sampler));

                return new_stage;
            }

        }



    }
}

#endif // TOA_FRAME_BENDER_STAGE_DEFINITION_H
