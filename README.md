# sftpfs

This extension, sftpfs, allow you to read from a remote server using sftp.

Note: This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

## Features

Read files from a remote server by providing all the information in the URI format `sftp://[username]:[password]@[host]:[port]/path/to/file`


```sql
SELECT * FROM read_csv_auto('sftp://testuser:testpass@localhost:2222/config/data/data_1.csv');
```

Configuration Options

Name	| Description | Type
-----|------|------
`sftp_identity_file` | Path to identity file, echivalent of the `-i` option | `VARCHAR`
`sftp_private_key` | The private key in PEM format | `VARCHAR`
`sftp_private_key_password` | The password required to decrypt the private key | `VARCHAR`
`sftp_username` | The user name | `VARCHAR`
`sftp_password` | The password | `VARCHAR`

Example

```sql
SET sftp_username = 'test';
SET sftp_password='abcd';
SELECT * FROM read_csv_auto('sftp://localhost:2222/config/data/data_1.csv');
```

Note: Currently the extension creates a new connection for each read and forwards all the read requests to the destination server. There is no support for chaching and does not include connection management.

## Building
### Managing dependencies
DuckDB extensions uses VCPKG for dependency management. Enabling VCPKG is very simple: follow the [installation instructions](https://vcpkg.io/en/getting-started) or just run the following:
```shell
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/sftpfs/sftpfs.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded.
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `sftpfs.duckdb_extension` is the loadable binary as it would be distributed.

## Running the extension
To run the extension code, simply start the shell with `./build/release/duckdb`.

Now we can use the features from the extension directly in DuckDB.
```
D select * from read_csv_auto('sftp://testuser:testpass@localhost:2222/config/data/data_1.csv');
┌───────┬─────────┐
│  id   │  name   │
│ int64 │ varchar │
├───────┼─────────┤
│     1 │ test1   │
│     2 │ test2   │
│     3 │ test3   │
│     4 │ test4   │
└───────┴─────────┘
```

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```

## Installing the deployed binaries
To install your extension binaries from S3, you will need to do two things. Firstly, DuckDB should be launched with the
`allow_unsigned_extensions` option set to true. How to set this will depend on the client you're using. Some examples:

CLI:
```shell
duckdb -unsigned
```

Python:
```python
con = duckdb.connect(':memory:', config={'allow_unsigned_extensions' : 'true'})
```

NodeJS:
```js
db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

Secondly, you will need load the extension
```sql
INSTALL 'build/release/extension/sftpfs/sftpfs.duckdb_extension'
LOAD sftpfs
```

In order to load extension into the system duckdb the version must match and you need to build the extension
using the corect duckdb version

```bash
cd duckdb
git fetch --all
git switch v1.3.0
cd extension-ci-tools

git fetch --all
git switch v1.3.0
```

and then build the extension with this version.

Alternativelly you can disable the extension metadata check by running

```sql
set allow_extensions_metadata_mismatch=true;
```

Check duckdb documentation how to load an extension from a remote repostory.



## TODO

- [ ] Implement file handle caching
- [ ] Implement globs
- [ ] Implement connection management - sftp client
