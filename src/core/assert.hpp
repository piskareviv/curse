#include <format>
#include <stdexcept>
#include <string>

#define ENSURE_MSG(expr, msg)                                                                              \
    if (!(expr)) {                                                                                         \
        throw std::runtime_error(                                                                          \
            std::format("{}:{}: critical assertion \"{}\" failed, {}", __FILE__, __LINE__, #expr, (msg))); \
    }

#define ENSURE(expr) ENSURE_MSG(expr, "")
