#pragma once

#include "duckdb/common/file_open_flags.hpp"
#include "duckdb/common/file_system.hpp"
#include <libssh2.h>
#include <libssh2_sftp.h>

namespace duckdb {

struct SFTPParams {
	string file_path;
	string username;
	string password;
	string host;
	int port;
	string identity_file;
	string private_key;
	string private_key_password;
};

class SFTPFileHandle : public FileHandle {

public:
	SFTPFileHandle(FileSystem &fs, const OpenFileInfo &file, FileOpenFlags flags, const SFTPParams &params);
	~SFTPFileHandle() override;

	SFTPParams sftp_params;

	// sftp
	LIBSSH2_SFTP_HANDLE *sftp_file_handle;
	int sock;
	LIBSSH2_SESSION *session;
	LIBSSH2_SFTP *sftp_session;

	// file handle
	FileOpenFlags flags;
	idx_t length;
	time_t last_modified;
	string etag;

	// read info
	idx_t file_offset;

public:
	void Close() override;
	void Initialize();
private:
	int OpenConnection(const string &hostname, int port, string &err_msg);
	void GetSftpError(const uint64_t err_code, string &err_msg);
};

class SFTPFileSystem : public FileSystem {
public:
	~SFTPFileSystem() override;
	vector<OpenFileInfo> Glob(const string &path, FileOpener *opener = nullptr) override {
		return {path}; // FIXME
	}

	void Read(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) override;
	int64_t Read(FileHandle &handle, void *buffer, int64_t nr_bytes) override;
	void Write(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) override;
	int64_t Write(FileHandle &handle, void *buffer, int64_t nr_bytes) override;
	void FileSync(FileHandle &handle) override;
	int64_t GetFileSize(FileHandle &handle) override;
	time_t GetLastModifiedTime(FileHandle &handle) override;
	string GetVersionTag(FileHandle &handle) override;
	bool FileExists(const string &filename, optional_ptr<FileOpener> opener) override;
	void Seek(FileHandle &handle, idx_t location) override;
	idx_t SeekPosition(FileHandle &handle) override;
	bool CanHandleFile(const string &fpath) override;
	bool CanSeek() override {
		return true;
	}
	bool OnDiskFile(FileHandle &handle) override {
		return false;
	}
	bool IsPipe(const string &filename, optional_ptr<FileOpener> opener) override {
		return false;
	}
	string GetName() const override {
		return "SFTPFileSystem";
	}
	string PathSeparator(const string &path) override {
		return "/";
	}

protected:
	unique_ptr<FileHandle> OpenFileExtended(const OpenFileInfo &file, FileOpenFlags flags,
	                                        optional_ptr<FileOpener> opener) override;
	bool SupportsOpenFileExtended() const override {
		return true;
	}

private:
	bool ParseSftpParams(const string &path, SFTPParams &params, string &err_msg);
};

} // namespace duckdb
