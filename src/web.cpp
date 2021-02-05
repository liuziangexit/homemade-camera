#include "web/web.h"
#include "assert.h"
#include "config/config.h"
#include "util/logger.h"
#include <functional>

namespace hcam {

void wrap(
    boost::beast::error_code ec, boost::asio::ip::tcp::socket socket,
    boost::beast::net::ip::tcp::acceptor &_acceptor,
    std::function<void(boost::beast::error_code, boost::asio::ip::tcp::socket)>
        on_accept) {
  on_accept(ec, std::move(socket));
  _acceptor.async_accept(
      boost::asio::make_strand(_acceptor.get_executor()),
      [&_acceptor, on_accept](boost::beast::error_code ec,
                              boost::asio::ip::tcp::socket socket) {
        wrap(ec, std::move(socket), _acceptor, on_accept);
      });
}

bool run_acceptor(boost::beast::net::ip::tcp::acceptor &_acceptor,
                  boost::asio::ip::tcp::endpoint endpoint,
                  std::function<void(boost::beast::error_code ec,
                                     boost::asio::ip::tcp::socket socket)>
                      on_accept) {
  // FIXME 里面有问题全部returnfalse
  boost::beast::error_code ec;

  // Open the acceptor
  _acceptor.open(endpoint.protocol(), ec);
  if (ec) {
    throw std::runtime_error("acceptor_.open");
  }

  // Allow address reuse
  _acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    throw std::runtime_error("acceptor_.open");
  }

  // Bind to the server address
  _acceptor.bind(endpoint, ec);
  if (ec) {
    throw std::runtime_error("acceptor_.bind");
  }

  // Start listening for connections
  _acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    throw std::runtime_error("acceptor_.listen");
  }
  hcam::logger::info("web", "listening at ", endpoint);
  _acceptor.async_accept(
      boost::asio::make_strand(_acceptor.get_executor()),
      [&_acceptor, on_accept](boost::beast::error_code ec,
                              boost::asio::ip::tcp::socket socket) {
        wrap(ec, std::move(socket), _acceptor, on_accept);
      });
  return true;
}

web::web()
    : state{STOPPED}, thread_count(config::get().web_thread_count),
      io_context(thread_count), //
      acceptor(io_context), ssl_port_acceptor(io_context) {}

web::~web() { stop(); }

void web::run() {
  if (!change_state(STOPPED, STARTING)) {
    logger::error("web", "web start change_state failed");
    return;
  }

  if (!run_acceptor(acceptor,
                    // endpoint
                    boost::asio::ip::tcp::endpoint{
                        boost::asio::ip::make_address(config::get().web_addr),
                        (unsigned short)config::get().port},
                    // on_accept
                    std::bind(&web::on_accept, this, std::placeholders::_1,
                              std::placeholders::_2))) {
    logger::error("web", "web start run_acceptor failed");
    change_state_certain(STARTING, STOPPED);
    return;
  }

  for (int i = 0; i < thread_count; i++) {
    io_threads.emplace_back([this] { io_context.run(); });
  }

  change_state_certain(STARTING, RUNNING);
}

void web::stop() {
  logger::debug("web", "web stopping");
  if (!change_state(RUNNING, CLOSING)) {
    logger::debug("web", "stop change_state failed");
    return;
  }
  io_context.stop();
  for (auto &t : io_threads) {
    if (t.joinable())
      t.join();
  }
  assert(io_context.stopped());
  change_state_certain(CLOSING, STOPPED);
  logger::debug("web", "web stopped");
}

bool web::change_state(state_t expect, state_t desired) {
  return state.compare_exchange_strong(expect, desired);
}

void web::change_state_certain(state_t expect, state_t desired) {
  if (!change_state(expect, desired)) {
    logger::fatal("change_state_certain(", expect, ", ", desired, ") failed");
    abort();
  }
}

void web::on_accept(boost::beast::error_code ec,
                    boost::asio::ip::tcp::socket socket) {
  logger::debug("web", "we got one!");
  socket.close();
}
} // namespace hcam
