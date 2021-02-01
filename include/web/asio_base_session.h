#ifndef __HOMECAM_ASIO_BASE_SESSION_H_
#define __HOMECAM_ASIO_BASE_SESSION_H_
#include "boost/beast.hpp"
#include "config/config.h"
#include "find_layer.h"
#include "util/logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>
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

template <bool SSL> struct stream {
  using type = beast::ssl_stream<beast::tcp_stream>;
};
template <> struct stream<false> { using type = beast::tcp_stream; };

template <bool SSL, typename STREAM>
class asio_base_session
    : public std::enable_shared_from_this<asio_base_session<SSL, STREAM>> {
protected:
  //注意，这个stream和remote的声明顺序是有讲究的，看构造函数那里...我们必须先初始化stream再初始化remote，而初始化顺序就是声明顺序
  std::unique_ptr<STREAM> stream_;
  const beast::tcp_stream::endpoint_type remote_;
  using modify_session_callback_type =
      std::function<std::shared_ptr<void>(void *)>;
  std::function<bool(tcp::endpoint, modify_session_callback_type)>
      modify_session_;
  //我们会在web service里保留每个session的一个引用计数
  // session自杀的时候，需要把自己在service里的引用消掉
  std::function<bool()> unregister_;
  bool ssl_handshaked_ = false;
  //表示此对象已被move走
  bool empty = false;

private:
  void set_tcp_timeout() {
    // TODO load from configmanager
    beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));
  }

public:
  // ssl
  asio_base_session(
      tcp::socket &&socket, ssl::context &ssl_ctx,
      std::function<bool()> unregister,
      std::function<bool(tcp::endpoint, modify_session_callback_type)>
          modify_session)
      : stream_(std::make_unique<STREAM>(std::move(socket), ssl_ctx)),
        remote_(beast::get_lowest_layer(*stream_).socket().remote_endpoint()),
        unregister_(std::move(unregister)),
        modify_session_(std::move(modify_session)) {
    set_tcp_timeout();
  }

  // tcp
  asio_base_session(
      tcp::socket &&socket, std::function<bool()> unregister,
      std::function<bool(tcp::endpoint, modify_session_callback_type)>
          modify_session)
      : stream_(std::make_unique<STREAM>(std::move(socket))),
        remote_(beast::get_lowest_layer(*stream_).socket().remote_endpoint()),
        unregister_(std::move(unregister)),
        modify_session_(std::move(modify_session)) {
    set_tcp_timeout();
  }

  asio_base_session(const asio_base_session &) = delete;

  virtual ~asio_base_session() {
    if (!empty) {
      if (this->unregister_()) {
        logger::fatal("asio_base_session destruct failed");
        abort();
      }
    }
    hcam::logger::debug(this->remote_, " asio_base_session destructed");
  }

  virtual void run() { throw std::exception(); }

  virtual void close() {
    if (!this->unregister_()) {
      /*      hcam::logger::debug(
                this->remote_,
                " asio_base_session close: \"this\" has been closed before");*/
    }
    beast::get_lowest_layer(*stream_).close();
  }

protected:
  asio_base_session(
      beast::tcp_stream::endpoint_type remote, std::function<bool()> unregister,
      std::function<bool(tcp::endpoint, modify_session_callback_type)>
          modify_session,
      bool ssl_handshaked)
      : remote_(remote), unregister_(std::move(unregister)),
        modify_session_(std::move(modify_session)),
        ssl_handshaked_(ssl_handshaked) {}

  template <typename CALLBACK> void ssl_handshake(CALLBACK callback) {
    if constexpr (SSL) {
      if (!ssl_handshaked_) {
        hcam::logger::debug(this->remote_, " SSL handshake begin");
        find_layer<stream<true>::type>(*this->stream_.get())
            .async_handshake(
                ssl::stream_base::server,
                [this, callback,
                 shared_this = this->shared_from_this()](beast::error_code ec) {
                  if (ec) {
                    hcam::logger::debug(this->remote_,
                                        " SSL handshake error: ", ec.message());
                  } else {
                    ssl_handshaked_ = true;
                    hcam::logger::debug(this->remote_, " SSL handshake OK");
                  }
                  callback(ec);
                });
      }
    } else {
      auto callback_copy = callback;
      callback_copy(beast::error_code());
    }
  }
};

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
