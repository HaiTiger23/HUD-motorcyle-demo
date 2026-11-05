# Render giao diện tìm điểm đến bằng LVGL trên Mac

Dùng cùng `lib/hud_destination_ui/hud_destination_ui.h` và font firmware, không
phải bản HTML vẽ lại. Cần thư viện LVGL đã tải bởi PlatformIO (`pio run`).

```sh
uv run --with cmake cmake -S tools/destination_ui_preview -B /tmp/hud-destination-preview-build
uv run --with cmake cmake --build /tmp/hud-destination-preview-build --target hud_destination_preview -j 8
mkdir -p output/destination-ui
/tmp/hud-destination-preview-build/hud_destination_preview output/destination-ui
```

Xuất 17 ảnh PPM 320×240: danh sách tối, cuối danh sách, chờ GPS, mic, danh sách
sáng, xem tuyến, tên dài, kính lúp tối/sáng, xác nhận kết thúc, tên dài cuộn cuối.
Thêm 6 cảnh trang chủ Maps: lịch sử sáng/tối, rỗng, mất kết nối, đang nghe và
cuộn tới mục cuối. Có kiểm tra khoảng cách giữa nội dung cuộn và nút Bắt đầu, giới hạn màn hình,
và danh sách thực sự có thể cuộn. Đây là kiểm tra bố cục, không thay kiểm tra
chạm/giữ mic/Bluetooth trên board.

Chuyển sang PNG nếu cần:

```sh
uv run --with pillow python - <<'PY'
from pathlib import Path
from PIL import Image
for path in Path('output/destination-ui').glob('scene-*.ppm'):
    Image.open(path).save(path.with_suffix('.png'))
PY
```
