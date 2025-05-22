#include "exception.hpp"

namespace duckdb {
SftpException::SftpException(const string &msg) : Exception(ExceptionType::UNKNOWN_TYPE , msg) {
}

} // namespace duckdb
