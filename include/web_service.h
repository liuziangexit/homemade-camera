#ifndef __HOMECAM_WEB_SERVICE_H_
#define __HOMECAM_WEB_SERVICE_H_
#include "asio_listener.h"
#include "asio_session.h"
#include "boost/beast.hpp"
#include "config.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace homemadecam {

class web {
public:
  web(const std::string &config_file) : conf(config_file) {
    // TODO frome config
    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(std::atoi("8080"));
    auto const threads = std::max<int>(1, std::atoi("2"));

    // The io_context is required for all I/O
    this->ioc = new net::io_context(threads);

    // Create and launch a listening port
    this->listener = std::make_shared<asio_listener>(
        *ioc, (ssl::context *)NULL, tcp::endpoint{address, port});
  }

  void run() {
    listener->run();
    ioc->run();
  }

private:
  config conf;
  std::shared_ptr<asio_listener> listener;
  net::io_context *ioc;
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H