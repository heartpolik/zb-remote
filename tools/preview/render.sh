#!/bin/sh
# Render the firmware UI (src/oled.c) on the host into docs/preview.png. Needs cc + python3 with Pillow.
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d)
cc -w -Istubs -I../../src -o "$OUT/preview" preview.c ../../src/font_terminus12.c ../../src/icons.c
(cd "$OUT" && ./preview)
python3 - "$OUT" ../../docs/preview.png <<'PY'
import sys
from PIL import Image
d, out = sys.argv[1], sys.argv[2]
ims = [Image.open(f'{d}/{n}.pbm').convert('L').point(lambda p: 255 - p) for n in ['home_l', 'alert_l', 'home_p', 'alert_p']]
ims = [i.resize((i.width * 4, i.height * 4), Image.NEAREST) for i in ims]
a, b, c, e = ims
o = Image.new('L', (a.width + c.width + e.width + 60, max(c.height, a.height * 2 + 20) + 20), 60)
o.paste(a, (10, 10)); o.paste(b, (10, a.height + 30)); o.paste(c, (a.width + 30, 10)); o.paste(e, (a.width + c.width + 50, 10))
o.save(out)
print('wrote', out)
PY
rm -rf "$OUT"
