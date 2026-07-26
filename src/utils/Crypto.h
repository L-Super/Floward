//
// Created by LMR on 25-9-07.
//
// Symmetric encryption helper for clipboard data transmitted to the sync
// server. Uses AES-256-GCM with a per-message random nonce. The key is
// supplied at build time via the CRYPTO_KEY_HEX compile definition (64 hex
// chars == 32 bytes). When the macro is absent a compile-time error is
// raised so the feature can never silently fall back to a no-op.
//

#pragma once

#include <QByteArray>

/**
 * Stateless AES-256-GCM encrypt/decrypt.
 *
 * Wire format produced by Encrypt():
 *   [12-byte nonce][N-byte ciphertext + 16-byte GCM auth tag]
 *
 * The same blob is what should be uploaded to the server and fed back to
 * Decrypt() on the receiving device. The server never sees the key.
 */
class Crypto {
public:
  /// Encrypt arbitrary bytes. Returns nonce || ciphertext+tag on success,
  /// or an empty QByteArray on failure.
  static QByteArray Encrypt(const QByteArray& plaintext);

  /// Decrypt a blob produced by Encrypt(). Returns the original plaintext
  /// on success, or an empty QByteArray on failure (wrong key, truncated
  /// payload, tampered auth tag, etc.).
  static QByteArray Decrypt(const QByteArray& payload);

  /// Length of the AES-256 key in bytes.
  static constexpr int kKeyLen = 32;
  /// Length of the GCM nonce in bytes.
  static constexpr int kNonceLen = 12;
  /// Length of the GCM authentication tag in bytes.
  static constexpr int kTagLen = 16;
};
