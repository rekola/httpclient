#include <Base64.h>
#include <HTTPClient.h>
#include <URI.h>
#ifdef _WIN32
#include <WinHTTPClient.h>
#else
#include <CurlClient.h>
#endif

#include <cstdio>
#include <memory>

int main() {
#ifdef _WIN32
  httpclient::WinHTTPClientFactory factory;
#else
  httpclient::CurlClientFactory factory;
#endif
  std::unique_ptr<httpclient::HTTPClient> client = factory.createClient("consumer");
  httpclient::URI uri("https://example.com/a?b=c");
  bool ok = client && uri.getDomain() == "example.com" && httpclient::Base64::encode(std::string("foo")) == "Zm9v";
  std::puts(ok ? "ok" : "failed");
  return ok ? 0 : 1;
}
