"""Pixel-art icon set for zb-remote (24x24 and 32x32 1-bit).
Drawn with PIL primitives at 1x, then hand-tuned. Outputs src/icons.c + preview PNG."""
import sys, math
from PIL import Image, ImageDraw

S = 24
def new(s=S): 
    im = Image.new('1', (s, s), 0); return im, ImageDraw.Draw(im)

ICONS = {}
def icon(name):
    def deco(f): ICONS[name] = f; return f
    return deco

@icon('bell')
def _(d, s=S):
    k = s / 24
    d.pieslice([5*k, 3*k, 19*k, 17*k], 180, 360, fill=1)
    d.rectangle([5*k, 10*k, 19*k, 17*k], fill=1)
    d.polygon([(5*k, 17*k), (3*k, 19*k), (21*k, 19*k), (19*k, 17*k)], fill=1)
    d.ellipse([10*k, 19*k, 14*k, 23*k], fill=1)
    d.rectangle([11*k, 1*k, 13*k, 3*k], fill=1)
    d.line([(8*k, 8*k), (8*k, 15*k)], fill=0, width=max(1, int(k)))

@icon('door')
def _(d, s=S):
    d.rectangle([5, 2, 19, 22], outline=1, width=2)
    d.rectangle([8, 5, 16, 11], outline=1)
    d.rectangle([8, 13, 16, 19], outline=1)
    d.rectangle([15, 12, 16, 13], fill=1)
    d.line([(2, 22), (22, 22)], fill=1, width=1)

@icon('door_open')
def _(d, s=S):
    d.rectangle([5, 2, 19, 22], outline=1, width=1)
    d.polygon([(5, 2), (13, 5), (13, 23), (5, 22)], fill=1)
    d.rectangle([10, 12, 11, 13], fill=0)
    d.line([(15, 8), (21, 6)], fill=1); d.line([(15, 12), (22, 12)], fill=1); d.line([(15, 16), (21, 18)], fill=1)

@icon('lock')
def _(d, s=S):
    d.arc([6, 1, 18, 15], 180, 360, fill=1, width=2)
    d.line([(6, 8), (6, 11)], fill=1, width=2); d.line([(17, 8), (17, 11)], fill=1, width=2)
    d.rounded_rectangle([3, 10, 20, 22], 2, fill=1)
    d.rectangle([11, 14, 12, 18], fill=0)

@icon('unlock')
def _(d, s=S):
    d.arc([6, 1, 18, 15], 180, 330, fill=1, width=2)
    d.line([(6, 8), (6, 11)], fill=1, width=2)
    d.rounded_rectangle([3, 10, 20, 22], 2, fill=1)
    d.rectangle([11, 14, 12, 18], fill=0)

@icon('bulb')
def _(d, s=S):
    d.ellipse([6, 3, 18, 15], fill=1)
    d.rectangle([9, 13, 15, 17], fill=1)
    d.line([(9, 19), (15, 19)], fill=1); d.line([(10, 21), (14, 21)], fill=1)
    for a in range(0, 360, 45):
        if 200 < a < 340: continue
        x, y = 12 + 9.5*math.cos(math.radians(a)), 9 - 9.5*math.sin(math.radians(a))
        x2, y2 = 12 + 11.5*math.cos(math.radians(a)), 9 - 11.5*math.sin(math.radians(a))
        d.line([(x, y), (x2, y2)], fill=1)
    d.point([(10, 7), (10, 8), (11, 6)], fill=0)

@icon('bulb_off')
def _(d, s=S):
    d.ellipse([6, 3, 18, 15], outline=1)
    d.rectangle([9, 14, 15, 17], outline=1)
    d.line([(9, 19), (15, 19)], fill=1); d.line([(10, 21), (14, 21)], fill=1)

@icon('temp')
def _(d, s=S):
    d.rounded_rectangle([9, 1, 15, 16], 3, outline=1, width=1)
    d.ellipse([6, 14, 18, 23], fill=1)
    d.rectangle([11, 6, 13, 16], fill=1)
    for y in (4, 7, 10): d.line([(17, y), (19, y)], fill=1)

@icon('power')
def _(d, s=S):
    d.polygon([(14, 1), (5, 13), (11, 13), (9, 23), (19, 9), (13, 9), (16, 1)], fill=1)

@icon('battery')
def _(d, s=S):
    d.rectangle([2, 7, 20, 17], outline=1, width=1)
    d.rectangle([21, 10, 22, 14], fill=1)
    d.rectangle([4, 9, 18, 15], fill=1)

@icon('home')
def _(d, s=S):
    d.polygon([(12, 2), (1, 12), (4, 12), (4, 22), (20, 22), (20, 12), (23, 12)], fill=1)
    d.rectangle([10, 15, 14, 22], fill=0)

@icon('moon')
def _(d, s=S):
    d.ellipse([3, 3, 21, 21], fill=1)
    d.ellipse([9, 0, 25, 16], fill=0)
    d.point([(18, 17), (21, 13), (15, 21)], fill=1)

@icon('sun')
def _(d, s=S):
    d.ellipse([7, 7, 17, 17], fill=1)
    for a in range(0, 360, 45):
        x, y = 12 + 8*math.cos(math.radians(a)), 12 + 8*math.sin(math.radians(a))
        x2, y2 = 12 + 11*math.cos(math.radians(a)), 12 + 11*math.sin(math.radians(a))
        d.line([(x, y), (x2, y2)], fill=1, width=2)

@icon('flame')
def _(d, s=S):
    d.polygon([(12, 1), (16, 7), (19, 11), (20, 16), (17, 21), (12, 23), (7, 21), (4, 16), (5, 11), (8, 13), (9, 7)], fill=1)
    d.polygon([(12, 12), (15, 16), (14, 20), (12, 21), (10, 20), (9, 17)], fill=0)

@icon('snow')
def _(d, s=S):
    for a in (0, 60, 120):
        x, y = 11*math.cos(math.radians(a)), 11*math.sin(math.radians(a))
        d.line([(12 - x, 12 - y), (12 + x, 12 + y)], fill=1, width=2)
    for a in range(0, 360, 60):
        cx, cy = 12 + 7*math.cos(math.radians(a)), 12 + 7*math.sin(math.radians(a))
        for b in (a + 45, a - 45):
            d.line([(cx, cy), (cx + 3*math.cos(math.radians(b)), cy + 3*math.sin(math.radians(b)))], fill=1)

@icon('drop')
def _(d, s=S):
    d.polygon([(12, 1), (18, 11), (6, 11)], fill=1)
    d.ellipse([5, 8, 19, 22], fill=1)
    d.arc([8, 11, 16, 19], 100, 170, fill=0)

@icon('plug')
def _(d, s=S):
    d.rectangle([8, 2, 9, 7], fill=1); d.rectangle([14, 2, 15, 7], fill=1)
    d.rounded_rectangle([5, 7, 18, 14], 2, fill=1)
    d.polygon([(7, 14), (16, 14), (13, 18), (10, 18)], fill=1)
    d.rectangle([11, 18, 12, 23], fill=1)

@icon('fan')
def _(d, s=S):
    for a in (0, 90, 180, 270):
        r = math.radians(a)
        cx, cy = 12 + 6*math.cos(r), 12 + 6*math.sin(r)
        d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=1)
        d.ellipse([cx - 4 + 3*math.cos(r + 1.6), cy - 4 + 3*math.sin(r + 1.6), cx + 4 + 3*math.cos(r + 1.6), cy + 4 + 3*math.sin(r + 1.6)], fill=0)
    d.ellipse([9, 9, 15, 15], fill=1); d.ellipse([11, 11, 13, 13], fill=0)

@icon('check')
def _(d, s=S):
    d.line([(3, 12), (9, 18), (21, 5)], fill=1, width=3)

@icon('cross')
def _(d, s=S):
    d.line([(4, 4), (20, 20)], fill=1, width=3); d.line([(20, 4), (4, 20)], fill=1, width=3)

@icon('link')
def _(d, s=S):  # zigbee-ish: Z in circle
    d.ellipse([1, 1, 22, 22], outline=1, width=2)
    d.line([(7, 7), (17, 7)], fill=1, width=2); d.line([(17, 7), (7, 17)], fill=1, width=2); d.line([(7, 17), (17, 17)], fill=1, width=2)

@icon('search')
def _(d, s=S):
    d.ellipse([2, 2, 15, 15], outline=1, width=2)
    d.line([(14, 14), (21, 21)], fill=1, width=3)

@icon('star')
def _(d, s=S):
    pts = []
    for i in range(10):
        r = 11 if i % 2 == 0 else 4.5
        a = math.radians(-90 + i * 36)
        pts.append((12 + r*math.cos(a), 12.5 + r*math.sin(a)))
    d.polygon(pts, fill=1)

@icon('cam')
def _(d, s=S):
    d.rounded_rectangle([1, 7, 16, 18], 2, fill=1)
    d.polygon([(17, 10), (23, 7), (23, 18), (17, 15)], fill=1)
    d.ellipse([4, 10, 8, 14], fill=0)

@icon('tv')
def _(d, s=S):
    d.rectangle([1, 4, 22, 18], outline=1, width=2)
    d.line([(8, 21), (16, 21)], fill=1, width=2)
    d.line([(8, 1), (12, 4)], fill=1); d.line([(16, 1), (12, 4)], fill=1)

@icon('button')
def _(d, s=S):
    d.ellipse([2, 2, 21, 21], outline=1, width=2)
    d.ellipse([7, 7, 16, 16], fill=1)

def render(name, s=S):
    im, d = new(s)
    if s == S:
        ICONS[name](d)
    else:  # scale up the 24px art (crisp nearest neighbour)
        im24, d24 = new(S); ICONS[name](d24)
        im = im24.resize((s, s), Image.NEAREST)
    return im

def emit(path):
    names = list(ICONS)
    out = ['/* generated by tools/icons.py */', '#include "icons.h"', '']
    for n in names:
        im = render(n)
        rows = []
        for y in range(S):
            v = 0
            for x in range(S):
                if im.getpixel((x, y)): v |= 1 << x
            rows.append(v)
        out.append('static const uint32_t ic_%s[24] = {%s};' % (n, ', '.join('0x%06x' % r for r in rows)))
    out.append('')
    out.append('const struct icon icons[] = {')
    for n in names: out.append('\t{"%s", ic_%s},' % (n, n))
    out.append('};')
    out.append('const int icons_count = %d;' % len(names))
    open(path, 'w').write('\n'.join(out) + '\n')
    print('icons:', ' '.join(names))

def preview(path, scale=5):
    names = list(ICONS)
    cols = 8
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('L', (cols * 30, rows * 30), 40)
    for i, n in enumerate(names):
        im = render(n).convert('L').point(lambda p: 255 if p else 0)
        sheet.paste(im, ((i % cols) * 30 + 3, (i // cols) * 30 + 3))
    sheet.resize((sheet.width * scale // 2, sheet.height * scale // 2), Image.NEAREST).save(path)

if __name__ == '__main__':
    emit(sys.argv[1])
    if len(sys.argv) > 2: preview(sys.argv[2])
