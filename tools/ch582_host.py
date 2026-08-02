#!/usr/bin/env python3
"""
CH582_VIAL_PAD host tool — custom raw HID commands (README §5.9).

Protocol: 32-byte reports, [0xFE][cmd][params...] (report ID 0).
Commands:
  0xE1  RTC time-set:   [FE E1][y_lo][y_hi][mo][d][h][mi][s]
  0xE2  set screen text:[FE E2][len][ascii...]
  0xE3  read screen text
  0xE4  backlight level:[FE E4][0-255]
  0xE5  diagnostic      (returns RTC time)

Usage:
  python ch582_host.py time "2026-08-02 12:34:56"
  python ch582_host.py text "FinPad22"
  python ch582_host.py get-text
  python ch582_host.py diag
  python ch582_host.py brightness 128

Requires: pip install hidapi
"""
import argparse
import datetime
import sys

try:
    import hid
except ImportError:
    sys.exit("Install hidapi: pip install hidapi")

VID = 0x9273
PID = 0x9157
REPORT_LEN = 32


def open_dev():
    for dev in hid.enumerate(VID, PID):
        if dev.get('usage_page') == 0xFF60:   # raw HID vendor page
            d = hid.device()
            d.open_path(dev['path'])
            return d
    sys.exit(f"CH582 raw HID (VID={VID:04X} PID={PID:04X}) not found — check USB link")


def send(d, data):
    """data = payload after 0xFE; build 32-byte report and read response."""
    payload = [0xFE] + list(data)
    buf = [0] + payload + [0] * (REPORT_LEN - len(payload))   # report ID 0
    d.write(bytes(buf))
    return d.read(REPORT_LEN, timeout_ms=2000)


def cmd_time(d, timestr):
    t = datetime.datetime.strptime(timestr, "%Y-%m-%d %H:%M:%S")
    data = [t.year & 0xFF, (t.year >> 8) & 0xFF, t.month, t.day, t.hour, t.minute, t.second]
    resp = send(d, [0xE1] + data)
    ok = len(resp) >= 2 and resp[0] == 0xE1 and resp[1] == 0x01
    print("RTC set:", "OK" if ok else "FAIL", f"({timestr})")


def cmd_text(d, text):
    b = text.encode('ascii')[:15]
    resp = send(d, [0xE2, len(b)] + list(b))
    print("screen text set:", "OK" if len(resp) and resp[0] == 0xE2 else "FAIL", f"({text})")


def cmd_get_text(d):
    resp = send(d, [0xE3])
    if len(resp) >= 2 and resp[0] == 0xE3:
        n = resp[1]
        print("screen text:", repr(resp[2:2 + n].decode('ascii', 'replace')))
    else:
        print("read screen text: FAIL")


def cmd_diag(d):
    resp = send(d, [0xE5])
    if len(resp) >= 8 and resp[0] == 0xE5:
        y = resp[1] | (resp[2] << 8)
        print(f"RTC time: {y:04d}-{resp[3]:02d}-{resp[4]:02d} "
              f"{resp[5]:02d}:{resp[6]:02d}:{resp[7]:02d}")
    else:
        print("diag: FAIL")


def cmd_brightness(d, level):
    resp = send(d, [0xE4, level & 0xFF])
    print("brightness:", "OK" if len(resp) and resp[0] == 0xE4 else "FAIL", f"({level})")


def main():
    p = argparse.ArgumentParser(description="CH582_VIAL_PAD custom host tool")
    sub = p.add_subparsers(dest='cmd')
    sub.add_parser('time').add_argument('time', help='"YYYY-MM-DD HH:MM:SS"')
    sub.add_parser('text').add_argument('text', help='max 15 ASCII chars')
    sub.add_parser('get-text')
    sub.add_parser('diag')
    sub.add_parser('brightness').add_argument('level', type=int, help='0-255')
    args = p.parse_args()

    if not args.cmd:
        p.print_help()
        return

    d = open_dev()
    try:
        if args.cmd == 'time':
            cmd_time(d, args.time)
        elif args.cmd == 'text':
            cmd_text(d, args.text)
        elif args.cmd == 'get-text':
            cmd_get_text(d)
        elif args.cmd == 'diag':
            cmd_diag(d)
        elif args.cmd == 'brightness':
            cmd_brightness(d, args.level)
    finally:
        d.close()


if __name__ == '__main__':
    main()
