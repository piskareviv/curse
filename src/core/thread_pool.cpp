#include "src/core/thread_pool.hpp"

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "dependencies/ctpl/ctpl_stl.h"

namespace curse {

struct ThreadPool::Impl {
    ctpl::thread_pool pool;
};

ThreadPool::ThreadPool(size_t n_threads) {
    if (n_threads == 0) {
        n_threads = std::max<int>(1, std::thread::hardware_concurrency());
    }
    m_impl = std::make_unique<Impl>(static_cast<int>(n_threads));
}

std::future<void> ThreadPool::Push(std::function<void()> f) {
    return m_impl->pool.push([g = std::move(f)](int) -> void { g(); });
}

ThreadPool::~ThreadPool() {}

ThreadPool thread_pool;

}  // namespace curse
