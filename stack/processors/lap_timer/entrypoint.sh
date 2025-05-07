#!/bin/sh

# If host.docker.internal doesn't resolve and we're on Linux, patch it
if ! getent hosts host.docker.internal > /dev/null; then
    if [ "$(uname)" = "Linux" ]; then
        HOST_IP=$(ip route | awk '/default/ { print $3 }')
        echo "$HOST_IP host.docker.internal" >> /etc/hosts
    fi
fi

exec "$@"