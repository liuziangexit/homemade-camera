#ifndef __HOMECAM_ASIO_BASE_SESSION_H_
#define __HOMECAM_ASIO_BASE_SESSION_H_
#include "boost/beast.hpp"
#include "config.h"
#include "logger.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace homemadecam {

template <bool SSL> struct stream {
  using type = beast::ssl_stream<beast::tcp_stream>;
};
template <> struct stream<false> { using type = beast::tcp_stream; };

template <bool SSL, typename STREAM>
class asio_base_session
    : public std::enable_shared_from_this<asio_base_session<SSL, STREAM>> {
protected:
  STREAM stream_;

  const beast::tcp_stream::endpoint_type remote_;

public:
  // ssl
  asio_base_session(tcp::socket &&socket, ssl::context &ssl_ctx)
      : stream_(std::move(socket), ssl_ctx),
        remote_(beast::get_lowest_layer(stream_).socket().remote_endpoint()) {}

  // tcp
  explicit asio_base_session(tcp::socket &&socket)
      : stream_(std::move(socket)),
        remote_(beast::get_lowest_layer(stream_).socket().remote_endpoint()) {}

  asio_base_session(const asio_base_session &) = delete;

  virtual ~asio_base_session() {
    homemadecam::logger::info("asio_base_session destruct");
  }

  /// virtual void run();
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
