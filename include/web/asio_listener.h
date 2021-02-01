#ifndef __HOMECAM_ASIO_LISTENER_H_
#define __HOMECAM_ASIO_LISTENER_H_
#include "asio_http_session.h"
#include "asio_ws_session.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "util/logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <functional>
#include <stdexcept>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace hcam {

// Accepts incoming connections and launches the sessions
class asio_listener : public std::enable_shared_from_this<asio_listener> {
  net::io_context &ioc_;
  tcp::endpoint endpoint_;
  std::function<void(tcp::socket &&)> create_session_;
  tcp::acceptor acceptor_;

public:
  asio_listener(net::io_context &ioc, tcp::endpoint endpoint,
                const std::function<void(tcp::socket &&)> &create_session)
      : ioc_(ioc), endpoint_(endpoint), create_session_(create_session),
        acceptor_(net::make_strand(ioc)) {}

  ~asio_listener() { hcam::logger::debug("asio_listener destruct"); }

  // Start accepting incoming connections
  void run() {
    beast::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint_.protocol(), ec);
    if (ec) {
      throw std::runtime_error("acceptor_.open");
    }

    // Allow address reuse
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      throw std::runtime_error("acceptor_.open");
    }

    // Bind to the server address
    acceptor_.bind(endpoint_, ec);
    if (ec) {
      throw std::runtime_error("acceptor_.bind");
    }

    // Start listening for connections
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      throw std::runtime_error("acceptor_.listen");
    }
    hcam::logger::info("listening at ", this->endpoint_);
    do_accept();
  }

  void stop() {
    try {
      acceptor_.close();
    } catch (const std::exception &ex) {
      hcam::logger::debug("listener stop failed: ", ex.what());
    }
  }

private:
  void do_accept() {
    // The new connection gets its own strand
    acceptor_.async_accept(net::make_strand(ioc_),
                           beast::bind_front_handler(&asio_listener::on_accept,
                                                     shared_from_this()));
  }

  void on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
      hcam::logger::debug("asio accept fail: ", ec.message());
      if (ec == boost::asio::error::operation_aborted) {
        return;
      }
    } else {
      hcam::logger::info(socket.remote_endpoint(), " session created");
      hcam::logger::debug(socket.remote_endpoint(), " TCP handshake OK");
      // Create the session and run it
      this->create_session_(std::move(socket));
    }

    // Accept another connection
    do_accept();
  }
};

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
