#include "web/web.h"
#include "assert.h"
#include "config/config.h"
#include "util/logger.h"
#include "web/session.h"
#include <functional>

namespace hcam {

void wrap(
    boost::beast::error_code ec, boost::asio::ip::tcp::socket socket,
    boost::beast::net::ip::tcp::acceptor &_acceptor,
    std::function<bool(boost::beast::error_code, boost::asio::ip::tcp::socket)>
        on_accept) {
  try {
    if (!on_accept(ec, std::move(socket)))
      return;
  } catch (const std::exception &ex) {
    logger::fatal("web", "on_accept throws an exception! ", ex.what());
    abort();
  }
  _acceptor.async_accept(
      boost::asio::make_strand(_acceptor.get_executor()),
      [&_acceptor, on_accept](boost::beast::error_code ec,
                              boost::asio::ip::tcp::socket socket) {
        wrap(ec, std::move(socket), _acceptor, on_accept);
      });
}

bool run_acceptor(
    boost::beast::net::ip::tcp::acceptor &_acceptor,
    boost::asio::ip::tcp::endpoint endpoint,
    std::function<bool(boost::beast::error_code, boost::asio::ip::tcp::socket)>
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
      acceptor(boost::asio::make_strand(io_context)),
      ssl_acceptor(boost::asio::make_strand(io_context)) {
  load_server_certificate(ssl_ctx);
}

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
                    std::bind(&web::on_accept, this, false,
                              std::placeholders::_1, std::placeholders::_2))) {
    logger::error("web", "web start run_acceptor failed");
    change_state_certain(STARTING, STOPPED);
    return;
  }

  if (!run_acceptor(ssl_acceptor,
                    // endpoint
                    boost::asio::ip::tcp::endpoint{
                        boost::asio::ip::make_address(config::get().web_addr),
                        (unsigned short)config::get().ssl_port},
                    // on_accept
                    std::bind(&web::on_accept, this, true,
                              std::placeholders::_1, std::placeholders::_2))) {
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
  logger::info("web", "web stopping");
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
  //由于ioctx里未完成的异步任务里面引用了session的shared_ptr，导致这些session没有destruct，我们这样做
  //清空任务队列的唯一方法是destruct，但是他的dtor调多次会出问题，所以就调完再构建一个
  io_context.~io_context();
  new (&io_context) boost::asio::io_context(config::get().web_thread_count);
  assert(online == 0);
  assert(subscribed.empty());
  change_state_certain(CLOSING, STOPPED);
  logger::info("web", "web stopped");
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

bool web::on_accept(bool ssl, boost::beast::error_code ec,
                    boost::asio::ip::tcp::socket socket) {
  if (ec) {
    logger::error("web", //
                  "accept new connection failed, ", ec.message());
    return false;
  }

  boost::asio::ip::tcp::endpoint endpoint;
  try {
    endpoint = socket.remote_endpoint();
  } catch (const std::exception &e) {
    logger::debug("web", //
                  "accept new connection failed, ", e.what());
    return true;
  }

  if (ssl) {
    auto new_session = std::make_shared<session<true>>(
        *this, endpoint, std::move(socket), ssl_ctx);
    new_session->run();
  } else {
    auto new_session =
        std::make_shared<session<false>>(*this, endpoint, std::move(socket));
    new_session->run();
  }

  return true;
}

//这函数只能在一个线程调用!
void web::foreach_session(std::function<void(const session_context &)> viewer) {
  std::unique_lock<std::mutex> l(subscribed_mut);
  for (const auto &p : subscribed) {
    viewer(p.second);
  }
}

} // namespace hcam
