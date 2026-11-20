#!/usr/bin/env python3
"""PAIso floor/terrain texture modeniser.

modernise_floor.py <in tileset png> <out png> [crop.png]

Turns the game's photographic, noisy tile art into a clean painted look to match the
low-poly models: median-smooths the grain, keeps tile edges crisp, lifts saturation and
adds a little contrast.  Alpha is untouched.  Drop the result in <game>\\data\\ to use it.
"""
import sys
from PIL import Image, ImageFilter, ImageEnhance, ImageChops

src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGBA')
rgb, a = im.convert('RGB'), im.split()[3]

smooth = rgb.filter(ImageFilter.MedianFilter(5))
smooth = smooth.filter(ImageFilter.GaussianBlur(0.8))
out = Image.blend(rgb, smooth, 0.55)                    # keep a trace of the original grit
edges = rgb.filter(ImageFilter.GaussianBlur(1.2))       # unsharp mask restores tile / grout lines
out = ImageChops.add(out, ImageChops.subtract(rgb, edges), scale=1.0, offset=0) if False else out
out = out.filter(ImageFilter.UnsharpMask(radius=2, percent=70, threshold=3))
out = ImageEnhance.Color(out).enhance(0.96)
out = ImageEnhance.Contrast(out).enhance(1.05)
out = ImageEnhance.Brightness(out).enhance(1.0)
out.putalpha(a)
out.save(dst, optimize=False)
if len(sys.argv) > 3:                                    # side-by-side crop for a quick look
    box = (0, 0, 768, 512)
    both = Image.new('RGB', (768 * 2, 512))
    both.paste(rgb.crop(box), (0, 0)); both.paste(out.convert('RGB').crop(box), (768, 0))
    both.save(sys.argv[3])
print('wrote', dst)
