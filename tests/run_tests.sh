#!/usr/bin/env bash
# Test runner for justfile compatibility
# This is a wrapper around run_all.sh

exec "$(dirname "$0")/run_all.sh" "$@"
