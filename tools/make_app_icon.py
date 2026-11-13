from PIL import Image, ImageDraw, ImageFilter
import math
S = 2048
k = S / 1024
def P(x, y): return (x * k, y * k)
def W(v): return int(v * k)

# Background: deep night-blue diagonal gradient
bg = Image.new("RGB", (S, S)); px = bg.load()
top, bottom = (7, 17, 34), (9, 52, 68)
for y in range(S):
    for x in range(0, S, 4):
        t = min(1, max(0, (x * 0.3 + y * 0.7) / S))
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for dx in range(4): px[x + dx, y] = c
img = bg.convert("RGBA")

def layer(): return Image.new("RGBA", (S, S), (0, 0, 0, 0))

# Glow around the HUD
g = layer(); ImageDraw.Draw(g).rounded_rectangle([*P(200, 190), *P(824, 640)], radius=W(100), fill=(65, 218, 245, 110))
img = Image.alpha_composite(img, g.filter(ImageFilter.GaussianBlur(W(70))))

# Handlebar: smooth curve close under the screen, short clamp, rounded grips
def bez(p0, p1, p2, n=60):
    return [((1-t)**2*p0[0] + 2*(1-t)*t*p1[0] + t*t*p2[0], (1-t)**2*p0[1] + 2*(1-t)*t*p1[1] + t*t*p2[1]) for t in [i/n for i in range(n+1)]]
h = layer(); hd = ImageDraw.Draw(h)
metal = (196, 208, 222, 255)
left, mid, right = (150, 790), (512, 660), (874, 790)
curve = bez(left, mid, right)
hd.line([P(*p) for p in curve], fill=metal, width=W(40), joint="curve")
hd.line([P(512, 600), P(512, 712)], fill=metal, width=W(56))                 # short stem
hd.rounded_rectangle([*P(468, 690), *P(556, 742)], radius=W(18), fill=(150, 165, 182, 255))
def capsule(a, b, width, colour):
    hd.line([P(*a), P(*b)], fill=colour, width=W(width))
    for (x, y) in (a, b):
        r = W(width) / 2; cx, cy = P(x, y); hd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=colour)
for end_pt, inner in ((left, curve[8]), (right, curve[-9])):
    dx, dy = end_pt[0] - inner[0], end_pt[1] - inner[1]
    n = math.hypot(dx, dy); ux, uy = dx / n, dy / n
    capsule((end_pt[0] - ux * 70, end_pt[1] - uy * 70), (end_pt[0] + ux * 25, end_pt[1] + uy * 25), 62, (62, 74, 88, 255))
img = Image.alpha_composite(img, h)

# HUD screen: cyan rim, dark glass
d = ImageDraw.Draw(img)
d.rounded_rectangle([*P(222, 210), *P(802, 630)], radius=W(78), fill=(65, 218, 245, 255))
d.rounded_rectangle([*P(242, 230), *P(782, 610)], radius=W(62), fill=(8, 16, 29, 255))

# Turn-right arrow, road shaped, white with cyan glow
a = layer(); ad = ImageDraw.Draw(a)
white = (255, 255, 255, 255)
stem = [P(430, 560), P(430, 425), P(560, 425)]
ad.line(stem, fill=white, width=W(62), joint="curve")
r = W(31); x, y = P(430, 560); ad.ellipse([x - r, y - r, x + r, y + r], fill=white)
ad.polygon([P(545, 335), P(650, 425), P(545, 515)], fill=white)
a = a.filter(ImageFilter.GaussianBlur(1.2))   # soften polygon edge before downsampling
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
glow.paste((65, 218, 245, 255), mask=a.split()[3])
img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(W(22))))
img = Image.alpha_composite(img, a)

# "Riding" status bar on the screen
d = ImageDraw.Draw(img)
d.rounded_rectangle([*P(300, 268), *P(380, 296)], radius=W(14), fill=(69, 212, 131, 255))
d.rounded_rectangle([*P(395, 268), *P(445, 296)], radius=W(14), fill=(65, 218, 245, 110))

img.convert("RGB").resize((1024, 1024), Image.LANCZOS).save("AppIcon-1024.png")
img.convert("RGB").resize((180, 180), Image.LANCZOS).save("AppIcon-180-preview.png")
print("ok")
