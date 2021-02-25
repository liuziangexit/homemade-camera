#ifndef HCAM_WEB_H
#define HCAM_WEB_H
#include "config/config.h"
#include "ssl_context_manager.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace hcam {

void run_net_ipc_handler(int ipc_fd);

template <bool SSL> class session;
class web {
public:
  struct session_context {
    bool ssl;
    std::weak_ptr<void> session;
  };

private:
  template <bool SSL> friend class session;

  enum state_t { STOPPED, STARTING, RUNNING, CLOSING };
  std::atomic<state_t> state;
  const int thread_count;
  boost::beast::net::io_context io_context;
  std::vector<std::thread> io_threads;
  boost::beast::net::ip::tcp::acceptor acceptor, ssl_acceptor;
  std::atomic<uint32_t> online = 0;
  boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tlsv12};

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
    bool operator()(const boost::asio::ip::tcp::endpoint &x,
                    const boost::asio::ip::tcp::endpoint &y) const {
      return hash(x) < hash(y);
    }
  };
  using session_map_t = std::map<boost::asio::ip::tcp::endpoint,
                                 session_context, endpoint_compare>;

  std::mutex subscribed_mut;
  session_map_t subscribed;
  int cap_web_fd;

public:
  web(int cap_web_fd);
  ~web();
  void run(int ipc_fd);
  void stop();
  void foreach_session(std::function<void(const session_context &)> viewer);

private:
  bool change_state(state_t expect, state_t desired);
  void change_state_certain(state_t expect, state_t desired);
  bool on_accept(bool ssl, boost::beast::error_code ec,
                 boost::asio::ip::tcp::socket socket);
};
} // namespace hcam

#endif // HOMECAM_WEB_H
