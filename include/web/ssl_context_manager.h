#ifndef HOMECAM_SSL_CONTEXT_MANAGER_H
#define HOMECAM_SSL_CONTEXT_MANAGER_H
#include "config/config.h"
#include "util/file_helper.h"
#include "util/logger.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

namespace hcam {

inline void load_server_certificate(boost::asio::ssl::context &ctx) {
  std::vector<uint8_t> cert, key;
  if (!read_file(config::get().ssl_cert, cert)) {
    logger::fatal("web", "load ssl cert failed");
    abort();
  }
  if (!read_file(config::get().ssl_key, key)) {
    logger::fatal("web", "load ssl key failed");
    abort();
  }

  ctx.set_options(boost::asio::ssl::context::default_workarounds |
                  boost::asio::ssl::context::no_sslv2);

  ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

  ctx.use_private_key(boost::asio::buffer(key.data(), key.size()),
                      boost::asio::ssl::context::file_format::pem);
}
} // namespace hcam

#endif // HOMECAM_SSL_CONTEXT_MANAGER_H