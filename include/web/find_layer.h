#ifndef __HOMECAM_FIND_LAYER_H_
#define __HOMECAM_FIND_LAYER_H_
#include "boost/beast.hpp"
#include <boost/beast/core.hpp>
#include <type_traits>
#include <utility>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace hcam {

template <typename LAYER, typename STREAM> LAYER &find_layer(STREAM &stream) {
  if constexpr (!std::is_same_v<LAYER, std::decay_t<STREAM>>) {
    return find_layer<LAYER>(stream.next_layer());
  } else {
    return stream;
  }
}

} // namespace hcam

#endif // HOMECAM_WEB_SERVICE_H
