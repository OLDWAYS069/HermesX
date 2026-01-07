#include "HTTPSConnection.hpp"
#include "mbedtls/net_sockets.h"

namespace httpsserver {


HTTPSConnection::HTTPSConnection(ResourceResolver * resResolver):
  HTTPConnection(resResolver) {
  _sslCreated = false;
  _socket = 0;
}

HTTPSConnection::~HTTPSConnection() {
  // Close the socket
  closeConnection();
}

bool HTTPSConnection::isSecure() {
  return true;
}

bool HTTPSConnection::setup(mbedtls_ssl_config *sslConfig) {
  mbedtls_ssl_init(&_ssl);
  int res = mbedtls_ssl_setup(&_ssl, sslConfig);
  if (res == 0) {
    return true;
  } else {
    mbedtls_ssl_free(&_ssl);
    return false;
  }
}

bool HTTPSConnection::handshake() {
  int res;
  while (true) {
    res = mbedtls_ssl_handshake(&_ssl);
    if (res == 0) {
      return true;
    }
    if (res != MBEDTLS_ERR_SSL_WANT_READ && res != MBEDTLS_ERR_SSL_WANT_WRITE) {
      return false;
    }
  }
}

/**
 * Initializes the connection from a server socket.
 *
 * The call WILL BLOCK if accept(serverSocketID) blocks. So use select() to check for that in advance.
 */
int HTTPSConnection::initialize(int serverSocketID, mbedtls_ssl_config *sslConfig, HTTPHeaders *defaultHeaders) {
  if (_connectionState == STATE_UNDEFINED) {
    // Let the base class connect the plain tcp socket
    int resSocket = HTTPConnection::initialize(serverSocketID, defaultHeaders);

    // Build up SSL Connection context if the socket has been created successfully
    if (resSocket >= 0) {

      _socket = resSocket;
      _sslCreated = setup(sslConfig);

      if (_sslCreated) {
        // Bind SSL to the socket
        mbedtls_ssl_set_bio(&_ssl, &_socket, mbedtls_net_send, mbedtls_net_recv, NULL);

        // Perform the handshake
        if (handshake()) {
          return resSocket;
        } else {
          HTTPS_LOGE("SSL handshake failed. Aborting handshake. FID=%d", resSocket);
        }
      } else {
        HTTPS_LOGE("SSL setup failed. Aborting handshake. FID=%d", resSocket);
      }

    } else {
      HTTPS_LOGE("Could not accept() new connection. FID=%d", resSocket);
    }

    _connectionState = STATE_ERROR;
    _clientState = CSTATE_ACTIVE;

    // This will only be called if the connection could not be established and cleanup
    // variables like _ssl etc.
    closeConnection();
  }
  // Error: The connection has already been established or could not be established
  return -1;
}

int HTTPSConnection::shutdown() {
  int res;
  while (true) {
    res = mbedtls_ssl_close_notify(&_ssl);
    if (res == 0) {
      return 1;
    }
    if (res != MBEDTLS_ERR_SSL_WANT_WRITE) {
      return 0;
    }
  }
}

void HTTPSConnection::closeConnection() {

  // FIXME: Copy from HTTPConnection, could be done better probably
  if (_connectionState != STATE_ERROR && _connectionState != STATE_CLOSED) {

    // First call to closeConnection - set the timestamp to calculate the timeout later on
    if (_connectionState != STATE_CLOSING) {
      _shutdownTS = millis();
    }

    // Set the connection state to closing. We stay in closing as long as SSL has not been shutdown
    // correctly
    _connectionState = STATE_CLOSING;
  }

  // Try to tear down SSL while we are in the _shutdownTS timeout period or if an error occurred
  if (_sslCreated) {
    if (_connectionState == STATE_ERROR || shutdown() == 0) {
      // SSL shutdown will return 1 as soon as the client answered with close notify
      // This means we are safe to close the socket
      mbedtls_ssl_free(&_ssl);
      _sslCreated = false;
    } else if (_shutdownTS + HTTPS_SHUTDOWN_TIMEOUT < millis()) {
      // The timeout has been hit, we force SSL shutdown now by resetting the session
      mbedtls_ssl_free(&_ssl);
      _sslCreated = false;
      HTTPS_LOGW("SSL shutdown did not receive close notification from the client");
      _connectionState = STATE_ERROR;
    }
  }

  // If SSL has been brought down, close the socket
  if (!_sslCreated) {
    HTTPConnection::closeConnection();
  }
}

size_t HTTPSConnection::writeBuffer(byte* buffer, size_t length) {
  while (true) {
    int res = mbedtls_ssl_write(&_ssl, buffer, length);
    if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
      continue;
    }
    return res;
  }
}

size_t HTTPSConnection::readBytesToBuffer(byte* buffer, size_t length) {
  while (true) {
    int res = mbedtls_ssl_read(&_ssl, buffer, length);
    if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
      continue;
    }
    return res;
  }
}

size_t HTTPSConnection::pendingByteCount() {
  return mbedtls_ssl_get_bytes_avail(&_ssl);
}

bool HTTPSConnection::canReadData() {
  return HTTPConnection::canReadData() || (mbedtls_ssl_get_bytes_avail(&_ssl) > 0);
}

} /* namespace httpsserver */
