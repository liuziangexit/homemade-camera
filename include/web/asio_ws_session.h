#ifndef __HOMECAM_ASIO_WS_SESSION_H_
#define __HOMECAM_ASIO_WS_SESSION_H_
#include "asio_base_session.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "livestream.h"
#include "util/logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <condition_variable>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace hcam {

template <bool SSL> class asio_http_session;

template <bool SSL>
class asio_ws_session
    : public asio_base_session<
          SSL, websocket::stream<typename stream<SSL>::type, true>> {
  friend class hcam::asio_http_session<SSL>;
  beast::flat_buffer buffer_;
  std::optional<http::request<http::string_body>> handshake_req_;

  std::atomic<bool> is_writing = false;
  std::condition_variable write_cv;
  std::mutex write_mut;

public:
  using base_type =
      asio_base_session<SSL,
                        websocket::stream<typename stream<SSL>::type, true>>;
  using ssl_enabled = std::bool_constant<SSL>;
  template <typename... ARGS>
  asio_ws_session(ARGS &&...args)
      : asio_base_session<SSL,
                          websocket::stream<typename stream<SSL>::type, true>>(
            std::forward<ARGS>(args)...) {}

  ~asio_ws_session() {
    hcam::logger::debug("web", this->remote_, " asio_ws_session destructed");
  }

  // Get on the correct executor
  virtual void run() override {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(
        this->stream_->get_executor(),
        [this, shared_this = this->shared_from_this()] { this->on_run(); });
  }

  // Start the asynchronous operation
  void on_run() {
    this->ssl_handshake([this](beast::error_code e) { on_handshake(e); });
  }

  // Websocket handshake
  void on_handshake(beast::error_code ec) {
    if (ec) {
      this->close();
      return;
    }

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(*this->stream_).expires_never();

    // Set suggested timeout settings for the websocket
    websocket::stream_base::timeout timeout =
        websocket::stream_base::timeout::suggested(beast::role_type::server);
    timeout.handshake_timeout = std::chrono::seconds(10);
    timeout.keep_alive_pings = true;
    timeout.idle_timeout =
        std::chrono::seconds(config_manager::get().idle_timeout);
    this->stream_->set_option(timeout);

    // Set a decorator to change the Server of the handshake
    this->stream_->set_option(
        websocket::stream_base::decorator([](websocket::response_type &res) {
          res.set(http::field::server, "homemade-camera");
        }));

    // Accept the websocket handshake
    logger::debug("web", this->remote_, " WebSocket handshake begin");
    if (handshake_req_) {
      /*这个session是从http session转来的，使用此前收到的handshake
      request去做握手*/
      this->stream_->async_accept(
          *this->handshake_req_,
          [this, shared_this = this->shared_from_this()](beast::error_code ec) {
            if (ec) {
              hcam::logger::debug("web", this->remote_,
                                  " ws handshake error: ", ec.message());
            } else {
              hcam::logger::debug("web", this->remote_,
                                  " WebSocket handshake OK");
            }
            this->on_accept(ec);
          });
    } else {
      this->stream_->async_accept(
          [this, shared_this = this->shared_from_this()](beast::error_code ec) {
            if (ec) {
              hcam::logger::debug("web", this->remote_,
                                  " ws handshake error: ", ec.message());
            } else {
              hcam::logger::debug("web", this->remote_,
                                  " WebSocket handshake OK");
            }
            this->on_accept(ec);
          });
    }
  }

  void on_accept(beast::error_code ec) {
    if (ec) {
      this->close();
      return;
    }

    // Read a message
    read();
  }

  void read() {
    // Read a message into our buffer
    buffer_.consume(buffer_.size());
    this->stream_->async_read(
        buffer_, [this, shared_this = this->shared_from_this()](
                     beast::error_code ec, std::size_t bytes_transferred) {
          this->on_read(ec, bytes_transferred);
        });
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed) {
      this->close();
      return;
    }

    if (ec) {
      hcam::logger::debug("web", this->remote_,
                          " Websocket read failed: ", ec.message());
      this->close();
      return;
    } else {
      hcam::logger::debug("web", this->remote_, " WebSocket read OK");
    }

    auto reply = [this](const char *msg) {
      this->write(msg, strlen(msg), false, false, true);
    };

    // handle
    if (!this->stream_->got_text()) {
      reply("can not handle binary message");
      this->close();
      return;
    }

    std::string txt = beast::buffers_to_string(buffer_.cdata());
    if (txt == "STREAM_ON") {
      if (!this->livestream_->add(this->remote_, this->shared_from_this())) {
        reply("livestream add failed");
        this->close();
      } else {
        reply("ok");
      }
    } else if (txt == "STREAM_OFF") {
      if (!this->livestream_->remove(this->remote_)) {
        reply("livestream remove failed");
        this->close();
      } else {
        reply("ok");
      }
    } else {
      reply("unknown request");
      this->close();
      return;
    }

    read();
  }

  template <typename BUFFER,
            typename = typename std::enable_if<
                !std::is_same_v<std::decay_t<BUFFER>, std::shared_ptr<void>>,
                int>::type>
  bool write(BUFFER &&buf, bool binary, bool wait_on_busy = false) {
    std::unique_lock l(write_mut);
    write_cv.wait(
        l, [this, wait_on_busy] { return !wait_on_busy || !is_writing; });
    if (is_writing)
      return false;
    is_writing = true;
    l.unlock();

    this->stream_->binary(binary);
    this->stream_->async_write(
        buf, [this, shared_this = this->shared_from_this()](
                 beast::error_code ec, std::size_t bytes_transferred) {
          std::unique_lock l(write_mut);
          assert(is_writing);
          is_writing = false;
          write_cv.notify_one();
          l.unlock();
          this->on_write(ec, bytes_transferred);
        });

    return true;
  }

  bool write(const char *src, uint32_t len, bool binary, bool copy = true,
             bool wait_on_busy = false) {
    return write((const unsigned char *)src, len, binary, copy, wait_on_busy);
  }

  bool write(const unsigned char *src, uint32_t len, bool binary,
             bool copy = true, bool wait_on_busy = false) {
    std::unique_lock l(write_mut);
    write_cv.wait(
        l, [this, wait_on_busy] { return !wait_on_busy || !is_writing; });
    if (is_writing)
      return false;
    is_writing = true;
    l.unlock();

    this->stream_->binary(binary);
    if (copy) {
      std::shared_ptr<void> shared_buf(new unsigned char[len], [](void *p) {
        // FIXME 确认一下delete是编译时候根据类型去做的，还是运行时的信息
        delete static_cast<unsigned char *>(p);
      });
      memcpy(shared_buf.get(), src, len);
      this->stream_->async_write(
          net::buffer(shared_buf.get(), len),
          [this, shared_this = this->shared_from_this()](
              beast::error_code ec, std::size_t bytes_transferred) {
            std::unique_lock l(write_mut);
            assert(is_writing);
            is_writing = false;
            write_cv.notify_one();
            l.unlock();
            this->on_write(ec, bytes_transferred);
          });
    } else {
      this->stream_->async_write(
          net::buffer(src, len),
          [this, shared_this = this->shared_from_this()](
              beast::error_code ec, std::size_t bytes_transferred) {
            std::unique_lock l(write_mut);
            assert(is_writing);
            is_writing = false;
            write_cv.notify_one();
            l.unlock();
            this->on_write(ec, bytes_transferred);
          });
    }
    return true;
  }

  //认为src是unsigned char*
  bool write(std::shared_ptr<void> src, uint32_t len, bool binary,
             bool wait_on_busy = false) {
    std::unique_lock l(write_mut);
    write_cv.wait(
        l, [this, wait_on_busy] { return !wait_on_busy || !is_writing; });
    if (is_writing)
      return false;
    is_writing = true;
    l.unlock();

    this->stream_->binary(binary);
    this->stream_->async_write(
        net::buffer((const unsigned char *)src.get(), len),
        [this, src, shared_this = this->shared_from_this()](
            beast::error_code ec, std::size_t bytes_transferred) {
          std::unique_lock l(write_mut);
          assert(is_writing);
          is_writing = false;
          write_cv.notify_one();
          l.unlock();
          this->on_write(ec, bytes_transferred);
        });
    return true;
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    if (ec) {
      hcam::logger::debug("web", this->remote_,
                          " Websocket write failed: ", ec.message());
      this->close();
    } else {
      hcam::logger::debug("web", this->remote_, " WebSocket write OK");
    }
  }

  virtual void close() override {
    std::lock_guard l(this->close_mutex);
    if (this->closed)
      return;

    this->livestream_->remove(this->remote_);

    // close websocket
    try {
      this->stream_->close(boost::beast::websocket::normal);
    } catch (const beast::system_error &e) {
      logger::debug("web", this->remote_,
                    " close WebSocket failed: ", e.what());
    }
    logger::debug("web", this->remote_, " WebSocket closed");
    // call base class
    asio_base_session<
        SSL, websocket::stream<typename stream<SSL>::type, true>>::close();

    this->closed = true;
  }

public:
  static void send_frame(void *session, unsigned char *frame, uint32_t len) {
    auto *this_ = (asio_ws_session<SSL> *)session;

    /*//发消息头
    static const unsigned char head[] = "FRAME";
    this_->write(head, strlen((const char *)head), false, false);*/
    //发帧
    std::shared_ptr<void> frame_copy(new unsigned char[len], [](void *p) {
      delete static_cast<unsigned char *>(p);
    });
    memcpy(frame_copy.get(), frame, len);
    this_->write(frame_copy, len, true);
  }
};

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
