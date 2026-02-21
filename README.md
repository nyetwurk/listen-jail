# listen-jail

An `LD_PRELOAD` shared library that intercepts `bind()` and `listen()` to restrict which addresses and interfaces applications can bind to. Useful for jailing applications that default to binding on all interfaces (0.0.0.0 or ::).

## Overview

listen-jail rewrites or constrains socket bind behavior without modifying the application:

- **BIND_IP**: Rewrites binds to `0.0.0.0` / `::` / `::ffff:0.0.0.0` to a specific IP
- **BIND_INTERFACE**: Applies `SO_BINDTODEVICE` when `listen()` is called on a socket bound to any address

Only "any" addresses are intercepted. Binds to specific addresses (e.g. `127.0.0.1`, `::1`) pass through unchanged.

## Build

```sh
make
```

Produces `listen-jail.so`. Optional:

```sh
make install   # installs to /usr/local/lib/listen-jail/
make test      # runs test.sh
make clean     # removes *.so
```

## Usage

Set environment variables and preload the library:

```sh
export LD_PRELOAD=/usr/local/lib/listen-jail/listen-jail.so
export BIND_IP=127.0.0.1        # rewrite any bind to this IP (optional)
export BIND_INTERFACE=lo        # bind to this interface on listen (optional)
your-application
```

- **BIND_IP**: When set, binds to `0.0.0.0` (IPv4) or `::` / `::ffff:0.0.0.0` (IPv6) are rewritten to this IP. IPv4 or IPv6 format supported.
- **BIND_INTERFACE**: When set, `listen()` on an any-bound socket will call `setsockopt(SO_BINDTODEVICE)` with this interface name.
- **LISTEN_JAIL_DEBUG**: Set to any non-empty value to enable debug logging to stderr.

## Example: TP-Link Omada EAP Controller

`control-jail.sh` and `control.sh` are Omada controller startup scripts. The jail variant uses listen-jail via `/etc/default/tpeap`:

**tpeap-example** — copy to `/etc/default/tpeap` and adjust:

```sh
LD_PRELOAD="/usr/local/lib/listen-jail/listen-jail.so"
HTTP_HOST="192.168.1.1"
export LISTEN_JAIL_DEBUG=1
export BIND_INTERFACE="eth1"
```

`control-jail.sh` sources this file and passes `LD_PRELOAD` to JSVC so the controller only binds on the chosen interface.

## Testing

```sh
make test
# or
./test.sh
```

Runs bind and listen tests for any_ip and non-any_ip addresses (IPv4 and IPv6).

## Files

| File              | Description                                               |
|-------------------|-----------------------------------------------------------|
| `listen-jail.c`   | Core LD_PRELOAD library (bind/listen interception)       |
| `Makefile`        | Build, install, clean, test                               |
| `test.sh`         | Test script for bind/listen behavior                      |
| `control.sh`      | Omada controller startup (no listen-jail)                 |
| `control-jail.sh` | Omada controller startup with listen-jail                 |
| `tpeap-example`   | Example `/etc/default/tpeap` config for jail mode         |

## Requirements

- Linux (uses `SO_BINDTODEVICE`, Linux if.h)
- gcc
- Python 3 (for tests)
