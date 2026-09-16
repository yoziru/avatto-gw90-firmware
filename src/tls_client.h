#ifndef TLS_CLIENT_H_
#define TLS_CLIENT_H_

#include <Arduino.h>
#include <NetworkClientSecure.h>

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

inline void trustEspCertificateBundle(NetworkClientSecure &client)
{
    client.setCACertBundle(x509_crt_bundle_start,
                           reinterpret_cast<uintptr_t>(x509_crt_bundle_end) -
                               reinterpret_cast<uintptr_t>(x509_crt_bundle_start));
}

#endif
