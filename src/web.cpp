#include "web/web.h"
#include "assert.h"
#include "config/config.h"
#include "ipc/ipc.h"
#include "util/logger.h"
#include "web/session.h"
#include <functional>
#include <signal.h>

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
    logger::fatal("on_accept throws an exception! ", ex.what());
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
  hcam::logger::info("listening at ", endpoint);
  _acceptor.async_accept(
      boost::asio::make_strand(_acceptor.get_executor()),
      [&_acceptor, on_accept](boost::beast::error_code ec,
                              boost::asio::ip::tcp::socket socket) {
        wrap(ec, std::move(socket), _acceptor, on_accept);
      });
  return true;
}

web::web(int cap_web_fd)
    : state{STOPPED}, thread_count(config::get().web_thread_count),
      io_context(thread_count), //
      acceptor(boost::asio::make_strand(io_context)),
      ssl_acceptor(boost::asio::make_strand(io_context)),
      cap_web_fd(cap_web_fd) {
  if (config::get().ssl_enabled) {
    load_server_certificate(ssl_ctx);
  }
}

web::~web() { stop(); }

void web::run(int ipc_fd) {
  if (!change_state(STOPPED, STARTING)) {
    logger::error("web start change_state failed");
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
    logger::error("web start run_acceptor failed");
    change_state_certain(STARTING, STOPPED);
    return;
  }

  if (config::get().ssl_enabled) {
    if (!run_acceptor(ssl_acceptor,
                      // endpoint
                      boost::asio::ip::tcp::endpoint{
                          boost::asio::ip::make_address(config::get().web_addr),
                          (unsigned short)config::get().ssl_port},
                      // on_accept
                      std::bind(&web::on_accept, this, true,
                                std::placeholders::_1,
                                std::placeholders::_2))) {
      logger::error("web start run_acceptor failed");
      change_state_certain(STARTING, STOPPED);
      return;
    }
  }

  for (int i = 0; i < thread_count; i++) {
    io_threads.emplace_back([this] { io_context.run(); });
  }

  change_state_certain(STARTING, RUNNING);

  //处理ipc
  if (ipc::send(cap_web_fd, "READY")) {
    logger::error("unable to send first READY message");
    return;
  }
  while (true) {
    int ret;
    fd_set fds;

    int max_fd = ipc_fd;
    if (cap_web_fd > ipc_fd) {
      max_fd = cap_web_fd;
    }

  SELECT:
    FD_ZERO(&fds);
    FD_SET(ipc_fd, &fds);
    FD_SET(cap_web_fd, &fds);
    ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
    if (ret == -1) {
      if (EINTR == errno)
        goto SELECT;
      logger::info("select failed, quitting...");
      return;
    }

    if (FD_ISSET(ipc_fd, &fds)) {
      auto msg = ipc::recv(ipc_fd);
      if (msg.first) {
        logger::error("ipc handler read ipc_fd failed, quitting...", msg.first);
        return;
      }
      std::string text((char *)msg.second.content, msg.second.size);
      if (text == "PING") {
        //心跳
        if (ipc::send(ipc_fd, "PONG")) {
          // something goes wrong
          exit(SIGABRT);
        }
      } else if (text == "EXIT") {
        logger::debug("IPC EXIT, quitting...");
        return;
      }
    } else if (FD_ISSET(cap_web_fd, &fds)) {
      auto msg = ipc::recv(cap_web_fd);
      if (msg.first) {
        logger::error("ipc handler read cap_web_fd failed, quitting...",
                      msg.first);
        return;
      }
      this->foreach_session([msg](const web::session_context &_session) {
        auto shared_session = _session.session.lock();
        if (shared_session) {
          // FIXME 这拷贝是O(n)的，害怕。。其实用引用计数可以消掉
          std::vector<unsigned char> copy(msg.second.size, ' ');
          memcpy(copy.data(), msg.second.content, msg.second.size);
          if (_session.ssl) {
            static_cast<session<true> *>(shared_session.get())
                ->ws_write(std::move(copy), true, 1);
          } else {
            static_cast<session<false> *>(shared_session.get())
                ->ws_write(std::move(copy), true, 1);
          }
        }
      });
      if (ipc::send(cap_web_fd, "READY")) {
        return;
      }
    }
  }
  /*STOP:*/
  /*this->stop();*/
}

void web::stop() {
  logger::info("web stopping");
  if (!change_state(RUNNING, CLOSING)) {
    logger::debug("stop change_state failed");
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
  logger::info("web stopped");
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
    logger::error( //
        "accept new connection failed, ", ec.message());
    return false;
  }

  boost::asio::ip::tcp::endpoint endpoint;
  try {
    endpoint = socket.remote_endpoint();
  } catch (const std::exception &e) {
    logger::debug( //
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
