#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
	PREFIX="gdb --args"
else
	PREFIX=
fi

NAME="kucoin"

CONFIG_FILE="$CWD/config/$NAME.toml"

URI="api.kucoin.com"

REST_URI="https://$URI"

$PREFIX ./roq-kucoin \
	--name "kucoin" \
	--config_file "$CONFIG_FILE" \
	--client_listen_address $CWD/$NAME.sock \
	--metrics_listen_address 1234 \
	--rest_uri "$REST_URI" \
	$@
