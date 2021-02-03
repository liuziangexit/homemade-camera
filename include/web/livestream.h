#ifndef __HOMECAM_LIVESTREAM_H_
#define __HOMECAM_LIVESTREAM_H_
#include "boost/beast.hpp"
#include "util/logger.h"
#include <boost/beast/core.hpp>
#include <functional>
#include <memory>
#include <tbb/concurrent_hash_map.h>
#include <type_traits>
#include <utility>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace hcam {

template <bool SSL> class livestream {
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
  // tbb的set都不提供线程安全的删除操作，所以干脆用hashmap了
  using session_map_t =
      tbb::concurrent_hash_map<tcp::endpoint, std::shared_ptr<void>,
                               endpoint_compare>;
  session_map_t subscribed;
  std::function<void(void *, unsigned char *, uint32_t)> send_frame_;

public:
  livestream(std::function<void(void *, unsigned char *, uint32_t)> send_frame)
      : send_frame_(std::move(send_frame)) {}

  bool add(tcp::endpoint ep, std::shared_ptr<void> session) {
    typename session_map_t::value_type kv{ep, session};
    return subscribed.insert(kv);
  }

  bool remove(tcp::endpoint ep) {
    typename session_map_t::accessor row;
    if (!subscribed.find(row, ep)) {
      return false;
    }
    if (!subscribed.erase(row)) {
      std::ostringstream fmt(" livestream remove failed!!!");
      fmt << ep.address() << ":" << ep.port();
      throw std::runtime_error(fmt.str());
    }
    return true;
  }

  void send_frame_to_all(unsigned char *frame, uint32_t len) {
    std::vector<tcp::endpoint> keys(this->subscribed.size());
    for (typename session_map_t::iterator it = this->subscribed.begin();
         it != this->subscribed.end(); ++it) {
      keys.push_back(it->first);
    }
    for (const auto &endpoint : keys) {
      typename session_map_t::accessor row;
      if (subscribed.find(row, endpoint)) {
        send_frame_(row->second.get(), frame, len);
      }
    }
    /*logger::debug("web", "send frame to ", keys.size(), " clients");*/
  }
};
} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
