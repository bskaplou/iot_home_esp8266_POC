#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. $DIR/settings

#nodemcu-uploader --baud $BAUD --port $PORT terminal
nodemcu-uploader --port $PORT terminal
