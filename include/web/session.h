#ifndef HCAM_SESSION_H
#define HCAM_SESSION_H
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
  // TODO 把base移进ws_stream之后，base_stream就应该destruct
  std::unique_ptr<UNDERLYING_STREAM> base_stream;
  std::unique_ptr<boost::beast::websocket::stream<UNDERLYING_STREAM, true>>
      ws_stream;
  boost::asio::ip::tcp::endpoint remote;

private:
  template <typename... ARGS> void on_tcp_handshake(ARGS &&...args) {
    base_stream.reset(new UNDERLYING_STREAM(std::forward<ARGS>(args)...));

    if constexpr (SSL) {
      hcam::logger::debug("web", this->remote, " SSL handshake begin");
      base_stream->async_handshake(
          boost::asio::ssl::stream_base::server,
          [this, shared_this =
                     this->shared_from_this()](boost::beast::error_code ec) {
            if (ec) {
              hcam::logger::debug("web", this->remote, " SSL handshake error, ",
                                  ec.message());
            } else {
              hcam::logger::debug("web", this->remote_, " SSL handshake OK");
            }
            on_http_read(ec);
          });
    } else {
    }

    boost::asio::dispatch(base_stream->get_executor(),
                          [this, shared_this = this->shared_from_this()]() {});
  }
  void on_http_ready(boost::beast::error_code ec) {}
  void on_ws_handshake() {}

protected:
  void http_write() {}
  void on_http_read(boost::beast::error_code ec,
                    std::size_t bytes_transferred) {}
  void on_http_write() {}

  void on_ws_read() {}
  void on_ws_write() {}

public:
  template <typename... ARGS>
  session(boost::asio::ip::tcp::endpoint _remote, ARGS &&...args)
      : remote(_remote) {
    on_tcp_handshake(std::forward<ARGS>(args)...);
  }
  virtual ~session() {}

  void ws_write() {}
};
} // namespace hcam

#endif // HOMECAM_SESSION_H
