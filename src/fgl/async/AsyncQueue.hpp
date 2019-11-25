//
//  AsyncQueue.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <memory>
#include <fgl/async/Common.hpp>
#include <fgl/async/Promise.hpp>
#include <fgl/async/Generator.hpp>

namespace fgl {
	class AsyncQueue {
	public:
		struct Options {
			bool cancelUnfinishedTasks = false;
		};
		
		class Task {
			friend class AsyncQueue;
		public:
			struct Options {
				String name;
				String tag;
			};
			
			struct Status {
				double progress = 0;
				String text;
			};
			
			const String& getTag() const;
			const String& getName() const;
			
			void cancel();
			bool isCancelled() const;
			bool isDone() const;
			
			Status getStatus() const;
			void setStatus(Status);
			
		private:
			Task(std::shared_ptr<Task>& ptr, Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor);
			
			Promise<void> perform();
			
			std::weak_ptr<Task> self;
			Options options;
			Function<Promise<void>(std::shared_ptr<Task>)> executor;
			Optional<Promise<void>> promise;
			Status status;
			bool cancelled;
			bool done;
		};
		
		
		AsyncQueue(Options options = Options{.cancelUnfinishedTasks=false});
		
		size_t taskCount() const;
		
		std::shared_ptr<Task> getTaskWithName(const String& name);
		std::shared_ptr<Task> getTaskWithTag(const String& tag);
		LinkedList<std::shared_ptr<Task>> getTasksWithTag(const String& tag);
		
		struct RunResult {
			std::shared_ptr<Task> task;
			Promise<void> promise;
		};
		struct RunOptions {
			String name;
			String tag;
			Task::Status initialStatus = Task::Status();
			ArrayList<String> cancelTags;
			bool cancelMatchingTags = false;
			bool cancelAll = false;
		};
		template<typename Work>
		RunResult run(RunOptions options, Work work);
		template<typename Work>
		RunResult run(Work work);
		
		template<typename Work>
		RunResult runSingle(RunOptions options, Work work);
		
		void cancelAllTasks();
		void cancelTasksWithTag(const String& tag);
		void cancelTasksWithTags(const ArrayList<String>& tags);
		
		Promise<void> waitForCurrentTasks() const;
		Promise<void> waitForTasksWithTag(const String& tag) const;
		
	private:
		template<typename Work, typename is_promise<typename lambda_traits<Work>::return_type>::null_type = nullptr>
		Promise<void> performWork(std::shared_ptr<Task> task, Work work);
		template<typename Work, typename is_generator<typename lambda_traits<Work>::return_type>::null_type = nullptr>
		Promise<void> performWork(std::shared_ptr<Task> task, Work work);
		template<typename Work, typename std::enable_if<std::is_same<typename lambda_traits<Work>::return_type,void>::value, std::nullptr_t>::type = nullptr>
		Promise<void> performWork(std::shared_ptr<Task> task, Work work);
		
		template<typename GeneratorType>
		static Promise<void> runGenerator(GeneratorType generator, Function<bool()> shouldStop);
		
		void removeTask(std::shared_ptr<Task> task);
		
		struct TaskNode {
			std::shared_ptr<Task> task;
			Promise<void> promise;
		};
		
		Options options;
		LinkedList<TaskNode> taskQueue;
		Optional<Promise<void>> taskQueuePromise;
		mutable std::recursive_mutex mutex;
	};



#pragma mark AsyncQueue implementation

	template<typename Work>
	AsyncQueue::RunResult AsyncQueue::run(RunOptions options, Work work) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		if(this->options.cancelUnfinishedTasks || options.cancelAll) {
			cancelAllTasks();
		}
		if(options.cancelMatchingTags) {
			cancelTasksWithTag(options.tag);
		}
		if(options.cancelTags.size() > 0) {
			cancelTasksWithTags(options.cancelTags);
		}
		auto taskOptions = Task::Options{
			.name=options.name,
			.tag=options.tag
		};
		std::shared_ptr<Task> task;
		new Task(task, taskOptions, [=](auto task) {
			return performWork(task, work);
		});
		typename Promise<void>::Resolver resolveTask;
		typename Promise<void>::Rejecter rejectTask;
		auto taskPromise = Promise<void>([&](auto resolve, auto reject) {
			resolveTask = resolve;
			rejectTask = reject;
		});
		ASYNC_CPP_LIST_PUSH(taskQueue, TaskNode{ .task=task, .promise=taskPromise });
		taskQueuePromise = taskQueuePromise.value_or(Promise<void>::resolve()).then([=]() -> Promise<void> {
			if(task->isCancelled()) {
				removeTask(task);
				resolveTask();
				return Promise<void>::resolve();
			}
			return task->perform().then([=]() {
				removeTask(task);
				resolveTask();
			}, [=](std::exception_ptr error) {
				removeTask(task);
				rejectTask(error);
			});
		});
		return RunResult{ .task=task, .promise=taskPromise };
	}

	template<typename Work>
	AsyncQueue::RunResult AsyncQueue::run(Work work) {
		return run(RunOptions(), work);
	}

	template<typename Work>
	AsyncQueue::RunResult AsyncQueue::runSingle(RunOptions options, Work work) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == options.tag) {
				return RunResult{
					.task=taskNode.task,
					.promise=taskNode.promise
				};
			}
		}
		return run(options, work);
	}



	template<typename Work, typename is_promise<typename lambda_traits<Work>::return_type>::null_type>
	Promise<void> AsyncQueue::performWork(std::shared_ptr<Task> task, Work work) {
		auto promise = work(task);
		if constexpr(std::is_same<Promise<void>,decltype(promise)>::value) {
			return promise;
		} else {
			return promise.toVoid();
		}
	}

	template<typename GeneratorType>
	Promise<void> AsyncQueue::runGenerator(GeneratorType gen, Function<bool()> shouldStop) {
		return gen.next().then(nullptr, [=](typename GeneratorType::YieldResult yieldResult) -> Promise<void> {
			if(yieldResult.done || shouldStop()) {
				return Promise<void>::resolve();
			}
			return runGenerator(gen,shouldStop);
		});
	}

	template<typename Work, typename is_generator<typename lambda_traits<Work>::return_type>::null_type>
	Promise<void> AsyncQueue::performWork(std::shared_ptr<Task> task, Work work) {
		return Promise<void>([&](auto resolve, auto reject) {
			auto gen = work(task);
			runGenerator(gen, [=]() {
				return task->isCancelled();
			}).then(nullptr, [=]() {
				resolve();
			}, [=](std::exception_ptr error) {
				reject(error);
			});
		});
	}

	template<typename Work, typename std::enable_if<std::is_same<typename lambda_traits<Work>::return_type,void>::value, std::nullptr_t>::type>
	Promise<void> AsyncQueue::performWork(std::shared_ptr<Task> task, Work work) {
		return Promise<void>([&](auto resolve, auto reject) {
			try {
				work();
			} catch(...) {
				reject(std::current_exception());
				return;
			}
			resolve();
		});
	}
}
