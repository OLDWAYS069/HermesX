#include "HTTPSServer.hpp"

namespace httpsserver {


HTTPSServer::HTTPSServer(SSLCert * cert, const uint16_t port, const uint8_t maxConnections, const in_addr_t bindAddress):
  HTTPServer(port, maxConnections, bindAddress),
  _cert(cert) {

  // Configure runtime data
  mbedtls_ssl_config_init(&_ssl_config);
  mbedtls_x509_crt_init(&_ssl_cert);
  mbedtls_pk_init(&_ssl_pk);
  mbedtls_entropy_init(&_ssl_entropy);
  mbedtls_ctr_drbg_init(&_ssl_ctr_drbg);

  mbedtls_ctr_drbg_seed(&_ssl_ctr_drbg, mbedtls_entropy_func, &_ssl_entropy, nullptr, 0);
  mbedtls_ssl_conf_rng(&_ssl_config, mbedtls_ctr_drbg_random, &_ssl_ctr_drbg);
}

HTTPSServer::~HTTPSServer() {

  mbedtls_ctr_drbg_init(&_ssl_ctr_drbg);
  mbedtls_entropy_init(&_ssl_entropy);
  mbedtls_pk_free(&_ssl_pk);
  mbedtls_x509_crt_free(&_ssl_cert);
  mbedtls_ssl_config_free(&_ssl_config);
}

/**
 * This method starts the server and begins to listen on the port
 */
uint8_t HTTPSServer::setupSocket() {
  if (isRunning()) {
    return 1;
  }

  int res = mbedtls_ssl_config_defaults(&_ssl_config,
    MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (res != 0) {
    Serial.println("Setting SSL default config failed");
    return 0;
  }

  if (setupCert() != 0) {
    Serial.println("setupCert failed");
    return 0;
  }

  if (!HTTPServer::setupSocket()) {
    Serial.println("setupSockets failed");
    return 0;
  }

  return 1;
}

void HTTPSServer::teardownSocket() {

  HTTPServer::teardownSocket();
}

int HTTPSServer::createConnection(int idx) {
  HTTPSConnection * newConnection = new HTTPSConnection(this);
  _connections[idx] = newConnection;
  return newConnection->initialize(_socket, &_ssl_config, &_defaultHeaders);
}

/**
 * This method configures the certificate and private key for the given
 * ssl context
 */
int HTTPSServer::setupCert() {
  int res = 0;

  res = mbedtls_x509_crt_parse(&_ssl_cert, _cert->getCertData(), _cert->getCertLength());
  if (res != 0) {
    return res;
  }

#if ESP_IDF_VERSION_MAJOR > 4
  res = mbedtls_pk_parse_key(&_ssl_pk, _cert->getPKData(), _cert->getPKLength(), nullptr, 0,
                             mbedtls_ctr_drbg_random, &_ssl_ctr_drbg);
#else
  res = mbedtls_pk_parse_key(&_ssl_pk, _cert->getPKData(), _cert->getPKLength(), nullptr, 0);
#endif
  if (res != 0) {
    return res;
  }

  return mbedtls_ssl_conf_own_cert(&_ssl_config, &_ssl_cert, &_ssl_pk);
}

} /* namespace httpsserver */
