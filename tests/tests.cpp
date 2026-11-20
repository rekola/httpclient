// Tests of what needs no network: the utilities, the authorization headers and how a response is read. A plain
// program: each CHECK counts, and the exit status says whether any failed.
#include <Base64.h>
#include <Base64Url.h>
#include <BasicAuth.h>
#include <BearerAuth.h>
#include <FNV.h>
#include <HTTPRequest.h>
#include <HTTPResponse.h>
#include <SHA1.h>
#include <URI.h>
#include <sha2.h>

#include <cstdio>
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

static std::string hex(const unsigned char * p, size_t n) {
  static const char digits[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) {
    s += digits[p[i] >> 4];
    s += digits[p[i] & 15];
  }
  return s;
}

static void testBase64() {
  CHECK(Base64::encode(std::string("")) == "");
  CHECK(Base64::encode(std::string("f")) == "Zg==");
  CHECK(Base64::encode(std::string("fo")) == "Zm8=");
  CHECK(Base64::encode(std::string("foo")) == "Zm9v");
  CHECK(Base64::encode(std::string("foobar")) == "Zm9vYmFy");
  // Long input is broken into lines unless the line length is raised.
  std::string long_input(100, 'a');
  CHECK(Base64::encode(long_input).find('\n') != std::string::npos);
  CHECK(Base64::encode(long_input, 0, 1000).find('\n') == std::string::npos);
  CHECK(Base64::encode64BitId(0) == "");
  CHECK(Base64::encode64BitId(63).size() == 1 && Base64::encode64BitId(64).size() == 2);
}

static void testBase64Url() {
  const unsigned char bytes[] = {0xfb, 0xff, 0xfe};
  std::string e = Base64Url::encode(bytes, 3);
  CHECK(e == "-__-");  // the URL alphabet: - and _ for + and /, no padding
  CHECK(Base64Url::encode(std::string("f")) == "Zg");
  unsigned char out[8];
  size_t n = sizeof out;
  CHECK(Base64Url::decode("-__-", 4, out, &n) && n == 3 && out[0] == 0xfb && out[1] == 0xff && out[2] == 0xfe);
  CHECK(Base64Url::decode_id(Base64Url::encode_id(123456789012345LL)) == 123456789012345LL);
}

static void testHashes() {
  unsigned char out[32];
  sha1_context ctx;
  sha1_starts(&ctx);
  sha1_update(&ctx, reinterpret_cast<const unsigned char *>("abc"), 3);
  sha1_finish(&ctx, out);
  CHECK(hex(out, 20) == "a9993e364706816aba3e25717850c26c9cd0d89d");

  sha2("abc", 3, out, 0);
  CHECK(hex(out, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  sha2("abc", 3, out, 1);  // SHA-224
  CHECK(hex(out, 28) == "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");

  // FNV-1a test vectors: the empty string is the offset basis, "a" is a known value.
  CHECK(FNV::calcFNV1a_64(std::string("")) == static_cast<long long>(0xcbf29ce484222325ULL));
  CHECK(FNV::calcFNV1a_64(std::string("a")) == static_cast<long long>(0xaf63dc4c8601ec8cULL));
  CHECK(static_cast<unsigned>(FNV::calcFNV1a_32(std::string(""))) == 0x811c9dc5u);
  CHECK(static_cast<unsigned>(FNV::calcFNV1a_32(std::string("a"))) == 0xe40c292cu);
}

static void testURI() {
  URI u("https://example.com/a/b?x=1&y=2#frag");
  CHECK(u.getScheme() == "https");
  CHECK(u.getDomain() == "example.com");
  CHECK(u.getPath() == "/a/b");
  CHECK(u.getQueryString() == "x=1&y=2");
  CHECK(u.getFragment() == "frag");
  auto q = u.parseQueryString();
  CHECK(q.size() == 2 && q["x"] == "1" && q["y"] == "2");

  CHECK(URI::urlencode("a b&c") == "a%20b%26c");
  CHECK(URI::urlencode("a b", true) == "a+b");
  CHECK(URI::urldecode("a%20b%26c") == "a b&c");
  CHECK(URI::urldecode(URI::urlencode("päivää / 100%")) == "päivää / 100%");

  std::map<std::string, std::string> pairs{{"b", "2 3"}, {"a", "1"}};
  std::string formatted = URI::formatQueryString(pairs);
  CHECK(formatted == "a=1&b=2%203");
  auto back = URI::parseQueryString(formatted);
  CHECK(back["a"] == "1" && back["b"] == "2 3");
}

// The host name as URI folds it: lower case, composed, without what is ignorable. The same with either Unicode library.
static void testReducedDomain() {
  auto reduced = [](const std::string & domain) {
    URI u;
    u.setDomain(domain);
    return u.getReducedDomain();
  };
  CHECK(reduced("Example.COM") == "example.com");
  CHECK(reduced("WWW.Example.COM") == "example.com");
  CHECK(reduced("") == "");
  CHECK(reduced("\xc3\x84PYR\xc3\x84.FI") == "\xc3\xa4pyr\xc3\xa4.fi");        // ÄPYRÄ.FI
  CHECK(reduced("A\xcc\x88.fi") == "\xc3\xa4.fi");                               // A + combining diaeresis
  CHECK(reduced("Stra\xc3\x9f" "e.de") == "strasse.de");                          // full case folding
  CHECK(reduced("ex\xc2\xad" "ample.com") == "example.com");                      // a soft hyphen is ignorable
  CHECK(reduced("exa\x01mple.com") == "example.com");                              // a control character is dropped
  CHECK(reduced("ex\nample.com") == "ex ample.com");                               // a newline is one space
  CHECK(reduced("ex\r\nample.com") == "ex ample.com");
  CHECK(reduced("\xff.com").empty());                                              // not UTF-8
  CHECK(URI("http://Example.COM/").getDomainHash() == FNV::calcFNV1a_64(std::string("example.com")));
}

static void testAuthorization() {
  CHECK(BasicAuth("user", "pass").createHeader() == "Basic dXNlcjpwYXNz");
  CHECK(BasicAuth("user", "pass").getHeaderName() == std::string("Authorization"));
  CHECK(BearerAuth("token123").createHeader() == "Bearer token123");
  CHECK(Authorization().createHeader().empty());
}

static void testRequest() {
  CHECK(to_string(Method::GET) == "GET" && to_string(Method::POST) == "POST" && to_string(Method::OPTIONS) == "OPTIONS");
  HTTPRequest req(Method::POST, "http://example.com/");
  req.addHeader("X-One", "1");
  req.addHeader("X-One", "2");  // the same name replaces
  req.setContent("body");
  CHECK(req.getHeaders().size() == 1 && req.getHeaders().at("X-One") == "2");
  CHECK(req.getContent() == "body" && req.getURI() == "http://example.com/" && req.getMethod() == Method::POST);
}

// The raw header block a backend hands over: the status, the headers and the redirect.
static void testResponseHeaders() {
  HTTPResponse r;
  CHECK(r.handleHeaderChunk("HTTP/1.1 302 Found\r\nContent-Type: text/plain\r\nLocation: http://example.com/next\r\n\r\n"));
  CHECK(r.getResultCode() == 302 && r.isRedirect() && !r.isSuccess() && !r.isError());
  CHECK(r.getRedirectUrl() == "http://example.com/next");
  CHECK(r.getHeaders().at("Content-Type") == "text/plain");

  HTTPResponse ok, notFound, none;
  ok.handleHeaderChunk("HTTP/2 200 OK\r\n\r\n");
  notFound.handleHeaderChunk("HTTP/1.1 404 Not Found\r\n\r\n");
  CHECK(ok.isSuccess() && !ok.isError());
  CHECK(notFound.isError());
  CHECK(none.isError() && none.getResultCode() == 0);  // nothing came back

  // A redirect the program refuses stops the transfer.
  struct Refusing : HTTPResponse {
    bool handleRedirectUrl(std::string_view) override { return false; }
  } refusing;
  CHECK(!refusing.handleHeaderChunk("HTTP/1.1 301 Moved\r\nLocation: http://elsewhere/\r\n\r\n"));

  HTTPResponse body;
  CHECK(body.handleChunk("hel") && body.handleChunk("lo"));
  CHECK(body.getContent() == "hello");
}

int main() {
  testBase64();
  testBase64Url();
  testHashes();
  testURI();
  testReducedDomain();
  testAuthorization();
  testRequest();
  testResponseHeaders();
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
