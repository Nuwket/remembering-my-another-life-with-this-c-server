#!/usr/bin/env python3
"""duotone.py - Turn a grayscale photo into a black/amber duotone.

Used to tint the public-domain Trinity fireball photograph so it matches the
project's terminal palette. Derivative of a public-domain work (17 U.S.C. 105),
so redistribution is unrestricted; attribution is kept in docs/assets/CREDITS.md.

Usage: python3 scripts/duotone.py <input.jpg> <output.jpg> [crop_top:bottom]
"""
import sys

from PIL import Image, ImageEnhance, ImageOps


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("usage: duotone.py <input> <output> [crop_top:bottom]", file=sys.stderr)
        return 2

    source, target = sys.argv[1], sys.argv[2]
    image = Image.open(source)

    if len(sys.argv) == 4:
        top, bottom = (int(part) for part in sys.argv[3].split(":", 1))
        height = image.height
        image = image.crop((0, int(height * top), image.width, int(height * bottom)))

    # Flatten to grayscale, then normalize contrast so the fireball reads.
    gray = ImageOps.grayscale(image)
    gray = ImageOps.autocontrast(gray, cutoff=1)
    gray = ImageEnhance.Contrast(gray).enhance(1.15)

    # Duotone ramp: pure black through amber to white-hot.
    ramp = []
    for channel in range(3):
        stops = [(0.00, 0), (0.35, 40), (0.70, 150), (1.00, 255)]
        values = []
        for position in range(256):
            value = 0
            for index in range(len(stops) - 1):
                start_pos, start_val = stops[index]
                end_pos, end_val = stops[index + 1]
                if start_pos <= position <= end_pos:
                    ratio = (position - start_pos) / (end_pos - start_pos)
                    value = int(start_val + ratio * (end_val - start_val))
                    break
            values.append(value)
        ramp.append(values)

    amber = Image.merge("RGB", [gray.point(ramp[channel]) for channel in range(3)])
    amber.save(target, "JPEG", quality=88, optimize=True, progressive=True)
    print("duotone: %s -> %s (%dx%d)" % (source, target, amber.width, amber.height))
    return 0


if __name__ == "__main__":
    sys.exit(main())
