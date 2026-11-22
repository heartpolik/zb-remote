#!/bin/sh
# Render the firmware UI (src/oled.c) on the host: docs/screens/<scene>.png + docs/preview.png.
# Simulates the two-colour SSD1306 (rows 0..15 yellow, the rest blue). Needs cc + python3 with Pillow.
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
cc -w -Istubs -I../../src -o "$OUT/preview" preview.c ../../src/font_terminus12.c ../../src/icons.c
(cd "$OUT" && ./preview)
mkdir -p ../../docs/screens
python3 - "$OUT" ../../docs <<'PY'
import sys, glob, os
from PIL import Image
d, docs = sys.argv[1], sys.argv[2]
YEL, BLU, OFF, BG = (255, 214, 0), (80, 170, 255), (8, 8, 12), (40, 40, 40)
def colour(path):
    im = Image.open(path).convert('L')
    out = Image.new('RGB', im.size, OFF)
    for y in range(im.height):
        for x in range(im.width):
            if im.getpixel((x, y)) == 0:  # PBM ink
                out.putpixel((x, y), YEL if y < 16 else BLU)
    out = out.resize((im.width * 4, im.height * 4), Image.NEAREST)
    framed = Image.new('RGB', (out.width + 16, out.height + 16), BG)
    framed.paste(out, (8, 8))
    return framed
shots = {}
for p in sorted(glob.glob(f'{d}/*.pbm')):
    name = os.path.basename(p)[:-4]
    shots[name] = colour(p)
    shots[name].save(f'{docs}/screens/{name}.png')
    print('wrote', f'docs/screens/{name}.png')
row = [shots[n] for n in ('overview', 'room', 'alert')]
o = Image.new('RGB', (sum(i.width for i in row), row[0].height), BG)
x = 0
for i in row:
    o.paste(i, (x, 0)); x += i.width
o.save(f'{docs}/preview.png')
print('wrote docs/preview.png')
PY
rm -rf "$OUT"
