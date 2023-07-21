#!/usr/bin/env bash

if [ "$1" == "debug" ]; then
  PREFIX="gdb --args"
else
  PREFIX=
fi

NAME="okx"

CONFIG="${CONFIG:-$NAME}"

CONFIG_FILE="$ROQ_CONFIG_PATH/roq-okx/$CONFIG.toml"

URI="okx.com"

WS_URI="wss://ws.$URI:8443/ws/v5"

$PREFIX ./roq-okx \
  --name "okx" \
  --config_file "$CONFIG_FILE" \
  --cache_dir "$HOME/var/lib/roq/cache" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink true \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --service_listen_address "$HOME/run/metrics/${NAME}.sock" \
  --ws_public_uri "$WS_URI/public" \
  --ws_private_uri "$WS_URI/private" \
  $@
