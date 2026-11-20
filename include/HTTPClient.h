#ifndef _HTTPCLIENT_H_
#define _HTTPCLIENT_H_

#include "Authorization.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HTTPClientInterface.h"

#include <map>
#include <string>
#include <memory>

namespace httpclient {  
  class HTTPClient {
  public:
    HTTPClient(std::string user_agent, bool enable_cookies, bool enable_keepalive) noexcept
      : user_agent(std::move(user_agent)),
	enable_cookies(enable_cookies),
	enable_keepalive(enable_keepalive)
    { }  
    HTTPClient(const HTTPClient & other) = delete;

    virtual ~HTTPClient() = default;
  
    HTTPClient & operator=(const HTTPClient & other) = delete;
    
    HTTPResponse Post(std::string uri, std::string data) {
      HTTPRequest req(Method::POST, std::move(uri));
      req.setContent(data);
      req.setContentType("application/x-www-form-urlencoded;charset=UTF-8");
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, noAuth, response);
      return response;
    }

    HTTPResponse Post(std::string uri, std::string data, const Authorization & auth) {
      HTTPRequest req(Method::POST, std::move(uri));
      req.setContent(std::move(data));
      req.setContentType("application/x-www-form-urlencoded;charset=UTF-8");
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, auth, response);
      return response;
    }
  
    void Post(std::string uri, std::string data, const Authorization & auth, HTTPClientInterface & callback) {
      HTTPRequest req(Method::POST, std::move(uri));
      req.setContent(std::move(data));
      req.setContentType("application/x-www-form-urlencoded;charset=UTF-8");
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      request(req, auth, callback);
    }

    void Get(std::string uri, std::string_view data, const Authorization & auth, HTTPClientInterface & callback) {
      uri += '?';
      uri += data;
      HTTPRequest req(Method::GET, std::move(uri));
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      request(req, auth, callback);
    }

    HTTPResponse Get(std::string uri, std::string_view data, const Authorization & auth, bool follow_location = true) {
      uri += '?';
      uri += data;
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, auth, response);
      return response;
    }

    HTTPResponse Get(std::string uri, std::string_view data, bool follow_location = true) {
      uri += '?';
      uri += data;
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, noAuth, response);
      return response;
    }

    void Get(std::string uri, std::string_view data, HTTPClientInterface & callback, bool follow_location = true) {
      uri += '?';
      uri += data;
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      request(req, noAuth, callback);
    }

    HTTPResponse Get(std::string uri, bool follow_location = true) {
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, noAuth, response);
      return response;
    }
  
    HTTPResponse Get(std::string uri, const Authorization & auth, bool follow_location = true) {
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      HTTPResponse response;
      request(req, auth, response);
      return response;
    }

    void Get(std::string uri, HTTPClientInterface & callback, bool follow_location = true) {
      HTTPRequest req(Method::GET, std::move(uri));
      req.setFollowLocation(follow_location);
      req.setConnectTimeout(getConnectTimeout());
      req.setReadTimeout(getReadTimeout());
      req.setConnectionTimeout(getConnectionTimeout());
      request(req, noAuth, callback);
    }

    void setCookieJar(std::string filename) { cookie_jar = std::move(filename); }

    // A single string_view overload: having both std::string and
    // std::string_view overloads made calls with string literals ambiguous.
    void addDefaultHeader(std::string_view name, std::string_view value) {
      default_headers.insert_or_assign(std::string{ name }, std::string{ value });
    }

    virtual void request(const HTTPRequest & req, const Authorization & auth, HTTPClientInterface & callback) = 0;
    virtual void clearCookies() = 0;

    void setConnectTimeout(int t) { connect_timeout = t; }
    void setReadTimeout(int t) { read_timeout = t; }
    void setConnectionTimeout(int t) { connection_timeout = t; }
  
    int getConnectTimeout() const { return connect_timeout; }
    int getReadTimeout() const { return read_timeout; }
    int getConnectionTimeout() const { return connection_timeout; }

    Authorization noAuth;

  protected:
    const std::string & getUserAgent() const noexcept { return user_agent; }

    std::string user_agent;
    std::string cookie_jar;
    bool enable_cookies, enable_keepalive;
    std::map<std::string, std::string> default_headers;
    int connect_timeout = 0, read_timeout = 0, connection_timeout = 0;
  };

  class HTTPClientFactory {
  public:
    HTTPClientFactory(std::string default_user_agent = "httpclient") noexcept : default_user_agent_(std::move(default_user_agent)) { }
    virtual ~HTTPClientFactory() { }

    std::unique_ptr<HTTPClient> createClient(std::string user_agent, bool enable_cookies = true, bool enable_keepalive = true) {
      return createClient2(std::move(user_agent), enable_cookies, enable_keepalive);
    }

    std::unique_ptr<HTTPClient> createClient(bool enable_cookies = true, bool enable_keepalive = true) {
      return createClient2(getDefaultUserAgent(), enable_cookies, enable_keepalive);
    }

    const std::string & getDefaultUserAgent() const noexcept { return default_user_agent_; }

  protected:
    virtual std::unique_ptr<HTTPClient> createClient2(std::string user_agent, bool enable_cookies = true, bool enable_keepalive = true) = 0;

  private:
    std::string default_user_agent_;
  };
};

#endif
