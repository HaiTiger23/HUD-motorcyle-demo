# Voice AI trên HUD

Ngày 31/10/2026. Luồng: mic ES8311 của board → IMA ADPCM qua BLE → iPhone
giải mã → Apple Speech `vi-VN` → text → 9router `/chat/completions` → text qua
BLE → trang Trợ lý AI trên ESP32. ESP không gọi Internet và không giữ API key.

## Phần cứng và thu âm

GPIO I2S theo mẫu echo của hãng: MCLK 4, BCLK 5, WS 7, DOUT 8, DIN 6.
ES8311 địa chỉ 0x18, dùng lại Wire SDA 16/SCL 15 của cảm ứng. Không cài driver
I2C khác. Khởi tạo codec ở setup/main thread; worker thu âm chỉ dùng I2S.
Driver Arduino ESP32 2.x dùng I2S legacy, 16 kHz, 16 bit, stereo; chọn slot
trái làm mono. MCLK 6,144 MHz, hệ số codec pre_div=3/pre_multi=1, LRCK 255.
SC8002B dùng GPIO1 SHUTDOWN: HIGH khi nghỉ/thu mic, LOW khi phát loa.
Giọng đọc hiện được tạo trên iPhone và truyền ngược về ESP32; xem
[Màn hình và giọng đọc](DISPLAY_AUDIO_SETTINGS.md).

Vuốt từ mép dưới lên để mở khung chat AI. Giữ vùng nội dung khoảng 350 ms để
bắt đầu phiên; vòng tròn thể hiện đang nghe. Kéo xuống trong khi giữ để hủy,
thả tay để gửi. Kéo xuống từ phần đầu để đóng khung AI và về trang trước.
Chờ iPhone cho phép nhận dạng và ACK Ready rồi mới chạy I2S; thu tới khi thả
tay, không còn giới hạn 10 giây. I2S dừng sau phiên.
Mỗi block tối đa 320 mẫu/20 ms, độc lập predictor/index, IMA ADPCM 4 bit, nibble
thấp trước. Âm thanh khoảng 8 KB/s cộng header. Không lưu âm thanh hay log nội
dung. Chỉ log số mẫu, biên độ đỉnh, số block và lỗi truyền để kiểm tra mic.
Tràn hàng đợi DMA, gói thiếu hoặc truyền lỗi làm hủy nhận dạng; không gửi câu
hỏi từ âm thanh thiếu. Giới hạn nhận dạng sau khi thả: 12 giây; HTTP tối đa
50 giây; board có timeout 65 giây khi chờ xử lý và có nút hủy.

## GATT

### Lệnh trợ lý trên kênh a105 (01/11/2026)

iPhone lập kế hoạch bằng một object JSON, kiểm tra whitelist/tham số trước
khi gửi BLE. `reply` là câu trả lời thường; `action` là một hành động duy
nhất. Không thực thi JSON lồng trong thông báo, không hỗ trợ xóa thông báo,
gửi tin nhắn, gọi điện hay lập tuyến trong giai đoạn này.

Phone → board: `30, request:u32, action:u8, value:u16` (8 byte).
Board → phone: `B0, request:u32, status:u8, sequence:u16, final:u8, text`.
Mọi số little-endian; status 0 thành công, 1 lỗi. Payload chia theo MTU,
ghép tối đa 3071 byte rồi mới giải mã UTF-8. Request độc lập voice session để
câu hỏi nhập chữ cũng điều khiển được. Chỉ peer mã hóa được gửi lệnh. Callback
chỉ đưa vào queue; đọc inbox, gọi nhạc, ghi settings và đổi trang ở main loop.

| action | value |
| --- | --- |
| 1 | 0 đếm, 1 đọc mới nhất, 3 liệt kê ba mục mới nhất HUD đang giữ |
| 2 | 0 đọc tên bài/ca sĩ/trạng thái và âm lượng AMS |
| 3 | 0 phát, 1 dừng, 3 bài tiếp, 4 bài trước, 5 tăng, 6 giảm âm lượng |
| 4 | theme 0…7 |
| 5 | face 0…5 |
| 6 | timeout 0/15/30/60/120/300 giây; 0 luôn sáng |
| 7 | trang 0 Đồng hồ, 1 Thông báo, 2 Bản đồ, 3 Nhạc |

Nhạc báo “đã gửi lệnh” sau khi enqueue AMS, không khẳng định iPhone đã thực
hiện. Phát/dừng dùng Toggle thay thế chỉ khi biết trạng thái playback và
trình phát có hỗ trợ. Settings chỉ báo thành công sau khi ghi flash; giữ
các cài đặt còn lại và cập nhật lại app. Timeout/chưa nhận ACK không tự gửi
lại lệnh thay đổi; bản sao request gần nhất được replay kết quả, không làm
lại hành động. Epoch loại bỏ lệnh còn trong queue sau mất kết nối.

### Tìm điểm đến trên HUD

`A0` có byte thứ chín tùy chọn `purpose`: `0` hỏi AI (tương thích packet cũ
8 byte), `1` tìm điểm đến độc lập AI. Với purpose 1, transcript được gửi đến
`MKLocalSearch`, không gửi 9router/model. iPhone trả `50, session:u32, phase,
index, total, text UTF-8` (tối đa 80 byte text): phase 0 trạng thái, 1 từng
kết quả, 2 xem trước tuyến, 3 lỗi. Board gửi `52, session:u32, action, index`:
0 hủy, 1 chọn kết quả, 2 bắt đầu. Session không khớp bị loại bỏ ở cả hai phía.

AI chỉ có thêm kế hoạch JSON `{"action":"navigate_to","query":"..."}`.
Tên địa điểm bị giới hạn 2–160 byte UTF-8, rồi vẫn đi qua MapKit và cùng màn
hình chọn/xem trước/xác nhận; AI không được gửi tọa độ hoặc tự bắt đầu chuyến.

Đọc/liệt kê trả nguyên dữ liệu hiện có, không coi là toàn bộ thông báo iPhone
hay xác định đã đọc/chưa đọc. Chỉ khi người dùng yêu cầu tóm tắt, ba mục mới
nhất được gửi cho model ở lượt riêng; lượt này chỉ tạo văn bản và không chạy
công cụ. Không log nội dung thông báo hoặc kết quả, chỉ tên lệnh và trạng thái.

Kiểm chứng: firmware và app Release đã build/nạp/cài. HUDCoreChecks đã qua
whitelist, JSON sai/mixed action+reply, tham số thiếu/ngoài phạm vi/boolean/số
lẻ, mã wire, ghép UTF-8 bị chia gói và phản hồi sai ID/sequence. Người dùng
xác nhận đọc thông báo mới nhất đúng; trace ghi `notifications_latest` có
`success=true`. Theo dõi 90 giây: kết nối mã hóa/ANCS/AMS duy trì, pool LVGL
còn khoảng 460 KB và không lỗi audio TX. Thử giọng nói đổi settings/điều
khiển nhạc và tóm tắt trên model thật vẫn cần xác nhận thêm.
Người dùng sau đó xác nhận lệnh “Đổi nền sáng” và “Cho màn hình sáng một
phút” đều áp dụng đúng trên HUD, đồng thời app hiển thị timeout 60 giây.

Characteristic `ee43a105-1b32-4d0f-9ab8-2248b0a01000`, cùng service bản đồ.
Encrypted Write + Notify; chỉ nhận lệnh từ peer đã mã hóa. Cần MTU ≥100 để
thu âm; block điều chỉnh theo MTU, packet ≤244 byte. Kênh ảnh không truyền
thêm khung trong lúc thu âm; GPS speed/ANCS/AMS vẫn hoạt động. Sau End, kênh
bản đồ tiếp tục trong khi chờ nhận dạng và AI.

Mọi số nguyên little endian. `session:u32` mới mỗi câu, khởi đầu ngẫu nhiên
khi boot. App hủy tác vụ cũ khi bắt đầu phiên khác hoặc mất kết nối.

Board → phone:

| Op | Dữ liệu sau op |
| --- | --- |
| A0 | session:u32, rate:u16=16000, codec:u8=1 |
| A1 | session:u32, sequence:u16, predictor:i16, step_index:u8(0..88), sample_count:u16(1..320), ADPCM |
| A2 | session:u32, total_blocks:u16 |
| A3 | session:u32; hủy/lỗi audio |

ADPCM chứa `sample_count-1` nibble, tổng packet A1 dài `12+sample_count/2`.
Mẫu đầu là predictor. App yêu cầu sequence từ 0 liên tục theo modulo 65536,
End khớp số block modulo 65536 và ít nhất 1.600 mẫu, rồi mới dùng kết quả
nhận dạng. Không áp trần tổng số mẫu theo thời lượng câu nói.

Phone → board:

| Op | Dữ liệu sau op |
| --- | --- |
| 10 | session:u32; iPhone đã tạo recognizer, sẵn sàng nhận audio |
| 11 | session:u32, state:u8: 0=hủy, 2=đang xử lý, 3=xong, 4=lỗi |
| 20 | session:u32, field:u8, append:u8, sequence:u16, text UTF-8 |

Field 0=status tối đa127 byte; 1=câu hỏi4095 byte; 2=câu trả lời4095 byte.
Gói đầu append=0/sequence=0 thay nội dung cũ, gói sau append=1 và tăng sequence.
Không chẻ UTF-8 giữa packet. Text dùng Write With Response, hàng đợi có giới
hạn100 packet và timeout5 giây. BLE callback chỉ đổi buffer có mutex; LVGL
đọc ở main thread. UI giữ đồng hồ trên đầu, vuốt ngang giữa bốn trang chính;
AI mở bằng vuốt lên, lưu bốn lượt hỏi/đáp trong RAM và có thể cuộn lịch sử.
Callback đọc cảm ứng chỉ phân loại cử chỉ; thay đổi giao diện và lệnh thu âm
được thực hiện tại tick của main thread.

## iPhone và 9router

Ngữ cảnh thiết bị luôn được gửi trong system message: HUD ESP32-S3, màn hình
240×320 pixel dùng ngang 320×240. Prompt trả lời mặc định yêu cầu tiếng Việt,
đúng trọng tâm, 1–3 câu/khoảng 20–50 từ, văn bản thuần; giải thích dài khi
người dùng yêu cầu. Mục Hướng dẫn AI trên iPhone vẫn cho sửa tùy ý. Bản cập
nhật chỉ thay prompt mặc định cũ, không ghi đè prompt riêng đã lưu.

HUD nhận kết quả rồi tạo hiệu ứng hiện chữ tại chỗ, khoảng 50 ký tự Unicode
mỗi giây; không gửi thêm packet BLE cho từng ký tự và chưa dùng SSE từ API.
Chỉ cập nhật bong bóng trả lời mới nhất, giữ ranh giới UTF-8 để không hiện
nửa ký tự tiếng Việt. Tự cuộn khi đang theo cuối chat; cuộn lên đọc lịch sử
sẽ tạm ngừng bám cuối. Khi có câu hỏi mới, câu trả lời trước hiện đầy đủ.

Kiểm chứng 01/11/2026: firmware và app Release đã build/nạp/cài. ASan/UBSan
kiểm tra vị trí cắt chuỗi tiếng Việt, emoji bốn byte, hết chuỗi và packet UTF-8
chưa đủ. Người dùng xác nhận câu hỏi về thiết bị nhận được trả lời đúng ngữ
cảnh, chữ hiện dần và tự cuộn trên HUD. Phần điều chỉnh prompt trên iPhone
vẫn được giữ nguyên.

Tab AI lưu endpoint/model trong UserDefaults, key gateway trong Keychain
AfterFirstUnlockThisDeviceOnly, scope theo URL. Không gửi key cho ESP32.
Model được chọn qua GET `/models` hoặc nhập thủ công, không đoán tên model.
HTTPS bắt buộc, không chuyển tiếp Authorization khi redirect. Phản hồi giới
hạn256 KiB, không log key/câu hỏi/câu trả lời. Chat không tự retry để tránh
gửi trùng câu hỏi. Một kết quả STT cuối chỉ được submit đúng một lần.

Nhận dạng tiếng Việt dùng SFSpeechRecognizer; cần người dùng cấp quyền và có
thể gửi âm thanh tới Apple. Không hứa nhận dạng offline. iPhone không dùng mic
của chính nó. Câu hỏi/đáp án giữ trong RAM, lịch sử gửi model tối đa4 message.
Kết quả model trả về là chữ, chưa dùng SSE. App đã bổ sung text-to-speech
bằng giọng iPhone, phát qua loa ESP32 theo tùy chọn Giọng đọc AI.

Nhận dạng dài chia thành các phiên Apple Speech khoảng 44 giây. Trong lúc
đợi phiên cũ kết thúc, PCM tiếp tục được đệm (tối đa 12 giây), rồi chuyển cho
phiên kế tiếp. Chỉ gửi một câu hỏi tới AI sau khi người dùng thả tay và nhận
đủ kết quả các phiên. Mất audio quá 5 giây hoặc dịch vụ không hoàn tất trong
12 giây báo lỗi; đây là giới hạn chờ dịch vụ, không phải giới hạn độ dài câu.
Chuyển phiên nhận dạng dài cần kiểm tra thực tế thêm trên iPhone.

Ngày 01/11/2026 bổ sung tác vụ nền hữu hạn theo từng câu hỏi, kết thúc sau
ACK gói trả lời BLE hoặc khi hủy/lỗi/hết thời gian iOS. Người dùng đã xác nhận
nhận câu nói và thấy trả lời trên HUD khi iPhone khóa màn hình, không mở lại
app. Không gắn debugger trong lần thử này. Khả năng hoạt động lâu dài khi
máy khóa, tiếng gió/động cơ và audio đồng thời dẫn đường còn cần thử thêm.

## Kiểm chứng

### Cập nhật giao diện và bộ nhớ 01/11/2026

- Cử chỉ mở/đóng AI, giữ/thả, kéo hủy và giữ 100 giây đã qua kiểm tra native
  C++ ASan/UBSan. App iPhone Release và firmware mới đã build, cài/nạp.
- Sau khi thêm các bong bóng chat, pool LVGL mặc định 64 KiB chỉ còn 2.496
  byte trống. Backtrace xác nhận `lv_realloc` thất bại khi tạo circle mask
  trong `circ_calc_aa4`; assertion mặc định làm board treo im.
- Chuyển cả cấu hình TLSF và backing pool sang 512 KiB PSRAM bằng
  `LV_MEM_SIZE`/`LV_MEM_POOL_ALLOC`. Chỉ thêm một pool 512 KiB sau khởi động
  không đủ: TLSF cấu hình 64 KiB từ chối pool lớn hơn giới hạn của nó.
- Log máy thật đã vượt 80 giây, còn 461.672 byte trống (khoảng 451 KiB),
  không lỗi allocator; một lượt AI đã trả lời. Lần ngắt USB
  sau đó được người dùng xác nhận là tự reset/cắm lại dây.
- Sau khi kết nối lại, log ghi nhận hai lượt chat, không lỗi truyền audio,
  32/32 phản hồi ANCS, vùng nhớ còn 461.416 byte. Đây là kiểm tra ngắn;
  thao tác hủy và nhận dạng câu nói dài đang chờ xác nhận trên máy thật.
- Theo dõi tiếp: tổng cộng bảy phiên thu sau khi cắm lại dây, không lỗi TX;
  cả hai phía ghi nhận một câu 165.760 mẫu (10,36 giây), 518 block, AI trả
  581 byte. Tổng console có năm câu trả lời hoàn tất qua hai lần khởi động.
  Kiểm tra cuối pool còn 460.704 byte trống và `lv_mem_test()` thành công.
- Người dùng xác nhận giữ nói liên tục 15–20 giây, thả tay và nhận được câu
  trả lời; kéo xuống khi đang giữ đã hủy đúng, không gửi câu nói cho AI.
  Câu nói dài tới mức chuyển phiên Apple Speech (khoảng 44 giây) vẫn chưa
  được kiểm chứng trên máy thật.

### Luồng AI trước khi đổi giao diện

- Build firmware và iPhone Release thành công. Board báo codec=1 sau nạp.
- C++ ASan/UBSan và Swift kiểm tra cùng golden vector IMA ADPCM, saturation,
  packet sai độ dài/index/count, UTF-8 có emoji ở ranh giới fragment.
- Endpoint do người dùng cung cấp trả HTTP403 cho GET chưa kèm key. Chưa coi
  đây là bằng chứng key hợp lệ hay chat hoạt động; cần kiểm tra trong app.
- Người dùng xác nhận tải/chọn được model từ endpoint với key nhập trong app,
  và câu nói qua mic board đã được nhận dạng đúng. Như vậy codec, truyền audio,
  giải mã và nhận dạng đã qua bước thử foreground trên máy thật.
- Người dùng xác nhận câu trả lời AI đã xuất hiện trên HUD. Luồng đầy đủ
  mic board → BLE → nhận dạng tiếng Việt → 9router/model → BLE → ESP32 đã được
  xác nhận hoạt động ở foreground. Chưa coi đây là kiểm chứng khi khóa máy,
  đi ngoài đường có gió/động cơ hoặc chạy song song với điều hướng lâu dài.

Nguồn: mẫu `Example_17_echo` trong bộ tài liệu board (chỉ dùng như tham khảo
phần cứng), [Apple Speech](https://developer.apple.com/documentation/speech/sfspeechrecognizer),
[9router integration](https://github.com/decolua/9router/blob/master/gitbook/content/en/integration/other-tools.md).

### Modal touch handling regression (2026-11-01)

`AiGesture` suppresses recognition while notification confirmation or pairing
panels are visible. Suppressing AI must not consume an ordinary contact:
otherwise neither confirm nor cancel can receive LVGL clicks, and the modal
also disables page swipes. The main loop and BLE remain alive, making this
look like a freeze (dismiss counters stay at zero).

A `Suppressed` state now passes modal contacts through and waits for release
before recognizing a fresh AI gesture. Contacts already owned by an AI
recording remain consumed until release and produce one cancellation. Wake
contacts are still swallowed separately by `readTouchInput`.

`test/voice_protocol_test.cpp` verifies modal taps, hiding a modal mid-contact,
subsequent hold-to-talk and interruption of an active recording. These checks
fail against the previous handler and pass with AddressSanitizer/UBSan.
