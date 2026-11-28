# launcher-server protocol

`ac launcher-server` is a small RPC server for the launcher. It speaks a
custom TCP protocol rather than HTTP: the launcher needs a handful of
commands, not resource semantics, and a raw length-prefixed body lets file
downloads stream without base64 or a full HTTP stack.

Source: `src/launcher-server/` (`protocol.zig` wire format, `session.zig`
connection/response model, `commands.zig` handlers, `main.zig` listener).
Command handlers only return a `Response`; framing, JSON envelopes, body
lengths and flushing stay in `session.zig`.

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

| `cmd`      | request payload      | success payload        | body |
|------------|----------------------|------------------------|------|
| `ping`     | none                 | `{"message":"pong"}`   | none |
| `echo`     | `{"text": string}`   | `{"text": string}`     | none |
| `get_file` | `{"path": string}`   | `{"size": u64}`        | file |

Error codes currently emitted: `unknown_command`, `invalid_payload`,
`unexpected_body`, `file_serving_disabled`, `invalid_path`, `not_found`,
`access_denied`, `is_directory`, `not_a_directory`, `not_a_file`, `io_error`.

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

## Examples

```sh
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
```

## Tests and fuzzing

`zig build launcher-server-test` runs the protocol/path/session unit tests;
`zig build test` includes them.

The in-process request/response path has a real `std.testing.fuzz` target
(parse + dispatch + response framing). Run Zig's built-in coverage-guided
fuzzer with:

```sh
zig build launcher-server-test -Doptimize=ReleaseSafe --fuzz=10000
```

Use `ReleaseSafe`: zig 0.17.1's Debug/self-hosted backend produces an empty
coverage file for fuzz builds.
