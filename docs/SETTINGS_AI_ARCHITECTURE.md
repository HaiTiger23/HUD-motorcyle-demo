# Web cài đặt, âm nhạc và AI trên ES3C28P

> Cập nhật 31/10/2026: đã triển khai app iPhone, cài đặt BLE và trang AI dùng mic
> board → iPhone nhận dạng tiếng Việt → 9router → trả chữ lên HUD. Xem
> [giao thức và trạng thái kiểm chứng hiện tại](AI_VOICE_PROTOCOL.md).
> Các phần thiết kế web/AI bên dưới là nghiên cứu ban đầu, không thay thế trạng
> thái triển khai mới trong tài liệu này.

Nghiên cứu ngày 31/10/2026. Phần cứng hiện dùng là ESP32-S3, không phải STM32.

## Kết luận

Chưa cần viết app iPhone để có bộ lọc thông báo và hỏi đáp AI. Phương án đầu
tiên nên là web cài đặt chạy trên ESP32-S3, AI dùng mic của board qua Wi-Fi
và backend/API. iPhone tiếp tục gửi ANCS và AMS qua BLE như hiện tại.
Khi đi ngoài đường có thể dùng Điểm truy cập cá nhân của iPhone cấp Internet.

App iPhone riêng hữu ích nếu muốn cài đặt qua BLE, dùng mạng di động mà không
bật hotspot, dùng mic/loa của iPhone, hoặc bổ sung GPS và điều hướng riêng.
Đây là một sản phẩm lớn hơn, cần thử hoạt động nền và khóa màn hình trên máy thật.

## Phần cứng đã đối chiếu

Nguồn gốc: bộ tài liệu hãng do người dùng cung cấp, không phải chỉ dẫn để chạy
các firmware mẫu hay dùng tài khoản/API trong ví dụ.

- Sơ đồ `5-原理图_Schematic/2.8inch_ESP32-S3_Display_Schematic.pdf`:
  mic MEMS LMA2718B381-OA7, codec ES8311, mạch khuếch đại và đầu nối loa P5.
  Có đường ra loa không có nghĩa là board đang được gắn loa.
- `Example_17_echo`: mẫu thu/phát lại âm thanh.
- `Example_30_ai_chat`: mẫu thu âm, gọi dịch vụ nhận dạng giọng nói, chat qua
  API và phát câu trả lời; chứng minh hướng tích hợp của hãng, chưa kiểm thử
  chất lượng tiếng Việt hoặc chất lượng mic trên board đang nối máy.
- `ESP_Panel_Board_Custom.h` và phần cấu hình I2S của mẫu echo thống nhất:

| Tín hiệu | GPIO ESP32-S3 |
| --- | --- |
| ES8311 I2C SCL / SDA | 15 / 16, dùng chung bus với FT6336 |
| I2S MCLK | 4 |
| I2S BCLK | 5 |
| I2S WS/LRCK | 7 |
| ESP nhận dữ liệu mic / DIN | 6 |
| ESP xuất dữ liệu loa / DOUT | 8 |

Codec có địa chỉ I2C 0x18 khi CE ở mức thấp theo driver hãng. Cần kiểm tra ACK
và bản ghi thử trước khi triển khai AI. Không cài chồng một I2C driver mới lên
bus `Wire` đang dùng cho cảm ứng. Mẫu hãng dùng API I2S mới; firmware hiện tại
đang dùng Arduino ESP32 2.0.17, do đó cần chọn driver tương thích hoặc nâng
framework có kiểm chứng, không chép nguyên sketch vào dự án.

## Web cài đặt đề xuất

ESP32 mở mạng Wi-Fi cấu hình khi người dùng yêu cầu. Điện thoại kết nối mạng
đó rồi mở trang cài đặt do chính ESP phục vụ. Có thể chọn địa chỉ 192.168.4.1
cho bản triển khai; đây chưa phải URL đang hoạt động trong firmware hiện tại.
Khi đã cấu hình Wi-Fi nhà, thiết bị có thể phục vụ cùng trang trên mạng LAN.

Các mục hữu ích:

- Bật/tắt hiển thị từng ứng dụng, chọn ứng dụng ưu tiên.
- Thời gian popup, ẩn nội dung xem trước, độ sáng ban ngày/ban đêm.
- Wi-Fi, địa chỉ dịch vụ AI, lựa chọn trả lời chữ/giọng nói.
- Lưu lựa chọn vào NVS/Preferences để giữ sau khi khởi động lại.

Lọc thực hiện trên board sau khi nhận AppIdentifier từ ANCS. Bộ lọc không
thay đổi cài đặt thông báo của iPhone. Danh sách ứng dụng lấy từ những app
đã thực sự gửi thông báo, kèm các lựa chọn phổ biến; ANCS không cung cấp
một danh sách đầy đủ mọi ứng dụng được cài trên iPhone.

Web thông thường trong Safari chưa có Web Bluetooth gốc để thay thế một
app CoreBluetooth. Vì vậy web cài đặt dùng Wi-Fi, còn ANCS/AMS vẫn dùng BLE.
AP cấu hình nên bật khi cần rồi tắt. Espressif hỗ trợ Wi-Fi STA đồng thời BLE;
cần thử tải âm thanh cùng ANCS/AMS vì chúng dùng chung radio 2.4 GHz, và một
số tình huống SoftAP + BLE có hiệu năng không ổn định theo bảng coexistence.

Nguồn:
[Espressif Wi-Fi AP/STA](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html),
[coexistence ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32s3/api-guides/coexist.html),
[Web Bluetooth và khả năng tương thích](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API).

## AI: hai đường kết nối

| Phương án | Luồng xử lý | Cần app iPhone? | Điều kiện |
| --- | --- | --- | --- |
| Board qua Wi-Fi | Mic → ES8311 → ESP → backend nhận dạng giọng nói/chat → chữ hoặc âm thanh trả về | Không | Board có Internet qua Wi-Fi nhà hoặc hotspot; cần backend/API hoạt động |
| iPhone làm trung gian | Mic board → BLE riêng → app iPhone → API → BLE trả kết quả | Có | Viết giao thức audio/flow control, app CoreBluetooth và xử lý nền; thử khi khóa máy |

Cho phiên bản đầu: nhấn để nói, tối đa khoảng 10 giây, hiện câu nhận dạng và
câu trả lời ngắn bằng tiếng Việt trên tab AI. Có thể bổ sung phát giọng nói
khi đã thử loa. PCM mono 16 kHz, 16 bit cần 32.000 byte/giây, khoảng 320.000
byte cho 10 giây; bộ đệm nên dùng PSRAM và giới hạn thời gian ghi rõ ràng.
Thực hiện mạng và audio ngoài luồng LVGL để cảm ứng vẫn hoạt động.

Nên để khóa API của nhà cung cấp ở backend, board chỉ giữ thông tin truy cập
dành cho thiết bị; không nhúng khóa vào trang HTML cài đặt. Cần chọn dịch vụ
hỗ trợ nhận dạng tiếng Việt và xác minh thực tế trước khi hứa thời gian đáp ứng.
API có thể tính phí; chưa chọn nhà cung cấp hoặc thực hiện cuộc gọi API nào.

Nếu dùng iPhone cấp mạng, ESP32-S3 hỗ trợ Wi-Fi 2.4 GHz. Với iPhone hỗ trợ
**Tối đa hóa tương thích**, tùy chọn này giới hạn hotspot về 2.4 GHz.

Không thể mặc định biến board thành tai nghe Bluetooth Classic của iPhone:
ESP32-S3 hỗ trợ BLE, không có Bluetooth Classic/HFP. Muốn đưa âm thanh qua
điện thoại cần app và kênh BLE riêng, hoặc dùng Wi-Fi.

App iPhone có thể khai báo chế độ nền CoreBluetooth để được đánh thức khi
có sự kiện BLE, nhưng điều đó không bảo đảm một phiên voice chat dài chạy
mãi khi khóa máy. Apple giới hạn công việc nền và có thể kết thúc app;
cần thiết kế state restoration, xử lý hết thời gian, mất kết nối và thử trên máy thật.

Nguồn:
[ESP32-S3 Wi-Fi/BLE](https://www.espressif.com/products/socs/esp32-s3),
[Bluetooth LE, không hỗ trợ Classic](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/ble/overview.html),
[hotspot tương thích 2.4 GHz](https://support.apple.com/en-ca/guide/security/secfd166f620/web),
[CoreBluetooth chạy nền](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/CoreBluetoothBackgroundProcessingForIOSApps/PerformingTasksWhileYourAppIsInTheBackground.html).

## Phần đã triển khai trong lượt này

- Bốn tab dùng icon: trang chính/đồng hồ, thông báo, Maps, nhạc.
- Popup tin mới tự ẩn sau 8 giây; chạm để đọc trong hộp thư, nút X chỉ ẩn popup.
  Bỏ dòng thông báo cố định dưới đồng hồ. Không popup lại các thông báo ANCS
  đánh dấu đã tồn tại trước khi đăng ký.
- Tab nhạc nhận tên bài, ca sĩ, trạng thái phát qua AMS; phát/dừng, trước/sau.
  Chỉ bật nút mà iPhone thông báo đang hỗ trợ. GATT chạy ở worker, không ở LVGL.
  AMS điều khiển trình phát trên iPhone, không chuyển luồng nhạc sang loa ESP.
  Tên bài/ca sĩ được giới hạn 159 byte và cắt an toàn UTF-8.
- Parser AMS có kiểm thử dữ liệu thiếu, trường không hỗ trợ, chuỗi trống,
  tiếng Việt, cắt UTF-8 và tập lệnh điều khiển.
- Đã build và nạp lên board thật. Log xác nhận ANCS và AMS cùng hoạt động,
  không lỗi đọc thông báo; iPhone gửi cập nhật nhạc, nhận lệnh điều khiển.
  Người dùng đã xác nhận tab nhạc hoạt động, bao gồm tên bài/ca sĩ và các nút.
  Kiểm thử parser chạy với AddressSanitizer và UndefinedBehaviorSanitizer đều qua.

Web settings, bộ lọc lưu NVS và tab AI là thiết kế đề xuất, chưa triển khai
trong lượt này. Không có tab AI giả hoặc lời hứa gọi API khi chưa có dịch vụ.

Nguồn AMS:
[đặc tả](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html),
[mã lệnh và trường dữ liệu](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Appendix/Appendix.html).
