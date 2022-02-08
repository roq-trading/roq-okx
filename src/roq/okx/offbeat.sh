#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
	PREFIX="gdb --args"
else
	PREFIX=
fi

NAME="offbeat"

CONFIG_FILE="$CWD/config/$NAME.toml"

URI="okx.com"

WS_URI="wss://ws.$URI:8443/ws/v5"

$PREFIX ./roq-okx \
	--name "okx" \
	--config_file "$CONFIG_FILE" \
	--client_listen_address $CWD/$NAME.sock \
	--metrics_listen_address 2345 \
	--ws_public_uri "$WS_URI/public" \
	--ws_private_uri "$WS_URI/private" \
  --event_log_symlink \
	$@
