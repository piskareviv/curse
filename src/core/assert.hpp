#include <exception>
#include <stdexcept>
#include <string>

#define ENSURE_MSG(expr, msg)                                                                                        \
    if (!(expr)) {                                                                                                   \
        throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " critical assertion \"" + \
                                 #expr + "\" failed, " + (msg));                                                     \
    }

#define ENSURE(expr) ENSURE_MSG(expr, "")
