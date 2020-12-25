#ifndef __HOMECAM_ASIO_LISTENER_H_
#define __HOMECAM_ASIO_LISTENER_H_
#include "asio_http_session.h"
#include "asio_ws_session.h"
#include "boost/beast.hpp"
#include "config.h"
#include "logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <stdexcept>
#include <string>
//#include <tbb/concurrent_hash_map.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace homemadecam {

struct endpoint_compare {
  static size_t hash(const tcp::endpoint &e) {
    uint64_t hash = e.address().to_v4().to_ulong();
    hash <<= 32;
    hash |= e.port();
    return hash;
  }
  //! True if strings are equal
  static bool equal(const tcp::endpoint &x, const tcp::endpoint &y) {
    return hash(x) == hash(y);
  }
};

// Accepts incoming connections and launches the sessions
class asio_listener : public std::enable_shared_from_this<asio_listener> {
  net::io_context &ioc_;
  ssl::context *ssl_ctx_;
  tcp::acceptor acceptor_;
  tcp::endpoint endpoint_;
  // tbb::concurrent_hash_map<tcp::endpoint, std::shared_ptr<>()> test;

public:
  asio_listener(net::io_context &ioc, tcp::endpoint endpoint,
                ssl::context *ssl_ctx)
      : ioc_(ioc), ssl_ctx_(ssl_ctx), acceptor_(net::make_strand(ioc)),
        endpoint_(endpoint) {}

  ~asio_listener() { homemadecam::logger::info("asio_listener destruct"); }

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
    homemadecam::logger::info("listening at ", this->endpoint_);
    do_accept();
  }

  void stop() {
    try {
      acceptor_.close();
    } catch (const std::exception &ex) {
      homemadecam::logger::error("listener quitting: ", ex.what());
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
      homemadecam::logger::error("asio accept fail: ", ec.message());
      if (ec == boost::asio::error::operation_aborted) {
        return;
      }
    } else {
      homemadecam::logger::info(socket.remote_endpoint(), " TCP handshake OK");
      // TODO 也许ssl_ctx_可以是constexpr的
      // Create the session and run it
      if (false) {
        if (this->ssl_ctx_) {
          std::make_shared<asio_http_session<true>>(std::move(socket),
                                                    *ssl_ctx_)
              ->run();
        } else {
          std::make_shared<asio_http_session<false>>(std::move(socket))->run();
        }
      } else {
        if (this->ssl_ctx_) {
          std::make_shared<asio_ws_session<true>>(std::move(socket), *ssl_ctx_)
              ->run();
        } else {
          std::make_shared<asio_ws_session<false>>(std::move(socket))->run();
        }
      }
    }

    // Accept another connection
    do_accept();
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
