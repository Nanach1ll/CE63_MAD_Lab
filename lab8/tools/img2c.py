#!/usr/bin/env python3
"""Turn a picture into a C array for ILI9341_Draw_Image_Area().

    ./img2c.py photo.jpg 90 130 my_photo Core/Inc/my_photo.h

Output is RGB565 with the high byte first, which is the order the ILI9341
expects, stored row by row from the top left corner.
"""

import sys
from PIL import Image


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        print("usage: img2c.py <image> <width> <height> <array_name> [output.h]")
        return 1

    source = sys.argv[1]
    width = int(sys.argv[2])
    height = int(sys.argv[3])
    name = sys.argv[4]
    output = sys.argv[5] if len(sys.argv) > 5 else name + ".h"

    image = Image.open(source).convert("RGB")

    # Crop to the target aspect ratio first so the face is not squashed,
    # then scale down to the exact pixel size.
    target_ratio = width / height
    source_ratio = image.width / image.height

    if source_ratio > target_ratio:
        crop_w = int(image.height * target_ratio)
        left = (image.width - crop_w) // 2
        image = image.crop((left, 0, left + crop_w, image.height))
    else:
        crop_h = int(image.width / target_ratio)
        top = (image.height - crop_h) // 2
        image = image.crop((0, top, image.width, top + crop_h))

    image = image.resize((width, height), Image.LANCZOS)

    data = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b = image.getpixel((x, y))
            pixel = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            data.append(pixel >> 8)
            data.append(pixel & 0xFF)

    guard = name.upper() + "_H"
    lines = []
    for i in range(0, len(data), 16):
        chunk = ", ".join("0x%02X" % byte for byte in data[i:i + 16])
        lines.append("  " + chunk + ",")

    with open(output, "w") as handle:
        handle.write("/* Generated from %s by tools/img2c.py */\n" % source)
        handle.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
        handle.write("#include <stdint.h>\n\n")
        handle.write("#define %s_WIDTH   %d\n" % (name.upper(), width))
        handle.write("#define %s_HEIGHT  %d\n\n" % (name.upper(), height))
        handle.write("const uint8_t %s[%d * %d * 2] = {\n" % (name, width, height))
        handle.write("\n".join(lines))
        handle.write("\n};\n\n#endif /* %s */\n" % guard)

    print("wrote %s: %d x %d, %d bytes of image data" % (output, width, height, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
