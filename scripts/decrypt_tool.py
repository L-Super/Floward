#!/usr/bin/env python3
"""
Offline decryption tool for Floward sync payloads.

Use this to inspect clipboard data captured from the sync server when
debugging. The key is the same 64-char hex string used at build time
(the CRYPTO_KEY GitHub Actions secret or the local CRYPTO_KEY env var).

Wire format (matches src/utils/Crypto.cpp + ClipboardApiClient):
    base64( [12B nonce][N-byte ciphertext][16B GCM auth tag] )

Both the HTTP body (multipart "data"/"file" field) and the WebSocket
JSON "data" field carry the same base64-encoded blob, so the server
only ever handles ASCII strings and needs no binary-aware logic.

Examples
--------
# Decrypt a base64 payload (the canonical wire format)
python scripts/decrypt_tool.py --key 3f7a9b2c... payload.b64

# Pipe a base64 payload from stdin
echo 'QosOWdsx...=' | python scripts/decrypt_tool.py --key 3f7a9b2c... -

# Decrypt a raw binary blob (nonce||cipher||tag, no base64 wrapping)
python scripts/decrypt_tool.py --key 3f7a9b2c... --raw dump.bin

# Decrypt and write to file (useful for image payloads)
python scripts/decrypt_tool.py --key 3f7a9b2c... -o plain.png payload.b64
"""
from __future__ import annotations

import argparse
import base64
import sys
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    sys.stderr.write(
        "Missing dependency: cryptography\n"
        "Install with:  pip install cryptography\n"
    )
    sys.exit(2)


NONCE_LEN = 12
TAG_LEN = 16


def decrypt(key_hex: str, payload: bytes) -> bytes:
    """Decrypt a nonce||ciphertext||tag blob with AES-256-GCM."""
    if len(key_hex) != 64:
        raise ValueError(f"Key must be 64 hex chars (32 bytes), got {len(key_hex)}")
    try:
        key = bytes.fromhex(key_hex)
    except ValueError as exc:
        raise ValueError(f"Key must be hex chars only: {exc}") from exc

    if len(payload) < NONCE_LEN + TAG_LEN:
        raise ValueError(
            f"Payload too short ({len(payload)}B); need >= {NONCE_LEN + TAG_LEN}B"
        )

    nonce = payload[:NONCE_LEN]
    cipher_and_tag = payload[NONCE_LEN:]
    return AESGCM(key).decrypt(nonce, cipher_and_tag, None)


def read_payload(path: str, is_raw: bool) -> bytes:
    """Read payload from a file or stdin.

    By default the payload is base64 (the canonical wire format). Pass
    --raw to skip base64 decoding and treat the bytes as a raw
    nonce||cipher||tag blob.
    """
    if path == "-":
        raw = sys.stdin.buffer.read()
    else:
        raw = Path(path).read_bytes()

    if is_raw:
        return raw

    cleaned = b"".join(raw.split())  # tolerate whitespace/newlines
    try:
        return base64.b64decode(cleaned, validate=False)
    except Exception as exc:
        raise ValueError(f"Invalid base64 payload: {exc}") from exc


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Decrypt a Floward sync payload (AES-256-GCM, base64 wire format)."
    )
    parser.add_argument(
        "--key",
        required=True,
        help="64-char hex AES-256 key (same as build-time CRYPTO_KEY)",
    )
    parser.add_argument(
        "file",
        help="Path to the ciphertext file, or '-' for stdin",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Treat input as raw nonce||cipher||tag bytes (skip base64 decode)",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Write plaintext to file (default: stdout)",
    )
    args = parser.parse_args()

    try:
        payload = read_payload(args.file, args.raw)
        plain = decrypt(args.key, payload)
    except ValueError as exc:
        sys.stderr.write(f"Error: {exc}\n")
        return 1
    except Exception as exc:  # noqa: BLE001
        sys.stderr.write(f"Decryption failed (wrong key? tampered?): {exc}\n")
        return 1

    if args.output:
        Path(args.output).write_bytes(plain)
        sys.stderr.write(f"Wrote {len(plain)} bytes to {args.output}\n")
    else:
        # Write raw bytes to stdout; let the terminal decide how to render
        sys.stdout.buffer.write(plain)
        sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
