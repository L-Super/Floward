//
// Created by LMR on 25-9-07.
//

#include "Crypto.h"

#include <QRandomGenerator>
#include <QScopeGuard>

// CRYPTO_KEY_HEX must be provided by the build system as a 64-char hex
// string (32-byte AES-256 key). See the top-level CMakeLists.txt for how
// it is sourced (CRYPTO_KEY cmake var ← CRYPTO_KEY env var ← dev default).
#ifndef CRYPTO_KEY_HEX
#error "CRYPTO_KEY_HEX is not defined. Set CRYPTO_KEY via cmake or the CRYPTO_KEY env var."
#endif

#include <openssl/evp.h>

namespace {

/// Convert a hex character to its nibble value.
inline int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

/// Convert a hex string to a byte array.
QByteArray HexToBytes(const char* hex, int len) {
  QByteArray result;
  result.reserve(len / 2);
  for (int i = 0; i + 1 < len; i += 2) {
    result.append(static_cast<char>((HexNibble(hex[i]) << 4) | HexNibble(hex[i + 1])));
  }
  return result;
}

/// Lazily decode the compile-time key. Called at most once.
const QByteArray& GetKey() {
  static const QByteArray key = HexToBytes(CRYPTO_KEY_HEX, 64);
  return key;
}

} // namespace

QByteArray Crypto::Encrypt(const QByteArray& plaintext) {
  const auto& key = GetKey();
  if (key.size() != kKeyLen) {
    return {};
  }

  // Random 12-byte nonce — MUST be unique per message under the same key.
  QByteArray nonce(kNonceLen, '\0');
  auto* gen = QRandomGenerator::system();
  for (int i = 0; i < kNonceLen; ++i) {
    nonce[i] = static_cast<char>(gen->generate() & 0xFF);
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return {};
  }

  auto guard = qScopeGuard([&] { EVP_CIPHER_CTX_free(ctx); });

  // Init AES-256-GCM
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    return {};
  }
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) {
    return {};
  }
  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                        reinterpret_cast<const unsigned char*>(key.constData()),
                        reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
    return {};
  }

  // Encrypt plaintext
  QByteArray ciphertext(plaintext.size(), '\0');
  int outLen = 0;
  if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &outLen,
                        reinterpret_cast<const unsigned char*>(plaintext.constData()),
                        plaintext.size()) != 1) {
    return {};
  }
  int finalLen = 0;
  if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen,
                          &finalLen) != 1) {
    return {};
  }

  // Get 16-byte GCM auth tag
  QByteArray tag(kTagLen, '\0');
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag.data()) != 1) {
    return {};
  }

  // Wire format: nonce || ciphertext || tag
  return nonce + ciphertext.left(outLen + finalLen) + tag;
}

QByteArray Crypto::Decrypt(const QByteArray& payload) {
  const auto& key = GetKey();
  if (key.size() != kKeyLen) {
    return {};
  }
  // Minimum: nonce (12) + tag (16), ciphertext can be empty
  if (payload.size() < kNonceLen + kTagLen) {
    return {};
  }

  const QByteArray nonce = payload.left(kNonceLen);
  const QByteArray tag = payload.right(kTagLen);
  const QByteArray ciphertext = payload.mid(kNonceLen, payload.size() - kNonceLen - kTagLen);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return {};
  }

  auto guard = qScopeGuard([&] { EVP_CIPHER_CTX_free(ctx); });

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    return {};
  }
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) {
    return {};
  }
  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                        reinterpret_cast<const unsigned char*>(key.constData()),
                        reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
    return {};
  }

  // Decrypt
  QByteArray plaintext(ciphertext.size(), '\0');
  int outLen = 0;
  if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &outLen,
                        reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                        ciphertext.size()) != 1) {
    return {};
  }

  // Set expected auth tag before final
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                          const_cast<char*>(tag.constData())) != 1) {
    return {};
  }

  int finalLen = 0;
  if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + outLen,
                          &finalLen) != 1) {
    // Auth tag mismatch — wrong key, tampered payload, or truncated data.
    return {};
  }

  return plaintext.left(outLen + finalLen);
}
