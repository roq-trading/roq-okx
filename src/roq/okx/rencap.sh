#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
  PREFIX="gdb --args"
else
  PREFIX=
fi

NAME="okx"

CONFIG_FILE="$CWD/config/rencap.toml"

URI="okx.com"

WS_URI="wss://ws.$URI:8443/ws/v5"

$PREFIX ./roq-okx \
  --name "okx" \
  --config_file "$CONFIG_FILE" \
  --cache_dir "$HOME/var/lib/roq/cache" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink true \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --metrics_listen_address "$HOME/run/metrics/${NAME}.sock" \
  --ws_public_uri "$WS_URI/public" \
  --ws_private_uri "$WS_URI/private" \
  --ws_books_use_public true \
  $@
