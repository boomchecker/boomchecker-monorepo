"""Wire protocol: declarative spec, codec and high-level device client."""

from .client import DeviceClient, StreamHandle
from .codec import ProtocolError, StreamHeader, encode_command, pack_header, parse_header

__all__ = [
    "DeviceClient",
    "StreamHandle",
    "StreamHeader",
    "ProtocolError",
    "encode_command",
    "pack_header",
    "parse_header",
]
