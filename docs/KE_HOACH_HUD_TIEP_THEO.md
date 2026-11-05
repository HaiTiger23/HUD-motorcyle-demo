# Kế hoạch phát triển HUD tiếp theo

Cập nhật: **02/11/2026**, giờ Việt Nam.

**Trạng thái: M1–M5 đã được triển khai ở mức mã nguồn; M1/M3 đang chờ kiểm
thử giọng nói, MapKit và đường thực tế trên board/iPhone thật.**

## 1. Các mục tiêu đã chốt

| Mã | Mục tiêu | Kết quả mong muốn |
| --- | --- | --- |
| M1 | Tìm điểm đến bằng giọng nói ngay trên trang Maps của HUD | Nút tìm kiếm → nói địa điểm → danh sách kết quả → chọn → xem trước tuyến → xác nhận bắt đầu trên ESP32. Không phụ thuộc AI/9router. |
| M2 | Biểu tượng vị trí phù hợp trạng thái di chuyển | Đứng yên hiện chấm tròn; đang di chuyển và hướng GPS tin cậy thì hiện mũi tên. Tránh đổi qua lại liên tục. |
| M3 | Chỉ đường thông qua AI | Nói “Chỉ đường đến…” trong AI → tìm địa điểm thật bằng MapKit → chọn và xác nhận trên HUD → bắt đầu dẫn đường. |
| M4 | Mặt đồng hồ tốc độ dạng kim | Kiểu đồng hồ xe máy: vạch chia km/h, kim tốc độ chuyển động mượt, có giờ hiện tại. |
| M5 | Mặt đồng hồ tốc độ dạng số | Số tốc độ lớn, đơn vị km/h rõ, kèm giờ và ngày. |

Hai mặt tốc độ được chọn trong app như các mặt đồng hồ hiện tại; cần phù hợp
cả giao diện sáng và tối.

**Quy ước người dùng đã chốt cho cả M4 và M5:**

- Đứng yên với dữ liệu GPS còn hợp lệ: **0 km/h**.
- Mất GPS, mất kết nối hoặc dữ liệu quá cũ: hiện đúng **`---`**.
- Không dùng `--`, không giữ tốc độ cũ như thể vẫn là số đo hiện tại.

## 2. Kiến trúc chung và hiện trạng

Người dùng thao tác trên ESP32; iPhone vẫn là thành phần xử lý nền:

```text
Mic ESP32 → BLE → iPhone nhận dạng tiếng nói
                       ↓
                  MapKit tìm địa điểm / tính tuyến
                       ↓
HUD nhận danh sách, trạng thái, tuyến và bản đồ qua BLE
```

“Không qua app điện thoại” trong các mục tiêu này nghĩa là **không cần mở app
để bấm chọn điểm đến hoặc xác nhận chuyến đi**. Không có nghĩa là bỏ iPhone,
chuyển MapKit sang ESP32 hoặc để ESP32 tự xử lý toàn bộ bản đồ.

Những thành phần đang có để tái sử dụng:

- Mic board, truyền âm thanh qua BLE, nhận dạng tiếng Việt trên iPhone.
- Tìm điểm đến, chọn địa điểm, tính tuyến và cập nhật bản đồ trong app.
- Gửi bản đồ lên HUD; tương tác zoom, kéo và về vị trí hiện tại.
- Truyền tốc độ GPS từ iPhone tới HUD để hiển thị trên Maps.
- Chọn mặt đồng hồ, màu giao diện, lưu và đồng bộ cài đặt.
- Khung AI có cơ chế yêu cầu các hành động giới hạn, không chạy mã tùy ý.

Hiện mũi tên vị trí được app tự vẽ trong `NavigationModel.compose`.
Tọa độ lấy từ GPS iPhone; hướng lấy từ `CLLocation.course` khi hướng hợp lệ và
`speed > 1.5 m/s` (khoảng 5,4 km/h). Ngoài trường hợp này mã hiện dùng hướng
bản đồ (`snapshotHeading`). **Không lấy hướng điện thoại đang chĩa tới từ la bàn.**
Vì vậy đứng yên quay xe hiện chưa làm mũi tên quay đúng hướng xe.

## 3. M1 — Tìm điểm đến bằng giọng nói, độc lập AI

### Luồng người dùng

1. Trên trang Maps, bấm icon tìm kiếm 🔍.
2. Hiện “Nói nơi bạn muốn đến”; giữ nút mic để nói, thả để kết thúc.
3. Hiện câu vừa nhận dạng để người dùng phát hiện nhận sai. Có nút nói lại/hủy.
4. iPhone tìm bằng MapKit, ưu tiên khu vực quanh vị trí hiện tại.
5. HUD hiện danh sách cuộn được: tên nổi bật, địa chỉ nhỏ và khoảng cách từ
   vị trí hiện tại để phân biệt những nơi trùng tên.
6. Chạm một kết quả → xem trước tuyến, quãng đường và thời gian dự kiến.
7. Bấm “Bắt đầu” trên HUD → khởi động chuyến đi, trở về giao diện dẫn đường.

Có quay lại danh sách và hủy; không tự chọn kết quả đầu tiên hoặc tự bắt đầu.
Không cần cấu hình model, API key AI hoặc có lượt sử dụng 9router.

### Hướng triển khai

- Tách việc nhận âm thanh/nhận dạng thành dịch vụ dùng chung, không gọi hỏi đáp
  AI sau khi nhận dạng nếu phiên này có mục đích tìm điểm đến.
- Dùng `MKLocalSearch` để lấy kết quả thật; dùng cơ chế tính tuyến MapKit đang có.
- Thêm giao diện tìm kiếm và danh sách lựa chọn trên ESP32, tận dụng toàn bộ
  không gian màn hình và icon cho các nút phù hợp.
- Truyền trạng thái có mã phiên qua BLE: bắt đầu tìm, câu nhận dạng, kết quả,
  chọn kết quả, xem tuyến, xác nhận bắt đầu, hủy và lỗi.
- Gán ID cho kết quả và gắn xác nhận với đúng phiên/địa điểm/tuyến đã xem.
  Câu tìm mới phải vô hiệu hóa lựa chọn cũ. Bỏ phản hồi đến muộn và lệnh trùng.
- Giới hạn độ dài chuỗi, số kết quả mỗi đợt và kích thước gói theo bộ nhớ/MTU.
  Số lượng kết quả, định dạng gói và thời hạn phiên sẽ chốt khi triển khai.
- Việc mạng và MapKit chạy trên iPhone; thao tác BLE/mic không chặn luồng vẽ
  giao diện ESP32. Không để hai phiên ghi âm tranh chấp cùng mic.

### Các trạng thái cần xử lý

Chờ → đang nghe → đang nhận dạng → đang tìm → danh sách → đang tính tuyến
→ xem trước → đang dẫn đường. Mỗi bước có đường hủy/quay lại phù hợp.

Không nghe được, nhận sai, không có kết quả, nhiều nơi trùng tên, không tìm được
tuyến, chưa có GPS, mất Bluetooth hoặc mạng phải có phản hồi rõ và cách thử lại.
Không hiển thị khoảng cách như số đo thật khi chưa có vị trí hợp lệ.

## 4. M2 — Chấm tròn khi đứng yên, mũi tên khi di chuyển

- Chấm tròn thể hiện vị trí khi đứng yên hoặc chưa có hướng GPS tin cậy.
- Chuyển sang mũi tên khi tốc độ và chất lượng dữ liệu đủ xác định hướng đi.
- Hướng theo chuyển động GPS, không theo việc người dùng xoay điện thoại.
- Dùng hai ngưỡng chuyển trạng thái khác nhau và thời gian duy trì để tránh
  nhấp nháy lúc dừng đèn đỏ/đi chậm; ngưỡng cụ thể cần đo ngoài đường.
- Xem xét tốc độ, độ mới dữ liệu, độ chính xác vị trí và hướng hợp lệ; không
  chỉ dựa vào một mẫu GPS. Làm mượt hướng qua biên 359°/0° nếu cần.
- Vị trí cũ/mất GPS phải được biểu thị là dữ liệu cũ, không khiến chấm tròn
  bị hiểu nhầm là vị trí hiện tại chính xác. Cách thể hiện sẽ chốt lúc thiết kế.

## 5. M3 — Chỉ đường qua AI

Ví dụ: “Chỉ đường đến Trống Đồng Palace”, “Tìm cây xăng gần nhất”.

- AI chỉ phân tích ý định và tên địa điểm/loại địa điểm.
- Hành động tìm kiếm phải qua danh sách lệnh được phép, tham số kiểm tra chặt.
- Gọi chung dịch vụ tìm kiếm, danh sách, xem trước và xác nhận của M1.
- MapKit cung cấp địa điểm/tọa độ thật; không dùng tọa độ AI tự đoán.
- Kết quả chưa rõ thì hỏi lại hoặc cho chọn, không tự bắt đầu chuyến đi.
- Có thể hỗ trợ “Chọn địa điểm thứ hai” và “Đúng, bắt đầu” khi đúng phiên đang
  chờ; chỉ áp dụng vào danh sách/địa điểm hiện hành, không dùng ngữ cảnh cũ.
- Khi AI không hoạt động, M1 vẫn sử dụng bình thường.

Các câu “Còn bao xa?”, “Hủy chuyến đi” là hướng mở rộng có thể dùng chung
trạng thái chuyến đi; chưa coi là yêu cầu phải có trong đợt triển khai này.

## 6. M4 và M5 — Hai mặt đồng hồ tốc độ

### Dạng kim

- Vòng chia tốc độ, đơn vị km/h và kim dễ đọc như đồng hồ xe máy.
- Có giờ hiện tại; ưu tiên khả năng đọc trên màn hình nhỏ hơn trang trí.
- Kim chuyển động mượt, không làm chậm vẽ hoặc làm trễ dữ liệu thật.
- Khi dữ liệu không hợp lệ: hiện `---`; kim không được tiếp tục thể hiện một
  tốc độ cũ như còn đúng. Cách ẩn/đưa kim về vị trí nghỉ cần chốt khi làm UI.
- Thang tốc độ tối đa và khoảng chia chưa chốt; không tự coi là yêu cầu đã duyệt.

### Dạng số

- Tốc độ dạng số lớn, đơn vị km/h, giờ và ngày.
- Đứng yên hợp lệ hiện 0; không có dữ liệu hợp lệ hiện `---`.
- Bố cục rõ trong cả nền sáng/tối và khi lật màn hình 180°.

### Dữ liệu và cài đặt dùng chung

- Dùng tốc độ GPS iPhone đang cấp cho Maps; không suy tốc độ từ hoạt ảnh bản đồ.
- Chuẩn hóa giá trị âm/không hợp lệ, độ mới dữ liệu và tình trạng kết nối trước
  khi đưa tới bất kỳ mặt đồng hồ nào.
- Tốc độ phải được cập nhật cả khi HUD đang ở trang đồng hồ, không phụ thuộc
  có đang dẫn đường hoặc có khung ảnh Maps mới hay không. Kiểm tra luồng hiện
  có trước, bổ sung đường cập nhật độc lập nếu cần.
- Chống nhiễu GPS lúc đứng yên nhưng không kéo dài việc hiển thị dữ liệu cũ.
- Bổ sung hai lựa chọn và hình xem trước trong app. Đồng bộ giới hạn mã mặt
  đồng hồ ở UI, BLE, firmware và phần lưu cài đặt, giữ tương thích các mẫu cũ.

## 7. Chạy nền và các điều chưa được bảo đảm

Người dùng từng xác nhận hỏi AI được khi iPhone khóa. Điều đó **chưa chứng minh**
tìm địa điểm và bắt đầu chuyến mới cũng hoạt động trọn vẹn khi máy khóa.

Cần thử riêng quyền vị trí/nhận dạng, trạng thái app nền và khả năng iOS tạm
ngưng app. Không cam kết vẫn chạy khi người dùng vuốt tắt app hoặc hệ điều hành
đã chấm dứt app. Không thêm chế độ chạy nền giả để giữ app sống.

Nếu điều kiện nền không đáp ứng, HUD phải báo đúng nguyên nhân và hướng xử lý,
không treo ở “Đang tìm…” hoặc tự báo đã bắt đầu trong khi tuyến chưa có.

## 8. Thứ tự thực hiện đề xuất

**M2 → M5 → M4 → M1 → M3.** Đây là đề xuất, chưa phải lịch triển khai.

Tiến độ mã nguồn ngày 02/11/2026: đã hoàn tất đợt đầu theo đúng thứ tự M2 → M5
→ M4. M2 dùng hướng di chuyển GPS, ngưỡng vào/ra và thời gian duy trì; M5/M4
thêm hai mặt đồng hồ cùng luồng tốc độ GPS đã được kiểm tra độ mới. Cần thử
ngoài đường trước khi xem các ngưỡng và giao diện là đã nghiệm thu.

1. Kiểm tra đường truyền tốc độ/GPS và quy ước dữ liệu hợp lệ dùng chung.
2. Sửa biểu tượng vị trí; thêm mặt tốc độ số rồi mặt kim.
3. Tách nhận dạng giọng nói khỏi hỏi đáp AI, xây tìm kiếm và xác nhận trên HUD.
4. Nối AI vào luồng tìm kiếm đã hoạt động độc lập.
5. Kiểm tra thực tế khi đi đường, iPhone khóa và mất/kết nối lại Bluetooth.

## 9. Những nơi cần đọc lại trong mã khi bắt đầu

Các đường dẫn dưới đây tính từ thư mục gốc project; cần đọc trạng thái thực tế,
không coi mô tả trong kế hoạch là bằng chứng mã đã hỗ trợ đầy đủ:

| Thành phần | Tệp liên quan |
| --- | --- |
| MapKit, GPS, vẽ vị trí, tìm địa điểm và tuyến | `ios/HUDNavigator/NavigationModel.swift` |
| Giao diện iPhone, chọn mặt đồng hồ | `ios/HUDNavigator/HUDNavigatorApp.swift` |
| Mic, nhận dạng, AI và hành động | `ios/HUDNavigator/AIChat.swift`, `src/ai_voice.cpp` |
| BLE app và giao thức | `ios/HUDNavigator/BluetoothLink.swift`, `ios/Core/Sources/HUDNavCore/NavCore.swift`, `lib/nav_protocol/`, `lib/voice_protocol/` |
| Trang Maps, đồng hồ, danh sách và gesture trên HUD | `src/companion_ui.cpp`, `lib/voice_protocol/ai_gesture.h` |
| Dữ liệu bản đồ/tốc độ phía board | `src/nav_stream.cpp`, `include/nav_stream.h` |
| Lưu cài đặt, xoay và cảm ứng | `src/hud_settings.cpp`, `include/hud_settings.h`, `src/main.cpp` |

Tài liệu liên quan: [giao thức bản đồ](NAV_STREAM_PROTOCOL.md),
[giọng nói và AI](AI_VOICE_PROTOCOL.md), [cài đặt BLE](SETTINGS_BLE_PROTOCOL.md),
[màn hình và âm thanh](DISPLAY_AUDIO_SETTINGS.md).

## 10. Tiêu chí nghiệm thu khi triển khai

- [x] M1 có luồng mic, MapKit, danh sách, xem tuyến, xác nhận và hủy trên HUD khi AI chưa cấu hình.
- [ ] Địa điểm trùng tên, nhận sai và không có kết quả có thể chọn lại/nói lại/hủy.
- [ ] Tìm mới/hủy không bị phản hồi cũ làm mở nhầm địa điểm hoặc chuyến đi.
- [x] M2 có chấm/mũi tên theo hướng GPS, ngưỡng vào/ra và thời gian duy trì trong mã nguồn.
- [x] M3 chỉ nhận intent/tên địa điểm từ AI, sau đó dùng MapKit thật và yêu cầu xác nhận trên HUD.
- [x] Hai mặt tốc độ đổi được trong app, lưu qua khởi động lại, hỗ trợ sáng/tối trong mã nguồn.
- [x] Đứng yên hiện 0; mất GPS/BLE/dữ liệu quá cũ hiện chính xác `---` ở cả hai mẫu.
- [x] Tốc độ cập nhật ở trang đồng hồ kể cả khi chưa bắt đầu một tuyến Maps.
- [ ] Kiểm tra khóa iPhone, app nền, mất mạng, GPS kém và kết nối lại; ghi rõ
  trường hợp được hỗ trợ và giới hạn thực tế.
- [ ] Kiểm tra trên board và iPhone thật; không chỉ dựa vào build/test mô phỏng.
- [ ] Không làm hỏng vuốt đổi trang, giữ nói/thả gửi/vuốt hủy, AI đọc qua loa,
  điều khiển nhạc, ngủ/chạm đánh thức, lọc/xóa thông báo và lật **180°**.
- [ ] Hộp xác nhận chọn chuyến/hủy vẫn nhận chạm: không tái diễn lỗi gesture AI
  nuốt chạm khi modal mở (đã sửa cho xác nhận xóa thông báo).

## 11. Phạm vi chưa yêu cầu

Không chuyển sang Google Maps dẫn đường qua thông báo, không quay lại chia sẻ
toàn màn hình iPhone, không bỏ iPhone, không triển khai bản đồ vector/offline/SD
trong các mục tiêu này. Chưa thêm đo pin khi người dùng chưa có pin.

Chỉ bắt đầu triển khai khi người dùng yêu cầu; việc kiểm thử điện thoại thật
sẽ sắp xếp khi iPhone sẵn sàng. Trước lúc đó kế hoạch này là bản ghi để tiếp tục,
không phải báo cáo những tính năng đã hoàn thành.

### Cập nhật M3: chọn và xác nhận bằng AI (02/11/2026)

Đã bổ sung lệnh “Chọn kết quả đầu tiên/thứ hai…” và “Bắt đầu”, dùng danh sách
MapKit hiện hành. Chọn xong chuyển sang Maps để xác nhận; xác nhận bằng AI xong
đóng panel và mở bản đồ dẫn đường. Có thể vuốt mở AI từ mép dưới panel M1.
Lệnh dùng context có hạn để chặn chọn/xác nhận dựa trên danh sách hoặc tuyến cũ.

Đã qua Swift Core checks, C++ ASan/UBSan, build Release iOS và firmware; đã cài
app, nạp board và xác nhận board kết nối lại iPhone. **Chờ người dùng kiểm tra
trọn luồng chọn kết quả → xem tuyến → xác nhận bằng giọng nói.** Chi tiết tại
[AI_NAVIGATION_M3.md](AI_NAVIGATION_M3.md).

### Xác nhận thực tế M3 (02/11/2026)

Người dùng xác nhận trên thiết bị: “Chọn kết quả đầu tiên” đã mở xác nhận tuyến
trên Maps; “Bắt đầu” đã đóng xác nhận và hiện bản đồ dẫn đường. Luồng chính M3
đã hoạt động. Các thử nghiệm khóa máy, mất kết nối, lỗi và ngoài đường chưa được
xác nhận trong lần kiểm tra này.

### Xác nhận tìm kiếm khi iPhone khóa (03/11/2026)

Sau bản sửa đưa kết quả ra trước trang AI, người dùng xác nhận: khi iPhone khóa,
nói “Chỉ đường đến…” trong AI trên HUD đã tự chuyển sang Maps, hiện “Chọn điểm
đến” và danh sách kết quả. Luồng nhận dạng → AI tìm điểm → MapKit → hiển thị
kết quả khi máy khóa đã được xác nhận cho lần thử này. Chưa xác nhận tiếp bước
chọn kết quả và bắt đầu chuyến khi máy vẫn khóa, cũng như thử đường thực tế.

### Xác nhận chuẩn bị tuyến khi iPhone khóa (03/11/2026)

Sau bản bổ sung CLBackgroundActivitySession, người dùng mở app, bật “Giữ GPS
khi kết nối HUD”, chờ GPS, khóa máy khoảng một phút, hủy phiên cũ rồi tìm và
chọn lại địa điểm. Người dùng xác nhận HUD đã hiện xác nhận tuyến và nút
BẮT ĐẦU. Luồng tìm kiếm → chọn địa điểm → lấy GPS/tính tuyến → xác nhận khi
máy khóa đã đạt trong lần thử này. Chưa xác nhận thao tác bấm BẮT ĐẦU và cập
nhật dẫn đường khi di chuyển/khóa lâu, mất mạng hoặc kết nối lại.

### Giao diện Maps và kết thúc chuyến (03/11/2026)

Người dùng xác nhận bắt đầu chuyến khi iPhone khóa được; yêu cầu tiếp theo là
sửa danh sách/xem tuyến/Bắt đầu bị che chữ, icon tìm kiếm lỗi, bổ sung nút kết
thúc và giảm điều khiển hiện thường trực. Đã cập nhật mã:

- Danh sách cuộn, hai dòng tên/địa chỉ, xem tuyến cuộn riêng và footer Bắt đầu
  cố định; kính lúp vẽ hình học, màu tương phản cập nhật theo giao diện.
- Maps dẫn đường chỉ hiện bản đồ/chỉ dẫn/tốc độ cùng menu nhỏ. Chạm để mở nhóm
  điều khiển; tự ẩn sau 5 giây nếu không ở chế độ kéo. Bỏ POI/nhà nổi trên nền.
- Menu có dừng chuyến, hỏi Hủy/Kết thúc, gửi tới app và chờ clear-map. Chưa thấy
  phản hồi thì cho thử lại; không giả báo đã dừng chỉ vì gửi notify thành công.
- Đã render 11 cảnh bằng LVGL/font thực, kiểm tra danh sách/cuộn/preview dài,
  kính lúp sáng/tối và hộp dừng. Swift Core checks và build iOS/ESP32 đã qua.
  Chi tiết: `docs/AI_NAVIGATION_M3.md`, `tools/destination_ui_preview/README.md`.
- Chờ người dùng kiểm tra bố cục trên board, ẩn/hiện menu và dừng chuyến thật.

### Xác nhận giao diện Maps và kết thúc chuyến (03/11/2026)

Người dùng xác nhận trên board: tìm địa điểm → cuộn danh sách → xem tuyến có
tên/địa chỉ, kính lúp và nút BẮT ĐẦU rõ, không chồng lấn. Trong Maps đang dẫn
đường, menu → icon dừng → KẾT THÚC đã dừng chuyến và bỏ bản đồ cũ. Hai hạng mục
này đã được kiểm tra thực tế, ngoài các kiểm tra render/build trước đó. Chưa
suy rộng thành xác nhận chạy đường dài, mất mạng hoặc tái kết nối nền.

### Trang chủ Maps: lịch sử và nút giữ nói lớn (03/11/2026)

Đã bổ sung mã cho yêu cầu mới: khi chưa có chuyến đi, Maps hiện 5 điểm đến gần
đây đồng bộ từ app, danh sách cuộn tên/địa chỉ và nút cố định “Giữ để nói tìm
điểm đến”. Chạm lịch sử mở chuẩn bị/xem tuyến để xác nhận; giữ nút tìm bằng mic
M1 độc lập AI. Có trạng thái rỗng/mất kết nối, giữ nguyên menu Maps khi có chuyến
nhưng ảnh bị cũ. Đang kiểm tra build/render/giao thức và sẽ kiểm thử thiết bị;
chi tiết tại `docs/AI_NAVIGATION_M3.md`.

### Xác nhận trang chủ Maps trên thiết bị (03/11/2026)

Người dùng xác nhận sau đồng bộ app: HUD đã hiện lịch sử và chạm một mục mở
được xem tuyến/xác nhận. Nút lớn giữ để nói trên trang chủ nghe đến lúc thả
và hiện kết quả tìm kiếm. Hai luồng chính của trang chủ Maps đã được kiểm tra
trên thiết bị thật. Chưa suy rộng thành xác nhận cache sau mất kết nối, reboot
không có điện thoại hoặc khóa máy lâu ở mọi tình huống.
