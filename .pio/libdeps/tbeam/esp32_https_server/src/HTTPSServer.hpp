#ifndef SRC_HTTPSSERVER_HPP_
#define SRC_HTTPSSERVER_HPP_

// Standard library
#include <string>

// Arduino stuff
#include <Arduino.h>

// Required for SSL
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#undef read

// Internal includes
#include "HTTPServer.hpp"
#include "HTTPSServerConstants.hpp"
#include "HTTPHeaders.hpp"
#include "HTTPHeader.hpp"
#include "ResourceNode.hpp"
#include "ResourceResolver.hpp"
#include "ResolvedResource.hpp"
#include "HTTPSConnection.hpp"
#include "SSLCert.hpp"

namespace httpsserver {

/**
 * \brief Main implementation of the HTTP Server with TLS support. Use HTTPServer for plain HTTP
 */
class HTTPSServer : public HTTPServer {
public:
  HTTPSServer(SSLCert * cert, const uint16_t portHTTPS = 443, const uint8_t maxConnections = 4, const in_addr_t bindAddress = 0);
  virtual ~HTTPSServer();

private:
  // Static configuration. Port, keys, etc. ====================
  // Certificate that should be used (includes private key)
  SSLCert * _cert;
 
  //// Runtime data ============================================
  mbedtls_entropy_context _ssl_entropy;
  mbedtls_ctr_drbg_context _ssl_ctr_drbg;
  mbedtls_ssl_config _ssl_config;
  mbedtls_x509_crt _ssl_cert;
  mbedtls_pk_context _ssl_pk;

  // Status of the server: Are we running, or not?

  // Setup functions
  virtual uint8_t setupSocket();
  virtual void teardownSocket();
  int setupCert();

  // Helper functions
  virtual int createConnection(int idx);
};

} /* namespace httpsserver */

#endif /* SRC_HTTPSSERVER_HPP_ */
