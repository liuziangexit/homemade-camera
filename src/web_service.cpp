#include "web/web_service.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "config/config_manager.h"
#include "web/ssl_context_manager.h"
#include <boost/asio/ssl/context.hpp>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <tbb/concurrent_hash_map.h>
#include <thread>
#include <utility>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace hcam {

web_service::web_service()
    : livestream_instance(
          &asio_ws_session<HCAM_WEB_SERVICE_SSL_ENABLED>::send_frame) {
  const auto address = net::ip::make_address(config_manager::get().web_addr);
  const unsigned short port = config_manager::get().web_port;
  const auto threads = config_manager::get().web_thread_count;

  // The io_context is required for all I/O
  this->ioc = new net::io_context(threads);

  // Create and launch a listening port
  this->listener = std::make_shared<asio_listener>(
      *ioc, tcp::endpoint{address, port},
      std::bind(&web_service::create_session<TYPE>, this, std::placeholders::_1,
                std::placeholders::_2));
}

web_service::~web_service() {
  stop();
  delete ioc;
}

void web_service::run() {
  logger::info("web", "starting service...");
  ioc_stopped = false;
  listener->run();
  std::thread([this] {
    ioc->run();
    logger::debug("web", "ioc run finished!");
    cvm.lock();
    cv.notify_one();
    ioc_stopped = true;
    cvm.unlock();
  }).detach();
  stopped = false;
}

void web_service::stop() {
  logger::info("web", "shutting down service...");
  if (stopped) {
    logger::info("web", "service already stopped");
    return;
  }
  listener->stop();
  //遍历map，干掉session们
  std::vector<tcp::endpoint> keys(this->sessions.size());
  for (session_map_t::iterator it = this->sessions.begin();
       it != this->sessions.end(); ++it) {
    keys.push_back(it->first);
  }
  for (const auto &endpoint : keys) {
    std::shared_ptr<void> ref;
    TYPE *p = nullptr;
    //竟然是不可重入的。。。
    {
      session_map_t::accessor row;
      if (sessions.find(row, endpoint)) {
        p = static_cast<TYPE *>(row->second.get());
        ref = row->second;
        sessions.erase(row);
        logger::debug("web", endpoint,
                      " has been removed from session map due to stop request");
      }
    }
    if (p)
      p->close();
  }
  //等待session全部被杀之后，ioc的停止
  logger::debug("web", "waiting ioc");
  std::unique_lock<std::mutex> guard(cvm);
  cv.wait(guard, [this] { return ioc_stopped; });
  logger::info("web", "service stopped");
  stopped = true;
}

template <typename SESSION_TYPE>
void web_service::create_session(tcp::socket &&sock,
                                 beast::tcp_stream::endpoint_type remote) {
  auto endpoint = remote;

  //创建session对象的回调
  auto unregister = [this, endpoint]() -> bool {
    //这个在session自杀的时候被session调用
    session_map_t::accessor row;
    if (!sessions.find(row, endpoint)) {
      std::ostringstream fmt(" session->unregister_ session not found");
      return false;
    }
    if (!sessions.erase(row)) {
      std::ostringstream fmt(" session->unregister_ failed!!!");
      fmt << endpoint.address() << ":" << endpoint.port();
      throw std::runtime_error(fmt.str());
    }
    hcam::logger::debug("web", endpoint, " session->unregister_ ok");
    return true;
  };
  std::shared_ptr<void> session;
  if constexpr (SESSION_TYPE::ssl_enabled::value) {
    ssl::context ssl_ctx{ssl::context::tlsv12};
    load_server_certificate(ssl_ctx);
    auto new_session = new SESSION_TYPE(
        std::move(sock), remote, ssl_ctx, unregister,
        std::function<bool(tcp::endpoint, modify_session_callback_type)>{
            [this](tcp::endpoint key, modify_session_callback_type callback)
                -> bool { return modify_session(key, std::move(callback)); }});
    new_session->livestream = &this->livestream_instance;
    session.reset(new_session);
  } else {
    auto new_session = new SESSION_TYPE(
        std::move(sock), remote, unregister,
        std::function<bool(tcp::endpoint, modify_session_callback_type)>{
            [this](tcp::endpoint key, modify_session_callback_type callback)
                -> bool { return modify_session(key, std::move(callback)); }});
    new_session->livestream_ = &this->livestream_instance;
    session.reset(new_session);
  }

  //做一个引用计数
  session_map_t::value_type kv{endpoint, session};
  if (!sessions.insert(kv))
    throw std::runtime_error("tbb concurrent map insert failed");
  //开始运行session!
  ((SESSION_TYPE *)session.get())->run();
}

bool web_service::modify_session(
    tcp::endpoint key, modify_session_callback_type callback) noexcept {
  //这个在session自杀的时候被session调用
  session_map_t::accessor row;
  if (!sessions.find(row, key)) {
    return false;
  }
  void *session = row->second.get();
  auto ret = callback(session);
  if (!ret) {
    logger::fatal(
        "web",
        "web_service::modify_session: no replacement returned by callback");
    abort();
  }
  row->second = ret;
  return true;
}

livestream<HCAM_WEB_SERVICE_SSL_ENABLED> &
web_service::get_livestream_instace() {
  return livestream_instance;
}

} // namespace hcam
