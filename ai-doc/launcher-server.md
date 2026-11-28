# launcher-server protocol

`ac launcher-server` is a small RPC server for the launcher. It speaks a
custom TCP protocol rather than HTTP: the launcher needs a handful of
commands, not resource semantics, and a raw length-prefixed body lets file
downloads stream without base64 or a full HTTP stack.

Source: `src/launcher-server/` (`protocol.zig` wire format, `session.zig`
connection/response model, `commands.zig` handlers, `main.zig` listener).
Command handlers only return a `Response`; framing, JSON envelopes, body
lengths and flushing stay in `session.zig`.

On startup the server also builds two MPQ archives from the server's own
content directories and keeps them in memory for `get_patch`. See
[Patch archives](#patch-archives).

## Framing

One connection can carry any number of request/response frames, processed in
order:

```
<json envelope>\n
<body_len raw bytes>
```

* The envelope is exactly one line of JSON, terminated by `\n` (`\r\n` is
  tolerated). Blank lines are ignored.
* In replies, `body_len` is the length of the optional opaque body. The body
  may contain `\n`, `\0`, or any other byte, and the next envelope starts
  immediately after it.
* Request bodies are not supported yet: a request with a non-zero `body_len`
  gets an `unexpected_body` error and the connection closes. Keeping `body_len`
  in the envelope reserves the framing for uploads without having to guess how
  to resynchronize today.
* Headers are limited to 16 KiB.

This is the content-safety answer: **line delimiting only applies to the JSON
envelope; file bytes are length-delimited**. Base64-in-JSON would also be safe
but costs 33% and prevents streaming; sending file bytes as JSON strings would
break on NULs and need escaping anyway.

## Limits and failure behavior

* A malformed envelope or one longer than 16 KiB closes the connection: there
  is no trustworthy `id` to correlate an error with.
* A request with a non-zero `body_len` gets `unexpected_body` and the
  connection closes (it cannot be resynchronized without reading a body).
* Unknown envelope fields and unknown payload fields are ignored, so adding
  fields later is backward-compatible.
* A handler error before the response header is sent becomes an `ok:false`
  frame and the connection stays usable. An I/O failure while streaming a
  success body closes the connection.
* At most 1024 clients are handled concurrently (each client is a thread on
  `std.Io.Threaded`). Hitting the cap is fatal: the accept loop panics on
  `error.ConcurrencyUnavailable` rather than silently dropping the connection.

## Request

```json
{"id":7,"cmd":"get_file","payload":{"path":"addons/a.zip"},"body_len":0}
```

| field      | type   | notes                                                        |
|------------|--------|--------------------------------------------------------------|
| `id`       | u64    | client-chosen, echoed in the response; optional (default 0)   |
| `cmd`      | string | one of the commands below                                    |
| `payload`  | object | command-specific; may be omitted for commands with no input  |
| `body_len` | u64    | request body length; must be 0 (bodies not supported yet)     |

The server processes requests sequentially on a connection. `id` is there so
pipelining/multiplexing can be added without changing the envelope.

## Response

Success:

```json
{"id":7,"cmd":"get_file","ok":true,"payload":{"size":1234},"body_len":1234}
<1234 raw file bytes>
```

Failure:

```json
{"id":7,"cmd":"get_file","ok":false,"error":{"code":"not_found","message":"file not found"},"body_len":0}
```

`ok:false` replies never carry a body. Once a success header with a non-zero
`body_len` is sent, the server writes exactly that many bytes; a file that
changes size mid-transfer truncates the connection rather than desynchronizing
the framing.

## Commands

| `cmd`          | request payload      | success payload      | body |
|----------------|----------------------|----------------------|------|
| `ping`         | none                 | `{"message":"pong"}` | none |
| `echo`         | `{"text": string}`   | `{"text": string}`   | none |
| `get_file`     | `{"path": string}`   | `{"size": u64}`      | file |
| `get_patch`    | `{"name": string}`   | `{"size": u64}`      | MPQ archive |
| `get_manifest` | `{"name": string}`   | `{"size": u64}`      | `*.sum.txt` text |

`get_patch`/`get_manifest` accept `"D"`, `"R"`, `"patch-D"` or `"patch-R"`
(case-insensitive).

Error codes currently emitted: `unknown_command`, `invalid_payload`,
`unexpected_body`, `file_serving_disabled`, `invalid_path`, `not_found`,
`access_denied`, `is_directory`, `not_a_directory`, `not_a_file`, `io_error`,
`patches_disabled`, `unknown_patch`.

### `get_file` and `--root`

`get_file` is disabled unless the server starts with `--root <dir>`, which it
opens once at startup. Paths are relative to that directory and:

* must not be absolute, empty, contain `\` or `:`, or contain empty, `.` or
  `..` components (`:` also blocks Windows drive-relative paths such as
  `C:foo`);
* are walked component by component with symlinks disabled, so a symlink
  inside the root cannot expose files outside it (Linux does not implement
  `resolve_beneath` as an `O_*` flag, hence the explicit walk);
* must resolve to a regular file; directories, symlinks and FIFOs are refused.

The server intentionally does not default to the working directory, because
that would expose `.env` and other server files.

## Patch archives

Both archives are built once at startup, kept in memory, and served through
`get_patch`. They are deliberately named `patch-D.mpq` / `patch-R.mpq`: `D` and
`R` are not used by other common patches, so the client's alphabetical patch
load order cannot collide with them. Nothing is written to disk.

### `patch-D.mpq`

Packs a **static whitelist** of DBCs from `<DataDir>/dbc`, not the whole
directory (`dbc_files` in `patches.zig`; currently only `Spell.dbc`). Files
are stored under `DBFilesClient\...`, which is the path the 3.3.5a client
uses (see `src/tools/map_extractor/System.cpp`). Add a name to the whitelist
when the client should receive it; files not listed are ignored.

### `patch-R.mpq`

Packs every regular file below `<AssetsDir>` (default `<DataDir>/../assets`).
The directory tree is mirrored at the archive root; `/` becomes `\` so paths
are canonical MPQ paths. Symlinks and directories are skipped.

### Manifests

Each archive embeds its own manifest at the archive root:

* `patch-D.sum.txt`
* `patch-R.sum.txt`

Archive-wide names (rather than a single `sum.txt`) mean both files stay
addressable if the client mounts both patches at once. A line is:

```
<md5 hex>  <archive path>
```

The MD5 covers the **input file bytes**, not the compressed MPQ payload. Lines
are sorted by path. The manifest itself is not listed, so a client can fetch
`get_manifest` and compare it against its local files before deciding whether
to download `get_patch`. MD5 is used for cheap comparison, not authentication;
the launcher transport is not a trust boundary.

### MPQ details

`mpq.zig` is a small MPQ v1 writer, not a general-purpose library:

* v1 header, encrypted block/hash tables (the same hash/crypt table as
  `deps/libmpq`), hash table size rounded up to a power of two;
* every file is a single-unit stream (`MPQ_FILE_SINGLE_UNIT`), so zlib
  compresses the whole DBC as one stream instead of restarting per 4 KiB
  sector;
* zlib level 9 (system zlib, the same library libmpq/the client use; `mpq.zig`
  declares `compress2`/`compressBound` directly); if compression does not make
  the file smaller it is stored raw;
* 32-bit offsets/sizes: an archive or file above 4 GiB is rejected;
* duplicate archive paths (after case-insensitive MPQ hashing) are rejected.

Verified against the repo's libmpq: a generated `patch-D.mpq` extracts
`DBFilesClient\Spell.dbc` byte-for-byte identical to `data/dbc/Spell.dbc`.

### Configuration

The patch inputs are configured by environment variables only (`Cli.cpp`
loads `.env` and `ConfigMgr` like the other `ac` subcommands). There are
deliberately no `--data-dir`/`--assets-dir` flags, so there is one source of
truth:

| env              | default             | meaning |
|------------------|---------------------|---------|
| `AC_DATA_DIR`    | `<exe dir>/data`    | directory containing `dbc/` |
| `AC_ASSETS_DIR`  | `<DataDir>/../assets` | tree packed into `patch-R.mpq` |

Relative paths resolve against the executable directory, matching `DataDir`
elsewhere. Paths are normalized (`.`/`..` and trailing separators), so
`AC_DATA_DIR=/srv/ac/data/` still derives `/srv/ac/assets`. `AssetsDir`
defaults to the `assets` **sibling of the resolved data directory**, so the
usual repo layout (`AC_DATA_DIR=../../data`) automatically picks up
`../../assets` without a second variable. A missing
`<DataDir>/dbc/<whitelisted file>` or an unreadable `<AssetsDir>` fails
startup: a launcher downloading a stale/absent patch is worse than a server
that refuses to start. An existing but empty assets tree is allowed and
produces an archive containing only its manifest.

## Examples

```sh
# builds patch-D.mpq/patch-R.mpq from ./data/dbc and ./assets, then listens.
# patch-R defaults to the `assets` sibling of AC_DATA_DIR.
AC_DATA_DIR="$PWD/data" \
  ./zig-out/bin/ac launcher-server --root ./launcher-files
```

`ping` and `echo`:

```sh
printf '%s\n' '{"id":1,"cmd":"ping"}' | nc 127.0.0.1 5815
# {"id":1,"cmd":"ping","ok":true,"payload":{"message":"pong"},"body_len":0}
```

Downloading a binary file requires reading the header line, then exactly
`body_len` bytes:

```python
import json, socket

s = socket.create_connection(("127.0.0.1", 5815))
f = s.makefile("rb")
s.sendall(b'{"id":2,"cmd":"get_file","payload":{"path":"addons/a.zip"}}\n')
env = json.loads(f.readline())
body = f.read(env["body_len"]) if env["body_len"] else b""

# Patch archives work the same way. Fetch and compare the manifest first.
s.sendall(b'{"id":3,"cmd":"get_manifest","payload":{"name":"D"}}\n')
env = json.loads(f.readline())
manifest = f.read(env["body_len"])
s.sendall(b'{"id":4,"cmd":"get_patch","payload":{"name":"D"}}\n')
env = json.loads(f.readline())
archive = f.read(env["body_len"])  # starts with b"MPQ\\x1a"
```

## Tests and fuzzing

`zig build launcher-server-test` runs the protocol/path/session unit tests plus
the MPQ writer and patch-builder tests; `zig build test` includes them. The MPQ
tests check the crypt table and hash values against libmpq's generated table,
then parse a built archive back (header, encrypted tables, zlib payload).

The in-process request/response path has a real `std.testing.fuzz` target
(parse + dispatch + response framing). Run Zig's built-in coverage-guided
fuzzer with:

```sh
zig build launcher-server-test -Doptimize=ReleaseSafe --fuzz=10000
```

Use `ReleaseSafe`: zig 0.17.1's Debug/self-hosted backend produces an empty
coverage file for fuzz builds.
