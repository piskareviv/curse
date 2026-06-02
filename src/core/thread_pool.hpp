#pragma once

#include <functional>
#include <future>
#include <memory>

class ThreadPool {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    ThreadPool(size_t n_threads = 0);

    std::future<void> Push(std::function<void()>);

    ~ThreadPool();
};

extern ThreadPool thread_pool;
