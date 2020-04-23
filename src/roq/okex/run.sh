#!/usr/bin/env bash

CWD="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

if [ "$1" == "debug" ]; then
	PREFIX="libtool --mode=execute gdb --args"
else
	PREFIX=
fi

NAME="okex"

CONFIG_FILE="$CWD/config/$NAME.toml"

URI="okex.com"

REST_URI="https://api.$URI/api/2"
WS_URI="wss://api.$URI/api/2/ws"

$PREFIX ./roq-okex \
	--name "$NAME" \
	--config-file "$CONFIG_FILE" \
	--rest-uri "$REST_URI" \
	--ws-uri "$WS_URI" \
	--listen "$CWD/$NAME.sock" \
	--metrics "$CWD/${NAME}_metrics.sock" \
	$@
