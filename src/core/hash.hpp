#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>

namespace curse {

namespace {

using u128 = __uint128_t;  // NOLINT

constexpr uint64_t kMod = (1ull << 61) - 1;

inline uint64_t Add(uint64_t a, uint64_t b) {
    return a + b - kMod * (a + b >= kMod);
}

inline uint64_t Mul(uint64_t a, uint64_t b) {
    u128 prod = static_cast<u128>(a) * static_cast<u128>(b);
    return Add(prod >> 61, prod & kMod);
}
inline uint64_t Mod(uint64_t val) {
    return Add(val >> 61, val & kMod);
}

inline uint64_t GenSeed() {
    std::random_device rd;
    uint64_t cnt = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::vector<int> vec(1);
    uint64_t addr = reinterpret_cast<uintptr_t>(&vec[0]);

    std::seed_seq seq{
        1585056363u,
        rd(),
        static_cast<uint32_t>(cnt),
        static_cast<uint32_t>(cnt >> 32),
        static_cast<uint32_t>(addr),
        static_cast<uint32_t>(addr >> 32),
    };

    uint32_t ar[2];
    seq.generate(ar, ar + 2);

    uint64_t res = ar[0] | static_cast<uint64_t>(ar[1]) << 32;
    return res;
}

inline uint64_t MakeBase(uint64_t val) {
    return std::max<uint64_t>(1e10 + 998'244'353, Mod(val));
}

constexpr uint64_t kBigRandomConstant = 0xed2aad7f8a401e61;

struct PolynomialHash {
private:
    uint64_t m_base;
    uint64_t m_hash;

public:
    PolynomialHash(uint64_t base, uint64_t init = 0) : m_base(base), m_hash(init) {}

    void Update(uint64_t val) {
        m_hash = Add(Mul(m_hash, m_base), val);
    }

    uint64_t Get() const {
        return m_hash;
    }
};

}  // namespace

inline uint64_t GenSeedFast() {
    thread_local static std::mt19937_64 rnd(GenSeed());
    return rnd();
}

struct BytesHasher {
private:
    uint64_t m_base;

public:
    BytesHasher(uint64_t seed = 1) : m_base(MakeBase(seed * kBigRandomConstant)) {}

    template <size_t sz>
    size_t operator()(const std::array<char, sz>& ar) const {
        const uint64_t k = sizeof(uint64_t) / 2;
        const uint64_t n = sz / k;

        PolynomialHash hs(m_base, sz);
        for (uint64_t i = 0; i < n; i += 1) {
            uint64_t val = 0;
            memcpy(&val, &ar[i * k], k);
            hs.Update(val);
        }

        if constexpr (n * k != sz) {
            const size_t dlt = sz - n * k;
            uint64_t val = 0;
            memcpy(&val, &ar[n * k], dlt);
            hs.Update(val);
        }

        return hs.Get();
    }

    size_t operator()(std::span<const char> sp) const {
        const uint64_t k = sizeof(uint64_t) / 2;
        const size_t n = sp.size() / k;

        PolynomialHash hs(m_base, sp.size());

        for (uint64_t i = 0; i < n; i += 1) {
            uint64_t val = 0;
            memcpy(&val, &sp[i * k], k);
            hs.Update(val);
        }

        if (n * k != sp.size()) {
            const size_t dlt = sp.size() - n * k;
            uint64_t val = 0;
            memcpy(&val, &sp[n * k], dlt);
            hs.Update(val);
        }

        return hs.Get();
    }

    size_t operator()(std::string_view sv) const {
        return operator()(std::span<const char>(sv));
    }

    size_t operator()(const std::string& s) const {
        return operator()(std::span<const char>(s));
    }
};

template <typename Hasher>
struct Remixer {
private:
    Hasher m_hasher;
    uint64_t m_seed = 0;

public:
    Remixer(Hasher hasher = Hasher(), uint64_t seed = 1)
        : m_hasher(std::move(hasher)), m_seed(seed * kBigRandomConstant) {}

    template <typename T>
    size_t operator()(const T& val) const {
        uint64_t hs = m_hasher(val);

        uint64_t z = (hs ^ m_seed) + 0x9e3779b97f4a7c15;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
};

struct MyHasher {
    using is_transparent = void;  // NOLINT

    template <typename T>
    std::size_t operator()(const T& value) const {
        if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point> ||
                      std::is_same_v<T, std::chrono::year_month_day>) {
            if constexpr (std::is_same_v<T, std::chrono::year_month_day>) {
                auto x = std::chrono::sys_days{value}.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            } else {
                auto x = value.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            }
        } else {
            return std::hash<T>()(value);
        }
    }
};

template <typename T, typename Hasher = std::hash<T>>
struct VecHasher {
private:
    uint64_t m_base;
    Hasher m_hasher;

public:
    VecHasher(Hasher hasher = Hasher(), uint64_t seed = 1)
        : m_base(MakeBase(seed * kBigRandomConstant)), m_hasher(std::move(hasher)) {}

    size_t operator()(std::span<const T> vec) const {
        PolynomialHash hs(m_base, vec.size());
        for (size_t i = 0; i < vec.size(); i++) {
            uint64_t h = m_hasher(vec[i]);
            hs.Update(Mod(h ^ (h >> 30)));
        }
        return hs.Get();
    }
};

};  // namespace curse
