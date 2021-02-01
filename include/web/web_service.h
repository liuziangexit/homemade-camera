#ifndef __HOMECAM_WEB_SERVICE_H_
#define __HOMECAM_WEB_SERVICE_H_
#include "boost/beast.hpp"
#include "config/config.h"
#include "web/asio_http_session.h"
#include "web/asio_listener.h"
#include "web/asio_ws_session.h"
#include <condition_variable>
#include <mutex>
#include <string>
#include <tbb/concurrent_hash_map.h>

namespace hcam {

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

class web_service {
  using TYPE = asio_http_session<true>;
  using session_map_t =
      tbb::concurrent_hash_map<tcp::endpoint, std::shared_ptr<void>,
                               endpoint_compare>;

public:
  std::condition_variable cv;
  std::mutex cvm;
  bool ioc_stopped;

  web_service();
  ~web_service();
  void run();
  void stop();

  //将会把session map里的session换成callback返回的session
  using modify_session_callback_type =
      std::function<std::shared_ptr<void>(void *)>;
  bool modify_session(tcp::endpoint key,
                      modify_session_callback_type callback) noexcept;
  template <typename SESSION_TYPE> void create_session(tcp::socket &&);

private:
  std::shared_ptr<asio_listener> listener;
  net::io_context *ioc;
  session_map_t sessions;
};

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
