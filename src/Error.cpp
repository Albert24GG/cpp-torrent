#include "Error.hpp"

#include "cpptrace/cpptrace.hpp"

#include <sstream>

[[nodiscard]] std::string err::err_msg_with_trace(const std::string& msg) {
    std::ostringstream trace;

    cpptrace::generate_trace().print(trace);

    return std::string{msg + '\n' + trace.str()};
}
