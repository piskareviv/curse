#pragma once

#include <cassert>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/core/assert.hpp"

namespace curse {

template <typename T, typename... Args>
std::vector<T> Concat(std::vector<T> a, const Args... args) {
    (a.insert(a.end(), args.begin(), args.end()), ...);
    return a;
}

template <typename T>
void ValueToBytes(const T& value, std::span<char> bytes) {
    ENSURE(bytes.size() == sizeof(T));
    memcpy(bytes.data(), reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
std::vector<char> ValueToBytes(const T& value) {
    std::vector<char> bytes(sizeof(T));
    ValueToBytes(value, bytes);
    return bytes;
}

template <typename T>
T ValueFromBytes(std::span<const char> bytes) {
    ENSURE(bytes.size() == sizeof(T));  // NOLINT

    T value;
    memcpy(reinterpret_cast<char*>(&value), bytes.data(), sizeof(T));
    return value;
}

template <typename T>
std::vector<char> VecToBytes(const std::vector<T>& vec) {
    if (vec.empty()) {
        return {};
    }
    std::vector<char> bytes(sizeof(T) * vec.size());
    memcpy(bytes.data(), reinterpret_cast<const char*>(vec.data()), sizeof(T) * vec.size());
    return bytes;
}

template <typename T>
std::vector<T> VecFromBytes(std::span<const char> bytes) {
    if (bytes.empty()) {
        return {};
    }

    ENSURE(bytes.size() % sizeof(T) == 0);  // NOLINT

    std::vector<T> data(bytes.size() / sizeof(T));
    memcpy(reinterpret_cast<char*>(data.data()), bytes.data(), bytes.size());
    return data;
}

template <typename T>
std::pair<std::span<T>, std::span<T>> SplitSpan(std::span<T> span, size_t ind) {
    return {span.subspan(0, ind), span.subspan(ind)};
}

template <size_t... seq>
void StaticForEach(std::index_sequence<seq...>, auto&& f) {
    (f(std::integral_constant<size_t, seq>()), ...);
}
template <size_t N>
void StaticFor(auto&& f) {
    StaticForEach(std::make_index_sequence<N>(), std::forward<decltype(f)>(f));
}

}  // namespace curse
