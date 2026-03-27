#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <host-or-ip> [output-dir]" >&2
  exit 1
fi

host="$1"
output_dir="${2:-$(pwd)/local-dev-tls}"

mkdir -p "$output_dir"

ca_key="$output_dir/local-dev-ca.key.pem"
ca_cert="$output_dir/local-dev-ca.cert.pem"
ca_cert_ios="$output_dir/local-dev-ca.cer"
server_key="$output_dir/${host}.key.pem"
server_csr="$output_dir/${host}.csr.pem"
server_cert="$output_dir/${host}.cert.pem"
server_ext="$output_dir/${host}.ext"

if [[ ! -f "$ca_key" || ! -f "$ca_cert" ]]; then
  openssl genrsa -out "$ca_key" 4096 >/dev/null 2>&1
  openssl req -x509 -new -nodes -key "$ca_key" -sha256 -days 3650 \
    -out "$ca_cert" \
    -subj "/CN=EmptyEpsilon Local Dev CA" >/dev/null 2>&1
fi

cp "$ca_cert" "$ca_cert_ios"

openssl genrsa -out "$server_key" 2048 >/dev/null 2>&1
openssl req -new -key "$server_key" -out "$server_csr" \
  -subj "/CN=$host" >/dev/null 2>&1

cat >"$server_ext" <<EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:$host,IP:$host
EOF

if [[ "$host" != *[0-9]* || "$host" == *[!0-9.]* ]]; then
  cat >"$server_ext" <<EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:$host
EOF
fi

openssl x509 -req -in "$server_csr" -CA "$ca_cert" -CAkey "$ca_key" -CAcreateserial \
  -out "$server_cert" -days 825 -sha256 -extfile "$server_ext" >/dev/null 2>&1

echo "Created local CA:"
echo "  $ca_cert"
echo "iPad import copy:"
echo "  $ca_cert_ios"
echo "Created server certificate:"
echo "  $server_cert"
echo "Created server key:"
echo "  $server_key"
