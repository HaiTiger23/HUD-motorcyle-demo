# HUD iPhone · ESP32-S3 2.8 inch

Firmware cho ES3C28P/ES3N28P, màn hình ILI9341V 320×240. Phiên bản mặc định
hiện có bốn trang cảm ứng: **Đồng hồ**, **Thông báo**, **Maps** và **Nhạc**.
Vuốt trái/phải để đổi trang; bốn chấm nhỏ ở đầu cho biết trang hiện tại.
Đã bỏ hàng tab dưới, mở rộng vùng nội dung từ 300×150 lên 300×200 px.
Trang chủ dùng giờ chữ đậm 80 px, ngày tháng tiếng Việt chữ đậm 24 px.
Màn hình mặc định tắt đèn nền sau **30 giây không chạm**; có thể đổi trong app
HUD Connect thành 15/30/60/120/300 giây hoặc luôn sáng. Chạm một lần để bật lại;
lần chạm đánh thức không kích hoạt nút bên dưới, chạm tiếp để thao tác bình thường.
Bluetooth, cảm ứng và cập nhật nội dung vẫn chạy khi tắt đèn nền. Thông báo mới
không tự bật màn hình. Nếu không phát hiện được cảm ứng lúc khởi động, firmware
giữ màn hình sáng để tránh tắt rồi không đánh thức được.

| Trang | Chức năng và giới hạn |
| --- | --- |
| Đồng hồ | Đọc giờ iPhone qua BLE Current Time Service (CTS), gồm múi giờ khi iPhone cung cấp. Tin mới hiện popup 8 giây rồi ẩn; không còn dòng thông báo cố định. Có thể đặt giờ qua USB. Tiếp tục chạy khi mất kết nối; cần đồng bộ lại sau khi mất nguồn/reset. |
| Thông báo | Danh sách tối đa 10 mục mới nhất trước, có giờ; chạm xem chi tiết và thời điểm nhận. Icon làm mới, xóa từng mục/xóa tất cả. Gửi xóa trên iPhone khi ANCS cung cấp nhãn Clear/Dismiss phù hợp, nếu không chỉ ẩn HUD sau xác nhận. Không trả lời tin nhắn. |
| Maps | App HUD có luồng tìm điểm đến → xem tuyến → bắt đầu, tính tuyến MapKit ô tô/đi bộ và gửi ảnh 300×200. ESP tự mở trang và giữ sáng khi bản đồ cập nhật. Khi không có bản đồ trực tiếp, hiện thông báo Google Maps/Apple Maps qua ANCS bằng chữ 24 px. Chưa có tuyến xe máy riêng hoặc kiểm thử ngoài đường. |
| Nhạc | Nhận tên bài, ca sĩ, trạng thái phát và phần trăm âm lượng bằng Apple Media Service (AMS). Phát/dừng, bài trước/sau, nút −/+ chỉnh âm lượng iPhone tùy lệnh trình phát hỗ trợ. Không truyền âm thanh nhạc sang ESP. |

[App iPhone HUD Connect](ios/README.md) có tám mặt đồng hồ, gồm tốc độ số và
tốc độ kim lấy từ GPS,
tám bảng màu (bốn tối, bốn sáng), thời gian sáng và bộ lọc từng app. Cài đặt lưu NVS trên ESP;
không lưu nội dung thông báo. Tối đa 24 app, có sẵn các app phổ biến và học thêm
khi nhận ANCS. Bật/tắt bộ lọc chỉ ảnh hưởng HUD, không đổi cài đặt iPhone.
Google Maps iOS chưa hỗ trợ Navigation Connect; không có luồng tự động lấy
bản đồ đang mở. Phương án chia sẻ màn hình đã thử nhưng người dùng không chấp
nhận kết quả, nên đã gỡ khỏi app chính. Người dùng sau đó chấp nhận tìm đường
trong app HUD; bản hiện tại có tab Chỉ đường và Cài đặt. Đọc thông báo Maps vẫn
giữ làm dự phòng; trong bộ lọc cần bật Google Maps/Apple Maps tương ứng.
Chưa có kết quả thử di chuyển ngoài đường hoặc chạy lâu khi khóa màn hình.

Đã kiểm chứng với iPhone thật: ghép đôi có mã hóa, đăng ký ANCS thành công,
đồng bộ giờ CTS; người dùng đã xác nhận hiển thị thông báo WhatsApp trên board.
Người dùng cũng đã xác nhận tab nhạc AMS hiển thị và điều khiển hoạt động.
Ứng dụng khác và Google Maps còn cần thử bằng thông báo thực tế; nhận WhatsApp
chưa chứng minh Google Maps cung cấp hướng dẫn rẽ liên tục.

App cũng nhận điểm đến qua Chia sẻ từ Google Maps/Apple Maps và cho chọn điểm
trực tiếp trên bản đồ; cả hai luồng đã được người dùng thử thành công.
Chế độ **Theo chỉ dẫn Google Maps** gửi nền bản đồ quanh GPS và ghép thông báo
Google lên HUD, không dựng toàn tuyến Google từ thông báo. Đọc hướng dẫn và
giới hạn trong [tài liệu app](ios/README.md).

## Ghép iPhone

1. Cấp nguồn cho board, mở **Cài đặt → Bluetooth** trên iPhone.
2. Tìm **HUD iPhone**. Nếu không thấy, chờ vài giây hoặc bật/tắt Bluetooth trên iPhone.
3. Khi có yêu cầu ghép đôi, nhập mã sáu chữ số hiện trên màn hình board.
4. Cho phép chia sẻ thông báo. Trong thông tin thiết bị Bluetooth, bật
   **Chia sẻ thông báo hệ thống / Share System Notifications** nếu có.
5. Chạm các tab dưới màn hình hoặc vuốt trái/phải để đổi trang. Trong hộp thư,
   dùng `<` / `>` để xem các thông báo đang có, kéo phần nội dung để đọc thêm.
   Chạm **Làm mới** ở góc trên bên phải để đăng ký lại ANCS và yêu cầu iPhone
   gửi lại thông báo hiện có. Khi đang tải, nút tạm khóa; khi mất kết nối,
   nút báo **Chưa kết nối**. Thao tác này không xóa thông báo trên iPhone.

Chấm góc trên bên phải có màu xanh lá khi Bluetooth đã kết nối, xám khi ngắt.
Tin mới nổi tối đa 8 giây trên các trang ngoài hộp thư. Chạm popup để đọc đầy
đủ; chạm X để ẩn, không xóa trên iPhone. Tin cũ iPhone gửi khi kết nối/làm mới
vẫn lưu trong hộp thư nhưng không bật popup nếu ANCS đánh dấu PreExisting.
Trong tab nốt nhạc, mở và phát nhạc trên iPhone để nhận tên bài và điều khiển.
Nút chưa được trình phát hỗ trợ sẽ bị vô hiệu hóa.
Hai nút ngoài cùng **− / +** giảm/tăng âm lượng iPhone; số phần trăm được cập
nhật từ iPhone, không tự tăng/giảm giả khi gửi lệnh. Khi chưa nhận được mức âm
lượng, màn hình hiện `--%`.
Việc gửi lại danh sách qua ANCS phụ thuộc iPhone, không bảo đảm gửi đủ mọi mục
([đặc tả Apple](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html)).

Nội dung xem trước phụ thuộc cài đặt thông báo của iPhone và từng ứng dụng.
Nếu một ứng dụng có thông báo trên iPhone nhưng board không hiện, kiểm tra
**Cài đặt → Thông báo → [ứng dụng]**: bật **Trung tâm thông báo**. Để thử
nhận cả nội dung khi khóa máy, chọn **Hiển thị bản xem trước → Luôn luôn**,
rồi gửi một thông báo mới. Người dùng đã xác nhận Messenger hiển thị sau
khi chỉnh các tùy chọn này. Xem
[hướng dẫn cài đặt thông báo của Apple](https://support.apple.com/guide/iphone/change-notification-settings-iph7c3d96bab/ios).
Thông báo chỉ lưu trong RAM và được xóa khi ngắt kết nối; firmware không in
nội dung riêng tư ra Serial và không chuyển dữ liệu đến máy chủ.

Nếu đồng hồ vẫn hiện `--:--`, iPhone chưa cung cấp giờ CTS. Có thể gửi lệnh USB
`TIME <unix-seconds> <UTC-offset-minutes>`; ví dụ offset Việt Nam là `420`.
Lệnh `STATUS` in trạng thái ứng dụng, heap và việc phát hiện FT6336.
Lệnh này cũng in số sự kiện ANCS, gói dữ liệu, yêu cầu đọc, phản hồi và lỗi
để phân biệt chưa nhận thông báo với lỗi đọc nội dung; không in nội dung tin.
Lệnh `REFRESH` yêu cầu làm mới ANCS giống nút trên màn hình.

Xem [nghiên cứu web cài đặt và hỏi đáp AI](docs/SETTINGS_AI_ARCHITECTURE.md):
board có mic/ES8311, có thể triển khai AI qua Wi-Fi/API mà chưa cần app iPhone.
Web cài đặt và AI hiện mới là phương án thiết kế, chưa có trong firmware.

### Google Maps trên iPhone

Google Maps URL scheme cho phép mở ứng dụng và yêu cầu chỉ đường, không cung cấp
luồng hướng dẫn rẽ liên tục cho firmware này. Google Navigation SDK dùng để xây
app dẫn đường riêng. ANCS chỉ cung cấp những thông báo iOS thực sự xuất bản.
Do đó, tab Maps chưa phải hệ thống dẫn đường hoàn chỉnh.

Xem [đối chiếu ba dự án tham khảo](docs/IPHONE_NAVIGATION_RESEARCH.md).

## Giao diện HUD gốc (demo cũ)

Mã EEZ/LVGL ban đầu vẫn được giữ trong `lib/ui/`. Để chạy lại bản đồng hồ xe máy
mô phỏng, thêm `-DHUD_DEMO_ONLY` vào `build_flags`, rồi build/nạp lại.
Các số trên bản demo này là dữ liệu ngẫu nhiên, chưa lấy từ ECU/cảm biến.

## Hình ảnh demo

<img width="2268" height="1441" alt="1791392909255_2303028224121812024_2347431114217680222_efc3d665e21d9063921ab5aeb4866314" src="https://github.com/user-attachments/assets/6b9e0cc7-2129-41db-b335-3e47ab8ef3de" />

## Sơ đồ mạch

Xem sơ đồ kết nối trên Cirkit Designer:
https://app.cirkitdesigner.com/project/0c11ae4d-57a4-40f7-b794-aa193b1a1176

## Tính năng

- Hiển thị giao diện HUD trên màn hình TFT 320×240.
- Hiển thị tốc độ, cấp số và đồng hồ thời gian.
- Hiển thị điện áp ắc quy, quãng đường chuyến đi, odo và mức tiêu thụ trung bình.
- Hiển thị nhiệt độ, mức nhiên liệu và vòng tua máy.
- Mô phỏng trạng thái các đèn báo:
  - Xi-nhan trái/phải.
  - Đèn pha.
  - Check engine.
  - Dầu máy.
  - Nhiệt độ.
- Cập nhật dữ liệu mô phỏng định kỳ khoảng 700 ms.
- Giao diện được thiết kế và sinh mã bằng EEZ Studio/LVGL.

## Phần cứng mục tiêu

Dự án đã được cấu hình trực tiếp cho bộ **2.8 inch ESP32-S3 Display
ES3C28P/ES3N28P V1.0** trong thư mục tham khảo: ILI9341V, 320×240 ở chế độ
ngang. Không cần sửa `User_Setup.h` trong thư viện TFT_eSPI.

| Chức năng | GPIO |
| --- | ---: |
| TFT CS / DC / MOSI / SCLK / MISO | 10 / 46 / 11 / 12 / 13 |
| TFT backlight | 45 |
| Cảm ứng FT6336: SCL / SDA / INT / RST | 15 / 16 / 17 / 18 |

Board đã kiểm tra qua USB: ESP32-S3, flash 16 MB, PSRAM 8 MB. Cấu hình build
sử dụng flash 16 MB và PSRAM Octal. Bộ lệnh khởi tạo IPS riêng của hãng nằm
trong `include/ili9341v_panel.h`.

Cảm ứng FT6336 được đăng ký làm thiết bị trỏ LVGL. Các tab mới có xử lý nhấn
và vuốt. Riêng các biểu tượng cảnh báo trong HUD demo cũ vẫn do mô phỏng điều
khiển. Bản ES3N28P không có cảm ứng sẽ bỏ qua đầu đọc nếu không tìm thấy FT6336.

## Công nghệ và thư viện

- [PlatformIO](https://platformio.org/)
- Arduino framework cho ESP32.
- [LVGL](https://lvgl.io/) `9.6.0`
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) `2.5.43`
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) `2.5.2`
- Phông tiếng Việt được tạo từ Noto Sans (SIL OFL 1.1, kèm giấy phép trong `lib/companion_fonts/OFL.txt`).
- `LV_USE_FONT_COMPRESSED=1` bắt buộc cho phông này. Khi khởi động, firmware thử vẽ các ký tự Latin/tiếng Việt và dùng phông mặc định nếu kiểm tra thất bại.

## Cấu trúc dự án

```text
.
├── include/
│   ├── board_config.h  # Chân kết nối của board
│   ├── ili9341v_panel.h # Khởi tạo tấm IPS theo demo hãng
│   └── lv_conf.h       # Cấu hình LVGL
├── lib/
│   └── ui/             # Mã giao diện được sinh từ EEZ Studio
├── src/
│   └── main.cpp        # Khởi tạo màn hình và vòng lặp HUD
├── platformio.ini      # Cấu hình board và thư viện
└── README.md
```

## Cài đặt và chạy

### Yêu cầu

- Visual Studio Code.
- Extension [PlatformIO IDE](https://platformio.org/install/ide).
- Cáp USB có hỗ trợ truyền dữ liệu.

### Các bước

1. Clone repository và mở thư mục dự án bằng Visual Studio Code.
2. Kết nối ESP32-S3 với máy tính.
3. Kiểm tra lại cấu hình board trong `platformio.ini`:

   ```ini
   [env:esp32-s3-devkitm-1]
   board = esp32-s3-devkitm-1
   framework = arduino
   ```

4. Build và nạp firmware bằng PlatformIO:

   ```bash
   pio run
   pio run --target upload
   ```

   Để chọn cổng USB cụ thể, xem `pio device list` rồi nạp, ví dụ trên macOS:

   ```bash
   pio run --target upload --upload-port /dev/cu.usbmodem101
   ```

   Cổng USB có thể đổi tên sau khi cắm lại board. Nếu dừng ở `Connecting...`,
   giữ BOOT, nhấn rồi thả RESET, thả BOOT và thử nạp lại. Đóng Serial Monitor
   khác nếu báo cổng đang được sử dụng.

5. Mở Serial Monitor ở tốc độ `115200` để kiểm tra log khởi động:

   ```bash
   pio device monitor -b 115200
   ```

Khi khởi động thành công, Serial Monitor sẽ in:

```text
HUD iPhone ready
```

## Ghi chú phát triển

- Logic khởi tạo màn hình, FT6336 touch và vòng lặp chính nằm trong [`src/main.cpp`](src/main.cpp).
- Kết nối iPhone và worker GATT nằm trong `src/phone_link.cpp`; cập nhật LVGL chỉ chạy ở vòng lặp chính.
- Giao diện ba trang nằm trong `src/companion_ui.cpp`.
- Parser ANCS/CTS độc lập nền tảng nằm trong `lib/phone_protocol/`.
- Cấu hình chân phần cứng được lưu trong [`include/board_config.h`](include/board_config.h); các macro TFT_eSPI tương ứng nằm trong `platformio.ini`.
- Các widget và hình ảnh của giao diện nằm trong [`lib/ui/`](lib/ui/).
- Có thể thay các giá trị ngẫu nhiên trong `updateRandomHud()` bằng dữ liệu đọc từ cảm biến, CAN bus hoặc ECU.
- Khi thay đổi giao diện bằng EEZ Studio, cần cập nhật lại các tệp sinh mã trong `lib/ui/`.

## Hướng phát triển

- Kiểm chứng ghép đôi, quyền ANCS, CTS, khóa màn hình và kết nối lại với iPhone thật.
- Xác định Google Maps phiên bản trên iPhone có phát thông báo hướng dẫn qua ANCS hay không.
- Nếu cần dẫn đường liên tục có cấu trúc, phải chọn nguồn dữ liệu/app tích hợp phù hợp.

- Đọc tốc độ và vòng tua thực tế từ ECU/CAN bus.
- Kết nối cảm biến nhiên liệu, nhiệt độ và điện áp.
- Thêm cảnh báo vượt tốc độ và cảnh báo lỗi.
- Lưu dữ liệu chuyến đi và odo vào bộ nhớ không mất dữ liệu.
- Tối ưu độ sáng và khả năng đọc ngoài trời.

## Giấy phép

Chưa thiết lập giấy phép cho dự án. Vui lòng bổ sung file `LICENSE` trước khi phân phối hoặc sử dụng lại mã nguồn.

## Kiểm thử giao thức trên máy tính

```bash
clang++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ilib/phone_protocol test/phone_protocol_test.cpp \
  lib/phone_protocol/phone_protocol.cpp -o /tmp/hud-phone-protocol-test
/tmp/hud-phone-protocol-test
```

Bộ kiểm thử bao phủ phản hồi ANCS bị chia tại mọi vị trí byte, giới hạn độ dài,
cắt UTF-8 an toàn, UID không khớp, gói bị thiếu và ngày/múi giờ CTS không hợp lệ.
