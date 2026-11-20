# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`httpclient` is a C++17 library (namespace `httpclient`) that wraps platform HTTP stacks behind one interface: libcurl, Windows WinHTTP, Android `HttpURLConnection` (via JNI), and iOS (CFNetwork / `NSMutableURLRequest`). It is meant to be consumed by a host project: as a subdirectory or with `FetchContent`, or installed.

Headers live in `include/`, sources in `src/`. It builds with CMake (`CMakeLists.txt`): a single `httpclient` library (alias `httpclient::httpclient`) of the utilities plus the backends the options select (`HTTPCLIENT_CURL`, `HTTPCLIENT_WINHTTP`, `HTTPCLIENT_ANDROID`, `HTTPCLIENT_CFNETWORK`, `HTTPCLIENT_NSURL`; the defaults pick the platform's own). Only compile the backend(s) for the platform you target — the others need platform SDKs (`jni.h`, CoreFoundation, `<winhttp.h>`, libcurl). `URI.cpp` needs utf8proc (a system copy via pkg-config, else fetched by CMake).

    cmake -B build && cmake --build build -j && ctest --test-dir build --output-on-failure

- `tests/tests.cpp` (`httpclient_tests`): the utilities, the authorization headers and response-header parsing; no network.
- `tests/smoke.cpp` + `tests/run_smoke.py` (`httpclient_smoke`): the backend against a Python server on this machine. Add a case to both when you change what a backend does.
- `tests/consumer/`: a project that uses the installed library (`find_package(httpclient)`); CI installs to a prefix and builds it.
- CI (`.github/workflows/`): Linux x86-64 and ARM64, macOS, Windows (MSYS2 UCRT64, WinHTTP). The Android and Apple (CFNetwork, NSURL) backends are not built by CI.

## Architecture

Three layers, spread across several headers:

- **`HTTPClient`** (`include/HTTPClient.h`) — abstract base per platform. All the `Get`/`Post` convenience overloads are non-virtual and just build an `HTTPRequest`, copy the client's timeouts into it, and funnel into the single pure-virtual `request(req, auth, callback)`. A backend therefore implements only `request()` and `clearCookies()`. The `HTTPResponse`-returning overloads work because `HTTPResponse` is itself an `HTTPClientInterface` callback that accumulates the body/headers.
- **`HTTPClientInterface`** (`include/HTTPClientInterface.h`) — the streaming callback interface (`handleChunk` is the only pure virtual; also result code, headers, redirect, error/log, disconnect, `onIdle`). Backends feed raw header blocks to `handleHeaderChunk`, which parses the status line and headers and calls `handleResultCode` / `handleHeader` / `handleRedirectUrl` (returning false from a redirect or chunk handler aborts).
- **`HTTPClientFactory`** — one subclass per backend (`CurlClientFactory`, `AndroidClientFactory`, `iOSClientFactory`, `iOSCFClientFactory`, `WinHTTPClientFactory`). Public `createClient()` forwards to the protected virtual `createClient2(user_agent, cookies, keepalive)`. The concrete client classes are defined inside each `.cpp` (not exposed in headers). Some backends have static setup: `CurlClientFactory::globalInit/globalCleanup`, `AndroidClientFactory::initialize(JNIEnv*)`.

Auth is a separate strategy: `Authorization` (`getHeaderName`, `createHeader`, `getToken`) with `BasicAuth`, `BearerAuth`, and `OAuth` (OAuth 1.0 signing, uses `SHA1`, `Base64`, `URI`; `content` map must stay ordered). `HTTPClient::noAuth` is the default no-op instance.

`HTTPRequest` only supports `GET`, `POST`, `OPTIONS` (PUT/DELETE are commented out). `to_string(Method)` values are used verbatim by backends, so don't change them.

Utility code (self-contained, no platform deps): `URI` (parse/canonicalize), `Base64`/`Base64Url`, `SHA1`, `sha2`, `FNV`.

## Gotchas

- When changing the `HTTPClient`/`HTTPClientFactory` interface, update **every** backend. They can drift: the base `createClient2` takes `std::string user_agent` by value (as `CurlClient`/`WinHTTPClient` do), while `AndroidClient`, `iOSClient` and `iOSCFClient` still declare `const std::string &`, which won't match the base signature (and `override` in `AndroidClient.h` would fail to compile). Check this when touching those files.
- Include style is mixed (`<HTTPClient.h>` in Android, `"HTTPClient.h"` elsewhere); header guards use `_NAME_H_` style, not `#pragma once`.
- `.gitattributes` normalizes line endings (`* text=auto`); source files use two-space indentation (some tabs inside `HTTPClientInterface.h`).
