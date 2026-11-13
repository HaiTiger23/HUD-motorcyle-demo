# HUD Connect — phim giới thiệu (~45 s, 1920×1080, 60 fps)

Dựa trên skill product-launch-motion v2 (một hình khối không bao giờ cắt, lò xo, con trỏ/ngón tay
điều khiển, nhạc + SFX đặt theo nhịp) kết hợp đồng bộ lời thoại theo từng từ (phần legacy của skill),
vì phim có giọng đọc ElevenLabs.

## Nhận diện
- Nền xanh đêm #07141C → #0B2433, xanh cyan thương hiệu #3DDCF5, xanh lá #4ADE80, chữ trắng.
- Một font UI (Be Vietnam Pro — đủ dấu tiếng Việt).
- Khối chính là "màn hình HUD" 4:3 bo góc như icon app; mọi cảnh là khối này đổi kích thước, bo góc, màu.

## Lời thoại (tiếng Việt, ElevenLabs eleven_v4)
1. Đi xe máy… mà cứ phải cúi xuống nhìn điện thoại?
2. Giới thiệu HUD Connect. Màn hình nhỏ trên tay lái, kết nối thẳng với iPhone.
3. Chỉ đường từng ngã rẽ. Bản đồ xoay theo hướng đi, tốc độ luôn trong tầm mắt.
4. Muốn đi đâu? Chỉ cần nói.
5. Tin nhắn hiện gọn gàng, không phải rút máy.
6. Sắp mưa? HUD báo trước khi bạn lên đường.
7. Mặt đồng hồ, ảnh nền, theo cách của bạn.
8. Rời xe, HUD tự khóa và nhớ chỗ đỗ.
9. HUD Connect. Mắt nhìn đường, mọi thứ trong tầm mắt.

## Cảnh (khối chính biến hình liên tục)
| # | Khối trở thành | Hành động chính |
|---|---|---|
| 1 | Thông báo iPhone rung lắc giữa đường đêm | vạch đường chạy, banner nhảy liên tục |
| 2 | Banner co lại thành màn HUD + tay lái | logo HUD Connect, đường BLE nối sang iPhone |
| 3 | HUD phóng to → bản đồ | mũi tên rẽ, đường đi tự vẽ, bản đồ xoay, huy hiệu tốc độ tròn đếm số |
| 4 | Thẻ rẽ thu thành viên thuốc micro | sóng âm, chữ "Trống Đồng Palace" gõ ra, 3 kết quả |
| 5 | Viên thuốc → thông báo dạng pill | tin nhắn trượt vào, chạm mở |
| 6 | Pill → thẻ thời tiết | mây, giọt mưa rơi theo nhịp, "Mưa lúc 17:40" |
| 7 | Thẻ → thư viện mặt đồng hồ | 5 mặt thật (iPhone, Infograph, Đi xe…) lướt qua, đổi ảnh nền |
| 8 | Mặt đồng hồ → màn khóa + bàn phím PIN, ghim chỗ đỗ | ổ khóa đóng, ghim P rơi xuống bản đồ |
| 9 | Khối co về icon app → wordmark | logo đóng dấu theo phách cuối |

## Âm thanh
- Giọng: ElevenLabs eleven_v4, tiếng Việt, mốc thời gian từng ký tự từ API /with-timestamps.
- Nhạc: thử ElevenLabs Music (bản nhạc gốc ~120 BPM, có cao trào ở cảnh 3); nếu tài khoản không có thì tự tổng hợp bằng numpy.
- SFX tổng hợp bằng numpy: click, whoosh, tick odometer, chime thông báo, giọt mưa, khóa.
