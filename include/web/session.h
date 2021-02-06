#ifndef HCAM_SESSION_H
#define HCAM_SESSION_H
#include "web/web.h"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <type_traits>

namespace hcam {

template <bool SSL, typename UNDERLYING_STREAM = std::conditional_t<
                        SSL, boost::beast::ssl_stream<boost::beast::tcp_stream>,
                        boost::beast::tcp_stream>>
class session : public std::enable_shared_from_this<session<SSL>> {
  web &service;

  // TODO 把base移进ws_stream之后，base_stream就应该clear
  std::unique_ptr<UNDERLYING_STREAM> base_stream;
  std::unique_ptr<boost::beast::websocket::stream<UNDERLYING_STREAM, true>>
      ws_stream;
  boost::asio::ip::tcp::endpoint remote;

  boost::beast::flat_buffer read_buffer;
  boost::beast::http::request<boost::beast::http::string_body> http_request;

private:
  // internal
  bool is_websocket() {
    if (base_stream)
      return false;
    if (ws_stream)
      return true;
    logger::fatal("web", "is_websocket: session invalid state!");
    abort();
  }

  auto get_executor() {
    if (is_websocket()) {
      return ws_stream->get_executor();
    } else {
      return base_stream->get_executor();
    }
  }

  // handshake

  void on_http_ready(boost::beast::error_code ec) {
    if (ec) {
      logger::debug("web", "on_http_ready failed");
      return;
    }
    http_read();
  }

  void on_ws_handshake() {}

  // http read
  void http_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    http_request = {};
    // Clear the buffer
    read_buffer.consume(read_buffer.size());
    // go
    boost::beast::http::async_read(
        *base_stream, read_buffer, http_request,
        [this, shared_this = this->shared_from_this()](
            boost::beast::error_code ec, std::size_t bytes_transferred) {
          on_http_read(ec, bytes_transferred);
        });
  }

  void on_http_read(boost::beast::error_code ec,
                    std::size_t bytes_transferred) {
    // This means they closed the connection
    if (ec) {
      if (ec != boost::beast::http::error::end_of_stream) {
        hcam::logger::debug("web", this->remote,
                            " HTTP read failed: ", ec.message());
      }
      return;
    }

    // handle request
    boost::beast::http::response<boost::beast::http::string_body> response;
    response.set(boost::beast::http::field::server, "hcam");
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(http_request.keep_alive());
    response.body() = std::string("hi");
    response.prepare_payload();
    http_write(std::move(response));
  }

  // http write

  template <bool isRequest, class Body, class Fields>
  void http_write(boost::beast::http::message<isRequest, Body, Fields> &&msg) {
    auto response =
        std::make_shared<boost::beast::http::message<isRequest, Body, Fields>>(
            std::move(msg));

    // Write the response
    boost::beast::http::async_write(
        *base_stream, *response,
        [this, shared_this = this->shared_from_this(),
         response](boost::beast::error_code ec, std::size_t bytes_transferred) {
          on_http_write(response->need_eof(), ec, bytes_transferred);
        });
  }

  void on_http_write(bool should_close, boost::beast::error_code ec,
                     std::size_t bytes_transferred) {
    if (ec) {
      hcam::logger::debug("web", remote, " HTTP write failed: ", ec.message());
      return;
    }

    if (!should_close) {
      http_read();
    }
  }

  // ws read
  void ws_read() {}
  void on_ws_read() {}

  // ws write
  void on_ws_write() {}

public:
  template <typename... ARGS>
  session(web &_service, boost::asio::ip::tcp::endpoint _remote, ARGS &&...args)
      : service(_service), remote(_remote) {
    base_stream.reset(new UNDERLYING_STREAM(std::forward<ARGS>(args)...));
    auto total = ++service.online;
    logger::debug("web", remote, " new connection online, total: ", total);
  }

  virtual ~session() {
    if (is_websocket()) {
      boost::beast::get_lowest_layer(*ws_stream).close();
    } else {
      boost::beast::get_lowest_layer(*base_stream).close();
    }
    auto remain = --service.online;
    logger::debug("web", remote, " connection closed, total: ", remain);
  }

  void run() {
    if constexpr (SSL) {
      //如果base stream是ssl stream，就去做ssl握手，然后调到on http read
      hcam::logger::debug("web", this->remote, " SSL handshake begin");
      base_stream->async_handshake(
          boost::asio::ssl::stream_base::server,
          [this, shared_this =
                     this->shared_from_this()](boost::beast::error_code ec) {
            if (ec) {
              hcam::logger::debug("web", this->remote, " SSL handshake error, ",
                                  ec.message());
            }
            on_http_ready(ec);
          });
    } else {
      //如果base stream就是tcp而已，那直接调到on http ready
      boost::asio::dispatch(get_executor(),
                            [this, shared_this = this->shared_from_this()]() {
                              on_http_ready(boost::beast::error_code());
                            });
    }
  }

  void ws_write() {}
};
} // namespace hcam

#endif // HOMECAM_SESSION_H