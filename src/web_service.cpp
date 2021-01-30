#include "web/web_service.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "config/config_manager.h"
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

web_service::web_service() {
  const auto address = net::ip::make_address(config_manager::get().web_addr);
  const unsigned short port = config_manager::get().web_port;
  const auto threads = config_manager::get().web_thread_count;

  // The io_context is required for all I/O
  this->ioc = new net::io_context(threads);

  // Create and launch a listening port
  this->listener = std::make_shared<asio_listener>(
      *ioc, tcp::endpoint{address, port},
      std::bind(&web_service::create_session<TYPE>, this,
                std::placeholders::_1));
}

web_service::~web_service() {
  stop();
  delete ioc;
}

void web_service::run() {
  logger::info("starting web service...");
  ioc_stopped = false;
  listener->run();
  std::thread([this] {
    ioc->run();
    logger::debug("ioc run finished!");
    cvm.lock();
    cv.notify_one();
    ioc_stopped = true;
    cvm.unlock();
  }).detach();
}

void web_service::stop() {
  logger::info("shutting down web service...");
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
        logger::debug(endpoint, " has been removed from session map");
      }
    }
    if (p)
      p->close();
  }
  //等待session全部被杀之后，ioc的停止
  logger::debug("waiting ioc");
  std::unique_lock<std::mutex> guard(cvm);
  while (!ioc_stopped) {
    cv.wait(guard);
  }
  logger::info("stop ok!");
}

template <typename SESSION_TYPE>
void web_service::create_session(tcp::socket &&sock) {
  auto endpoint = sock.remote_endpoint();

  //创建session对象的回调
  auto unregister = [this, endpoint]() -> bool {
    //这个在session自杀的时候被session调用
    session_map_t::accessor row;
    if (!sessions.find(row, endpoint)) {
      return false;
    }
    if (!sessions.erase(row)) {
      std::ostringstream fmt(" session->unregister_ failed!!!");
      fmt << endpoint.address() << ":" << endpoint.port();
      throw std::runtime_error(fmt.str());
    }
    hcam::logger::debug(endpoint, " session->unregister_ ok");
    return true;
  };
  std::shared_ptr<void> session;
  session.reset(new SESSION_TYPE(
      std::move(sock), unregister,
      std::function<bool(tcp::endpoint, modify_session_callback_type)>{
          [this](tcp::endpoint key, modify_session_callback_type callback)
              -> bool { return modify_session(key, std::move(callback)); }}));

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
        "web_service::modify_session: no replacement returned by callback");
    abort();
  }
  row->second = ret;
  return true;
}

} // namespace hcam
