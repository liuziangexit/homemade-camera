#ifndef __HOMECAM_ASIO_HTTP_SESSION_H_
#define __HOMECAM_ASIO_HTTP_SESSION_H_
#include "asio_base_session.h"
#include "boost/beast.hpp"
#include "config.h"
#include "logger.h"
#include <boost/asio/any_io_executor.hpp>
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

template <bool SSL>
class asio_http_session
    : public asio_base_session<SSL, typename stream<SSL>::type> {
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;

public:
  template <typename... ARGS>
  asio_http_session(ARGS &&...args)
      : asio_base_session<SSL, typename stream<SSL>::type>(
            std::forward<ARGS>(args)...) {}

  ~asio_http_session() {
    homemadecam::logger::info("asio_http_session destruct");
  }

  // Get on the correct executor
  // virtual void run() override {
  void run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.

    net::dispatch(
        this->stream_.get_executor(),
        [this, shared_this = this->shared_from_this()]() { this->on_run(); });
  }

  // Start the asynchronous operation
  void on_run() {
    // Set TCP timeout.
    // TODO load from configmanager
    beast::get_lowest_layer(this->stream_)
        .expires_after(std::chrono::seconds(30));

    if constexpr (SSL) {
      // Perform the SSL handshake
      this->stream_.async_handshake(
          ssl::stream_base::server,
          [this, shared_this = this->shared_from_this()](beast::error_code ec) {
            this->on_handshake(ec);
          });
    } else {
      this->on_handshake(beast::error_code());
    }
  }

  // previous layer handshake ok
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

    // post read req
    this->do_read();
  }

  void do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};
    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Set the timeout.
    beast::get_lowest_layer(this->stream_)
        .expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(this->stream_, buffer_, req_,
                     [this, shared_this = this->shared_from_this()](
                         beast::error_code ec, std::size_t bytes_transferred) {
                       this->on_read(ec, bytes_transferred);
                     });
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // This means they closed the connection
    if (ec == http::error::end_of_stream) {
      homemadecam::logger::info(this->remote_, " http closed");
      return;
    }

    if (ec) {
      homemadecam::logger::error(this->remote_,
                                 " http read failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote_, " http read OK");
    }

    // TODO ...
    // handle request
    http::response<http::string_body> res{http::status::ok, req_.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req_.keep_alive());
    res.body() = "naive!";
    res.prepare_payload();
    send(std::move(res));
  }

  template <bool isRequest, class Body, class Fields>
  void send(http::message<isRequest, Body, Fields> &&msg) {
    http::message<isRequest, Body, Fields> *response =
        new http::message<isRequest, Body, Fields>(std::move(msg));

    try {
      // Write the response
      http::async_write(
          this->stream_, *response,
          [this, shared_this = this->shared_from_this(), deleter([response] {
             delete response;
           })](beast::error_code ec, std::size_t bytes_transferred) {
            this->on_write(deleter, ec, bytes_transferred);
          });
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
      homemadecam::logger::info(this->remote_,
                                " http write failed: ", ec.message());
      return;
    } else {
      homemadecam::logger::info(this->remote_, " http write OK");
    }

    // Do another read
    do_read();
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
