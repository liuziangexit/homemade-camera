#ifndef __HOMECAM_WEB_SERVICE_H_
#define __HOMECAM_WEB_SERVICE_H_
#include "boost/beast.hpp"
#include "config/config.h"
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
#include <utility>

namespace homemadecam {

struct endpoint_compare;

struct endpoint_compare {
  static size_t hash(const tcp::endpoint &e) {
    uint64_t hash = e.address().to_v4().to_ulong();
    hash <<= 32;
    hash |= e.port();
    return hash;
  }
  //! True if strings are equal
  static bool equal(const tcp::endpoint &x, const tcp::endpoint &y) {
    return hash(x) == hash(y);
  }
};

//这个只是临时性的
using TYPE = //
    asio_http_session<false>;
// asio_ws_session<false>;

using session_map_t =
    tbb::concurrent_hash_map<tcp::endpoint, std::shared_ptr<void>,
                             endpoint_compare>;

class web {
public:
  web(const std::string &config_file) : conf(config_file) {
    // TODO frome config
    auto const address = net::ip::make_address("0.0.0.0");
    // auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(atoi("8080"));
    auto const threads = std::max<int>(1, atoi("2"));

    // The io_context is required for all I/O
    this->ioc = new net::io_context(threads);

    // Create and launch a listening port
    this->listener = std::make_shared<asio_listener>(
        *ioc, tcp::endpoint{address, port},
        std::bind(&web::create_session, this, std::placeholders::_1));
  }

  std::condition_variable cv;
  std::mutex cvm;
  bool ioc_stopped;

  ~web() {
    stop();
    delete ioc;
  }

  void run() {
    ioc_stopped = false;
    listener->run();
    std::thread([this] {
      ioc->run();
      logger::info("ioc run finished!");
      cvm.lock();
      cv.notify_one();
      ioc_stopped = true;
      cvm.unlock();
    }).detach();
  }

  void stop() {
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
          logger::info(endpoint, " has been removed from session map");
        }
      }
      if (p)
        p->close();
    }
    //等待session全部被杀之后，ioc的停止
    logger::info("waiting ioc");
    std::unique_lock<std::mutex> guard(cvm);
    while (!ioc_stopped) {
      cv.wait(guard);
    }
    logger::info("stop ok!");
  }

private:
  void create_session(tcp::socket &&sock) {
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
      homemadecam::logger::info(endpoint, " session->unregister_ ok");
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

private:
  config conf;
  std::shared_ptr<asio_listener> listener;
  net::io_context *ioc;
  session_map_t sessions;
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
