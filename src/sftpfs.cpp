#include "sftpfs.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/file_opener.hpp"
#include "exception.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/typedefs.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <regex>
#include <unistd.h>
#include <netdb.h>

namespace duckdb {

static const std::regex params_regex("^(\\S+?(:\\S+)?@)?([A-Za-z0-9\\.-]+)(:\\d+)?");

// SFTPFileHandle

SFTPFileHandle::SFTPFileHandle(FileSystem &fs, const OpenFileInfo &file, FileOpenFlags flags, const SFTPParams &params)
    : FileHandle(fs, file.path, flags), sftp_params(params), flags(flags), length(0) {
}

SFTPFileHandle::~SFTPFileHandle() {
	Close();
};

void SFTPFileHandle::Close() {
	if (sftp_file_handle) {
		libssh2_sftp_close(sftp_file_handle);
	}
	if (sftp_session) {
		libssh2_sftp_shutdown(sftp_session);
	}
	if (session) {
		libssh2_session_disconnect(session, "Normal Shutdown");
		libssh2_session_free(session);
	}
	if (sock != LIBSSH2_INVALID_SOCKET) {
		shutdown(sock, 2);
		LIBSSH2_SOCKET_CLOSE(sock);
	}
	libssh2_exit();
}

void SFTPFileHandle::Initialize() {

	string err_msg;
	char *sftp_err_msg;
	int sftp_err_msg_len = 0;
	int sftp_err_code = 0;

	sock = OpenConnection(sftp_params.host, sftp_params.port, err_msg);
	if (sock == LIBSSH2_INVALID_SOCKET) {
		throw SftpException("Unable to connect to %s on port %d: %s", sftp_params.host, sftp_params.port, err_msg);
	}

	session = libssh2_session_init();
	if (!session) {
		throw SftpException("Could not initialize session");
	}

	libssh2_session_set_blocking(session, 1);

	if (libssh2_session_handshake(session, sock)) {
		sftp_err_code = libssh2_session_last_error(session, &sftp_err_msg, &sftp_err_msg_len, 0);
		throw SftpException("Unable to establishing a ssh session: %s", sftp_err_msg);
	}

	// here we decide how we do the authentication
	const char *password = nullptr;
	if (!sftp_params.identity_file.empty()) {
		password = sftp_params.private_key_password.c_str();
	}
	if (sftp_params.private_key.size() > 0) {
		if (libssh2_userauth_publickey_frommemory(session, sftp_params.username.c_str(), sftp_params.username.size(),
		                                          nullptr, 0, sftp_params.private_key.c_str(),
		                                          sftp_params.private_key.size(), password)) {
			sftp_err_code = libssh2_session_last_error(session, &sftp_err_msg, &sftp_err_msg_len, 0);
			throw SftpException("Unable to authenticate: %s", sftp_err_msg);
		}
	} else if (sftp_params.identity_file.size() > 0) {
		if (libssh2_userauth_publickey_fromfile(session, sftp_params.username.c_str(), nullptr,
		                                        sftp_params.identity_file.c_str(), password)) {
			sftp_err_code = libssh2_session_last_error(session, &sftp_err_msg, &sftp_err_msg_len, 0);
			throw SftpException("Unable to authenticate: %s", sftp_err_msg);
		}
	} else {
		if (libssh2_userauth_password(session, sftp_params.username.c_str(), sftp_params.password.c_str())) {
			sftp_err_code = libssh2_session_last_error(session, &sftp_err_msg, &sftp_err_msg_len, 0);
			throw SftpException("Unable to authenticate: %s", sftp_err_msg);
		}
	}

	sftp_session = libssh2_sftp_init(session);
	if (!sftp_session) {
		sftp_err_code = libssh2_session_last_error(session, &sftp_err_msg, &sftp_err_msg_len, 0);
		throw SftpException("Unable to init a SFTP session: %s", sftp_err_msg);
	}

	sftp_file_handle = libssh2_sftp_open(sftp_session, sftp_params.file_path.c_str(), LIBSSH2_FXF_READ, 0);

	if (!sftp_file_handle) {
		GetSftpError(libssh2_sftp_last_error(sftp_session), err_msg);
		throw SftpException("Unable to open file: %s", err_msg);
	}

	LIBSSH2_SFTP_ATTRIBUTES attrs;
	auto fstat_err_code = libssh2_sftp_fstat_ex(sftp_file_handle, &attrs, 0);
	if (fstat_err_code != 0) {
		GetSftpError(libssh2_sftp_last_error(sftp_session), err_msg);
		throw SftpException("Unable to get file attributes: %s", err_msg);
	}

	length = attrs.filesize;
	last_modified = static_cast<time_t>(attrs.mtime);
}

void SFTPFileHandle::GetSftpError(const uint64_t err_code, string &err_msg) {

	switch (err_code) {
	case LIBSSH2_FX_OK:
		err_msg = "No error";
		return;
	case LIBSSH2_FX_EOF:
		err_msg = "End of file";
		return;
	case LIBSSH2_FX_NO_SUCH_FILE:
		err_msg = "No such file";
		return;
	case LIBSSH2_FX_PERMISSION_DENIED:
		err_msg = "Permission denied";
		return;
	case LIBSSH2_FX_NO_SUCH_PATH:
		err_msg = "No such path";
		return;
	case LIBSSH2_FX_INVALID_HANDLE:
		err_msg = "Invalid handle";
		return;
	case LIBSSH2_FX_INVALID_FILENAME:
		err_msg = "Invalid file name";
		return;
	case LIBSSH2_FX_NOT_A_DIRECTORY:
		err_msg = "Not a directory";
		return;
	case LIBSSH2_FX_NO_CONNECTION:
		err_msg = "No connection";
		return;
	case LIBSSH2_FX_CONNECTION_LOST:
		err_msg = "Connection lost";
		return;
	case LIBSSH2_FX_UNKNOWN_PRINCIPAL:
		err_msg = "Unknown principal";
		return;
	case LIBSSH2_FX_FAILURE:
		err_msg = "Generic failure";
		return;
	case LIBSSH2_FX_BAD_MESSAGE:
		err_msg = "Bad message";
		return;
	case LIBSSH2_FX_OP_UNSUPPORTED:
		err_msg = "Operation not supported";
		return;
	case LIBSSH2_FX_WRITE_PROTECT:
		err_msg = "Write protect";
		return;
	case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
		err_msg = "No space left";
		return;
	case LIBSSH2_FX_QUOTA_EXCEEDED:
		err_msg = "Quote exceeded";
		return;
	case LIBSSH2_FX_NO_MEDIA:
		err_msg = "No media";
		return;
	case LIBSSH2_FX_LOCK_CONFlICT:
		err_msg = "Lock conflict";
		return;
	case LIBSSH2_FX_DIR_NOT_EMPTY:
		err_msg = "Directory not empty";
		return;
	case LIBSSH2_FX_LINK_LOOP:
		err_msg = "Link loop";
		return;
	default:
		err_msg = "Unknown error";
	}
}

int SFTPFileHandle::OpenConnection(const string &hostname, int port, string &err_msg) {
	int sd, err;
	struct addrinfo hints = {}, *addrs;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	err = getaddrinfo(hostname.c_str(), std::to_string(port).c_str(), &hints, &addrs);
	if (err != 0) {
		err_msg = gai_strerror(err);
		return -1;
	}

	for (struct addrinfo *addr = addrs; addr != nullptr; addr = addr->ai_next) {
		sd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sd == -1) {
			err = errno;
			break;
		}
		if (connect(sd, addr->ai_addr, addr->ai_addrlen) == 0) {
			break;
		}
		err = errno;
		close(sd);
		sd = -1;
	}

	freeaddrinfo(addrs);

	if (sd == -1) {
		err_msg = strerror(err);
	}

	return sd;
}

// SFTPFileHandle

// SFTPFileSystem

SFTPFileSystem::~SFTPFileSystem() {};

void SFTPFileSystem::Read(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	// fprintf(stderr, "reading from file %s, bytes %lld, locations = %llu\n", sfh.path.c_str(), nr_bytes, location);
	// Save the current offset
	auto file_offset = sfh.file_offset;
	libssh2_sftp_seek64(sfh.sftp_file_handle, location);
	Read(handle, buffer, nr_bytes);
	// Restore file offset to the previous value
	Seek(handle, file_offset);
	//fprintf(stderr, "reading from file %s end, bytes %lld, locations = %llu\n", sfh.path.c_str(), nr_bytes, location);
}

int64_t SFTPFileSystem::Read(FileHandle &handle, void *buffer, int64_t nr_bytes) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	//fprintf(stderr, "reading from file %s, bytes %lld\n", sfh.path.c_str(), nr_bytes);
	int64_t buffer_available = nr_bytes;
	int64_t buffer_offset = 0;
	while (buffer_available > 0) {
		auto n_read =
		    libssh2_sftp_read(sfh.sftp_file_handle, reinterpret_cast<char *>(buffer) + buffer_offset, buffer_available);
		if (n_read < 0) {
			throw SftpException("Error read file %s: err %d", sfh.sftp_params.file_path, n_read);
		}
		if (n_read == 0) {
			break;
		}
		buffer_available -= n_read;
		buffer_offset += n_read;
	}
	sfh.file_offset += buffer_offset;
	//fprintf(stderr, "reading from file %s end, read %lld\n", sfh.path.c_str(), buffer_offset);
	return buffer_offset;
}

void SFTPFileSystem::Write(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) {
	throw NotImplementedException("SFTP Write not implemented");
}

int64_t SFTPFileSystem::Write(FileHandle &handle, void *buffer, int64_t nr_bytes) {
	throw NotImplementedException("SFTP Write not implemented");
}

void SFTPFileSystem::FileSync(FileHandle &handle) {
	throw NotImplementedException("SFTP FileSync not implemented");
}

int64_t SFTPFileSystem::GetFileSize(FileHandle &handle) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	return static_cast<int64_t>(sfh.length);
}

time_t SFTPFileSystem::GetLastModifiedTime(FileHandle &handle) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	return sfh.last_modified;
}

string SFTPFileSystem::GetVersionTag(FileHandle &handle) {
	return "";
}

bool SFTPFileSystem::FileExists(const string &filename, optional_ptr<FileOpener> opener) {
	try {
		auto handle = OpenFile(filename, FileFlags::FILE_FLAGS_READ, opener);
		auto &sfh = handle->Cast<SFTPFileHandle>();
		if (sfh.length == 0) {
			return false;
		}
		return true;
	} catch (...) {
		return false;
	};
}

void SFTPFileSystem::Seek(FileHandle &handle, idx_t location) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	libssh2_sftp_seek64(sfh.sftp_file_handle, location);
	sfh.file_offset = location;
}

idx_t SFTPFileSystem::SeekPosition(FileHandle &handle) {
	auto &sfh = handle.Cast<SFTPFileHandle>();
	return sfh.file_offset;
}

bool SFTPFileSystem::CanHandleFile(const string &fpath) {
	return fpath.rfind("sftp://", 0) == 0;
}

unique_ptr<FileHandle> SFTPFileSystem::OpenFileExtended(const OpenFileInfo &file, FileOpenFlags flags,
                                                        optional_ptr<FileOpener> opener) {
	FileOpenerInfo info;
	info.file_path = file.path;
	SFTPParams params;
	string err_msg;

	if (!ParseSftpParams(file.path, params, err_msg)) {
		throw SftpException("Unable to parse file path %s: %s", file.path, err_msg.c_str());
	}

	FileOpener::TryGetCurrentSetting(opener, "sftp_identity_file", params.identity_file, info);
	FileOpener::TryGetCurrentSetting(opener, "sftp_private_key", params.private_key, info);
	FileOpener::TryGetCurrentSetting(opener, "sftp_private_key_password", params.private_key_password, info);

	if (params.username.empty()) {
		FileOpener::TryGetCurrentSetting(opener, "sftp_username", params.username, info);
	}
	if (params.password.empty()) {
		FileOpener::TryGetCurrentSetting(opener, "sftp_username", params.password, info);
	}

	//  .ssh/config - use ssh config -
	//  https://superuser.com/questions/1276485/configure-ssh-key-path-to-use-for-a-specific-host

	auto handle = duckdb::make_uniq<SFTPFileHandle>(*this, file, flags, params);
	handle->Initialize();

	return std::move(handle);
}

bool SFTPFileSystem::ParseSftpParams(const string &input, SFTPParams &params, string &err) {

	params.port = 0;

	auto path_idx = input.find('/', 7);
	if (path_idx == string::npos) {
		err = "invalid file path";
		return false;
	}

	params.file_path = input.substr(path_idx);
	auto host = input.substr(7, path_idx - 7);

	std::smatch match;
	auto result = std::regex_match(host, match, params_regex);

	if (result) {
		// user name
		if (match[1].matched) {
			auto user = match[1].str();
			auto col_idx = user.find(':');
			if (col_idx == string::npos) {
				params.username = user.substr(0, user.size() - 1);
			} else {
				params.username = user.substr(0, col_idx);
			}
		}
		// pass
		if (match[2].matched) {
			params.password = match[2].str().substr(1);
		}
		// host
		if (match[3].matched) {
			params.host = match[3].str();
		}
		// port
		if (match[4].matched) {
			params.port = stoi(match[4].str().substr(1));
		}
		// set default to 22 if not specified
		if (params.port == 0) {
			params.port = 22;
		}
		return true;
	}

	err = "invalid host";
	return false;
}

// SFTPFileHandle

} // namespace duckdb
