# HUD Connect — cài đặt ESP32

## Độ sáng, loa board, lật180° và giọng đọc AI

Trong Cài đặt, chỉnh độ sáng5–100%, âm lượng **loa ESP32**0–100%, chọn bình
thường hoặc lật180° rồi bấm Áp dụng màn hình và loa. Có nút Nghe thử loa.
Các giá trị lưu trên board; không đổi âm lượng nhạc iPhone. Người dùng đã
xác nhận âm báo, độ sáng, hướng180° và cảm ứng hoạt động đúng.

Trong AI, bật Đọc câu trả lời qua loa HUD, chọn giọng có trên iPhone và Lưu
giọng đọc. Nút Nghe thử giọng trên HUD không cần gọi model. iPhone tạo giọng,
gửi ADPCM qua BLE cho board phát; giữ mic HUD để dừng lời đọc và hỏi tiếp.
Chất lượng giọng đọc đang kiểm tra lại sau sửa tốc độ truyền; chi tiết và
các giới hạn ở [Màn hình và âm thanh](../docs/DISPLAY_AUDIO_SETTINGS.md).

## Kết nối lại và AI khi chạy nền (01/11/2026)

App đọc quyền Speech trực tiếp từ iOS khi khởi động/trở lại foreground;
chỉ yêu cầu quyền nếu chưa từng chọn. Không dùng cờ RAM mặc định false để
hiện nút cấp quyền sau mỗi lần mở lại.

Bluetooth ưu tiên HUD đang kết nối với hệ thống trước bản ghi peripheral cũ,
khôi phục kênh khi về foreground và tự thử lại có giãn nhịp khi lỗi. GATT
khôi phục chỉ bắt đầu sau khi central poweredOn. Timeout không chuyển sang
chỉ quét quảng bá vì HUD có thể đang giữ kết nối ANCS và không quảng bá nữa.

Mỗi câu hỏi AI xin một `beginBackgroundTask` trước khi khởi tạo nhận dạng,
giữ đến khi hoàn tất và ACK các gói trả lời BLE. Hủy/mất kết nối/lỗi/hết thời
gian iOS đều dọn phiên; không phát âm thanh im lặng hay bật mic iPhone để
giữ app sống. Đây là thời gian xử lý nền hữu hạn, chưa phải bảo đảm luôn hoạt
động khi máy khóa. Cần kiểm chứng bằng bản Release không gắn debugger, sau
khi app đã vào nền đủ lâu rồi mới bắt đầu nói từ HUD. Không vuốt tắt app.

Trace cục bộ `Library/Caches/HUDRuntime.log` giới hạn khoảng 24 KB, chỉ ghi
trạng thái kết nối, quyền, appState, vòng đời phiên và số mẫu/byte. Không ghi
key, prompt, câu nói hay câu trả lời. Có thể lấy riêng file này bằng devicectl
để kiểm tra chạy nền mà không gắn debugger.

Kiểm chứng 01/11/2026: bản Release đã build/cài; ba lần terminate/relaunch
liên tiếp đều tự nhận HUD (`navigation ready=true`) và quyền Speech đã cấp
(`authorization=3`), không tắt/bật Bluetooth. Trace ghi nhận bắt đầu câu nói
khi `appState=2`, nhận dạng thành chữ và gửi yêu cầu AI. Người dùng xác nhận
thử khóa iPhone theo hướng dẫn chờ khoảng một phút: HUD nhận câu nói và hiện
câu trả lời mà không mở lại máy. Chưa coi đây là kiểm chứng qua đêm, sau force
quit hoặc nói nhiều phút liên tục; giới hạn thời gian nền của iOS vẫn áp dụng.

Tham khảo [Apple: tác vụ nền hữu hạn](https://developer.apple.com/documentation/uikit/extending-your-app-s-background-execution-time),
[Core Bluetooth chạy nền](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/CoreBluetoothBackgroundProcessingForIOSApps/PerformingTasksWhileYourAppIsInTheBackground.html).

## Trợ lý AI với mic trên board

### Điều khiển HUD bằng câu nói

AI có thể thực hiện một lệnh mỗi lượt, áp dụng cả câu hỏi qua mic HUD và ô
nhập chữ trên iPhone:

- Nhạc: “Dừng nhạc”, “Phát nhạc”, “Bài tiếp theo”, “Bài trước”, “Giảm âm
  lượng”, “Đang nghe bài gì?”. Cần trình phát iPhone đang cung cấp kênh AMS.
  Âm lượng tăng/giảm một bước, chưa đặt phần trăm tuyệt đối.
- Thông báo: “Có bao nhiêu thông báo?”, “Đọc thông báo mới nhất”, “Liệt kê
  thông báo”, “Tóm tắt thông báo”. Chỉ các mục HUD đã nhận; liệt kê/tóm tắt
  ba mục mới nhất. Tóm tắt gửi dữ liệu ba mục đến model; đọc/liệt kê không
  chuyển nội dung thông báo cho model.
- Cài đặt: “Đổi nền sáng”, “Đổi mặt đồng hồ kim”, “Cho màn hình sáng một
  phút”, “Luôn sáng màn hình”, “Mở trang nhạc”. Thay đổi lưu vào HUD và đồng
  bộ lại app; mở bản đồ chỉ đổi trang, không bắt đầu tuyến.

App kiểm tra lệnh/tham số, chờ kết quả thiết bị rồi mới trả lời. Lệnh nhạc
báo đã gửi tới iPhone, không giả định đã phát thành công. Lỗi/chưa có dữ liệu
được báo rõ. Không tự retry lệnh thay đổi khi mất ACK. Chưa hỗ trợ gọi điện,
gửi/xóa tin nhắn, xóa thông báo bằng AI hay điều hướng trong giai đoạn này.

Kiểm chứng: firmware/app Release đã build/nạp/cài. Bộ HUDCoreChecks kiểm tra
whitelist, JSON không hợp lệ, tham số ngoài phạm vi/boolean/số lẻ, mã lệnh,
packet trả sai ID/thứ tự và tiếng Việt bị chia giữa hai gói. Người dùng đã
xác nhận đọc thông báo mới nhất đúng tên ứng dụng, tiêu đề và nội dung.
Người dùng cũng xác nhận đổi nền sáng và đặt thời gian sáng một phút bằng
giọng nói thành công, với timeout 60 giây đồng bộ về Cài đặt trong app.

Tab AI có mục **Hướng dẫn AI (prompt)**: sửa vai trò, cách xưng hô, ngôn ngữ
hoặc độ dài rồi bấm **Lưu prompt**. Prompt được lưu trên iPhone, dùng làm
system message cho cả mic HUD và câu hỏi nhập chữ từ lần hỏi tiếp theo.
Đổi prompt xóa ngữ cảnh gửi model cũ; lịch sử đang hiển thị vẫn giữ nguyên.
**Khôi phục mặc định** lưu lại hướng dẫn ngắn gọn bằng tiếng Việt. Để trống
rồi lưu cũng dùng mặc định. Các nút chỉnh prompt tạm khóa trong khi AI xử lý.

Ngữ cảnh ESP32-S3 và màn hình 240×320 (xoay ngang 320×240) luôn được gửi kèm;
phần hướng dẫn trả lời phía trên vẫn sửa tùy ý. Mặc định mới ưu tiên đúng chủ
đề, 1–3 câu/khoảng 20–50 từ. Nâng cấp giữ prompt riêng đã lưu, chỉ thay nội
dung mặc định cũ. HUD có hiệu ứng hiện dần khoảng 50 ký tự/giây và tự cuộn
theo cuối câu trả lời; cuộn lên xem lại sẽ ngừng bám cuối. Hiệu ứng chạy trên
HUD sau khi nhận nội dung, API vẫn trả JSON hoàn chỉnh (chưa dùng SSE).

App có tab AI: endpoint HTTPS, key gateway lưu Keychain và model lấy từ 9router
hoặc nhập thủ công. Endpoint mặc định `https://api.proxy1990.id.vn/v1`; nhập key
trực tiếp trên iPhone, Lưu → Tải model → chọn model → Lưu. Bấm Cho phép nhận
dạng giọng nói. Trên ESP32, vuốt từ mép dưới lên để mở Trợ lý AI, giữ vùng
nội dung để nói, thả để gửi hoặc kéo xuống khi đang giữ để hủy.
Mic ES8311 truyền ADPCM qua BLE, iPhone giải mã và nhận dạng tiếng Việt, sau
đó gọi chat bằng text. HUD hiện bong bóng hỏi/đáp, cuộn dọc để đọc bốn lượt
gần nhất. Vuốt xuống từ phần đầu để quay về trang trước.
Không dùng mic iPhone; không yêu cầu nhập key trong firmware hay qua chat.
Đã bỏ mốc 10 giây; thu tới khi thả. Câu trả lời yêu cầu ngắn, tối đa4095 UTF-8
byte trên HUD. Chuyển phiên Apple Speech cho câu nói dài còn cần thử máy thật.
Thử ban đầu khi app đang mở; khóa màn hình/tiếng gió chưa được nghiệm thu.
Chi tiết và kết quả kiểm tra ở [AI voice protocol](../docs/AI_VOICE_PROTOCOL.md).
Người dùng đã xác nhận tải/chọn model thành công, nhận đúng câu nói qua mic
board và thấy câu trả lời AI trên ESP32. Luồng foreground đã hoạt động trên
máy thật; các giới hạn về khóa màn hình và môi trường chạy xe vẫn như trên.

## Danh sách thông báo ba dòng

Mỗi mục gồm tên ứng dụng 14 px và giờ ở dòng đầu, tiêu đề 18 px màu chữ chính,
nội dung 18 px màu dịu ở dòng cuối. Font nhỏ vẫn hỗ trợ tiếng Việt. Tiêu đề và
nội dung chỉ một dòng, dài thì dấu “…”; xuống dòng/tab từ iPhone được gộp thành
khoảng trắng trong bản xem trước. Chạm mở chi tiết vẫn xem nội dung đầy đủ.
Không thay đổi thứ tự mới nhất trước, giờ thông báo hoặc hành động xóa.

## Tương tác bản đồ trên ESP32

Khung hướng rẽ trong ảnh MapKit tự xuống dòng và đo chiều cao theo nội dung,
không dùng truncating-tail. Chỉ dẫn ngắn nằm cạnh khoảng cách; câu dài được
chuyển xuống dưới, dùng toàn chiều ngang. Với câu rất dài, chữ thu nhỏ có giới
hạn để khung không đè nút điều khiển hay attribution; bản đầy đủ luôn có trong
trang Chỉ đường của iPhone. Thay đổi này chỉ cần cập nhật app.

Sau khi bắt đầu dẫn đường, Maps có +/− để zoom, icon bốn mũi tên bật/tắt chế độ
kéo, và icon định vị để quay lại bám GPS. Trong chế độ kéo, kéo rồi thả mới gửi
thao tác sang iPhone để tạo nền mới. Thanh đầu hiển thị “KÉO BẢN ĐỒ”; vuốt trên
thanh đồng hồ phía trên vẫn đổi trang. Khi không bật kéo, vuốt ngang trên bản
đồ đổi trang như trước. Đây là thao tác một ngón, không phải pinch hai ngón.
Trong chế độ xem tự do, camera ở vị trí đã chọn; chỉ dẫn và tốc độ vẫn theo GPS
thật. Mỗi thao tác dùng projection của đúng frame đang hiện, tránh kéo sai hướng
khi bản đồ xoay. Các callback camera cũ không được ghi đè camera mới.
Cần app và firmware mới để có điều khiển; giữ nguyên UUID/kênh GATT hiện tại.
Các kiểm tra Swift đã bao gồm packet sai độ dài/opcode/giới hạn, số âm, frame ID,
đảo chiều kéo và camera xoay 90°. Đã cài bản Release lên iPhone và nạp firmware;
người dùng xác nhận zoom, kéo và về GPS đều hoạt động. Log máy thật ghi nhận
điều khiển zoom, bật xem tự do, kéo và về GPS; 15 khung ảnh được xác nhận nhận
thành công trong lượt kiểm tra. Chưa kiểm tra tương tác khi chạy xe/khóa máy lâu.

## MapKit: nhịp cập nhật và dán điểm đến (31/10/2026)

MapKit là nguồn dẫn đường chính. Đã bỏ nút dùng thông báo Google Maps khỏi
giao diện; vẫn nhận điểm đến chia sẻ từ Google Maps/Apple Maps và tính tuyến
bằng MapKit. Trang Chỉ đường có nút Dán của hệ thống, nhận URL và văn bản:
Google Maps → Chia sẻ → Sao chép liên kết → HUD Connect → Dán liên kết địa
điểm → xem tuyến → Bắt đầu trên HUD. Không đọc clipboard tự động.
Đường dẫn rút gọn chỉ được mở tới phần header/redirect; không tải toàn bộ HTML.

Luồng bản đồ cũ vừa đợi snapshot vừa đợi BLE, với timer 3 giây và điều kiện
`elapsed > 3`, có thể bỏ lỡ một nhịp timer. Luồng mới:

- GPS/chỉ dẫn vẽ lại trên snapshot gần nhất, tối đa 1 khung/giây khi di chuyển.
  Không xếp hàng ảnh: khi BLE đang bận, khung sau dùng vị trí mới nhất.
- Nền bản đồ làm mới độc lập: tối thiểu 1,5 giây khi đi thêm 18 m hoặc đổi
  hướng 12°, và mỗi 8 giây khi đứng yên. Giữ nguyên attribution MapKit.
  Snapshot quá 8 giây được hủy, callback từ chuyến/yêu cầu cũ bị bỏ qua.
- Khi đứng yên, gửi heartbeat tối đa mỗi 5 giây để board không báo mất dữ liệu.
  Chỉ gửi khi GPS còn mới trong 10 giây; không nội suy vị trí hay tốc độ giả.
- Chiếu polyline một lần cho mỗi nền thay vì mỗi khung; mũi tên dùng hướng GPS
  tương đối với hướng snapshot. JPEG chất lượng 0,55, giảm 0,42 nếu trên 12 KiB.
- BLE đợi cả ACK của firmware và callback ghi gói cuối trước khi cho gửi khung
  tiếp theo. Các bản ghi đo chỉ có thời gian, số byte và kết quả, không tọa độ.

Kiểm tra trên iPhone 14 Pro và ESP32 thật khi đứng yên: 28/28 khung được nhận,
tạo nền trung vị 274 ms (cao nhất 403 ms), gửi JPEG khoảng 12 KiB trung vị
502 ms (cao nhất 810 ms).
Người dùng xác nhận bản đồ hiện. Các kiểm tra Swift về nhịp cập nhật, heading
qua 0°, heartbeat, GPS, chia sẻ URL và gói BLE đều qua. Đây không phải phép đo
độ trễ từ GPS đến màn hình khi chạy xe; vẫn cần thử di chuyển/khóa màn hình.
Không cần đổi giao thức hay nạp lại firmware để dùng bản tối ưu này.

## Tên ứng dụng lấy từ iPhone

ANCS Get App Attributes / DisplayName trả tên hiển thị thật theo iPhone cho các
app đang gửi thông báo và các app đã lưu trong bộ lọc. HUD lưu tạm 32 tên trong
RAM, tự cập nhật danh sách/chi tiết/popup và gửi tên cho HUD Connect. Các tên
không phải thay bằng cách đoán phần cuối bundle ID. Chỉ fallback về bảng tên
phổ biến hoặc bundle ID nếu iPhone chưa trả/không cung cấp tên.
Mẫu phản hồi tên được kiểm thử ở mọi ranh giới fragment, UTF-8 bị cắt và ID
không khớp. Bộ lọc vẫn dùng bundle ID ổn định, tên hiển thị chỉ là nhãn UI;
không đổi cấu trúc NVS hay làm mất cài đặt đã lưu. Lệnh xóa được ưu tiên hơn
các truy vấn tên đang chờ, nhưng không ngắt giữa một phản hồi thuộc tính.
Thử máy thật: 16 yêu cầu tên/16 phản hồi, không lỗi, 15 tên có nội dung đã được
lưu trong cache. Người dùng xác nhận tên hiển thị đúng trên HUD và bộ lọc app.

## Bổ sung đồng hồ, lịch âm, tốc độ và hộp thư

- Đồng hồ HH:mm nhỏ cố định trên đầu cả bốn trang. Trang Đồng hồ có ngày âm
  Việt Nam theo UTC+7 (tính cục bộ từ giờ đã đồng bộ, giới hạn 2000–2099).
  Pin được bỏ qua theo yêu cầu mới vì người dùng hiện chưa gắn pin.
- Tốc độ GPS từ iPhone gửi qua characteristic `ee43a104…`. Board hiển thị km/h
  trên trang Maps, hết hạn sau 5 giây. App loại fix quá 5 giây, sai số ngang
  trên 25 m, speedAccuracy âm hoặc trên 3 m/s, và giá trị ngoài 0–300 km/h.
  Tốc độ dưới 0,5 m/s được xem là 0 để giảm nhiễu đứng yên; không thay thế đồng
  hồ xe. Chưa nghiệm thu độ chính xác khi chạy ngoài đường.
- Thông báo thành danh sách tối đa 10 mục có tên app, tiêu đề và giờ, sắp mới
  nhất trước theo ngày iPhone cung cấp (không lấy thứ tự replay ANCS); chạm xem
  chi tiết, nút mũi tên để quay lại. Chi tiết có giờ iPhone cung cấp nếu có và
  giờ HUD nhận lần đầu. Thông báo modified giữ nguyên giờ nhận lần đầu.
- Nút icon làm mới, xóa từng mục và xóa tất cả có hộp xác nhận. Chỉ gửi ANCS
  negative action khi iPhone báo có action và nhãn chính xác là hành động clear/
  dismiss được chấp nhận. Không gửi negative action cho cuộc gọi đến hoặc nhãn
  Decline/Delete conversation. UID và action bị ràng buộc với phiên ANCS;
  làm mới, đổi dịch vụ hoặc mất kết nối vô hiệu hóa lệnh từ phiên cũ.
- Mục được xóa trên iPhone chỉ biến mất khi nhận sự kiện Removed. Lỗi gửi hoặc
  chưa được xác nhận sau 8 giây sẽ báo lại, không giả vờ đã xóa. Mục không hỗ
  trợ có xác nhận chỉ ẩn trên HUD. Xóa tất cả giải thích rõ các mục không hỗ trợ
  chỉ ẩn HUD. Giữ tối đa 64 UID đã ẩn trong RAM của phiên và 64 dấu nhận diện
  nội dung/ngày nguồn để không lấy lại cùng mục khi làm mới/nối lại. Dấu này
  không lưu qua mất nguồn; nếu iPhone không cung cấp ngày nguồn thì chỉ ẩn
  được trong phiên UID hiện tại để tránh vô tình ẩn một tin mới có cùng chữ.

Nguồn: [Apple ANCS actions](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html),
[thuộc tính Date và action labels](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Appendix/Appendix.html),
[quy tắc lịch âm Hồ Ngọc Đức](https://honguyenviet.com/amlich/calrules.html).

Kiểm tra máy tính: parser metadata theo mọi ranh giới fragment, nhãn xóa/
từ chối, các mốc Tết 2024–2026 và tháng 2 nhuận 2004, giới hạn ngày và dữ liệu
tốc độ đều qua. Trên phần cứng, người dùng đã xác nhận đồng hồ nhỏ, danh sách,
chi tiết và thời gian hiển thị; đã xóa một mục khỏi cả iPhone/HUD, làm mới không
trở lại, và thứ tự mới nhất trước đúng. Xóa tất cả dùng cùng hàng lệnh có giới
hạn theo các mục trong danh sách HUD; chưa có xác nhận riêng thao tác xóa hàng
loạt hoặc nghiệm thu tốc độ khi di chuyển ngoài đường.

## Giao diện HUD không có hàng tab

Firmware dùng vuốt trái/phải để chuyển vòng Đồng hồ → Thông báo → Bản đồ → Nhạc.
Tên trang và bốn chấm chỉ vị trí nằm trên đầu; không còn bốn nút chuyển trang
ở dưới. Vùng nội dung 300×200, ảnh native MapKit cũng gửi 300×200 (phải cài app
và firmware cùng phiên bản). Chạm đánh thức vẫn được nuốt để không bấm nhầm;
vuốt đổi trang sẽ chờ nhả tay để không kích hoạt nút nhạc dưới ngón tay.

Trong Cài đặt có 8 mặt: Rõ nét, Sắc màu, Kim hiện đại, Có giây, Ngày lớn,
Kim cổ điển, Tốc độ số và Tốc độ kim; và 8 bảng màu: 4 tối, 4 sáng (Trắng
xanh, Bạc hà, Kem, Hồng sáng). Hai mặt tốc độ dùng GPS iPhone: đứng yên có
dữ liệu hợp lệ hiện `0`, còn GPS/Bluetooth mất hoặc dữ liệu cũ hiện `---`.
Nền sáng đổi cả nền, chữ, nút và màu bản đồ; tuyến/vị trí màu xanh trên nền sáng.
Thông báo có vùng đọc cao hơn và tiêu đề 24 px, tên bài hát 24 px, nút nhạc
cao 48 px. Chọn giao diện/màu rồi bấm Áp dụng lên HUD như trước.

Đã build/cài app và firmware, kiểm thử giao thức JPEG 300×200 cùng CRC/bounds
và parser link đều qua. Trong lúc nâng cấp, app cũ 300×150 tạm bị firmware mới
từ chối; sau khi cài đồng bộ app mới, người dùng xác nhận bản đồ đã hiện lại
và vuốt trái/phải đổi các trang hoạt động trên board thật.

## Bản hiện tại: tìm đường trong app

### Nhận điểm đến và chọn trên bản đồ

- Google Maps/Apple Maps → Chia sẻ địa điểm → **HUD Connect** → **Gửi điểm đến**.
  Sau đó mở HUD Connect: app nhận điểm và mở xem tuyến, hoặc hiển thị kết quả
  cần chọn nếu liên kết chỉ có tên/địa chỉ. Share extension không tự ép mở app.
- **Chọn điểm trên bản đồ**: kéo/phóng bản đồ, chạm đặt ghim rồi bấm
  **Chọn điểm này** để tính tuyến tới tọa độ đã chọn.

Người dùng đã xác nhận cả nhận đúng điểm qua Chia sẻ và chọn điểm/xem tuyến
trên bản đồ trong lần thử 31/10/2026. Bộ kiểm tra parser cũng đã qua Apple URL,
Google URL, URL directions, giới hạn tọa độ, domain giả và phân biệt tâm camera
`/@lat,lon` với tọa độ địa điểm `!3d..!4d..`. Các dạng link chưa nhận diện sẽ báo
lỗi để tìm/chọn lại, không lấy tâm camera làm điểm đến.

Target HUDShare nhúng trong app chính, dùng Keychain Access Group chung để
chuyển một địa điểm chờ; không cần App Groups hay máy chủ riêng. Đã build/ký
và thử bằng Personal Team hiện tại. Link được xóa khỏi hộp chờ khi app đọc,
hết hạn sau 24 giờ, không in nội dung link/tọa độ lên console. Link ngắn chỉ
theo redirect trong nhóm domain bản đồ được chấp nhận, timeout 15 giây.

### Nền bản đồ kèm thông báo Google

Bật **Theo chỉ dẫn Google Maps** trong HUD trước, rồi mở Google Maps để bắt
đầu chỉ đường. HUD gửi nền MapKit quanh GPS và mũi tên vị trí, không vẽ tuyến
MapKit trong chế độ này. ESP ghép thông báo Google nhận qua ANCS lên trên ảnh.
Không có dữ liệu toàn tuyến Google trong thông báo. Cần bật Google Maps trong
bộ lọc HUD và iPhone phải cung cấp thông báo/xem trước. Khi chưa có thông báo
HUD báo đang chờ; nội dung có sẵn trước kết nối hoặc cũ quá 2 phút không được
trình bày như chỉ dẫn mới. Thông báo bị xóa hoặc bị lọc sẽ bỏ khỏi lớp chỉ dẫn.
Giới hạn này không thay thế kiểm thử chạy xe, nền/khóa máy hoặc cập nhật rẽ thật.

Người dùng đã xác nhận nền bản đồ và chỉ dẫn Google cùng xuất hiện trên ESP32
trong lần thử này. Chưa thử di chuyển ngoài đường hoặc khóa máy dài; nhịp vẽ
lại dự kiến khoảng 3 giây khi GPS mới và BLE sẵn sàng, ảnh quá 15 giây không
được nhận sẽ chuyển sang trạng thái dữ liệu cũ.
Bộ đếm Serial trong chế độ external=1 tăng từ 14 lên 15 khung trong hai lần
đọc cách nhau khoảng 7 giây, rejected=0: ảnh tiếp tục được gửi, không chỉ một
khung cố định. Điều này chưa chứng minh độ chính xác GPS khi di chuyển.

Người dùng đã chấp nhận tìm điểm đến trong HUD. App có hai tab **Chỉ đường** và
**Cài đặt**. Tab đầu tìm địa điểm/địa chỉ, chọn kết quả, xem bản đồ tuyến cùng
quãng đường/thời gian rồi bấm **Bắt đầu trên HUD**. Nút bắt đầu chỉ hoạt động
khi có tuyến và HUD kết nối; chọn kết quả không tự bắt đầu dẫn đường.
Lưu tối đa 5 điểm đã bắt đầu đi trên iPhone để chọn lại; không gửi lịch sử lên
máy chủ riêng. Có nút mở Settings khi bị từ chối vị trí và nút thử tính tuyến lại.
Đang dẫn đường có ảnh HUD, hướng dẫn, tên điểm đến và nút kết thúc có xác nhận.

Firmware tự mở tab Maps khi nhận khung đầu, bật và giữ sáng trong khi tab này
có bản đồ mới. Rời tab hoặc dữ liệu cũ quá 15 giây sẽ dùng lại timeout đã cài.
Thông báo Maps không ghi đè bản đồ trực tiếp trong chuyến đi. Khi không có
luồng bản đồ riêng, tab vẫn nhận thông báo Google Maps/Apple Maps như trước.

Tuyến dùng MapKit ô tô/đi bộ, **chưa có xe máy**; tuyến ô tô có thể đi qua đường
cấm xe máy. App tự tính tuyến, không đọc tuyến của ứng dụng Maps đang mở.
Luồng xem trước/bắt đầu mới đã thử trên iPhone thật: người dùng xác nhận
ESP32 đã hiện bản đồ và tuyến; Serial ghi nhận khung ảnh được nhận, không bị
từ chối. Đã kiểm tra GPS và kết nối BLE. Chưa có nghiệm thu di chuyển ngoài
đường/khóa máy lâu.

## Ghi chép các quyết định trước đó

**Cập nhật sau thử thực tế:** người dùng đã thấy hình chia sẻ trên ESP32 nhưng
ảnh dọc quá nhỏ và chứa toàn màn hình iPhone, không đáp ứng chỉ đường riêng.
Đã bỏ giao diện chia sẻ và gỡ extension khỏi gói app chính. Mã HUDScreen chỉ
giữ làm tham khảo, không được build/nhúng khi chạy scheme HUDNavigator.
App chính hiện chỉ có settings. Người dùng đã chọn giữ đọc thông báo Maps:
firmware nhận Google Maps và Apple Maps, hiển thị tiêu đề/nội dung mới nhất
trong tab mũi tên bằng font 24 px, không tự đoán hướng rẽ hay khoảng cách.
Cần bật app tương ứng trong bộ lọc và iPhone phải thực sự phát thông báo ANCS.
Chưa có cách được xác minh để tự lấy tuyến
Google Maps/Apple Maps đang chạy. Phương án dẫn đường riêng cần được người
dùng chấp nhận trước khi đưa lại vào giao diện.

## Cài đặt thiết bị

App chính quản lý HUD qua Bluetooth: tám mặt đồng hồ (bao gồm Tốc độ số và
Tốc độ kim), tám bảng màu, thời gian sáng 15/30/60/120/300 giây hoặc luôn sáng,
bộ lọc app.
Nút **Áp dụng lên HUD** chờ phản hồi từ firmware; ESP ghi NVS trước khi xác nhận.
App đọc lại cài đặt từ board khi kết nối và có nút làm mới. Bộ lọc có tối đa
24 bundle ID, gồm app phổ biến và app được nhận biết qua ANCS. iOS không cung
cấp danh sách toàn bộ app đã cài. Mỗi công tắc thông báo áp dụng ngay, không
cần bấm nút Áp dụng. Tắt app xóa các hàng đang hiện của app đó trên HUD;
bật lại chỉ nhận sự kiện sau đó, có thể dùng nút làm mới ANCS trên board.

## Ghi chép thử ReplayKit đã loại khỏi bản dùng

Target **HUDScreen** là ReplayKit Broadcast Upload Extension, gửi ảnh màn hình
300×150 JPEG tới firmware qua BLE, tối đa một khung/giây và chỉ gửi khi khung
trước đã được xác nhận. Không dùng App Group, máy chủ hay âm thanh. Extension
tự tìm HUD đã được Bluetooth hệ thống kết nối. Đây là bản thử nghiệm cần kiểm
tra trên thiết bị thật, không phải API lấy tuyến của Google Maps/Apple Maps.

1. Ghép HUD iPhone và mở HUD Connect để cho phép Bluetooth/cài đặt.
2. Trong phần chia sẻ màn hình, bấm nút hệ thống, chọn **HUD Screen**, bắt đầu phát.
   Cũng có thể giữ nút Ghi màn hình trong Trung tâm điều khiển để chọn HUD Screen.
3. Mở Google Maps/Apple Maps và sử dụng như thường; mở tab mũi tên trên ESP.
4. Dừng phiên chia sẻ qua nút ghi màn hình của iPhone.

ReplayKit yêu cầu người dùng chủ động bắt đầu phiên chia sẻ. App không tự phát
hiện rồi âm thầm ghi màn hình khi Maps mở. Toàn bộ nội dung đang hiển thị được
gửi tới HUD trong phiên, ảnh dọc có viền để giữ đủ nội dung nên chữ nhỏ.
Chưa bảo đảm chia sẻ khi khóa iPhone. Không phân tích ảnh để dựng mũi tên/
khoảng cách riêng. Xem [Apple ReplayKit security](https://support.apple.com/guide/security/replaykit-security-seca5fc039dd/web),
[Google Navigation Connect requirements](https://developers.google.com/maps/documentation/navigation/connect/setup-overview-requirements).

Hai target đã build/ký được bằng Personal Team hiện tại; app và extension không
yêu cầu capability App Groups. Hướng dẫn MapKit dưới đây là ghi chép bản thử cũ;
giao diện tìm điểm đến đã được bỏ khỏi màn hình app chính theo yêu cầu mới.

Kiểm tra thực tế 31/10/2026: iPhone đọc được kênh settings; bài kiểm tra qua BLE
đổi ba mặt/màu, timeout, tắt/bật một bộ lọc, khôi phục và đọc lại đều PASS.
Serial xác nhận cấu hình do người dùng chọn theme=2, face=2, timeout=0 đã lưu.
ANCS/AMS vẫn sẵn sàng, không có lỗi parse hoặc mất gói trong lần kiểm tra này.
Chưa dùng kết quả đó để kết luận luồng ReplayKit hoạt động.

## Bản thử MapKit trước đây (lưu mã để tham khảo)

App SwiftUI/MapKit/CoreLocation/CoreBluetooth dành cho màn hình ES3C28P 320×240.
Không dùng nguyên app hay firmware của MOTO GPS. App hiện dùng Apple MapKit,
không lấy dữ liệu từ Google Maps đang mở trên điện thoại.

## Có gì trong mã nguồn

- Tìm địa điểm gần vị trí điện thoại, chọn điểm đến và lấy tuyến MapKit.
- Theo dõi tiến độ trên đường gấp khúc của tuyến; tính khoảng cách dọc tuyến
  tới bước tiếp theo; yêu cầu tính lại khi vị trí cách tuyến hơn 70 m.
- Vẽ snapshot bản đồ tối, tuyến trắng, vị trí, chỉ dẫn và khoảng cách thành
  JPEG 300×150; giữ phần dưới của snapshot cho thông tin bản đồ.
- Gửi ảnh BLE có thứ tự, CRC32, giới hạn dung lượng, kiểm soát back-pressure
  và xác nhận từ ESP. Bộ đệm ảnh nằm trong PSRAM của ESP.
- Kết nối lại thiết bị đã lưu hoặc HUD đang được Bluetooth hệ thống kết nối
  để nhận ANCS; khai báo location/bluetooth-central cho chạy nền.
  Các khai báo này chưa chứng minh đã chạy được khi khóa màn hình trên máy thật.
- Nhạc và ANCS trên ESP tiếp tục dùng firmware hiện có.

Đây là prototype. Đã chạy trên iPhone thật, tính được tuyến và nhận/hiển thị
bản đồ trên ESP; chưa có kết quả thử dẫn đường ngoài đường. Loại tuyến hiện có
**ô tô** và **đi bộ**, chưa có tuyến xe máy riêng;
không coi tuyến ô tô là tuyến đã tránh đường cấm xe máy. Icon rẽ hiện được suy
ra từ câu chỉ dẫn tiếng Việt/Anh, còn các kiểu thao tác khác dùng mũi tên thẳng.
Chưa có biển giới hạn tốc độ thật, đếm đèn đỏ hay bản đồ tải trước để dùng offline.
Độ mượt phụ thuộc tốc độ snapshot và BLE; nhịp dự kiến vài giây, không hứa hiệu
năng như một ứng dụng bản đồ chạy trực tiếp trên điện thoại.

## Cài thử bằng Apple Account thường

1. Cài Xcode đầy đủ từ App Store, mở lần đầu để hoàn tất cài thành phần iOS.
2. Mở `HUDNavigator.xcodeproj` trong thư mục này.
3. Chọn target HUDNavigator → Signing & Capabilities → chọn Personal Team của bạn.
   Đổi `com.hudnavigator.personal` thành Bundle Identifier riêng nếu Xcode báo trùng.
4. Cắm iPhone, tin cậy Mac; bật Developer Mode khi iPhone yêu cầu.
5. Chọn iPhone làm thiết bị chạy và bấm Run. Đăng nhập Apple Account trong Xcode
   trên máy của bạn; không cần cung cấp mật khẩu vào mã nguồn/chat.
6. Cho phép Bluetooth, kết nối HUD và đổi cài đặt. Nếu thử bản đồ, bật phiên
   chia sẻ HUD Screen rồi mở ứng dụng Maps; không tìm điểm đến trong HUD Connect.

Apple Account miễn phí hỗ trợ thử trực tiếp bằng Xcode, có giới hạn Personal
Team và thời hạn provisioning khoảng 7 ngày, sau đó phải ký/nạp lại.
Xem [Apple Developer account](https://developer.apple.com/help/account/basics/about-your-developer-account/).

## TestFlight và IPA

**TestFlight**: người cài bản thử không cần mua tài khoản developer. Bên phát
hành app cần Apple Developer Program, App Store Connect, bản archive được ký
và các bước kiểm tra phát hành của Apple. Tài khoản hiện tại của người dùng
là Apple Account thường, nên chọn cài bằng Xcode trước.
[Hướng dẫn Apple](https://developer.apple.com/testflight/).

**IPA**: có thể xuất sau khi build được ứng dụng, nhưng file cần được ký theo
phương thức cài tương ứng. IPA chưa ký không cài trực tiếp bằng cách bấm vào file.
Ad Hoc cần profile/chứng chỉ và iPhone đã đăng ký; xem
[Apple Ad Hoc](https://developer.apple.com/help/account/provisioning-profiles/create-an-ad-hoc-provisioning-profile).
Chưa có IPA ký sẵn, link TestFlight hoặc app được đưa lên App Store trong dự án.
Icon ứng dụng, metadata phát hành và kiểm chứng trên máy thật cần hoàn thiện
trước khi archive để phân phối công khai.

## Kiểm chứng hiện tại

Ngày 31/10/2026, đã build toàn bộ app thành công bằng Xcode 27.2 cho iOS Simulator
và iPhone arm64. Bản Debug được ký bằng Personal Team và đã cài lên iPhone 14 Pro
của người dùng. Đã mở app, cấp quyền và kết nối dịch vụ bản đồ với ESP.
Đã kiểm tra tìm địa điểm, tính tuyến MapKit và gửi ảnh BLE: firmware ghi nhận
7 khung liên tiếp, không từ chối khung nào; người dùng xác nhận thấy nền bản đồ
và tuyến trên màn hình. Chưa kiểm tra di chuyển thật, đi lệch tuyến và khóa máy lâu.
Project đã lưu Team/Bundle Identifier của bản cài hiện tại để lần Run kế tiếp
cập nhật cùng app. Khi dùng tài khoản khác cần chọn lại Personal Team.
Đã sửa trường hợp iPhone giữ cache GATT cũ trả về danh sách kênh trống:
firmware gửi Service Changed sau khi mã hóa; app đọc lại dịch vụ/kênh và xử lý
đăng ký Notify đã được khôi phục. Nút kết nối khóa khi đang nối, có timeout để
không chồng nhiều yêu cầu. GPS cập nhật khi đứng yên; nếu phải chờ fix mới,
app nhớ điểm đã chọn và tự tính tuyến khi có fix phù hợp, kèm chỉ báo sai số GPS.
Phần hình học/giao thức không phụ thuộc iOS được chạy bằng:

```sh
swift run --package-path ios/Core HUDCoreChecks
```

Lệnh kiểm tra iOS sau khi Xcode đã sẵn sàng:

```sh
xcodebuild -project ios/HUDNavigator.xcodeproj -scheme HUDNavigator \
  -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Simulator không thay thế thử BLE/GPS và trạng thái khóa màn hình trên iPhone.
Không dùng hình mô phỏng hay thành công ở unit test để tuyên bố đã dẫn đường thật.

## Luồng cần thử tiếp

Tìm địa điểm tại Việt Nam → tính tuyến → nhận ảnh trên board → kiểm tra khoảng
cách giảm khi di chuyển → dừng và xóa chỉ dẫn → mất GPS/mạng → tính lại → khóa
màn hình → mất/nối lại Bluetooth. Ghi riêng kết quả từng bước và chất lượng GPS.
Firmware tự ẩn ảnh sau 15 giây không có khung mới để tránh giữ hướng dẫn cũ.

Xem [giao thức truyền ảnh](../docs/NAV_STREAM_PROTOCOL.md).
