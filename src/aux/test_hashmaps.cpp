// NOLINTBEGIN

#include <cxxabi.h>
#include <malloc.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "dependencies/gtl/include/gtl/phmap.hpp"
#include "dependencies/sparsepp/sparsepp/spp.h"

// #include "src/dependencies/gtl"
// #include "dependencies/gtl/

#include <unistd.h>

#include <fstream>
#include <iostream>

size_t TotalMemory() {
    int64_t pages = 0;
    std::ifstream stat_file("/proc/self/statm");
    stat_file >> pages >> pages;
    int64_t page_sz = sysconf(_SC_PAGESIZE);
    return pages * page_sz;
}

size_t total_bytes_allocated = 0;
size_t max_bytes_allocated = 0;

template <class T>
struct TrackingAllocator {
    using value_type = T;                    // NOLINT
    using pointer = T*;                      // NOLINT
    using const_pointer = const T*;          // NOLINT
    using reference = T&;                    // NOLINT
    using const_reference = const T&;        // NOLINT
    using size_type = std::size_t;           // NOLINT
    using difference_type = std::ptrdiff_t;  // NOLINT

    template <class U>
    struct rebind {                          // NOLINT
        using other = TrackingAllocator<U>;  // NOLINT
    };

    TrackingAllocator() noexcept = default;

    template <class U>
    TrackingAllocator(const TrackingAllocator<U>&) noexcept {}

    [[nodiscard]]
    T* allocate(std::size_t n) {  // NOLINT
        total_bytes_allocated += n * sizeof(T);
        max_bytes_allocated = std::max(max_bytes_allocated, total_bytes_allocated);

        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept {  // NOLINT
        total_bytes_allocated -= n * sizeof(T);

        ::operator delete(p);
    };

    // template <class U>
    // friend bool operator==(const TrackingAllocator<U>&, const TrackingAllocator<U>&) {
    //     return true;
    // }

    template <class U>
    bool operator!=(const TrackingAllocator<U>&) const {
        // return false;
        return std::is_same_v<U, T>;
    }

    size_type max_size() const noexcept {  // NOLINT
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }
};

template <typename T>
std::string GetTypeName() {
    const char* name = typeid(T).name();
    int status = -1;
    std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};
    return (status == 0) ? res.get() : name;
}

std::string Pad(std::string s, size_t n, char ch = ' ') {
    s.resize(std::max(s.size(), n), ch);
    return s;
}

template <typename Key, typename Mapped, typename HashMap>
void TestHasMapImpl_(size_t n, std::string name = "") {

    std::cout << std::format("n: {} key: {} mapped: {}   ",
                             Pad(std::to_string(n), 10),
                             Pad(GetTypeName<Key>(), 6),
                             Pad(GetTypeName<Mapped>(), 6));
    std::cout.flush();

    std::mt19937 rnd;
    std::mt19937 rnd_64;

    size_t bytes1 = total_bytes_allocated;
    // size_t bytes2 = TotalMemory();
    double max_ratio1 = 0;
    double max_ratio2 = 0;

    clock_t beg = clock();
    HashMap map;
    for (size_t i = 0; i < n; i++) {
        Key key;
        if constexpr (sizeof(Key) <= 4) {
            key = rnd();
        } else {
            key = rnd_64();
        }

        max_bytes_allocated = 0;

        Mapped mapped = 0;
        map[key] = mapped;

        if (i >= n / 10) {
            max_ratio1 = std::max(max_ratio1, (max_bytes_allocated - bytes1) / static_cast<double>(i + 1));
            if (i % (1 << 13) == 0) {
                // max_ratio2 = std::max(max_ratio2, (TotalMemory() - bytes2) / static_cast<double>(i + 1));
            }
        }
    }
    double tm = (clock() - beg) * 1.0 / CLOCKS_PER_SEC;

    std::cout << std::format("max ratio: {} b/elem, max ratio2: - b/elem, time per element: {} ns",
                             Pad(std::format("{:.2f}", max_ratio1), 6),
                             //  Pad(std::format("{:.2f}", max_ratio2), 6),
                             Pad(std::format("{:.2f}", tm / n * 1e9), 7));
    std::cout << std::endl;
    // std::cout << std::endl;

    malloc_trim(0);
}

template <typename Key, typename Mapped, typename HashMap>
void TestHasMapImpl(std::vector<size_t> vec, std::string name = "") {

    std::cout << std::format("testing {}", name) << std::endl;

    for (size_t n : vec) {
        TestHasMapImpl_<Key, Mapped, HashMap>(n, name);
    }
    std::cout << std::endl;
}

template <typename Key, typename Mapped>
void TestHasMaps(std::vector<size_t> vec) {
    TestHasMapImpl<Key, Mapped, std::unordered_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "std::unordered_map");
    TestHasMapImpl<Key, Mapped, gtl::flat_hash_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "gtl::flat_hash_map");
    TestHasMapImpl<Key, Mapped, gtl::parallel_flat_hash_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "gtl::parallel_flat_hash_map");
    TestHasMapImpl<Key, Mapped, spp::sparse_hash_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "spp::sparse_hash_map");
    TestHasMapImpl<Key, Mapped, absl::flat_hash_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "absl::flat_hash_map");
    TestHasMapImpl<Key, Mapped, absl::node_hash_map<Key, Mapped, std::hash<Key>, std::equal_to<>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "absl::node_hash_map");
    TestHasMapImpl<Key, Mapped, absl::btree_map<Key, Mapped, std::less<Key>, TrackingAllocator<std::pair<const Key, Mapped>>>>(vec, "absl::btree_map");

    std::cout << std::endl;
}

int main() {
    TestHasMaps<size_t, size_t>({
        (size_t)1e4,
        (size_t)1e5,
        (size_t)1e6,
        (size_t)1e7,
    });
}

// NOLINTEND
