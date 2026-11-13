#!/usr/bin/env python3
"""Malformed-input probes against one running server process.

Starts <version>/nano_ssh_server once and throws things at it that a
well-behaved client never sends: broken version lines, impossible packet
lengths, a connection cut at every stage of the handshake, single bits
flipped in the first encrypted packet (through a small proxy, so the
client is real OpenSSH), wrong credentials, an oversized exec request.
After every probe a real login with the right password must still print
"Hello World": the same process has to survive all of it.

Needs ssh and sshpass on the PATH (nix-shell provides them).
"""

import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
PORT = 2222
PROXY_PORT = 2223
SSH = ["ssh", "-F", "none", "-o", "StrictHostKeyChecking=no",
       "-o", "UserKnownHostsFile=/dev/null", "-o", "LogLevel=ERROR",
       "-o", "NumberOfPasswordPrompts=1", "-o", "ConnectTimeout=10"]


def login(password="password123", user="user", port=PORT, options=(),
          command=None):
    """One OpenSSH login; returns what the client printed."""
    cmd = ["sshpass", "-p", password] + SSH + ["-p", str(port)] + \
        list(options) + [f"{user}@127.0.0.1"]
    if command is not None:
        cmd.append(command)
    r = subprocess.run(cmd, stdin=subprocess.DEVNULL, capture_output=True,
                       timeout=60)
    return r.stdout.decode(errors="replace")


def raw(data, read=True, linger=0.2):
    """Open a connection, send bytes, optionally read what comes back."""
    s = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    out = b""
    try:
        if data:
            s.sendall(data)
        time.sleep(linger)
        if read:
            s.settimeout(1.0)
            try:
                out = s.recv(65536)
            except (socket.timeout, ConnectionError):
                pass
    except ConnectionError:
        pass
    finally:
        s.close()
    return out


def plain_packet(payload, padlen=4, length=None):
    body = bytes([padlen]) + payload + bytes(padlen)
    return struct.pack(">I", len(body) if length is None else length) + body


class FlipProxy(threading.Thread):
    """Forward one connection to the server, flipping one bit in the
    client's first packet after its NEWKEYS (byte `offset` of it)."""

    def __init__(self, offset):
        super().__init__(daemon=True)
        self.offset = offset
        self.lsock = socket.socket()
        self.lsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.lsock.bind(("127.0.0.1", PROXY_PORT))
        self.lsock.listen(1)
        self.flipped = False

    def run(self):
        c, _ = self.lsock.accept()
        self.lsock.close()
        s = socket.create_connection(("127.0.0.1", PORT))

        def down():
            try:
                while True:
                    d = s.recv(65536)
                    if not d:
                        break
                    c.sendall(d)
            except OSError:
                pass
            finally:
                try:
                    c.shutdown(socket.SHUT_WR)
                except OSError:
                    pass

        threading.Thread(target=down, daemon=True).start()
        buf = b""
        state = "banner"
        try:
            while True:
                d = c.recv(65536)
                if not d:
                    break
                buf += d
                out = b""
                while True:
                    if state == "banner":
                        i = buf.find(b"\n")
                        if i < 0:
                            break
                        out += buf[:i + 1]
                        buf = buf[i + 1:]
                        state = "plain"
                    elif state == "plain":
                        if len(buf) < 4:
                            break
                        n = struct.unpack(">I", buf[:4])[0]
                        if len(buf) < 4 + n:
                            break
                        pkt, buf = buf[:4 + n], buf[4 + n:]
                        out += pkt
                        if pkt[5] == 21:            # NEWKEYS
                            state = "flip"
                    elif state == "flip":
                        if len(buf) <= self.offset:
                            break
                        b = bytearray(buf)
                        b[self.offset] ^= 0x10
                        out += bytes(b)
                        buf = b""
                        self.flipped = True
                        state = "pass"
                    else:
                        out += buf
                        buf = b""
                        break
                if out:
                    s.sendall(out)
        except OSError:
            pass
        finally:
            s.close()
            c.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", nargs="?", default="v32-asm")
    args = parser.parse_args()
    exe = ROOT / args.version / "nano_ssh_server"
    if not exe.exists():
        sys.exit(f"{exe} not found: just build {args.version}")

    subprocess.run(["pkill", "-x", "nano_ssh_server"])
    time.sleep(0.5)
    server = subprocess.Popen([str(exe)], cwd=exe.parent,
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    time.sleep(2)
    count = 0

    def check(name):
        nonlocal count
        assert server.poll() is None, f"server died after: {name}"
        out = login()
        assert "Hello World" in out, f"no login after: {name} ({out!r})"
        count += 1

    try:
        check("start")
        banner = raw(b"", linger=0.5)
        assert banner.startswith(b"SSH-2.0-"), banner
        check("connect and close")

        ident = b"SSH-2.0-probe\r\n"
        for name, data in [
            ("version line of 300 bytes without LF", b"S" * 300),
            ("version line of 300 bytes, then LF", b"S" * 300 + b"\n"),
            ("bare LF", b"\n"),
            ("CR LF only", b"\r\n"),
            ("LF-terminated version line", b"SSH-2.0-probe\n"),
            ("end of file inside the version line", b"SSH-2.0-pro"),
            ("end of file after the version line", ident),
            ("half a packet header", ident + b"\0\0"),
            ("packet length 0", ident + struct.pack(">I", 0)),
            ("packet length 1", ident + struct.pack(">I", 1) + b"\4"),
            ("packet length 4", ident + struct.pack(">I", 4) + b"\4abc"),
            ("packet length 4093", ident + struct.pack(">I", 4093) +
             bytes(4093)),
            ("packet length 0xffffffec", ident + struct.pack(">I", 0xffffffec)
             + bytes(64)),
            ("packet length 0xffffffff", ident + struct.pack(">I", 0xffffffff)
             + bytes(64)),
            ("padding longer than the packet", ident +
             plain_packet(b"\x14" + bytes(40), padlen=4)[:4] + b"\xff" +
             bytes(48)),
            ("padding equal to the packet", ident + struct.pack(">I", 12) +
             b"\x0b" + bytes(11)),
            ("largest packet, all padding", ident + struct.pack(">I", 4092) +
             b"\xff" + bytes(4091)),
            ("KEXINIT of garbage, then end of file", ident +
             plain_packet(b"\x14" + os.urandom(200))),
            ("KEXINIT, then a short KEX_ECDH_INIT", ident +
             plain_packet(b"\x14" + os.urandom(200)) +
             plain_packet(b"\x1e" + bytes(10))),
            ("KEXINIT, then a KEX_ECDH_INIT of garbage, then NEWKEYS", ident +
             plain_packet(b"\x14" + os.urandom(200)) +
             plain_packet(b"\x1e\0\0\0\x41\x04" + os.urandom(64)) +
             plain_packet(b"\x15")),
            ("the same, then 100 bytes of noise as an encrypted packet",
             ident + plain_packet(b"\x14" + os.urandom(200)) +
             plain_packet(b"\x1e\0\0\0\x41\x04" + os.urandom(64)) +
             plain_packet(b"\x15") + os.urandom(100)),
            ("four full-size packets of noise", ident + os.urandom(4 * 4096)),
        ]:
            raw(data, linger=1.5 if b"\x1e" in data else 0.3)
            check(name)

        # one bit flipped in the first encrypted client packet
        # (SERVICE_REQUEST: 44 bytes on the wire): its length word, its
        # payload, its tag
        for off, what in [(1, "length"), (9, "payload"), (40, "tag")]:
            proxy = FlipProxy(off)
            proxy.start()
            out = login(port=PROXY_PORT)
            proxy.join(timeout=30)
            assert proxy.flipped, "proxy never saw the encrypted packet"
            assert "Hello World" not in out, f"tampered {what} accepted"
            check(f"bit flipped in the encrypted {what}")

        for user, pw in [("user", "password12"), ("user", "password1234"),
                         ("user", "Password123"), ("root", "password123"),
                         ("use", "password123"), ("user", "")]:
            out = login(password=pw or "x", user=user)
            assert "Hello World" not in out, f"accepted {user}/{pw}"
            check(f"wrong credentials {user}/{pw}")

        out = login(command="true " + "x" * 3500)
        assert "Hello World" in out, "long exec request"
        check("3.5 KB exec request")
        out = login(options=("-tt",))
        assert "Hello World" in out, "forced pty"
        check("forced pty")
        out = login(options=("-T",))
        assert "Hello World" in out, "no pty"
        check("no pty")
        for i in range(10):
            check(f"login {i}")
    finally:
        server.kill()
        server.wait()
    print(f"{args.version}: PASS - {count} logins served by one process "
          f"between and after the probes")


if __name__ == "__main__":
    main()
