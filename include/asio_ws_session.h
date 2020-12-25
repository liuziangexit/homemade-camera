#ifndef __HOMECAM_ASIO_WS_SESSION_H_
#define __HOMECAM_ASIO_WS_SESSION_H_
#include "asio_base_session.h"
#include "boost/beast.hpp"
#include "config.h"
#include "logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace homemadecam {

// Echoes back all received WebSocket messages

template <bool SSL>
class asio_ws_session
    : public asio_base_session<
          SSL, websocket::stream<typename stream<SSL>::type, true>> {
  beast::flat_buffer buffer_;

public:
  template <typename... ARGS>
  asio_ws_session(ARGS &&...args)
      : asio_base_session<SSL,
                          websocket::stream<typename stream<SSL>::type, true>>(
            std::forward<ARGS>(args)...) {}

  ~asio_ws_session() { homemadecam::logger::info("asio_ws_session destruct"); }

  // Get on the correct executor
  virtual void run() override {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(
        this->stream_.get_executor(),
        [this, shared_this = this->shared_from_this()] { this->on_run(); });
  }

  // Start the asynchronous operation
  void on_run() {
    // Set TCP timeout.
    // TODO load from configmanager
    beast::get_lowest_layer(this->stream_)
        .expires_after(std::chrono::seconds(30));

    if constexpr (SSL) {
      // Perform the SSL handshake
      this->stream_.next_layer().async_handshake(
          ssl::stream_base::server,
          [this, shared_this = this->shared_from_this()](beast::error_code ec) {
            this->on_handshake(ec);
          });
    } else {
      // WS handshake
      this->on_handshake(beast::error_code());
    }
  }

  // Websocket handshake
  void on_handshake(beast::error_code ec) {
    if constexpr (SSL) {
      if (ec) {
        homemadecam::logger::error(this->remote_,
                                   " SSL handshake error: ", ec.message());
        return;
      } else {
        homemadecam::logger::info(this->remote_, " SSL handshake OK");
      }
    }

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(this->stream_).expires_never();

    // Set suggested timeout settings for the websocket
    this->stream_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    this->stream_.set_option(
        websocket::stream_base::decorator([](websocket::response_type &res) {
          res.set(http::field::server, "homemade-camera");
        }));

    // Accept the websocket handshake
    this->stream_.async_accept(
        [this, shared_this = this->shared_from_this()](beast::error_code ec) {
          this->on_accept(ec);
        });
  }

  void on_accept(beast::error_code ec) {
    if (ec) {
      homemadecam::logger::error(this->remote_,
                                 "Websocket handshake failed: ", ec.message());
      return;
    }
    homemadecam::logger::info(this->remote_, " Websocket handshake OK");

    // Read a message
    do_read();
  }

  void do_read() {
    // Read a message into our buffer
    this->stream_.async_read(
        buffer_, [this, shared_this = this->shared_from_this()](
                     beast::error_code ec, std::size_t bytes_transferred) {
          this->on_read(ec, bytes_transferred);
        });
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed) {
      homemadecam::logger::info(this->remote_, " Websocket closed");
      return;
    }

    if (ec) {
      homemadecam::logger::error(this->remote_,
                                 " Websocket read failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote_, " Websocket read OK");
    }

    // Echo the message
    this->stream_.text(this->stream_.got_text());
    this->stream_.async_write(
        buffer_.data(),
        [this, shared_this = this->shared_from_this()](
            beast::error_code ec, std::size_t bytes_transferred) {
          this->on_write(ec, bytes_transferred);
        });
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    if (ec) {
      homemadecam::logger::info(this->remote_,
                                " Websocket write failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote_, " Websocket write OK");
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
