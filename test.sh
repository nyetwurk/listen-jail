#!/bin/sh
export LISTEN_JAIL_DEBUG=1
export LD_PRELOAD=./listen-jail.so

run_bind() {
    family="$1"
    addr="$2"
    python3 -c "
import socket
fam = socket.AF_INET6 if '$family' == 'inet6' else socket.AF_INET
s = socket.socket(fam, socket.SOCK_STREAM)
s.bind(('$addr', 9999))
s.close()
print('done')
"
}

run_listen() {
    family="$1"
    addr="$2"
    python3 -c "
import socket
fam = socket.AF_INET6 if '$family' == 'inet6' else socket.AF_INET
s = socket.socket(fam, socket.SOCK_STREAM)
s.bind(('$addr', 9999))
s.listen(5)
s.close()
print('done')
"
}

for test in \
    "bind|inet|0.0.0.0|127.0.0.1||bind to any_ip (0.0.0.0)" \
    "bind|inet|127.0.0.1|127.0.0.1||bind to non-any_ip (127.0.0.1)" \
    "bind|inet6|::ffff:0.0.0.0|127.0.0.1||bind to any_ip (::ffff:0.0.0.0)" \
    "bind|inet6|::1|||bind to non-any_ip (::1)" \
    "listen|inet|0.0.0.0||lo|listen on any_ip (0.0.0.0)" \
    "listen|inet|127.0.0.1||lo|listen on non-any_ip (127.0.0.1)"
do
    IFS='|' read -r op family addr bind_ip bind_iface desc <<EOF
$test
EOF
    echo "=== $desc ==="
    [ -n "$bind_ip" ] && export BIND_IP="$bind_ip" || unset BIND_IP
    [ -n "$bind_iface" ] && export BIND_INTERFACE="$bind_iface" || unset BIND_INTERFACE

    if [ "$op" = "bind" ]; then
        run_bind "$family" "$addr"
    else
        run_listen "$family" "$addr"
    fi
done
