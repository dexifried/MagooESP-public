#!/usr/bin/env python3
"""Capture the ESP's current framebuffer as a native-resolution PNG.

The ESP must be reachable on the home network or through its MagooESP hotspot.
This reads the existing mirror; it does not photograph the physical screen.
"""
import argparse
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import ProxyHandler, Request, build_opener

from PIL import Image

WIDTH, HEIGHT = 240, 135
DEFAULT_URLS = ("http://magooesp.local", "http://192.168.4.1")


class DisplayUnavailable(Exception):
    """The ESP is reachable but its display cannot currently be captured."""


def capture(base_url, timeout):
    request = Request(base_url.rstrip("/") + "/screen", headers={"Cache-Control": "no-cache"})
    with build_opener(ProxyHandler({})).open(request, timeout=timeout) as response:
        if response.status == 204:
            state = response.headers.get("X-Display-State", "unavailable")
            raise DisplayUnavailable(f"ESP screen cannot be captured while {state}.")
        if response.headers.get_content_type() != "application/octet-stream":
            raise RuntimeError("Unexpected response; this address may not be the ESP.")
        data = response.read()
    if len(data) != WIDTH * HEIGHT * 2:
        raise RuntimeError(f"Incomplete screen data: {len(data)} bytes.")
    pixels = []
    for i in range(0, len(data), 2):
        color = data[i] | (data[i + 1] << 8)  # ESP's little-endian RGB565
        pixels.append((((color >> 11) & 31) * 255 // 31,
                       ((color >> 5) & 63) * 255 // 63,
                       (color & 31) * 255 // 31))
    image = Image.new("RGB", (WIDTH, HEIGHT))
    image.putdata(pixels)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", help="ESP base URL, e.g. http://192.168.4.1")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "artifacts" / "esp-screen.png")
    parser.add_argument("--timeout", type=float, default=4, help="Seconds per connection attempt")
    args = parser.parse_args()
    errors = []
    for base_url in (args.url,) if args.url else DEFAULT_URLS:
        for attempt in range(2):
            try:
                image = capture(base_url, args.timeout)
                args.output.parent.mkdir(parents=True, exist_ok=True)
                image.save(args.output, format="PNG")
                print(f"Captured {WIDTH}x{HEIGHT} framebuffer from {base_url} to {args.output.resolve()}")
                return 0
            except DisplayUnavailable as exc:
                print(str(exc), file=sys.stderr)
                return 2
            except HTTPError as exc:
                error = f"HTTP {exc.code}"
            except (URLError, TimeoutError, OSError, RuntimeError) as exc:
                error = str(exc)
            if attempt == 1:
                errors.append(f"{base_url}: {error}")
    print("Could not capture the ESP screen: " + "; ".join(errors), file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
