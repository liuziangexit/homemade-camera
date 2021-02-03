#ifndef __HOMECAM_ASIO_HTTP_SESSION_H_
#define __HOMECAM_ASIO_HTTP_SESSION_H_
#include "asio_base_session.h"
#include "asio_ws_session.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "file_loader.h"
#include "util/logger.h"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace hcam {

template <bool SSL>
class asio_http_session
    : public asio_base_session<SSL, typename stream<SSL>::type> {
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;

public:
  using base_type = asio_base_session<SSL, typename stream<SSL>::type>;
  using ssl_enabled = std::bool_constant<SSL>;
  template <typename... ARGS>
  asio_http_session(ARGS &&...args)
      : asio_base_session<SSL, typename stream<SSL>::type>(
            std::forward<ARGS>(args)...) {}

  ~asio_http_session() {
    hcam::logger::debug("web", this->remote_, " asio_http_session destructed");
  }

  // Get on the correct executor
  // virtual void run() override {
  virtual void run() override {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.

    net::dispatch(
        this->stream_->get_executor(),
        [this, shared_this = this->shared_from_this()]() { this->on_run(); });
  }

  // Start the asynchronous operation
  void on_run() {
    this->ssl_handshake([this](beast::error_code e) { on_handshake(e); });
  }

  // previous layer handshake ok
  void on_handshake(beast::error_code ec) {
    if (ec) {
      this->close();
      return;
    }

    // post read req
    this->read();
  }

  void read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};
    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Read a request
    http::async_read(*this->stream_, buffer_, req_,
                     [this, shared_this = this->shared_from_this()](
                         beast::error_code ec, std::size_t bytes_transferred) {
                       this->on_read(ec, bytes_transferred);
                     });
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // This means they closed the connection
    if (ec == http::error::end_of_stream) {
      this->close();
      return;
    }

    if (ec) {
      hcam::logger::debug("web", this->remote_,
                          " HTTP read failed: ", ec.message());
      this->close();
      return;
    } else {
      hcam::logger::debug("web", this->remote_, " HTTP read OK");
    }

    // handle request

    // handle websocket upgrade request
    auto upgrade_header = req_.base().find("Upgrade");
    if (upgrade_header != req_.base().end() &&
        upgrade_header->value() == "websocket") {
      logger::debug("web", this->remote_, " client request WebSocket upgrade!");

      auto upgrade = [this](void *current) -> std::shared_ptr<void> {
        assert((void *)this == current);

        // FIXME 底下如果抛了个异常，就会漏内存
        auto typed_ws_session = new asio_ws_session<SSL>(
            this->remote_, std::move(this->unregister_),
            std::move(this->modify_session_), this->ssl_established_);
        std::shared_ptr<void> ws_session(typed_ws_session);
        if constexpr (SSL) {
          typed_ws_session->stream_ = std::make_unique<
              websocket::stream<typename stream<true>::type, true>>(
              std::move(*this->stream_));
        } else {
          typed_ws_session->stream_ = std::make_unique<
              websocket::stream<typename stream<false>::type, true>>(
              beast::get_lowest_layer(*this->stream_).release_socket());
        }
        typed_ws_session->handshake_req_ = req_;
        typed_ws_session->livestream_ = this->livestream_;
        typed_ws_session->run();
        this->moved = true;
        return ws_session;
      };

      if (this->modify_session_(this->remote_, upgrade)) {
        logger::info("web", this->remote_, " WebSocket upgrade");
        return;
      } else {
        logger::fatal(
            "web", this->remote_,
            "modify_session returns false, that indicates the session can "
            "not find itself from the session pool! what?");
        abort();
      }
    }

    handle_request(beast::string_view(config_manager::get().web_root),
                   std::move(req_), [this](auto &&response) {
                     response.set(http::field::server, "homemade-camera");
                     this->write(std::move(response));
                   });

    read();
  }

  template <bool isRequest, class Body, class Fields>
  void write(http::message<isRequest, Body, Fields> &&msg) {
    http::message<isRequest, Body, Fields> *response =
        new http::message<isRequest, Body, Fields>(std::move(msg));

    auto deleter = [response] { delete response; };
    auto shared_this = this->shared_from_this();

    try {
      // Write the response
      http::async_write(
          *this->stream_, *response,
          [this, shared_this, response,
           deleter](beast::error_code ec, std::size_t bytes_transferred) {
            this->on_write(deleter, response->need_eof(), ec,
                           bytes_transferred);
          });
    } catch (const std::exception &ex) {
      delete response;
      throw ex;
    }
  }

  void on_write(const std::function<void()> &deleter, bool should_close,
                beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    //删除response对象
    deleter();

    if (ec) {
      hcam::logger::debug("web", this->remote_,
                          " HTTP write failed: ", ec.message());
      this->close();
      return;
    } else {
      hcam::logger::debug("web", this->remote_, " HTTP write OK");
    }

    //也许是因为"Connection: close"或者什么其他原因
    //总之，需要我们关掉
    if (should_close) {
      this->close();
      return;
    }
  }

  virtual void close() override {
    std::lock_guard l(this->close_mutex);
    if (this->closed)
      return;

    logger::debug("web", this->remote_, " unreferenced");
    asio_base_session<SSL, typename stream<SSL>::type>::close();

    this->closed = true;
  }
};

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
