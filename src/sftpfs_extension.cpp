#define DUCKDB_EXTENSION_MAIN

#include "sftpfs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "sftpfs.hpp"

namespace duckdb {

static void LoadInternal(DatabaseInstance &instance) {
	// register the file system here
	auto &fs = instance.GetFileSystem();

	fs.RegisterSubSystem(make_uniq<SFTPFileSystem>());

	auto &config = DBConfig::GetConfig(instance);
	config.AddExtensionOption("sftp_identity_file", "Identity file path to be used for ssh authentication",
	                          LogicalType::VARCHAR);
	config.AddExtensionOption("sftp_private_key", "The private key to be used for ssh authentication",
	                          LogicalType::VARCHAR);
	config.AddExtensionOption("sftp_private_key_password", "The password used to decrypt the private key",
	                          LogicalType::VARCHAR);
	config.AddExtensionOption("sftp_username", "User name to be used to authenticate all sftp requests",
	                          LogicalType::VARCHAR);
	config.AddExtensionOption("sftp_password", "Password used to be used to authenticate all sftp requests",
	                          LogicalType::VARCHAR);
}

void SftpfsExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string SftpfsExtension::Name() {
	return "sftpfs";
}

std::string SftpfsExtension::Version() const {
#ifdef EXT_VERSION_SFTPFS
	return EXT_VERSION_SFTPFS;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void sftpfs_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::SftpfsExtension>();
}

DUCKDB_EXTENSION_API const char *sftpfs_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
