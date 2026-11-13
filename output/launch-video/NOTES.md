# HUD Connect — phim giới thiệu v1

- `HUDConnect-launch-v1.mp4` — 1920×1080, 60 fps, có motion blur (4 khung con/khung), khoảng 47 giây.
- `storyboard.png` — mỗi giây một khung, để duyệt nhanh.
- `voice-samples/` — cùng một câu đọc bằng 4 giọng (liam, brian, eric, sarah), để chọn giọng khác.

## Giọng đọc
- ElevenLabs `eleven_v4`, tiếng Việt (`language_code: vi`), giọng có sẵn **Liam**.
- Tài khoản ElevenLabs đang ở gói miễn phí, nên API không cho dùng các giọng tiếng Việt trong
  thư viện (Tony Hoang, Anh…), không cho tự thiết kế giọng, và không cho tạo nhạc. Liam là giọng
  có sẵn, đọc tiếng Việt được, nhưng có thể nghe hơi "Tây". Lên gói Starter thì đổi sang giọng
  người Việt và tạo nhạc bằng ElevenLabs được.
- Mốc thời gian từng chữ lấy từ API `/with-timestamps`: mỗi chuyển cảnh rơi đúng vào chữ tương ứng.

## Nhạc và âm thanh
- Nhạc tự tổng hợp (numpy): 120 BPM, La thứ (Am–F–C–G). Cao trào rơi đúng chữ "HUD" (5,0 s),
  lặng xuống ở cảnh mưa, lên lại ở 31 s, rồi dồn vào cú chốt logo ở 41 s. Nhạc tự giảm âm lượng
  khi có giọng đọc.
- Toàn bộ hiệu ứng âm thanh (rung, chuông tin nhắn, click, gõ phím, giọt mưa, tiếng khóa, ghim)
  tự tổng hợp, đặt theo danh sách mốc lấy từ chính file phim.

## Dữ liệu thật hay dựng
- Chức năng đều có trong sản phẩm: chỉ đường kèm icon rẽ, bản đồ xoay theo hướng, ô tốc độ tròn,
  tìm bằng giọng nói, thông báo dạng viên thuốc, dự báo mưa, các mặt đồng hồ và ảnh nền, tự khóa,
  nhớ chỗ đỗ.
- Số liệu minh họa là dựng: giờ, %, quãng đường, tên "Minh", nội dung tin nhắn, khoảng cách và
  khu vực trong kết quả "Trống Đồng Palace", giờ mưa.
- Tên đường (Trần Duy Hưng, Láng Hạ) chỉ để minh họa, bản đồ là hình vẽ, không phải bản đồ thật.

## Quyết định của đạo diễn (đổi cái nào chỉ cần nói)
- Kết hợp skill: hình theo v2 (một khối biến hình, lò xo, con trỏ), nhịp theo giọng đọc như v1.
- Bảng màu lấy từ icon app: xanh đêm, cyan #3DDCF5, xanh lá #4ADE80.
- Mở đầu bằng vấn đề (điện thoại rung liên tục, phải cúi xuống), banner "Rẽ phải" biến thành HUD.
- Đường đêm có vạch và đèn đường chạy làm nền suốt phim.
- Cảnh bản đồ cho rẽ phải thật, nên bản đồ xoay 90° đúng lúc đọc chữ "xoay".
- Cảnh khóa có iPhone đi xa, đường Bluetooth đứt rồi HUD khóa.
