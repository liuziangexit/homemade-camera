#ifndef HCAM_WEB_H
#define HCAM_WEB_H
#include "boost/asio.hpp"
#include "boost/beast.hpp"
#include "config/config.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <tbb/concurrent_hash_map.h>
#include <thread>
#include <vector>

namespace hcam {
template <bool SSL> class session;
class web {
public:
  struct session_context {
    bool ssl;
    void *session;
  };

private:
  template <bool SSL> friend class session;

  struct endpoint_compare {
    static size_t hash(const boost::asio::ip::tcp::endpoint &e) {
      uint64_t hash = e.address().to_v4().to_ulong();
      hash <<= 32;
      hash |= e.port();
      return hash;
    }
    //! True if strings are equal
    static bool equal(const boost::asio::ip::tcp::endpoint &x,
                      const boost::asio::ip::tcp::endpoint &y) {
      return hash(x) == hash(y);
    }
  };
  using session_map_t =
      tbb::concurrent_hash_map<boost::asio::ip::tcp::endpoint, session_context,
                               endpoint_compare>;

  enum state_t { STOPPED, STARTING, RUNNING, CLOSING };
  std::atomic<state_t> state;
  const int thread_count;
  boost::beast::net::io_context io_context;
  std::vector<std::thread> io_threads;
  boost::beast::net::ip::tcp::acceptor acceptor, ssl_port_acceptor;
  std::atomic<uint32_t> online = 0;

  std::shared_mutex subscribed_mut;
  session_map_t subscribed;

public:
  web();
  ~web();
  void run();
  void stop();
  void foreach_session(std::function<void(const session_context &)> viewer);

private:
  bool change_state(state_t expect, state_t desired);
  void change_state_certain(state_t expect, state_t desired);
  bool on_accept(boost::beast::error_code ec,
                 boost::asio::ip::tcp::socket socket);
};
} // namespace hcam

#endif // HOMECAM_WEB_H
