#ifndef __HOMECAM_WEB_SERVICE_H_
#define __HOMECAM_WEB_SERVICE_H_
#include "config.h"
#include <stdexcept>
#include <string>
#define HTTPSERVER_IMPL
#include <webserver/httpserver.h>

namespace homemadecam {

class web {
public:
  web(const std::string &config_file) : conf(config_file) {
    this->http = http_server_init(this->conf.web_port, handle_request);
    if (!this->http)
      throw std::exception();
  }

  void run() { http_server_listen_addr(this->http, "127.0.0.1"); }

private:
  static void handle_request(struct http_request_s *request) {
    struct http_response_s *response = http_response_init();
    http_response_status(response, 200);
    http_response_header(response, "Content-Type", "text/plain");
    http_response_body(response, "naive", sizeof("naive") - 1);
    http_respond(request, response);
  }

private:
  config conf;
  struct http_server_s *http;
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
