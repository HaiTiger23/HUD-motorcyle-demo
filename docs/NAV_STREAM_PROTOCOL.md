# HUD map stream v1

Giao thức riêng cho app HUD Connect, không tương thích trực tiếp với IPA MOTO GPS.
ESP cung cấp service `ee43a100-1b32-4d0f-9ab8-2248b0a01000`.
RX `ee43a101-1b32-4d0f-9ab8-2248b0a01000` có Write/Write Without Response,
write yêu cầu mã hóa. ACK `ee43a102-1b32-4d0f-9ab8-2248b0a01000` có Notify/Read;
read yêu cầu mã hóa. Firmware chỉ xử lý/gửi ACK khi kết nối đã mã hóa.

Kênh tốc độ `ee43a104-1b32-4d0f-9ab8-2248b0a01000` có encrypted Write/Write NR:
hai byte UInt16 little endian đơn vị 0,1 km/h, giá trị 0..3000 hoặc 65535=không
có dữ liệu. Tách kênh để không làm rối trạng thái fragment/ACK ảnh. Board bỏ
dữ liệu tốc độ cũ quá 5 giây; app có thể bỏ qua một mẫu khi BLE back-pressure.

Tóm tắt chuyến trên kênh info `ee43a106-1b32-4d0f-9ab8-2248b0a01000` dùng
`[1, remaining_m:u32LE, turn_m:u32LE, eta_minute:u16LE, maneuver:u8, instruction:UTF-8]`.
Khi có lượt rẽ thứ hai hoặc đang chạy thử, app thêm
`[NUL, flags:u8, next_maneuver:u8, gap_m:u16LE]` sau chuỗi chỉ dẫn. Bit 0 của
`flags` đánh dấu chạy thử. `next_maneuver=255` nghĩa là chưa có lượt rẽ thứ
hai. `gap_m` tính từ lượt rẽ thứ nhất. Firmware cũ dừng đọc chữ ở NUL, còn
firmware mới vẫn nhận gói chuyến đi có chuỗi kết thúc theo chiều dài gói.

Giữ nguyên quảng bá ANCS/AMS. App tìm `HUD iPhone` khi mở lần đầu ở foreground,
sau đó lưu UUID CoreBluetooth để kết nối lại, không dựa vào quét mọi thiết bị nền.

Mỗi GATT write chứa một packet. Số nguyên là unsigned 32-bit little endian.

| Op | Payload | GATT mode |
| --- | --- | --- |
| 1 | frame_id, total_bytes, CRC32 | With Response |
| 2 | frame_id, offset, JPEG bytes | Without Response; tuân canSendWriteWithoutResponse |
| 3 | frame_id | With Response; chờ ACK |
| 5 | một byte: 0=tuyến HUD, 1=nền GPS kèm thông báo Google | With Response; trước packet 1 |
| 7 | frame_id:u32LE, speed:u16LE (0,1 px/s), flags:u8, margin:u8, previous_frame:u32LE, advance:i16LE | With Response; trước packet 1 của khung cuộn |
| 4 | không có payload | Dừng/xóa bản đồ |

CRC32 là chuẩn IEEE, polynomial đảo `0xedb88320`, init/final XOR `0xffffffff`.
Golden vector `123456789` cho `0xcbf43926`, được kiểm tra cả Swift và C++.
Packet <= min(maximumWriteValueLength, 244); payload data tối đa packet_size−9.
Ảnh baseline JPEG mặc định 320×240, precision 8 bit, 1 hoặc 3 thành phần; tối đa 48 KiB.
Khung có op 7 cùng frame_id dùng JPEG rộng 320 px, cao 240 + 2 × margin px.
Lề 12–64 px ở trên và dưới tùy tốc độ và độ phủ của ảnh Apple Maps. Bit 0 của
flags chọn mũi tên thay cho chấm và bit 1 đánh dấu nền sáng. HUD vẽ dấu xe cố
định và dịch ảnh tối đa đến mép lề hoặc 1,2 giây từ khi nhận op 7. Khi
previous_frame khớp ảnh đang hiển thị, advance cho biết tâm ảnh cũ nằm thấp
hơn tâm ảnh mới bao nhiêu pixel. HUD dùng độ lệch đó để giữ vị trí đường trên
màn hình khi đổi JPEG và điều chỉnh dần theo tốc độ mới. previous_frame = 0
bỏ qua bước căn chỉnh. Firmware vẫn nhận op 7 dài 8 byte kèm JPEG 344×264
từ app cũ. App dùng lề thay đổi cần firmware mới.
Không nhận ảnh quá kích thước hay Progressive JPEG. Nhận theo offset liên tục,
không có đường giải mã ảnh khi thiếu fragment, sai CRC hoặc sai kích thước.

ACK gồm `[status:u8, frame_id:u32]`, status 0 khi ảnh được nhận/kiểm tra và chờ
LVGL, 1 khi bị từ chối. ACK không chứng minh ảnh đã được render hoàn tất.

Kênh ACK còn gửi điều khiển bản đồ (Notify, chỉ peer đã mã hóa), đúng 10 byte:
`[0x80, command:u8, displayed_frame:u32LE, dx:i16LE, dy:i16LE]`.
Command 1=zoom in, 2=zoom out, 3=bám GPS, 4=kéo, 5=xem tự do.
Chỉ command 4 có dx/dy khác 0, giới hạn ±300/±200 pixel. ID lấy từ khung UI đã
nhận, không phải khung đang truyền dở. App giữ 6 ngữ cảnh snapshot gần nhất,
bỏ điều khiển có ID không còn trong cache hoặc khi không dẫn đường. Phép đảo
chiếu kéo dùng chính snapshot của ID đó, bao gồm hướng camera. Không thay tọa
độ GPS thật hay nguồn tính tiến độ/tốc độ. Zoom giới hạn khoảng camera 180–5000 m.
Packet này tách biệt ACK ảnh 5 byte; app cũ có thể bỏ qua mà không đổi GATT.
Khi camera đổi, callback nền cũ bị hủy và không gửi nền chưa đúng camera.

Hai bộ đệm JPEG nằm trong PSRAM. UI chỉ nhận ảnh hoàn chỉnh, đổi bộ đệm và xóa
cache ảnh LVGL ở main thread; BLE callback không gọi LVGL. Không nhận ảnh mới
khi ảnh hoàn chỉnh trước đó còn chờ main loop lấy.

Disconnect/reset phiên hủy dữ liệu dở dang và yêu cầu UI xóa ảnh. Op 4 xóa ngay;
không có ảnh mới sau 15 giây thì UI cảnh báo dữ liệu cũ. App timeout 12 giây
sẽ hủy kết nối để xóa hàng đợi ATT trước khi thử lại, không nối fragment cũ
vào khung mới. Chỉ hỗ trợ một điện thoại đồng thời như ANCS hiện tại.

Native checks: `test/nav_protocol_test.cpp` (ASan/UBSan); Swift checks trong
`ios/Core/Checks/main.swift`. Chúng kiểm tra frame/CRC/giới hạn, không chứng minh
toàn bộ JPEG decoder, BLE radio, bản đồ MapKit hay navigation hoạt động trên máy thật.
