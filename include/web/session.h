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
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

//这里本来想做成纯架构的代码，然后业务相关的代码通过继承这个类来做
//但是感觉没有这个必要，就把业务代码都写着里面了

#include "http_file_handler.h"

namespace hcam {

template <bool SSL>
class session : public std::enable_shared_from_this<session<SSL>> {
  using UNDERLYING_STREAM =
      std::conditional_t<SSL,
                         boost::beast::ssl_stream<boost::beast::tcp_stream>,
                         boost::beast::tcp_stream>;
  using WS_STREAM = boost::beast::websocket::stream<UNDERLYING_STREAM, true>;

  web &service;

  std::unique_ptr<UNDERLYING_STREAM> base_stream;
  std::unique_ptr<WS_STREAM> ws_stream;
  boost::asio::ip::tcp::endpoint remote;

  boost::beast::flat_buffer read_buffer;
  boost::beast::http::request<boost::beast::http::string_body> http_request;

  std::mutex ws_send_queue_mut;
  std::queue<std::pair<std::vector<unsigned char>, bool>> ws_send_queue;

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

  void on_ws_handshake(boost::beast::error_code ec) {
    if (ec) {
      logger::debug("web", "on_ws_handshake failed");
      return;
    }
    ws_read();
  }

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

    //处理ws升级
    if (boost::beast::websocket::is_upgrade(http_request)) {
      ws_stream.reset(new WS_STREAM(std::move(*base_stream)));
      base_stream.reset(nullptr);

      // Turn off the timeout on the tcp_stream, because
      // the websocket stream has its own timeout system.
      boost::beast::get_lowest_layer(*ws_stream).expires_never();

      // Set suggested timeout settings for the websocket
      boost::beast::websocket::stream_base::timeout timeout =
          boost::beast::websocket::stream_base::timeout::suggested(
              boost::beast::role_type::server);
      timeout.handshake_timeout = std::chrono::seconds(10);
      timeout.keep_alive_pings = true;
      timeout.idle_timeout = std::chrono::seconds(config::get().idle_timeout);
      ws_stream->set_option(timeout);

      // Set a decorator to change the Server of the handshake
      ws_stream->set_option(boost::beast::websocket::stream_base::decorator(
          [](boost::beast::websocket::response_type &res) {
            res.set(boost::beast::http::field::server, "homemade-camera");
          }));

      // Accept the websocket handshake
      ws_stream->async_accept(
          http_request,
          [this, shared_this = this->shared_from_this()](
              boost::beast::error_code ec) { on_ws_handshake(ec); });
      return;
    }

    // handle request
    handle_request(boost::beast::string_view(config::get().web_root),
                   std::move(http_request), [this](auto &&response) {
                     response.set(boost::beast::http::field::server,
                                  "homemade-camera");
                     this->http_write(std::move(response));
                   });
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
  void ws_read() {
    // Read a message into our buffer
    read_buffer.consume(read_buffer.size());
    ws_stream->async_read(
        read_buffer,
        [this, shared_this = this->shared_from_this()](
            boost::beast::error_code ec, std::size_t bytes_transferred) {
          on_ws_read(ec, bytes_transferred);
        });
  }

  void on_ws_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
      return;
    }
    auto reply = [this](const char *msg) {
      auto len = strlen(msg);
      std::vector<unsigned char> buffer(len, (unsigned char)0);
      memcpy(buffer.data(), msg, len);
      ws_write(std::move(buffer), false);
    };

    if (!ws_stream->got_text()) {
      reply("can not handle binary message");
      return;
    }

    std::string text = boost::beast::buffers_to_string(read_buffer.cdata());
    if (text == "STREAM_ON") {
      std::shared_lock l(service.subscribed_mut);
      if (!service.subscribed.insert(web::session_map_t::value_type{
              remote, web::session_context{SSL, this}})) {
        logger::error("web", "tbb concurrent map insert failed");
        reply("internal error");
        return;
      }
      l.unlock();
      reply("ok");
    } else if (text == "STREAM_OFF") {
      std::shared_lock l(service.subscribed_mut);
      web::session_map_t::accessor row;
      if (!service.subscribed.find(row, remote)) {
        logger::error("web", "tbb concurrent map erase failed");
        reply("internal error");
        return;
      }
      service.subscribed.erase(row);
      l.unlock();
      reply("ok");
    } else {
      reply("unknown request");
      return;
    }

    ws_read();
  }

  // ws write
  void on_ws_write(boost::beast::error_code ec, std::size_t bytes_transferred) {
    std::lock_guard l(ws_send_queue_mut);
    ws_send_queue.pop();
    if (ws_send_queue.empty())
      return;

    ws_stream->binary(ws_send_queue.front().second);
    ws_stream->async_write(
        boost::asio::buffer(ws_send_queue.front().first.data(),
                            ws_send_queue.front().first.size()),
        [this, shared_this = this->shared_from_this()](
            boost::beast::error_code ec, std::size_t bytes_transferred) {
          on_ws_write(ec, bytes_transferred);
        });
  }

public:
  template <typename... ARGS>
  session(web &_service, boost::asio::ip::tcp::endpoint _remote, ARGS &&...args)
      : service(_service), remote(_remote) {
    base_stream.reset(new UNDERLYING_STREAM(std::forward<ARGS>(args)...));
    auto total = ++service.online;
    logger::debug("web", remote, " new connection online, total: ", total);
  }

  virtual ~session() {
    web::session_map_t::accessor row;
    std::shared_lock l(service.subscribed_mut);
    if (service.subscribed.find(row, remote)) {
      service.subscribed.erase(row);
    }
    l.unlock();
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

  void ws_write(std::vector<unsigned char> &&msg, bool binary) {
    std::lock_guard l(ws_send_queue_mut);
    ws_send_queue.push(
        std::pair<std::vector<unsigned char>, bool>(std::move(msg), binary));
    if (ws_send_queue.size() == 1) {
      ws_stream->binary(binary);
      ws_stream->async_write(
          boost::asio::buffer(ws_send_queue.front().first.data(),
                              ws_send_queue.front().first.size()),
          [this, shared_this = this->shared_from_this()](
              boost::beast::error_code ec, std::size_t bytes_transferred) {
            on_ws_write(ec, bytes_transferred);
          });
    }
  }
};
} // namespace hcam

#endif // HOMECAM_SESSION_H
