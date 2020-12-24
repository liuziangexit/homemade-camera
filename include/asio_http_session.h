#ifndef __HOMECAM_ASIO_HTTP_SESSION_H_
#define __HOMECAM_ASIO_HTTP_SESSION_H_
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
#include <functional>
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
class asio_http_session
    : public std::enable_shared_from_this<asio_ws_session<UNDERLYING_STREAM>> {
  websocket::stream<UNDERLYING_STREAM> stream_;
  beast::flat_buffer buffer_;
  const beast::tcp_stream::endpoint_type remote;
  http::request<http::string_body> req_;

public:
  // ssl
  asio_http_session(tcp::socket &&socket, ssl::context &ssl_ctx)
      : stream_(std::move(socket), ssl_ctx),
        remote(beast::get_lowest_layer(stream_).socket().remote_endpoint()) {
    static_assert(SSL);
  }

  // tcp
  asio_http_session(tcp::socket &&socket)
      : stream_(std::move(socket)),
        remote(beast::get_lowest_layer(stream_).socket().remote_endpoint()) {
    static_assert(!SSL);
  }

  // Get on the correct executor
  void run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&asio_http_session::on_run,
                                            this->shared_from_this()));
  }

  // Start the asynchronous operation
  void on_run() {
    // Set TCP timeout.
    // TODO load from configmanager
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    if constexpr (SSL) {
      // Perform the SSL handshake
      stream_.next_layer().async_handshake(
          ssl::stream_base::server,
          beast::bind_front_handler(&asio_http_session::on_handshake,
                                    this->shared_from_this()));
    } else {
      this->on_handshake(beast::error_code());
    }
  }

  // previous layer handshake ok
  void on_handshake(beast::error_code ec) {
    if constexpr (SSL) {
      if (ec) {
        homemadecam::logger::error(this->remote,
                                   " SSL handshake error: ", ec.message());
        return;
      } else {
        homemadecam::logger::info(this->remote, " SSL handshake OK");
      }
    }

    // post read req
    this->do_read();
  }

  void on_accept(beast::error_code ec) {
    if (ec) {
      homemadecam::logger::error(this->remote,
                                 "Websocket handshake failed: ", ec.message());
      return;
    }
    homemadecam::logger::info(this->remote, " Websocket handshake OK");

    // Read a message
    do_read();
  }

  void do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};
    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Set the timeout.
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(stream_, buffer_, req_,
                     beast::bind_front_handler(&asio_http_session::on_read,
                                               this->shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // This means they closed the connection
    if (ec == http::error::end_of_stream) {
      homemadecam::logger::info(this->remote, " http closed");
      return;
    }

    if (ec) {
      homemadecam::logger::error(this->remote,
                                 " http read failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote, " http read OK");
    }

    // TODO ...
    // handle request
  }

  template <bool isRequest, class Body, class Fields>
  void send(http::message<isRequest, Body, Fields> &&msg) {
    http::message<isRequest, Body, Fields> *response =
        new http::message<isRequest, Body, Fields>(std::move(msg));
    std::function<void()> deleter([response] { delete response; });

    try {
      // Write the response
      http::async_write(stream_, *response,
                        beast::bind_front_handler(&asio_http_session::on_write,
                                                  this->shared_from_this(),
                                                  std::move(deleter)));
    } catch (const std::exception &ex) {
      delete response;
      throw ex;
    }
  }

  void on_write(const std::function<void()> &deleter, beast::error_code ec,
                std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    //删除response对象
    deleter();

    if (ec) {
      homemadecam::logger::info(this->remote,
                                " http write failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote, " http write OK");
    }

    // Do another read
    do_read();
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
