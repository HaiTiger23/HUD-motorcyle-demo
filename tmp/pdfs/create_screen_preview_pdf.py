from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader
import os

ROOT = "/Users/macbook/Project/HUD-motorcyle-demo"
OUT = os.path.join(ROOT, "output/pdf/viet-nam-hub-preview-cac-man-hinh.pdf")
IMG = os.path.join(ROOT, "tmp/pdfs")
pdfmetrics.registerFont(TTFont("ArialUnicode", "/Library/Fonts/Arial Unicode.ttf"))

items = [
    ("U01", "Đăng ký"), ("U02", "Đăng nhập"), ("U03", "Chọn tỉnh"), ("U04", "Chọn xã"),
    ("U05", "Trang chủ - bản đồ"), ("U06", "Trang chủ - không có kết quả"),
    ("U07", "Trang chủ - lọc theo loại"), ("U08", "Chi tiết địa điểm"),
    ("U09", "Tin tức"), ("U10", "Chi tiết tin"), ("U11", "Cá nhân"),
    ("U12", "Thông tin tài khoản"), ("U12P", "Đổi mật khẩu"), ("U13", "Góp ý"),
    ("U14", "Giao diện"), ("U15", "Cài đặt ứng dụng"),
]
W, H = A4
RED, INK, MUTED = colors.HexColor("#C62828"), colors.HexColor("#1F2937"), colors.HexColor("#667085")

def txt(c, value, x, y, size, color=INK):
    c.setFont("ArialUnicode", size); c.setFillColor(color); c.drawString(x, y, value)

def chrome(c, x, y, w, h):
    c.setFillColor(colors.HexColor("#E4E7EC")); c.roundRect(x-2.3*mm, y-2.3*mm, w+4.6*mm, h+4.6*mm, 4*mm, stroke=0, fill=1)
    c.setFillColor(colors.white); c.roundRect(x-1.3*mm, y-1.3*mm, w+2.6*mm, h+2.6*mm, 3*mm, stroke=0, fill=1)

c = canvas.Canvas(OUT, pagesize=A4)
c.setTitle("Việt Nam Hub - Preview các màn hình")
c.setAuthor("Codex")

# Cover
c.setFillColor(RED); c.rect(0, H-80*mm, W, 80*mm, stroke=0, fill=1)
txt(c, "Việt Nam Hub", 20*mm, H-31*mm, 20, colors.white)
txt(c, "Preview các màn hình", 20*mm, H-47*mm, 27, colors.white)
txt(c, "Prototype - bấm thử luồng chính", 20*mm, H-59*mm, 11, colors.white)
txt(c, "16 màn hình / trạng thái chính", 20*mm, H-112*mm, 18, RED)
txt(c, "Mỗi preview được render trực tiếp từ prototype HTML đã cung cấp.", 20*mm, H-124*mm, 10, MUTED)
txt(c, "Ghi chú: U05-U07 là các trạng thái khác nhau của luồng Trang chủ.", 20*mm, 22*mm, 8.5, MUTED)
c.showPage()

for page in range(8):
    c.setFillColor(RED); c.rect(0, H-8*mm, W, 8*mm, stroke=0, fill=1)
    txt(c, "Việt Nam Hub - Preview màn hình", 16*mm, H-5.3*mm, 8, colors.white)
    for slot in range(2):
        i = page*2 + slot
        code, title = items[i]
        image_path = os.path.join(IMG, f"screen-{i+1:02d}.png")
        x = (W-56.4*mm)/2
        y = H - (slot*136*mm + 27*mm + 122*mm)
        txt(c, f"{code}  {title}", x, y+125*mm, 11, INK)
        txt(c, f"Màn hình {i+1:02d}/16", x, y+119*mm, 8, MUTED)
        chrome(c, x, y, 56.4*mm, 122*mm)
        c.drawImage(ImageReader(image_path), x, y, width=56.4*mm, height=122*mm, preserveAspectRatio=True, mask='auto')
    txt(c, f"{page+2:02d}", W-22*mm, 10*mm, 8, MUTED)
    c.showPage()

c.save()
print(OUT)
