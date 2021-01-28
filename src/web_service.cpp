#include "web/web_service.h"
#include "boost/beast.hpp"
#include "config/config.h"
#include "config/config_manager.h"
#include "web/asio_http_session.h"
#include "web/asio_listener.h"
#include "web/asio_ws_session.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <tbb/concurrent_hash_map.h>
#include <thread>
#include <utility>

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
      std::bind(&web_service::create_session, this, std::placeholders::_1));
}

web_service::~web_service() {
  stop();
  delete ioc;
}

void web_service::run() {
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

void web_service::create_session(tcp::socket &&sock) {
  auto endpoint = sock.remote_endpoint();

  //创建session对象
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
  auto session = std::make_shared<TYPE>(std::move(sock), unregister);

  //做一个引用计数
  auto session_ref = session;
  session_map_t::value_type kv{endpoint, session_ref};
  if (!sessions.insert(kv))
    throw std::runtime_error("tbb concurrent map insert failed");
  //开始运行session!
  session->run();
}

} // namespace hcam
