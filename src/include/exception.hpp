#pragma once

#include "duckdb/common/exception.hpp"

namespace duckdb {
class SftpException : public Exception {
public:
	DUCKDB_API explicit SftpException(const string &msg);

	template <typename... ARGS>
	explicit SftpException(const string &msg, ARGS... params) : SftpException(ConstructMessage(msg, params...)) {
	}
};

} // namespace duckdb
