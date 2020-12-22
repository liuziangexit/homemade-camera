#ifndef __HOMECAM_ASIO_SESSION_H_
#define __HOMECAM_ASIO_SESSION_H_
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

// beast::ssl_stream<beast::tcp_stream>
// beast::tcp_stream
template <
    typename UNDERLYING_STREAM,
    bool SSL =
        std::is_same_v<UNDERLYING_STREAM, beast::ssl_stream<beast::tcp_stream>>,
    class =
        std::enable_if<std::is_same_v<UNDERLYING_STREAM, beast::tcp_stream> ||
                       std::is_same_v<UNDERLYING_STREAM,
                                      beast::ssl_stream<beast::tcp_stream>>>>
class asio_session
    : public std::enable_shared_from_this<asio_session<UNDERLYING_STREAM>> {
  websocket::stream<UNDERLYING_STREAM> ws_;
  beast::flat_buffer buffer_;

public:
  // ssl
  asio_session(tcp::socket &&socket, ssl::context &ssl_ctx)
      : ws_(std::move(socket), ssl_ctx) {
    static_assert(SSL);
  }

  // tcp
  asio_session(tcp::socket &&socket) : ws_(std::move(socket)) {
    static_assert(!SSL);
  }

  // Get on the correct executor
  void run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(ws_.get_executor(),
                  beast::bind_front_handler(&asio_session::on_run,
                                            this->shared_from_this()));
  }

  // Start the asynchronous operation
  void on_run() {
    // Set TCP timeout.
    // TODO load from configmanager
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    // Perform the SSL handshake
    if constexpr (SSL) {
      ws_.next_layer().async_handshake(
          ssl::stream_base::server,
          beast::bind_front_handler(&asio_session::on_handshake,
                                    this->shared_from_this()));
    } else {
      this->on_handshake(beast::error_code());
    }
  }

  // Websocket handshake
  void on_handshake(beast::error_code ec) {
    if (ec) {
      homemadecam::logger::error("handshake error: ", ec.message());
      return;
    }

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(ws_).expires_never();

    // Set suggested timeout settings for the websocket
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(
        websocket::stream_base::decorator([](websocket::response_type &res) {
          res.set(http::field::server, "homemade-camera");
        }));

    // Accept the websocket handshake
    ws_.async_accept(beast::bind_front_handler(&asio_session::on_accept,
                                               this->shared_from_this()));
  }

  void on_accept(beast::error_code ec) {
    if (ec) {
      homemadecam::logger::error("session on_accept error: ", ec.message());
      return;
    }

    // Read a message
    do_read();
  }

  void do_read() {
    // Read a message into our buffer
    ws_.async_read(buffer_,
                   beast::bind_front_handler(&asio_session::on_read,
                                             this->shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed) {
      homemadecam::logger::info("session closed");
      return;
    }

    if (ec) {
      homemadecam::logger::info("session read failed: ", ec.message());
      return;
    }

    // Echo the message
    ws_.text(ws_.got_text());
    ws_.async_write(buffer_.data(),
                    beast::bind_front_handler(&asio_session::on_write,
                                              this->shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    if (ec) {
      homemadecam::logger::info("session write failed: ", ec.message());
      return;
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H