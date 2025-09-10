#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace badger {

class Executor {
 public:
  Executor() = default;
  Executor(const Executor&) = default;
  Executor& operator=(const Executor&) = default;
  Executor(Executor&&) = default;
  Executor& operator=(Executor&&) = default;
  virtual ~Executor() = default;

  virtual void Schedule(std::function<void()> fn) = 0;
  virtual void Shutdown() = 0;
};

class ThreadPool : public Executor {
 public:
  using Task = std::function<void()>;

  explicit ThreadPool(int num_threads) {
    if (num_threads <= 0)
      throw std::invalid_argument("num_threads must be > 0");

    for (int i = 0; i < num_threads; ++i)
      workers_.emplace_back([this] { WorkerLoop(); });
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  ~ThreadPool() override { Shutdown(); }

  void Schedule(std::function<void()> fn) override {
    {
      std::unique_lock<std::mutex> lock(tasks_mtx_);
      if (shutdown_) throw std::runtime_error("ThreadPool has been shut down");

      task_queue_.push_back(std::move(fn));
    }

    tasks_cv_.notify_one();
  }

  void Shutdown() override {
    {
      std::unique_lock<std::mutex> lock(tasks_mtx_);
      shutdown_ = true;
    }

    tasks_cv_.notify_all();

    for (auto& t : workers_)
      if (t.joinable()) t.join();
  }

 private:
  void WorkerLoop() {
    while (true) {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(tasks_mtx_);
        tasks_cv_.wait(lock,
                       [this] { return shutdown_ || !task_queue_.empty(); });

        if (shutdown_ && task_queue_.empty()) return;

        task = std::move(task_queue_.front());
        task_queue_.pop_front();
      }

      task();
    }
  }

  bool shutdown_ = false;
  std::mutex tasks_mtx_;
  std::condition_variable tasks_cv_;
  std::vector<std::thread> workers_;
  std::deque<Task> task_queue_;
};

}  // namespace badger