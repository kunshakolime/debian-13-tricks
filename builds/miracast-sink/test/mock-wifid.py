#!/usr/bin/env python3
"""Headless mock of a Wi-Fi Display *source*.

Acts as both the miracle-wifid D-Bus service (so miracast-sink has a link
and a peer to talk to) and the WFD source's RTSP server + RTP streamer.

The sink dials OUT to 127.0.0.1:7236 (RTSP, TCP). The sink's own RTP
listener is UDP 7236, so after the handshake we stream RTP/MPEG-TS H264
to 127.0.0.1:7236 over UDP.
"""

import dbus
import dbus.service
import dbus.mainloop.glib
import socket
import subprocess
import threading
import time

from gi.repository import GLib

BUS_NAME = "org.freedesktop.miracle.wifi"
MGR_PATH = "/org/freedesktop/miracle/wifi"
LINK_PATH = "/org/freedesktop/miracle/wifi/link/0"
PEER_PATH = "/org/freedesktop/miracle/wifi/peer/0"
LINK_IFACE = "org.freedesktop.miracle.wifi.Link"
PEER_IFACE = "org.freedesktop.miracle.wifi.Peer"
PROPS_IFACE = "org.freedesktop.DBus.Properties"
OM_IFACE = "org.freedesktop.DBus.ObjectManager"

RTSP_HOST = "127.0.0.1"
RTSP_PORT = 7236
RTP_PORT = 7236


def log(msg):
    print(f"[mock] {msg}", flush=True)


_rtsp_buf = b""

def read_message(conn):
    """Read one RTSP message, preserving leftover data across calls."""
    global _rtsp_buf
    while b"\r\n\r\n" not in _rtsp_buf:
        chunk = conn.recv(4096)
        if not chunk:
            return None
        _rtsp_buf += chunk
    header, _, rest = _rtsp_buf.partition(b"\r\n\r\n")
    content_length = 0
    for line in header.decode("utf-8", "replace").split("\r\n"):
        if line.lower().startswith("content-length:"):
            content_length = int(line.split(":", 1)[1].strip())
    body = rest
    while len(body) < content_length:
        chunk = conn.recv(4096)
        if not chunk:
            break
        body += chunk
    _rtsp_buf = body[content_length:]
    return (header + b"\r\n\r\n" + body[:content_length]).decode(
        "utf-8", "replace")


def send(conn, text):
    conn.sendall(text.encode("utf-8"))


class WifiManager(dbus.service.Object):
    def __init__(self, bus, link, peer):
        super().__init__(bus, MGR_PATH)
        self.link = link
        self.peer = peer

    @dbus.service.method(OM_IFACE, out_signature="(a{oa{sa{sv}}})")
    def GetManagedObjects(self):
        # Return the managed objects as a tuple containing a dict
        # dbus-python will marshal this correctly as (a{oa{sa{sv}}})
        return ({
            LINK_PATH: {
                LINK_IFACE: dict(self.link.props()),
            },
            PEER_PATH: {
                PEER_IFACE: dict(self.peer.props()),
            },
        },)

    @dbus.service.signal(OM_IFACE, signature="oa{sa{sv}}")
    def InterfacesAdded(self, path, ifaces):
        pass


class Link(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, LINK_PATH)
        self._props = {
            "InterfaceName": "wlan0",
            "WfdSubelements": "000600111c4400c8",
            "FriendlyName": "mock-source",
            "P2PScanning": False,
            "Managed": True,
            "OperatingChannel": "6",
            "OperatingFrequency": "2437",
            "GOAddress": "02:00:00:00:00:00",
        }

    def props(self):
        return {k: v for k, v in self._props.items()}

    @dbus.service.method(PROPS_IFACE, in_signature="ss", out_signature="v")
    def Get(self, iface, prop):
        if prop not in self._props:
            raise dbus.exceptions.DBusException(
                f"no property {prop}", name="org.freedesktop.DBus.Error.UnknownProperty")
        return self._props[prop]

    @dbus.service.method(PROPS_IFACE, in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        return self.props()

    @dbus.service.method(PROPS_IFACE, in_signature="ssv")
    def Set(self, iface, prop, value):
        self._props[prop] = value

    @dbus.service.signal(PROPS_IFACE, signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass


class Peer(dbus.service.Object):
    def __init__(self, bus, on_connect):
        super().__init__(bus, PEER_PATH)
        self.on_connect = on_connect
        self._props = {
            "FriendlyName": "mock-phone",
            "PeerAddress": "02:00:00:00:00:01",
            "Connected": False,
            "RemoteAddress": "",
        }

    def props(self):
        return {k: v for k, v in self._props.items()}

    @dbus.service.method(PROPS_IFACE, in_signature="ss", out_signature="v")
    def Get(self, iface, prop):
        if prop not in self._props:
            raise dbus.exceptions.DBusException(
                f"no property {prop}", name="org.freedesktop.DBus.Error.UnknownProperty")
        return self._props[prop]

    @dbus.service.method(PROPS_IFACE, in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        return self.props()

    @dbus.service.method(PROPS_IFACE, in_signature="ssv")
    def Set(self, iface, prop, value):
        self._props[prop] = value

    @dbus.service.method(PEER_IFACE, in_signature="ss")
    def Connect(self, prov, pin):
        log(f"Connect called ({prov}/{pin}); marking connected")
        self._props["Connected"] = True
        self._props["RemoteAddress"] = RTSP_HOST
        self.PropertiesChanged(
            PEER_IFACE,
            {"Connected": True, "RemoteAddress": RTSP_HOST},
            [],
        )
        if self.on_connect:
            self.on_connect()

    @dbus.service.method(PEER_IFACE)
    def Disconnect(self):
        log("Disconnect called")
        self._props["Connected"] = False
        self.PropertiesChanged(PEER_IFACE, {"Connected": False}, [])

    @dbus.service.signal(PEER_IFACE, signature="ss")
    def GoNegRequest(self, prov, pin):
        pass

    @dbus.service.signal(PROPS_IFACE, signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass


class RtsSink:
    """Raw RTSP connection to the sink; drives the WFD handshake as source."""

    def __init__(self, conn):
        self.conn = conn

    def wait_for(self, expect_prefix, label):
        deadline = time.time() + 10
        while time.time() < deadline:
            msg = read_message(self.conn)
            if msg is None:
                raise RuntimeError(f"{label}: connection closed")
            first = msg.split("\r\n", 1)[0]
            log(f"RTSP << {first}")
            if first.startswith(expect_prefix):
                return msg
            log(f"RTSP (ignored) {first}")
        raise RuntimeError(f"{label}: timed out waiting for {expect_prefix}")

    def m1_options(self):
        send(self.conn,
             "OPTIONS rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
             "CSeq: 2\r\n"
             "Require: org.wfa.wfd1.0\r\n\r\n")
        log("RTSP >> OPTIONS")
        self.wait_for("RTSP/1.0 200", "OPTIONS reply")
        # sink then sends its own OPTIONS request
        self.wait_for("OPTIONS", "sink OPTIONS")
        send(self.conn, "RTSP/1.0 200 OK\r\nCSeq: 2\r\n\r\n")
        log("RTSP >> 200 (to sink OPTIONS)")

    def m3_get_parameter(self):
        body = ("wfd_content_protection\r\n"
                "wfd_video_formats\r\n"
                "wfd_audio_codecs\r\n"
                "wfd_client_rtp_ports\r\n")
        send(self.conn,
             "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
             f"CSeq: 3\r\n"
             f"Content-Type: text/parameters\r\n"
             f"Content-Length: {len(body)}\r\n\r\n"
             f"{body}")
        log("RTSP >> GET_PARAMETER")
        self.wait_for("RTSP/1.0 200", "GET_PARAMETER reply")

    def m4_set_parameter(self):
        body = ("wfd_presentation_URL: rtsp://127.0.0.1/wfd1.0 none\r\n"
                "wfd_video_formats: 00 00 03 10 0001ffff 1fffffff 00001fff 00 0000 0000 10 none none\r\n"
                "wfd_trigger_method: SETUP\r\n")
        send(self.conn,
             "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
             f"CSeq: 4\r\n"
             f"Content-Type: text/parameters\r\n"
             f"Content-Length: {len(body)}\r\n\r\n"
             f"{body}")
        log("RTSP >> SET_PARAMETER")
        self.wait_for("RTSP/1.0 200", "SET_PARAMETER reply")
        # sink then sends SETUP
        self.wait_for("SETUP", "sink SETUP")
        send(self.conn,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: 5\r\n"
             "Session: 00000001\r\n\r\n")
        log("RTSP >> 200 (SETUP reply, Session granted)")

    def m6_play(self):
        self.wait_for("PLAY", "sink PLAY")
        send(self.conn, "RTSP/1.0 200 OK\r\nCSeq: 6\r\n\r\n")
        log("RTSP >> 200 (PLAY reply)")


def run_rtsp(stream_duration=20):
    global _rtsp_buf
    _rtsp_buf = b""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((RTSP_HOST, RTSP_PORT))
    srv.listen(1)
    log(f"RTSP server listening on {RTSP_HOST}:{RTSP_PORT}")
    conn, addr = srv.accept()
    log(f"RTSP connection from {addr}")
    conn.settimeout(15)
    try:
        sink = RtsSink(conn)
        sink.m1_options()
        sink.m3_get_parameter()
        sink.m4_set_parameter()
        sink.m6_play()
        log("handshake complete; waiting for pipeline to initialize")
        time.sleep(0.5)
        log("handshake complete; starting RTP stream")
        gst = subprocess.Popen([
            "gst-launch-1.0", "-q",
            "videotestsrc", "is-live=true", "num-buffers=-1",
            "!", "x264enc", "tune=zerolatency", "speed-preset=ultrafast", "bframes=0", "key-int-max=30",
            "!", "h264parse", "config-interval=1",
            "!", "mpegtsmux",
            "!", "rtpmp2tpay",
            "!", "udpsink", f"host={RTSP_HOST}", f"port={RTP_PORT}",
        ])
        log(f"streaming for {stream_duration}s")
        time.sleep(stream_duration)
        gst.terminate()
        gst.wait()
        log("stream ended")
    except Exception as e:  # noqa: BLE001
        log(f"RTSP error: {e}")
    finally:
        conn.close()


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--stream-duration", type=int, default=20)
    args = parser.parse_args()

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    bus_name = dbus.service.BusName(BUS_NAME, bus, do_not_queue=True)
    assert bus_name
    log(f"owned {BUS_NAME}")

    link = Link(bus)
    peer = Peer(bus, on_connect=None)
    mgr = WifiManager(bus, link, peer)
    log("objects registered")

    def emit_go_neg():
        if not peer._props["Connected"]:
            log("emitting GoNegRequest")
            peer.GoNegRequest("none", "")
            return True
        return False

    GLib.timeout_add(2000, emit_go_neg)

    threading.Thread(target=run_rtsp, daemon=True,
                     kwargs={"stream_duration": args.stream_duration}).start()

    GLib.MainLoop().run()


if __name__ == "__main__":
    main()