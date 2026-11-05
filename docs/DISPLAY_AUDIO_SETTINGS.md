# Màn hình và giọng đọc — 01/11/2026

## Cài đặt trên iPhone

Tab Cài đặt có độ sáng HUD 5–100%, âm lượng **loa ESP32** 0–100%, hướng
bình thường hoặc lật **180°**. Không có tùy chọn dọc 90°/270°. Bấm áp dụng
mới ghi flash; nút Nghe thử loa phát tiếng bíp ngắn. Các giá trị nằm ở key
Preferences `hardware`, version 1, không đổi layout lưu theme/mặt đồng hồ/
timeout/bộ lọc cũ. Lật dùng TFT rotation 1 ↔ 3, giữ canvas 320×240 và đổi
tọa độ FT6336 tương ứng; cử chỉ/chạm đầu đánh thức vẫn giữ nguyên.

GPIO45 dùng LEDC channel 2, 5 kHz, 8 bit. Khi ngủ duty=0; đánh thức khôi phục
đúng mức sáng đã chọn. GPIO1 là SHUTDOWN của SC8002B: HIGH tắt, LOW phát.
ES8311 DAC register 0x32 có 0xBF=0 dB; phần mềm ánh xạ âm lượng theo biên độ,
không sử dụng vùng boost +32 dB. 0% đồng thời mute DAC. Không đổi gain mic.

## Phản hồi cài đặt

Kênh a103 vẫn dùng opcode/sequence cũ. Bổ sung:

- `14,seq`: đọc hardware; reply `94,seq,status,1,brightness,volume,rotation`.
- `15,seq,brightness,volume,rotation`: lưu hardware; reply cùng dạng, op95.
- `16,seq`: âm thử; reply `96,seq,status` (3=bận/chưa sẵn sàng).

App từng bị khóa các nút khi chưa nhận ACK notify. Bản hiện tại đọc lại sau
Write ACK để xác nhận **kết quả xử lý**, không coi ACK đường truyền là đã lưu.
OnRead a103 trả core snapshot 7 byte, hardware 3 byte, một byte độ dài rồi
RPC reply gần nhất (tối đa160 byte). Tổng tối đa171 byte. App đối chiếu op và
sequence với request hiện tại, rồi dùng cùng parser reply. Khi chưa khớp,
chỉ đọc lại, không gửi lại lệnh thay đổi. Notify vẫn gửi payload trực tiếp
để callback onRead không thay RPC reply bằng snapshot.

## Giọng đọc AI

Tab AI có bật/tắt tự đọc, chọn giọng do `AVSpeechSynthesisVoice.speechVoices()`
cung cấp, lưu giọng, nghe thử và dừng đọc. Mặc định tiếng Việt. iPhone dùng
`AVSpeechSynthesizer.write` tạo PCM, chuyển về mono 8 kHz bằng AVAudioConverter,
giữ headroom đỉnh 18000/32768 và mã hóa IMA ADPCM. Không gọi API TTS 9router.
Âm thanh chỉ nằm trong RAM; không lưu file hay log nội dung.

Kênh a105, số nguyên little-endian:

| Op | Dữ liệu sau opcode |
| --- | --- |
| 40 | playback:u32, rate:u16=8000, codec:u8=1, total_samples:u32 |
| 41 | playback:u32, sequence:u16, predictor:i16, step_index:u8, count:u16, ADPCM |
| 42 | playback:u32, total_blocks:u16 |
| 43 | playback:u32; hủy |
| B1 | playback:u32, phase:u8, consumed_blocks:u16 |

Phase 0=ready, 1=credit, 2=done, 3=error. Block tối đa320 mẫu (40 ms).
Chỉ block41 dùng Write Without Response và tôn trọng canSend/credit; Start,
End, Cancel, text và các lệnh điều khiển vẫn dùng Write With Response.
Giữ step index qua ranh giới block, nhưng ghi predictor/index ở từng header
để mỗi packet tự giải mã được. Sender tối đa12 block chưa được tiêu thụ;
board queue24 block, chờ min(10,tổng block) rồi phát. Chỉ gửi End sau credit
xác nhận toàn bộ block đã tiêu thụ, tránh End vượt các write command. Worker
giữ I2S 16 kHz/stereo, lặp mỗi mẫu 8 kHz hai lần và xuất hai kênh giống nhau.
Chờ amp ổn định trước âm đầu; giữ sáng màn hình khi phát.

Giữ mic trên HUD tắt amp ngay, hủy playback trước khi bắt đầu thu. Mất kết
nối/hủy/timeouts dọn tác vụ và queue. Kênh bản đồ tạm nhường BLE trong khi
thu/phát. Giới hạn PCM90 giây, thời gian tạo giọng20 giây; xử lý nền vẫn chịu
giới hạn iOS. Lỗi giọng đọc giữ nguyên câu trả lời bằng chữ.

## Kiểm chứng

- Firmware và app Release đã build/nạp/cài. C++ ASan/UBSan qua phép biến đổi
  chạm, PWM, gain không boost và gesture; Swift qua ADPCM golden vector,
  giới hạn packet và thử sai số dạng sóng khi giữ trạng thái giữa các block.
- Người dùng đã xác nhận nghe rõ tiếng bíp, xác nhận có giọng tiếng Việt
  nhưng bản đầu bị đứt/rè. Đã bổ sung giữ quantizer state, tăng đệm và
  headroom. Log sau đó xác định nghẽn Write With Response: 5.393 ms âm thanh
  phát trong7.698 ms,29 lần chờ. Chuyển riêng block âm thanh sang Write
  Without Response: ba lượt preview73 block,2.905 ms âm thanh phát trong
  3.019 ms,0 lần chờ và0 lỗi. Chất lượng nghe đang chờ người dùng xác nhận.
- Cơ chế read-back đã trả đủ hardware/app catalog, nhận lưu âm lượng và
  lệnh nghe thử. Người dùng xác nhận độ sáng, lật180° và chạm/vuốt đều đúng.

Tham khảo: schematic/ES8311_DS.pdf và Example_17_echo trong bộ tài liệu hãng;
[Apple speech buffers](https://developer.apple.com/documentation/avfaudio/avspeechsynthesizer/write(_:tobuffercallback:)),
[Apple sample-rate conversion](https://developer.apple.com/documentation/technotes/tn3136-avaudioconverter-performing-sample-rate-conversions),
[SC8002B datasheet](https://www.chipsourcetek.com/DataSheet/SC8002B.pdf).

## Edge TTS voices (2026-11-01)

The iPhone AI voice settings now default to Edge TTS with Vietnamese Hoài My
(`vi-VN-HoaiMyNeural`, female) and Nam Minh (`vi-VN-NamMinhNeural`, male).
The existing iPhone voice picker remains available as a separate source.
Save the selection and use “Nghe thử giọng trên HUD”. No additional TTS API key
or running Mac is required. Internet is required: the text to read is sent to
Microsoft's online Read Aloud endpoint. This is the unofficial Read Aloud
interface used by edge-tts, so availability may change.

`EdgeSpeech.swift` uses a dedicated TLS/WebSocket connection and v2 Sec-MS-GEC
protocol; AI provider credentials are never attached. Downloads are bounded to
1 MiB and 45 seconds per attempt, with one retry. MP3 decoding rejects audio
longer than 90 seconds. Temporary MP3 files are protected until first unlock
and deleted after decoding. Decoded audio uses the existing 8 kHz ADPCM speaker
transport; holding the mic cancels synthesis/playback. Network failure keeps
the text answer and reports an error; selecting iPhone voices is explicit.

Protocol references: https://github.com/rany2/edge-tts and
https://github.com/hs-CN/msedge-tts/blob/master/src/tts/drm.rs . The MIT notice
from msedge-tts is included in the application bundle.

Validation:

```
swiftc -parse-as-library ios/HUDNavigator/EdgeSpeech.swift \
  ios/Checks/EdgeSpeechChecks.swift -o /tmp/hud-edge-checks
/tmp/hud-edge-checks
/tmp/hud-edge-checks --online
```

Golden tokens match upstream v2 for two fixed timestamps. Checks cover XML
escaping, malformed audio frames and rejecting unsupported voice identifiers.
Both Vietnamese voices were synthesized directly by Swift and their MP3 data
decoded using AVAudioFile. Actual board sound quality needs a listening check.
