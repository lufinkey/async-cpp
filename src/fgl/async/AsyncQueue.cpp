//
//  AsyncQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "AsyncQueue.hpp"

namespace fgl {
	AsyncQueue::AsyncQueue(Options options)
	: options(options), aliveStatus(std::make_shared<AliveStatus>()) {
		//
	}

	AsyncQueue::~AsyncQueue() {
		aliveStatus->alive = false;
	}

	size_t AsyncQueue::taskCount() const {
		return taskQueue.size();
	}

	std::shared_ptr<AsyncQueue::Task> AsyncQueue::getTaskWithName(const String& name) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getName() == name) {
				return taskNode.task;
			}
		}
		return nullptr;
	}

	std::shared_ptr<AsyncQueue::Task> AsyncQueue::getTaskWithTag(const String& tag) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				return taskNode.task;
			}
		}
		return nullptr;
	}

	LinkedList<std::shared_ptr<AsyncQueue::Task>> AsyncQueue::getTasksWithTag(const String& tag) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<std::shared_ptr<AsyncQueue::Task>> tasks;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				ASYNC_CPP_LIST_PUSH(tasks, taskNode.task);
			}
		}
		return tasks;
	}

	void AsyncQueue::cancelAllTasks() {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			taskNode.task->cancel();
		}
	}

	void AsyncQueue::cancelTasksWithTag(const String& tag) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			if(taskNode.task->getTag() == tag) {
				taskNode.task->cancel();
			}
		}
	}

	void AsyncQueue::cancelTasksWithTags(const ArrayList<String>& tags) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto tasks = taskQueue;
		for(auto& taskNode : tasks) {
			for(auto& tag : tags) {
				if(taskNode.task->getTag() == tag) {
					taskNode.task->cancel();
					break;
				}
			}
		}
	}

	Promise<void> AsyncQueue::waitForCurrentTasks() const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		return taskQueuePromise.value_or(Promise<void>::resolve());
	}

	Promise<void> AsyncQueue::waitForTasksWithTag(const String& tag) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<Promise<void>> promises;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				ASYNC_CPP_LIST_PUSH(promises, taskNode.promise);
			}
		}
		return Promise<void>::all(ArrayList<Promise<void>>(std::make_move_iterator(promises.begin()), std::make_move_iterator(promises.end())));
	}

	void AsyncQueue::removeTask(std::shared_ptr<Task> task) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		for(auto it=taskQueue.begin(), end=taskQueue.end(); it!=end; it++) {
			if(it->task == task) {
				taskQueue.erase(it);
				break;
			}
		}
		if(taskQueue.size() == 0) {
			taskQueuePromise = std::nullopt;
		}
	}



	AsyncQueue::Task::Task(std::shared_ptr<Task>& ptr, Options options, Function<Promise<void>(std::shared_ptr<Task>)> executor)
	: options(options), executor(executor), cancelled(false), done(false) {
		ptr = std::shared_ptr<Task>(this);
		self = ptr;
	}

	Promise<void> AsyncQueue::Task::perform() {
		FGL_ASSERT(!promise.has_value(), "Cannot call Task::perform more than once");
		auto self = this->self.lock();
		promise = executor(self).then(nullptr, [=]() {
			self->promise = std::nullopt;
		});
		executor = nullptr;
		return promise.value();
	}

	const String& AsyncQueue::Task::getTag() const {
		return options.tag;
	}

	const String& AsyncQueue::Task::getName() const {
		return options.name;
	}
	
	void AsyncQueue::Task::cancel() {
		if(done) {
			return;
		}
		cancelled = true;
	}

	bool AsyncQueue::Task::isCancelled() const {
		return cancelled;
	}

	bool AsyncQueue::Task::isDone() const {
		return done;
	}
	
	AsyncQueue::Task::Status AsyncQueue::Task::getStatus() const {
		return status;
	}

	void AsyncQueue::Task::setStatus(Status status) {
		this->status = status;
	}
}
