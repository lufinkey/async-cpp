//
//  AsyncQueue.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <map>
#include <memory>
#include <fgl/async/Common.hpp>
#include <fgl/async/Promise.hpp>
#include <fgl/async/Generator.hpp>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
@protocol FGLAsyncQueueTaskEventListener;
#endif

namespace fgl {
	class AsyncQueue {
	public:
		class Task: public std::enable_shared_from_this<Task> {
			friend class AsyncQueue;
		public:
			using BeginListener = Function<void(std::shared_ptr<Task> task)>;
			using StatusChangeListener = Function<void(std::shared_ptr<Task> task, size_t listenerId)>;
			using ErrorListener = Function<void(std::shared_ptr<Task> task, std::exception_ptr)>;
			using EndListener = Function<void(std::shared_ptr<Task> task)>;
			using CancelListener = Function<void(std::shared_ptr<Task> task)>;
			
			class EventListener {
			public:
				virtual ~EventListener() {}
				virtual void onAsyncQueueTaskBegin(std::shared_ptr<Task> task) {};
				virtual void onAsyncQueueTaskCancel(std::shared_ptr<Task> task) {};
				virtual void onAsyncQueueTaskStatusChange(std::shared_ptr<Task> task) {};
				virtual void onAsyncQueueTaskError(std::shared_ptr<Task> task, std::exception_ptr error) {};
				virtual void onAsyncQueueTaskEnd(std::shared_ptr<Task> task) {};
			};
			
			struct Options {
				String name;
				String tag;
			};
			
			struct Status {
				double progress = 0;
				String text;
			};
			
			static std::shared_ptr<Task> new$(Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor);
			
			Task(Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor);
			~Task();
			
			const String& getTag() const;
			const String& getName() const;
			
			void addEventListener(EventListener* listener);
			void removeEventListener(EventListener* listener);
			#ifdef __OBJC__
			void addEventListener(id<FGLAsyncQueueTaskEventListener> listener);
			void removeEventListener(id<FGLAsyncQueueTaskEventListener> listener);
			#endif
			
			size_t addBeginListener(BeginListener listener);
			bool removeBeginListener(size_t listenerId);
			size_t addErrorListener(ErrorListener listener);
			bool removeErrorListener(size_t listenerId);
			size_t addEndListener(EndListener listener);
			bool removeEndListener(size_t listenerId);
			
			void cancel();
			bool isCancelled() const;
			size_t addCancelListener(CancelListener listener);
			bool removeCancelListener(size_t listenerId);
			
			bool isPerforming() const;
			bool isDone() const;
			
			Status getStatus() const;
			void setStatus(Status);
			void setStatusText(String text);
			void setStatusProgress(double progress);
			size_t addStatusChangeListener(StatusChangeListener listener);
			bool removeStatusChangeListener(size_t listenerId);
			
		private:
			Promise<void> perform();
			
			EventListener* functionalEventListener();
			
			Options options;
			mutable std::recursive_mutex mutex;
			Function<Promise<void>(std::shared_ptr<Task>)> executor;
			Optional<Promise<void>> promise;
			Status status;
			LinkedList<EventListener*> eventListeners;
			bool cancelled;
			bool done;
		};
		
		struct TaskNode {
			std::shared_ptr<Task> task;
			Promise<void> promise;
		};
		
		struct Options {
			DispatchQueue* dispatchQueue = getDefaultPromiseQueue();
			bool cancelUnfinishedTasks = false;
		};
		
		AsyncQueue(Options options = Options{.dispatchQueue=getDefaultPromiseQueue(),.cancelUnfinishedTasks=false});
		~AsyncQueue();
		
		size_t taskCount() const;
		
		Optional<TaskNode> getTaskWithName(const String& name);
		Optional<TaskNode> getTaskWithTag(const String& tag);
		LinkedList<TaskNode> getTasksWithTag(const String& tag);
		Optional<size_t> indexOfTaskWithTag(const String& tag) const;
		
		struct RunOptions {
			String name;
			String tag;
			Task::Status initialStatus = Task::Status();
			ArrayList<String> cancelTags;
			bool cancelMatchingTags = false;
			bool cancelAll = false;
		};
		template<typename Work>
		TaskNode run(RunOptions options, Work work);
		template<typename Work>
		TaskNode run(Work work);
		
		template<typename Work>
		TaskNode runSingle(RunOptions options, Work work);
		
		void cancelAllTasks();
		void cancelTasksWithTag(const String& tag);
		void cancelTasksWithTags(const ArrayList<String>& tags);
		
		Promise<void> waitForCurrentTasks() const;
		Promise<void> waitForTasksWithTag(const String& tag) const;
		
	private:
		template<typename Work>
		static Promise<void> performWork(DispatchQueue* dispatchQueue, std::shared_ptr<Task> task, Work work);
		
		template<typename GeneratorType>
		static Promise<void> runGenerator(DispatchQueue* dispatchQueue, GeneratorType generator, Function<bool()> shouldStop);
		
		void removeTask(std::shared_ptr<Task> task);
		
		Options options;
		LinkedList<TaskNode> taskQueue;
		Optional<Promise<void>> taskQueuePromise;
		mutable std::recursive_mutex mutex;

		struct AliveStatus {
			bool alive = true;
		};
		std::shared_ptr<AliveStatus> aliveStatus;
	};



#pragma mark AsyncQueue implementation

	template<typename Work>
	AsyncQueue::TaskNode AsyncQueue::run(RunOptions options, Work work) {
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
		auto dispatchQueue = this->options.dispatchQueue;
		auto task = Task::new$(taskOptions, [=](std::shared_ptr<Task> task) {
			return performWork<Work>(dispatchQueue, task, work);
		});
		typename Promise<void>::Resolver resolveTask;
		typename Promise<void>::Rejecter rejectTask;
		auto taskPromise = Promise<void>([&](auto resolve, auto reject) {
			resolveTask = resolve;
			rejectTask = reject;
		});
		auto taskNode = TaskNode{ .task=task, .promise=taskPromise };
		taskQueue.push_back(taskNode);
		auto aliveStatus = this->aliveStatus;
		taskQueuePromise = taskQueuePromise.value_or(Promise<void>::resolve()).then(dispatchQueue, [=]() -> Promise<void> {
			if(task->isCancelled()) {
				if(aliveStatus->alive) {
					removeTask(task);
				}
				resolveTask();
				return Promise<void>::resolve();
			}
			return task->perform().then(dispatchQueue, [=]() {
				if(aliveStatus->alive) {
					removeTask(task);
				}
				resolveTask();
			}, [=](std::exception_ptr error) {
				if(aliveStatus->alive) {
					removeTask(task);
				}
				rejectTask(error);
			});
		});
		return taskNode;
	}

	template<typename Work>
	AsyncQueue::TaskNode AsyncQueue::run(Work work) {
		return run<Work>(RunOptions(), work);
	}

	template<typename Work>
	AsyncQueue::TaskNode AsyncQueue::runSingle(RunOptions options, Work work) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == options.tag) {
				return taskNode;
			}
		}
		return run<Work>(options, work);
	}



	template<typename Work>
	Promise<void> AsyncQueue::performWork(DispatchQueue* dispatchQueue, std::shared_ptr<Task> task, Work work) {
		if constexpr(is_promise<decltype(work(task))>::value) {
			// promise
			auto promise = work(task);
			if constexpr(std::is_same<Promise<void>,decltype(promise)>::value) {
				return promise;
			} else {
				return promise.toVoid();
			}
		} else if constexpr(is_generator<decltype(work(task))>::value) {
			// generator
			auto gen = work(task);
			return runGenerator(dispatchQueue, gen, [=]() {
				return task->isCancelled();
			});
		} else if constexpr(std::is_same<decltype(work(task)),void>::value) {
			return Promise<void>([&](auto resolve, auto reject) {
				try {
					work(task);
				} catch(...) {
					reject(std::current_exception());
					return;
				}
				resolve();
			});
		} else {
			static_assert(
				(is_promise<decltype(work(task))>::value || is_generator<decltype(work(task))>::value || std::is_same<decltype(work(task)),void>::value),
				"invalid lambda type");
		}
	}

	template<typename GeneratorType>
	Promise<void> AsyncQueue::runGenerator(DispatchQueue* dispatchQueue, GeneratorType gen, Function<bool()> shouldStop) {
		return gen.next().then(dispatchQueue, [=](typename GeneratorType::YieldResult yieldResult) -> Promise<void> {
			if(yieldResult.done || shouldStop()) {
				return Promise<void>::resolve();
			}
			return runGenerator(dispatchQueue,gen,shouldStop);
		});
	}
}




#pragma mark FGLAsyncQueueTaskEventListener interface

#ifdef __OBJC__
@protocol FGLAsyncQueueTaskEventListener <NSObject>
-(void)asyncQueueTaskWillBegin:(std::shared_ptr<fgl::AsyncQueue::Task>)task;
-(void)asyncQueueTaskDidCancel:(std::shared_ptr<fgl::AsyncQueue::Task>) task;
-(void)asyncQueueTaskDidChangeStatus:(std::shared_ptr<fgl::AsyncQueue::Task>)task;
-(void)asyncQueueTask:(std::shared_ptr<fgl::AsyncQueue::Task>)task didThrowError:(std::exception_ptr)error;
-(void)asyncQueueTaskDidEnd:(std::shared_ptr<fgl::AsyncQueue::Task>)task;
@end
#endif
