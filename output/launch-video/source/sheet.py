import sys, glob, os
from PIL import Image, ImageDraw
d, out, cols = sys.argv[1], sys.argv[2], int(sys.argv[3]); w = int(sys.argv[4]) if len(sys.argv) > 4 else 480
fs = sorted(glob.glob(f"{d}/*.png")); h = w * 9 // 16
S = Image.new("RGB", (cols * w, ((len(fs) + cols - 1) // cols) * (h + 22)), "#222")
dr = ImageDraw.Draw(S)
for i, f in enumerate(fs):
    im = Image.open(f).convert("RGB").resize((w, h)); x, y = (i % cols) * w, (i // cols) * (h + 22)
    S.paste(im, (x, y + 22)); dr.text((x + 6, y + 4), os.path.basename(f), fill="#fff")
S.save(out)
