//
//  AsyncQueue.cpp
//  AsyncCpp
//
//  Created by Luis Finke on 11/17/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#include "AsyncQueue.hpp"
#include <mutex>

namespace fgl {
	size_t nextAsyncQueueTaskBeginListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskStatusChangeListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

	size_t nextAsyncQueueTaskCancelListenerId() {
		static std::mutex mtx;
		static size_t nextId = 0;
		mtx.lock();
		size_t listenerId = nextId;
		nextId++;
		mtx.unlock();
		return listenerId;
	}

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

	Optional<AsyncQueue::TaskNode> AsyncQueue::getTaskWithName(const String& name) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getName() == name) {
				return taskNode;
			}
		}
		return std::nullopt;
	}

	Optional<AsyncQueue::TaskNode> AsyncQueue::getTaskWithTag(const String& tag) {
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				return taskNode;
			}
		}
		return std::nullopt;
	}

	LinkedList<AsyncQueue::TaskNode> AsyncQueue::getTasksWithTag(const String& tag) {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		LinkedList<AsyncQueue::TaskNode> tasks;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				tasks.push_back(taskNode);
			}
		}
		return tasks;
	}

	Optional<size_t> AsyncQueue::indexOfTaskWithTag(const String& tag) const {
		std::unique_lock<std::recursive_mutex> lock(mutex);
		size_t index = 0;
		for(auto& taskNode : taskQueue) {
			if(taskNode.task->getTag() == tag) {
				return index;
			}
			index++;
		}
		return std::nullopt;
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
				promises.push_back(taskNode.promise);
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
		weakSelf = ptr;
	}

	Promise<void> AsyncQueue::Task::perform() {
		FGL_ASSERT(!promise.has_value(), "Cannot call Task::perform more than once");
		FGL_ASSERT(!done, "Cannot call Task::perform on a finished task");
		auto self = weakSelf.lock();
		std::map<size_t,BeginListener> beginListeners;
		beginListeners.swap(this->beginListeners);
		for(auto pair : beginListeners) {
			pair.second(self);
		}
		this->beginListeners.clear();
		promise = executor(self).then(nullptr, [=]() {
			self->done = true;
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

	size_t AsyncQueue::Task::addBeginListener(BeginListener listener) {
		if(promise || done) {
			return (size_t)-1;
		}
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskBeginListenerId();
		} while(beginListeners.find(listenerId) != beginListeners.end());
		beginListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeBeginListener(size_t listenerId) {
		auto it = beginListeners.find(listenerId);
		if(it == beginListeners.end()) {
			return false;
		}
		beginListeners.erase(it);
		return true;
	}
	
	void AsyncQueue::Task::cancel() {
		if(done) {
			return;
		}
		if(cancelled) {
			return;
		}
		cancelled = true;
		auto self = weakSelf.lock();
		auto listeners = cancelListeners;
		cancelListeners.clear();
		for(auto& pair : listeners) {
			pair.second(self);
		}
	}

	bool AsyncQueue::Task::isCancelled() const {
		return cancelled;
	}

	size_t AsyncQueue::Task::addCancelListener(CancelListener listener) {
		if(cancelled) {
			return (size_t)-1;
		}
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskCancelListenerId();
		} while(cancelListeners.find(listenerId) != cancelListeners.end());
		cancelListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeCancelListener(size_t listenerId) {
		auto it = cancelListeners.find(listenerId);
		if(it == cancelListeners.end()) {
			return false;
		}
		cancelListeners.erase(it);
		return true;
	}

	void AsyncQueue::Task::clearCancelListeners() {
		cancelListeners.clear();
	}

	bool AsyncQueue::Task::isPerforming() const {
		return promise.has_value();
	}

	bool AsyncQueue::Task::isDone() const {
		return done;
	}
	
	AsyncQueue::Task::Status AsyncQueue::Task::getStatus() const {
		return status;
	}

	void AsyncQueue::Task::setStatus(Status status) {
		this->status = status;
		auto self = weakSelf.lock();
		auto listeners = statusChangeListeners;
		for(auto& pair : listeners) {
			pair.second(self, pair.first);
		}
	}

	void AsyncQueue::Task::setStatusText(String text) {
		auto status = this->status;
		status.text = text;
		setStatus(status);
	}

	void AsyncQueue::Task::setStatusProgress(double progress) {
		auto status = this->status;
		status.progress = progress;
		setStatus(status);
	}

	size_t AsyncQueue::Task::addStatusChangeListener(StatusChangeListener listener) {
		size_t listenerId;
		do {
			listenerId = nextAsyncQueueTaskStatusChangeListenerId();
		} while(statusChangeListeners.find(listenerId) != statusChangeListeners.end());
		statusChangeListeners[listenerId] = listener;
		return listenerId;
	}

	bool AsyncQueue::Task::removeStatusChangeListener(size_t listenerId) {
		auto it = statusChangeListeners.find(listenerId);
		if(it == statusChangeListeners.end()) {
			return false;
		}
		statusChangeListeners.erase(it);
		return true;
	}

	void AsyncQueue::Task::clearStatusChangeListeners() {
		statusChangeListeners.clear();
	}
}
