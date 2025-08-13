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

$PREFIX ./roq-okx \
  --name "okx" \
  --config_file "$CONFIG_FILE" \
  --cache_dir "$HOME/var/lib/roq/cache" \
  --event_log_dir "$HOME/var/lib/roq/data" \
  --event_log_symlink true \
  --client_listen_address "$HOME/run/$NAME.sock" \
  --service_listen_address "$HOME/run/metrics/${NAME}.sock" \
  --rest_uri "https://www.$URI" \
  --ws_public_uri "wss://ws.$URI:8443/ws/v5/public" \
  --ws_private_uri "wss://ws.$URI:8443/ws/v5/private" \
  --ws_business_uri "wss://ws.$URI:8443/ws/v5/business" \
  --download_time_series_lookback "2h" \
  --time_series_interval "60s" \
  --time_series_realtime true \
  --time_series_gateway_lookback "12h" \
  $@
