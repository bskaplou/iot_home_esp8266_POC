#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. $DIR/settings

arduino-cli compile -v --fqbn $BOARD $SKETCH
