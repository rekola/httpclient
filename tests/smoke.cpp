// A backend against a server on this machine (run by run_smoke.py, which passes the server's URL): the requests
// the library builds, the responses it reads, redirects, authorization, streaming, errors and timeouts.
#include <BasicAuth.h>
#include <BearerAuth.h>
#include <HTTPClient.h>
#if defined(HTTPCLIENT_SMOKE_WINHTTP)
#include <WinHTTPClient.h>
#else
#include <CurlClient.h>
#endif

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

using namespace httpclient;

static int checks = 0, failures = 0;

#define CHECK(cond) \
  do { \
    checks++; \
    if (!(cond)) { \
      failures++; \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

// The body in pieces, as a backend hands it over; a limit makes it refuse more.
struct Collector : HTTPClientInterface {
  size_t total = 0, chunks = 0, limit = static_cast<size_t>(-1);
  bool handleChunk(std::string_view chunk) override {
    total += chunk.size();
    chunks++;
    return total < limit;
  }
};

static bool contains(const std::string & s, const std::string & part) { return s.find(part) != std::string::npos; }

int main(int argc, char ** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s BASE_URL\n", argv[0]);
    return 2;
  }
  const std::string base = argv[1];

#if defined(HTTPCLIENT_SMOKE_WINHTTP)
  WinHTTPClientFactory factory;
#else
  CurlClientFactory factory;
#endif
  std::unique_ptr<HTTPClient> client = factory.createClient("httpclient-smoke/1.0", /*enable_cookies=*/false, /*enable_keepalive=*/true);
  client->setConnectTimeout(5);
  client->setReadTimeout(5);
  client->setConnectionTimeout(20);

  {  // a plain GET
    HTTPResponse r = client->Get(base + "/hello");
    CHECK(r.getResultCode() == 200 && r.isSuccess());
    CHECK(r.getContent() == "hello");
    CHECK(r.getHeaders().count("Content-Type") == 1 && contains(r.getHeaders().at("Content-Type"), "text/plain"));
  }
  {  // an error status is a response, not a failure to get one
    HTTPResponse r = client->Get(base + "/status/404");
    CHECK(r.getResultCode() == 404 && r.isError() && r.getContent() == "nope");
  }
  {  // redirects are followed, or reported
    HTTPResponse followed = client->Get(base + "/redirect");
    CHECK(followed.getResultCode() == 200 && followed.getContent() == "hello");
    HTTPResponse stopped = client->Get(base + "/redirect", false);
    CHECK(stopped.isRedirect() && contains(stopped.getRedirectUrl(), "/hello"));
  }
  {  // the data of a GET is its query string (a std::string_view: a bare literal would pick the follow_location overload)
    HTTPResponse r = client->Get(base + "/query", std::string_view("a=1&b=2"));
    CHECK(r.getContent() == "a=1&b=2");
  }
  {  // a POST sends its body as a form
    HTTPResponse r = client->Post(base + "/echo", "a=1&b=two");
    CHECK(r.getResultCode() == 200 && r.getContent() == "a=1&b=two");
    CHECK(contains(r.getHeaders().at("X-Seen-Content-Type"), "application/x-www-form-urlencoded"));
  }
  {  // the user agent, default headers and the authorization strategies
    client->addDefaultHeader("X-Test", "yes");
    HTTPResponse plain = client->Get(base + "/headers");
    CHECK(contains(plain.getContent(), "user-agent: httpclient-smoke/1.0"));
    CHECK(contains(plain.getContent(), "x-test: yes"));
    CHECK(!contains(plain.getContent(), "authorization:"));
    HTTPResponse basic = client->Get(base + "/headers", BasicAuth("user", "pass"));
    CHECK(contains(basic.getContent(), "authorization: Basic dXNlcjpwYXNz"));
    HTTPResponse bearer = client->Get(base + "/headers", BearerAuth("token123"));
    CHECK(contains(bearer.getContent(), "authorization: Bearer token123"));
  }
  {  // a body larger than one chunk, streamed to a callback
    Collector c;
    client->Get(base + "/large", c);
    CHECK(c.total == 300000);
    HTTPResponse whole = client->Get(base + "/large");
    CHECK(whole.getContent().size() == 300000);
  }
  {  // a callback that refuses the rest ends the transfer early
    Collector c;
    c.limit = 1;
    client->Get(base + "/large", c);
    CHECK(c.total > 0 && c.total < 300000);
  }
  {  // nothing listening: no result code, and a reason
    HTTPResponse r = client->Get("http://127.0.0.1:1/");
    CHECK(r.getResultCode() == 0 && r.isError());
  }
  {  // a server that answers too late
    client->setReadTimeout(1);
    auto start = std::chrono::steady_clock::now();
    HTTPResponse r = client->Get(base + "/slow");
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
    CHECK(r.isError() && r.getContent().empty());
    CHECK(seconds < 4);
    client->setReadTimeout(5);
  }
  {  // and the client still works afterwards
    CHECK(client->Get(base + "/hello").getContent() == "hello");
  }

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
