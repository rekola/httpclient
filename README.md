# httpclient

[![Linux x86-64](https://github.com/rekola/httpclient/actions/workflows/ci-linux-x86_64.yml/badge.svg)](https://github.com/rekola/httpclient/actions/workflows/ci-linux-x86_64.yml)
[![Linux ARM64](https://github.com/rekola/httpclient/actions/workflows/ci-linux-arm64.yml/badge.svg)](https://github.com/rekola/httpclient/actions/workflows/ci-linux-arm64.yml)
[![Windows x86-64](https://github.com/rekola/httpclient/actions/workflows/ci-windows-x86_64.yml/badge.svg)](https://github.com/rekola/httpclient/actions/workflows/ci-windows-x86_64.yml)
[![macOS ARM64](https://github.com/rekola/httpclient/actions/workflows/ci-macos-arm64.yml/badge.svg)](https://github.com/rekola/httpclient/actions/workflows/ci-macos-arm64.yml)

A C++17 HTTP client that is one interface over the HTTP stack each platform
already has, so a program uses the system's own (WinHTTP on Windows, libcurl
elsewhere) without a line of platform code, and without bundling a TLS library.

- **One interface**: `HTTPClient` with `Get` and `Post`, or a streaming
  callback (`HTTPClientInterface`) that sees the status, headers and body as they
  arrive and can stop the transfer.
- **Authorization** as a strategy: `BasicAuth`, `BearerAuth` and `OAuth` (OAuth
  1.0 signing).
- **Utilities** the clients are built from, usable on their own: `URI`,
  `Base64`, `Base64Url`, `SHA1`, `sha2` and `FNV`.

## Backends

| Platform | Stack | Factory | CMake option | Built by CI |
|---|---|---|---|---|
| Linux, macOS, others | libcurl | `CurlClientFactory` | `HTTPCLIENT_CURL` (default off Windows and Android) | yes |
| Windows | WinHTTP | `WinHTTPClientFactory` | `HTTPCLIENT_WINHTTP` (default on Windows) | yes |
| Android | `HttpURLConnection`, through JNI | `AndroidClientFactory` | `HTTPCLIENT_ANDROID` (default on Android) | no |
| Apple | CFNetwork | `iOSCFClientFactory` | `HTTPCLIENT_CFNETWORK` (off) | no |
| Apple | `NSMutableURLRequest` | `iOSClientFactory` | `HTTPCLIENT_NSURL` (off) | no |

The utilities are always built. Only the backends of the platform in use can
build: the others need their own SDK (`jni.h`, CoreFoundation, `winhttp.h`).

## Use

```cpp
#include <BasicAuth.h>
#include <HTTPClient.h>
#ifdef _WIN32
#include <WinHTTPClient.h>
#else
#include <CurlClient.h>
#endif

#include <cstdio>

int main() {
#ifdef _WIN32
  httpclient::WinHTTPClientFactory factory;
#else
  httpclient::CurlClientFactory factory;
#endif
  // A user agent, then whether to keep cookies and connections.
  auto client = factory.createClient("my-program/1.0", /*enable_cookies=*/false, /*enable_keepalive=*/true);
  client->setConnectTimeout(10);  // seconds
  client->setReadTimeout(20);
  client->addDefaultHeader("Accept", "application/json");

  httpclient::HTTPResponse response = client->Get("https://example.com/");
  if (response.isSuccess()) {
    std::printf("%s\n", response.getContent().c_str());
  } else {
    // A status of 0 means nothing came back; the error text says why.
    std::printf("%d %s\n", response.getResultCode(), response.getErrorText().c_str());
  }
}
```

A response is not an error because its status is (a 404 is a response with
`isError()` true); `getResultCode()` is 0 when the connection itself failed.
Redirects are followed unless `follow_location` is false, and then
`isRedirect()` and `getRedirectUrl()` say where to.

The ways to call, all on `HTTPClient`:

```cpp
client->Get(url);                                   // query string in the URL
client->Get(url, "a=1&b=2");                        // the data is the query string
client->Get(url, httpclient::BearerAuth("token"));  // with authorization
client->Post(url, "a=1&b=2");                       // a form (x-www-form-urlencoded)
client->Post(url, body, httpclient::BasicAuth("user", "password"));
```

### Certificates

HTTPS certificates are checked: a server whose certificate does not check out
(unknown authority, expired, wrong host name) fails the request, with a result
code of 0. To accept any certificate, for example to read from servers with
self-signed ones:

```cpp
client->setIgnoreCertificateErrors(true);   // every request this client makes

httpclient::HTTPRequest request(httpclient::Method::GET, "https://self-signed.example/");
request.setIgnoreCertificateErrors(true);   // one request
client->request(request, client->noAuth, response);
```

This makes the connection readable and alterable by anyone on the path, so use it
for data you could fetch over plain HTTP. The libcurl, WinHTTP and CFNetwork
backends support it; the Android and NSURLRequest ones always check. (Before
this was an option, the libcurl and CFNetwork backends never checked
certificates, and the WinHTTP one dropped the check after a failed handshake.)

To read a body as it arrives, or to stop early, give the call a callback instead
of taking an `HTTPResponse`; returning `false` from `handleChunk` ends the
transfer:

```cpp
struct Counter : httpclient::HTTPClientInterface {
  size_t bytes = 0;
  bool handleChunk(std::string_view chunk) override {
    bytes += chunk.size();
    return bytes < 1'000'000;  // refuse more than a megabyte
  }
  void handleResultCode(int code) override { /* the status line */ }
  void handleHeader(std::string_view name, std::string_view value) override { /* each header */ }
};

Counter counter;
client->Get("https://example.com/big", counter);
```

The methods are `GET`, `POST` and `OPTIONS`.

## Build

The library builds with CMake 3.20 or later and a C++17 compiler.

    cmake -B build
    cmake --build build -j
    ctest --test-dir build --output-on-failure

What it needs:

| | Needed for | Linux (apt) | macOS (Homebrew) | Windows (MSYS2 UCRT64) |
|---|---|---|---|---|
| libcurl | the libcurl backend | `libcurl4-openssl-dev` | in the SDK | not used (WinHTTP) |
| utf8proc or libunistring | `URI` (it folds the case of host names) | `libutf8proc-dev` or `libunistring-dev` | `utf8proc` or `libunistring` | `mingw-w64-ucrt-x86_64-libunistring` |
| pkg-config | finding utf8proc (libunistring is found without it) | `pkg-config` | `pkg-config` | `mingw-w64-ucrt-x86_64-pkgconf` |
| Python 3 | the loopback test (skipped without it) | `python3` | `python3` | `mingw-w64-ucrt-x86_64-python` |
| openssl (the command) | the HTTPS cases of the loopback test, which make a self-signed certificate (skipped without it) | `openssl` | in the system | `mingw-w64-ucrt-x86_64-openssl` |

`HTTPCLIENT_UNICODE` picks the library: `AUTO` (the default) takes a system
utf8proc, else a system libunistring, else fetches and builds utf8proc
(`HTTPCLIENT_FETCH_UTF8PROC`, on by default; that build has no install rules,
because the installed library would need it too). `UTF8PROC` and `LIBUNISTRING`
insist on one. Both give the same results; the tests run under either, and CI
builds with each.

Options: the backend options above, `HTTPCLIENT_UNICODE`, `HTTPCLIENT_BUILD_TESTS` and
`HTTPCLIENT_INSTALL` (both on when httpclient is the top-level project), and
`BUILD_SHARED_LIBS`.

### Use it from a project

As a subdirectory or with `FetchContent` (pin a commit):

```cmake
include(FetchContent)
FetchContent_Declare(httpclient
  GIT_REPOSITORY https://github.com/rekola/httpclient
  GIT_TAG <commit>)
FetchContent_MakeAvailable(httpclient)
target_link_libraries(my_program PRIVATE httpclient::httpclient)
```

Or installed (`cmake --install build --prefix <dir>`) and found:

```cmake
find_package(httpclient REQUIRED)
target_link_libraries(my_program PRIVATE httpclient::httpclient)
```

Either way the headers are included by their bare names (`#include
<HTTPClient.h>`, `<CurlClient.h>`).

## Tests

`ctest` runs two programs:

- `httpclient_tests`: the utilities (known vectors for Base64, SHA-1, SHA-2 and
  FNV-1a; URI parsing and encoding), the authorization headers, and how a raw
  response header block is read.
- `httpclient_smoke`: the backend against a small server on this machine
  (`tests/run_smoke.py`): GET, POST, status codes, redirects, headers and
  authorization, a body streamed to a callback and refused part-way, a refused
  connection, a read timeout, and an HTTPS server with a self-signed
  certificate (refused unless told to accept it).

CI builds and tests on Linux (x86-64 and ARM64), macOS and Windows, and builds
a small project against the installed library.

## Credits

Mikael Rekola; Mikko Suni (Android support).

## License

MIT, see [LICENSE](LICENSE).
