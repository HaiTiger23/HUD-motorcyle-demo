from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

OUT = "/Users/macbook/Project/HUD-motorcyle-demo/output/pdf/viet-nam-hub-danh-sach-man-hinh.pdf"
FONT = "/Library/Fonts/Arial Unicode.ttf"
pdfmetrics.registerFont(TTFont("ArialUnicode", FONT))

screens = [
    ("U01", "Đăng ký", "Tạo tài khoản mới bằng email và mật khẩu."),
    ("U02", "Đăng nhập", "Đăng nhập để tiếp tục với địa phương đã chọn."),
    ("U03", "Chọn tỉnh", "Tìm kiếm và chọn tỉnh triển khai; Bắc Ninh là tỉnh demo."),
    ("U04", "Chọn xã", "Chọn xã Sơn Động rồi đi vào trải nghiệm."),
    ("U05", "Trang chủ - bản đồ", "Khám phá địa điểm trên bản đồ, tìm kiếm và lọc theo loại."),
    ("U06", "Trang chủ - danh sách", "Biến thể danh sách của các địa điểm trong xã."),
    ("U07", "Trang chủ - bộ lọc", "Trạng thái tìm kiếm/lọc địa điểm và kết quả tương ứng."),
    ("U08", "Chi tiết địa điểm", "Thông tin, địa chỉ, liên hệ và thao tác xem trên bản đồ."),
    ("U09", "Tin tức", "Danh sách tin, tìm kiếm, lọc theo chuyên mục và tải thêm."),
    ("U10", "Chi tiết tin", "Nội dung bài viết, tin liên quan và chia sẻ liên kết."),
    ("U11", "Cá nhân", "Trang tài khoản và các thiết lập của người dùng."),
    ("U12", "Thông tin tài khoản", "Xem email và thông tin tài khoản."),
    ("U12P", "Đổi mật khẩu", "Cập nhật mật khẩu hiện tại và mật khẩu mới."),
    ("U13", "Góp ý", "Gửi góp ý, đề nghị cập nhật hoặc báo lỗi ứng dụng."),
    ("U14", "Giao diện", "Chọn giao diện theo hệ thống, sáng hoặc tối."),
    ("U15", "Cài đặt ứng dụng", "Hướng dẫn thêm Việt Nam Hub vào màn hình chính."),
]

W, H = A4
M = 18 * mm
RED = colors.HexColor("#C62828")
INK = colors.HexColor("#1F2937")
MUTED = colors.HexColor("#667085")
LINE = colors.HexColor("#E4E7EC")
SOFT = colors.HexColor("#FFF5F5")

def text(c, s, x, y, size, color=INK):
    c.setFont("ArialUnicode", size)
    c.setFillColor(color)
    c.drawString(x, y, s)

def wrap(c, s, max_width, size):
    c.setFont("ArialUnicode", size)
    words, lines, line = s.split(), [], ""
    for word in words:
        candidate = (line + " " + word).strip()
        if c.stringWidth(candidate, "ArialUnicode", size) <= max_width:
            line = candidate
        else:
            lines.append(line); line = word
    if line: lines.append(line)
    return lines

def header(c, page, subtitle):
    c.setFillColor(RED); c.rect(0, H - 9*mm, W, 9*mm, stroke=0, fill=1)
    text(c, "Việt Nam Hub", M, H - 5.8*mm, 8, colors.white)
    text(c, subtitle, M, H - 19*mm, 9, MUTED)
    text(c, f"{page:02d}", W - M - 9*mm, 11*mm, 8, MUTED)

c = canvas.Canvas(OUT, pagesize=A4)
c.setTitle("Việt Nam Hub - Danh sách màn hình")
c.setAuthor("Codex")

# Cover
c.setFillColor(RED); c.rect(0, H-72*mm, W, 72*mm, stroke=0, fill=1)
text(c, "Việt Nam Hub", M, H-31*mm, 18, colors.white)
text(c, "Danh sách các màn hình", M, H-44*mm, 25, colors.white)
text(c, "Tài liệu tham chiếu nhanh cho nhóm xem prototype", M, H-55*mm, 11, colors.white)
c.setFillColor(SOFT); c.roundRect(M, H-133*mm, W-2*M, 43*mm, 4*mm, stroke=0, fill=1)
text(c, "16 màn hình / trạng thái chính", M+10*mm, H-108*mm, 17, RED)
text(c, "Phạm vi: luồng tài khoản, lựa chọn địa phương, địa điểm, tin tức và cá nhân.", M+10*mm, H-119*mm, 9.5, INK)
text(c, "Nguồn: prototype HTML được cung cấp.", M+10*mm, H-127*mm, 8.5, MUTED)
text(c, "Ghi chú: U05-U07 là ba trạng thái của luồng Trang chủ.", M, 25*mm, 8.5, MUTED)
c.showPage()

for page in range(2):
    header(c, page+2, "Danh sách màn hình prototype")
    subset = screens[page*8:(page+1)*8]
    y = H - 32*mm
    for code, title, desc in subset:
        c.setFillColor(colors.white); c.roundRect(M, y-22*mm, W-2*M, 19*mm, 3*mm, stroke=0, fill=1)
        c.setStrokeColor(LINE); c.roundRect(M, y-22*mm, W-2*M, 19*mm, 3*mm, stroke=1, fill=0)
        c.setFillColor(SOFT); c.roundRect(M+5*mm, y-16.5*mm, 19*mm, 8*mm, 2*mm, stroke=0, fill=1)
        text(c, code, M+8*mm, y-13.7*mm, 8.5, RED)
        text(c, title, M+29*mm, y-11*mm, 11, INK)
        lines = wrap(c, desc, W-M-(M+29*mm)-7*mm, 8.3)
        for i, line in enumerate(lines[:2]): text(c, line, M+29*mm, y-(16+i*3.6)*mm, 8.3, MUTED)
        y -= 24*mm
    c.showPage()

c.save()
print(OUT)
