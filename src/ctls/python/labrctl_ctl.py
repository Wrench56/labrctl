import socket
import struct
from collections.abc import Callable
from enum import IntEnum

__all__ = [
    "LABRCTL_PORT",
    "Op",
    "Packet",
    "LabrctlClient",
    "LabrctlError",
    "Timeout",
]

LABRCTL_PORT = 19552
LABRCTL_MAGIC = 0x4C
LABRCTL_VERSION = 0x00
_HDR = (LABRCTL_MAGIC << 8) | LABRCTL_VERSION

_PKT = struct.Struct(">H B 2s B 8s 4s")
PACKET_SZ = _PKT.size
assert PACKET_SZ == 18, f"packet is {PACKET_SZ} bytes, expected 18"


class Op(IntEnum):
    NOP = 0x00
    ACK = 0x01
    RESEQ = 0x02
    STORE = 0x03
    FETCH = 0x04
    SPAWN = 0x05
    KILL = 0x06
    QUIET_SET = 0x07
    QUIET_RESTORE = 0x08
    GPUSIG0 = 0x80
    GPUSIG1 = 0x81
    GPUSIG2 = 0x82
    GPUSIG3 = 0x83

    _USERSPACE = 0x80

    @property
    def userspace(self) -> bool:
        return bool(self.value & self._USERSPACE)


class Packet:
    __slots__ = ("op", "seq", "arg", "data")

    def __init__(self, op: int, seq: int, arg: bytes = b"\x00\x00", data: bytes = b"\x00" * 8):
        self.op = int(op) & 0xFF
        self.seq = int(seq) & 0xFF
        self.arg = (bytes(arg) + b"\x00\x00")[:2]
        self.data = (bytes(data) + b"\x00" * 8)[:8]

    def encode(self) -> bytes:
        return _PKT.pack(_HDR, self.op, self.arg, self.seq, self.data, b"\x00" * 4)

    @classmethod
    def decode(cls, buf: bytes) -> Packet:
        if len(buf) < PACKET_SZ:
            raise ValueError(f"short packet: {len(buf)} bytes")
        hdr, op, arg, seq, data, _rsvd = _PKT.unpack(buf[:PACKET_SZ])
        if hdr != _HDR:
            raise ValueError(f"bad magic/version: {hdr:#06x}")
        return cls(op, seq, arg, data)

    def __repr__(self) -> str:
        try:
            name = Op(self.op).name
        except ValueError:
            name = f"{self.op:#04x}"
        return f"Packet(op={name}, seq={self.seq}, arg={self.arg.hex()}, data={self.data.hex()})"


class LabrctlError(Exception):
    pass


class Timeout(LabrctlError):
    pass


class LabrctlClient:
    def __init__(
        self, server: str, port: int = LABRCTL_PORT, timeout: float = 0.2, retries: int = 4
    ):
        self.addr = (server, port)
        self.timeout = timeout
        self.retries = retries
        self.seq = 1
        self.on_log: Callable[[str], None] | None = None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock.settimeout(timeout)
            self.sock.connect(self.addr)
        except OSError:
            self.sock.close()
            raise

    def __enter__(self) -> LabrctlClient:
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        self.sock.close()

    def _log(self, msg: str) -> None:
        if self.on_log:
            self.on_log(msg)

    def _exchange(self, op: int, seq: int, arg: bytes, data: bytes) -> Packet | None:
        pkt = Packet(op, seq, arg, data)
        self._log(f"-> {pkt}")
        self.sock.send(pkt.encode())
        try:
            reply = Packet.decode(self.sock.recv(2048))
        except TimeoutError:
            return None
        except ValueError as e:
            raise LabrctlError(str(e)) from e
        self._log(f"<- {reply}")
        return reply

    def _command_reply(self, op: int, arg: bytes, data: bytes) -> Packet:
        for attempt in range(1, self.retries + 1):
            reply = self._exchange(op, self.seq, arg, data)
            if reply is None:
                self._log(f"   timeout {attempt}/{self.retries}, retransmit seq={self.seq}")
                continue
            if reply.op != Op.ACK:
                raise LabrctlError(f"reply op {reply.op:#04x} is not ACK {int(Op.ACK):#04x}")
            if reply.seq != (self.seq & 0xFF):
                self._log(f"   stale ack seq={reply.seq} (want {self.seq & 0xFF}), ignoring")
                continue

            self.seq = (self.seq + 1) & 0xFF
            return reply

        raise Timeout(f"no ACK for seq={self.seq} after {self.retries} tries")

    def command(self, op: int, arg: bytes = b"\x00\x00", data: bytes = b"\x00" * 8) -> int:
        seq = self.seq
        self._command_reply(op, arg, data)
        return seq

    def resync(self, retries: int = 5) -> int:
        old_timeout = self.sock.gettimeout()
        self.sock.settimeout(0.05)

        try:
            for _ in range(retries):
                reply = self._exchange(Op.RESEQ, 0, b"\x00\x00", b"\x00" * 8)
                if reply and reply.op == Op.ACK and reply.seq == 0:
                    self.seq = 1
                    return 0
        finally:
            self.sock.settimeout(old_timeout)

        self._log("resync failed")
        raise Timeout("resync failed")

    def fetch(self, reg: int, off: int = 0) -> int:
        reply = self._command_reply(
            Op.FETCH,
            bytes([reg & 0xFF, off & 0xFF]),
            b"\x00" * 8,
        )
        return int.from_bytes(reply.data, "little")

    def __getattr__(self, name: str):
        try:
            op = Op[name.upper()]
        except KeyError:
            raise AttributeError(name) from None

        def call(arg: bytes = b"\x00\x00", data: bytes = b"\x00" * 8) -> int:
            return self.command(op, arg, data)

        call.__name__ = name
        return call
