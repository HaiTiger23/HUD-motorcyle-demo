# M3 — AI chọn địa điểm và xác nhận chuyến đi

Ngày: 02/11/2026. Đã bổ sung mã và được người dùng xác nhận luồng chính trên thiết bị thật.

## Cách dùng

1. Tìm địa điểm bằng mic M1 trên Maps, hoặc nói trong AI “Chỉ đường đến …”.
2. Khi có kết quả, vuốt từ mép dưới lên để mở AI, giữ nói “Chọn kết quả đầu tiên”
   (hoặc thứ hai…thứ năm), rồi thả.
3. App chọn địa điểm thật từ danh sách hiện hành, tính tuyến. Khi sẵn sàng, HUD
   chuyển từ AI sang Maps, mở trang xác nhận có tên điểm đến và tóm tắt tuyến.
4. Bấm Bắt đầu, hoặc vuốt mở AI và nói “Bắt đầu”. Chỉ khi tuyến đã sẵn sàng mới
   thực hiện; sau đó HUD chuyển về Maps và đóng hộp chọn địa điểm.

Tìm kiếm/chọn/bắt đầu bằng chạm của M1 vẫn độc lập AI. Lệnh bằng AI cần model và
kết nối provider đang hoạt động. Chọn địa điểm không tự bắt đầu chuyến đi.

## Thực hiện

- `HUDAssistantPlan` thêm `navigation_select` (value 1…5) và `navigation_start`.
- Hai lệnh yêu cầu `context` UUID do NavigationModel cung cấp; không lấy context
  từ hội thoại cũ. Trạng thái, danh sách tên/địa chỉ và context hiện tại được đưa
  vào mỗi lượt hỏi AI như dữ liệu không phải chỉ dẫn.
- Kiểm tra context, tuổi <300 giây, phiên tìm kiếm còn tồn tại và giai đoạn.
  Tìm mới, chọn lại, hủy, dừng hoặc bắt đầu làm thay đổi context; phản hồi AI đến
  muộn không được áp dụng cho lựa chọn mới. Chỉ số ngoài danh sách bị từ chối.
- Bắt đầu yêu cầu preview sẵn sàng, route tồn tại, không đang tính tuyến, kênh
  HUD sẵn sàng và GPS mới <10 giây, sai số không quá 50 m.
- Lệnh voice tiếp theo có session âm thanh mới. App chuyển kết quả xem tuyến
  sang session mới để firmware chấp nhận, thay vì gửi nhầm session tìm kiếm cũ.
- Gói đích `0x50`: thêm phase 4 = đã bắt đầu/đóng panel và phase 5 = preview từ
  lựa chọn AI/mở xác nhận trên Maps. Firmware áp dụng chuyển trang một lần mỗi
  session. Phase 0…3 của M1 được giữ lại.
- Vuốt AI từ 40 pixel mép dưới vẫn nhận được khi panel tìm kiếm mở. Các chạm
  thông thường trong panel không trở thành ghi âm AI. Panel xóa/ghép đôi vẫn
  chặn gesture AI như trước.
- Sửa prompt cũ mâu thuẫn “không hỗ trợ tìm đường”. Không nới lệnh gửi/xóa tin
  nhắn, gọi điện hoặc thực thi mã tùy ý.

## Kiểm tra

- Swift Core checks: JSON đúng/sai, số thứ tự từ 1, cấm boolean/số lẻ/ngoài phạm
  vi/thiếu context/thừa tham số; từ chối phiên đã hủy, context khác, hết hạn,
  đang tìm và bắt đầu khi chưa có preview.
- C++ ASan/UBSan: gesture AI, chạm modal, hold/release/cancel và các kiểm tra
  giọng nói hiện có.
- Build app iOS Release và firmware ESP32.
- Cần kiểm tra thực tế: M1 tìm → AI chọn mục 1 → Maps xác nhận → AI nói Bắt đầu
  → Maps dẫn đường; chọn mục khác, hủy/tìm mới lúc AI đang xử lý, GPS kém, khóa
  iPhone và kết nối lại. Chưa khẳng định các thử nghiệm thực tế này đã đạt chỉ
  từ việc build hoặc kiểm tra giao thức.

## Kết quả người dùng xác nhận (02/11/2026)

- Nói “Chọn kết quả đầu tiên”: HUD tự chuyển sang Maps và hiện xác nhận tuyến.
- Nói “Bắt đầu”: HUD đóng hộp xác nhận và hiện bản đồ dẫn đường.
- Luồng chính M3 đã được xác nhận. Các tình huống lỗi, khóa máy, kết nối lại
  và thử ngoài đường vẫn cần kiểm tra riêng; không suy ra đã đạt từ luồng chính.

## Gia cố xử lý nền (03/11/2026)

Tiếp tục kiểm tra phần còn thiếu sau khi luồng chính M3 đã được người dùng xác
nhận. NavigationModel có tác vụ nền hữu hạn riêng cho tìm kiếm HUD và chuẩn bị
route, không phụ thuộc thời điểm phiên nhận dạng/AI kết thúc. Kết thúc tác vụ
khi có kết quả, lỗi, hủy hoặc dừng; khi iOS thu hồi thời gian nền, hủy yêu cầu,
vô hiệu hóa xác nhận cũ và báo mở app để thử lại. Không dùng keepalive giả,
không bảo đảm chạy nền vô hạn hoặc sau khi người dùng vuốt tắt app.

Hủy tìm trên HUD cũng tăng revision và hủy tính tuyến đang chờ, ngăn callback
đến sau lệnh hủy cập nhật lại tuyến. Tìm mới xóa kết quả/phiên cũ trước cả bước
kiểm tra câu tìm kiếm. Build iOS Release đã qua; kiểm thử đầy đủ khi iPhone khóa,
GPS kém và mất kết nối vẫn đang chờ phản hồi thực tế.

### Lần thử khóa máy chưa đạt và sửa vòng đời gửi BLE (03/11/2026)

Người dùng báo HUD dừng ở “Đang tìm địa điểm”. Log đã ghi nhận nhận dạng hoàn
thành với appState=2 (background) và tác vụ MapKit bắt đầu/kết thúc. Log cũ chưa
có số kết quả/mã lỗi, nên chưa đủ để kết luận MapKit tìm thành công.

Phát hiện tác vụ nền tìm kiếm kết thúc ngay sau khi enqueue kết quả, trước khi
hàng đợi BLE gửi xong. Sửa để chỉ kết thúc khi không đang tìm/tính tuyến/chờ
chuẩn bị và `voicePending` bằng false; nối callback queue-drained của Bluetooth
với NavigationModel. Thêm log số kết quả, mã lỗi MapKit và phase gửi đích, không
log tên địa điểm, câu nói hay nội dung âm thanh. Cần kiểm thử lại khi khóa máy.

### Log xác định lỗi hiển thị kết quả từ AI (03/11/2026)

Lần thử tiếp theo ghi: speech purpose=0, appState=2, nhận dạng hoàn thành;
MapKit `results=1,errorCode=0`; phase 1 gửi với voiceReady=true, tác vụ nền kết
thúc sau đó. Như vậy ở lần này nhận dạng và tìm kiếm nền đã có kết quả, chưa
đủ để quy lỗi còn lại cho giới hạn nền iOS.

Firmware trước đó mở panel kết quả ở Maps nhưng cố ý không chuyển khỏi trang
AI với phase 1. Người dùng tiếp tục thấy “Đang tìm địa điểm” ở AI. Ngoài ra tiêu
đề panel kết quả còn giữ chuỗi trạng thái phase 0. Sửa để khi có kết quả hoặc
lỗi, đưa Maps/panel ra trước một lần mỗi session; danh sách hiện tiêu đề “Chọn
điểm đến”. Không chuyển lại liên tục khi người dùng mở AI để chọn kết quả.
Chờ kiểm thử lại sau khi nạp firmware sửa giao diện.

### Xác nhận tìm kiếm khi iPhone khóa (03/11/2026)

Sau bản sửa đưa kết quả ra trước trang AI, người dùng xác nhận: khi iPhone khóa,
nói “Chỉ đường đến…” trong AI trên HUD đã tự chuyển sang Maps, hiện “Chọn điểm
đến” và danh sách kết quả. Luồng nhận dạng → AI tìm điểm → MapKit → hiển thị
kết quả khi máy khóa đã được xác nhận cho lần thử này. Chưa xác nhận tiếp bước
chọn kết quả và bắt đầu chuyến khi máy vẫn khóa, cũng như thử đường thực tế.

### Chọn địa điểm nhưng chưa sang xác nhận tuyến (03/11/2026)

Người dùng làm rõ: danh sách đã hiện, chạm địa điểm không sang xác nhận. Log có
nhiều lần bắt đầu chuẩn bị tuyến khi appState=2 nhưng không có gói preview.
Mã cũ chỉ bật cập nhật vị trí nền sau khi bắt đầu dẫn đường, dù chuẩn bị tuyến
trước đó cũng cần GPS mới. HUD không có trạng thái chờ tuyến riêng nên danh sách
cũ vẫn bấm được và tiêu đề bị cập nhật lại thành “Chọn điểm đến”.

Bổ sung phase 6 = chờ GPS/tính tuyến, ẩn danh sách/mic/Bắt đầu, giữ nút hủy;
chặn chọn lặp khi preparing/planning. Bật cập nhật vị trí nền trong giai đoạn
chuẩn bị HUD, dọn khi hủy/lỗi/dừng/tìm mới và giới hạn giai đoạn này 5 phút;
chuyển sang cập nhật dẫn đường khi bắt đầu. GPS bị từ chối báo lên HUD. Log bổ
sung tuổi GPS/quyền vị trí và mã kết quả tính tuyến, không ghi tọa độ. Cần thử
lại chạm kết quả khi iPhone khóa; chưa coi đã sửa xong từ build thành công.

### Chưa đạt sau khi đã chờ hơn 20 giây (03/11/2026)

Người dùng xác nhận HUD vẫn giữ trạng thái chờ. Log iPhone cho thấy GPS ban đầu
cũ 169 giây, sau 11 giây có vị trí mới, MapKit trả một route và app gửi phase 2
lúc chạy nền. Do đó chưa thể coi vấn đề đã được sửa bằng thay đổi GPS.

Bổ sung chẩn đoán STATUS phía ESP32: số gói đích nhận đúng/sai session, phase
cuối, phase đang giữ; trạng thái hiển thị panel và nút bắt đầu. Không ghi địa
điểm/nội dung nói. Cần đối chiếu với log iPhone trong lần tái hiện tiếp theo.

### Lần thử đối chiếu ESP32 và iPhone (03/11/2026)

Board giữ phase 6, RX accepted=3/stale=0, panel hiện nhưng chưa có nút bắt đầu;
log iPhone cùng lần thử có GPS cũ 47 giây, authorization=4 (When In Use), chưa
có kết quả route. Lần này thực sự còn chờ GPS, khác lần log trước đã gửi phase 2.

Thêm CLBackgroundActivitySession được khởi tạo khi app ở foreground, có quyền
vị trí và HUD đã kết nối. Công tắc “Giữ GPS khi kết nối HUD” mặc định bật, lưu
cài đặt; hiển thị giải thích chỉ báo vị trí iOS và mức dùng pin. Giữ cập nhật vị
trí thật cho tốc độ/tìm tuyến khi khóa, kết thúc phiên standby khi tắt công tắc,
mất kết nối hoặc quyền bị thu hồi. Điều hướng/chuẩn bị tuyến vẫn có vòng đời
vị trí riêng. Khởi tạo lại khi quay về foreground/kết nối HUD; không cam kết
khởi tạo từ trạng thái force-quit hay tái kết nối nền khi không còn phiên.

Chờ GPS trong bước chuẩn bị HUD có hạn 20 giây, sau đó báo lỗi và cho thử lại;
không thay GPS mới bằng tọa độ cũ để giả vờ tính tuyến thành công. Cần mở app
sau cập nhật để thiết lập phiên rồi kiểm thử lại khi máy khóa.

Tham khảo: https://developer.apple.com/documentation/corelocation/clbackgroundactivitysession-4nl4y

### Xác nhận chuẩn bị tuyến khi iPhone khóa (03/11/2026)

Sau bản bổ sung CLBackgroundActivitySession, người dùng mở app, bật “Giữ GPS
khi kết nối HUD”, chờ GPS, khóa máy khoảng một phút, hủy phiên cũ rồi tìm và
chọn lại địa điểm. Người dùng xác nhận HUD đã hiện xác nhận tuyến và nút
BẮT ĐẦU. Luồng tìm kiếm → chọn địa điểm → lấy GPS/tính tuyến → xác nhận khi
máy khóa đã đạt trong lần thử này. Chưa xác nhận thao tác bấm BẮT ĐẦU và cập
nhật dẫn đường khi di chuyển/khóa lâu, mất mạng hoặc kết nối lại.

## Bố cục tìm kiếm và điều khiển Maps (03/11/2026)

Người dùng xác nhận bắt đầu chuyến đi được, sau đó yêu cầu sửa responsive,
chữ bị che, icon tìm kiếm và bổ sung kết thúc chuyến/ẩn hiện điều khiển.

- Tách UI tìm kiếm thành component LVGL dùng lại được, bố trí theo kích thước
  vùng nội dung (hiện 300×200 bên dưới đồng hồ trên màn hình ngang 320×240).
- Danh sách có vùng cuộn riêng, hàng 52 px với tên 18 px và địa chỉ/khoảng cách
  14 px, số thứ tự rõ để dùng AI. Tên/địa chỉ dài dùng dấu chấm ở danh sách;
  phần xem chuyến có vùng cuộn, nút Bắt đầu nằm cố định bên ngoài vùng đó.
- Tăng trường dữ liệu destination lên 192 byte, gửi tối đa 180 byte UTF-8 và
  giới hạn theo maximumWriteValueLength; không dùng gói dài hơn khả năng BLE.
  Tóm tắt tuyến bỏ câu hướng dẫn trùng với nút. Khoảng cách đặt trước địa chỉ.
- Kính lúp vẽ bằng vòng tròn/đoạn thẳng thay ký tự F002 thiếu trong font. Đổi
  U+2026 thành ba dấu chấm ở trạng thái văn bản để tránh ô vuông thiếu glyph.
- Maps live: chạm bản đồ hoặc icon menu để mở nhóm +, −, kéo, về GPS, tìm kiếm,
  kết thúc. Tự ẩn sau 5 giây nếu không ở chế độ kéo; ẩn tốc độ trong lúc menu
  mở để giảm chồng lấn. Chỉ dẫn rẽ vẫn nằm trong ảnh bản đồ; đồng hồ trên đỉnh
  vẫn giữ. Bỏ nhãn POI và nhà nổi để nền bản đồ bớt rối.
- Kết thúc chuyến: icon dừng → hộp Hủy/Kết thúc → lệnh map control 6 → app dừng
  và gửi clear-map như nút dừng trên iPhone. Không xóa bản đồ lạc quan chỉ từ
  notify thành công; chưa thấy phản hồi sau 5 giây thì cho thử lại. Lệnh dừng
  idempotent, không phụ thuộc snapshot cũ còn trong cache.
- Swift Core checks bổ sung stop command, từ chối payload tọa độ ở stop và
  opcode ngoài phạm vi. Native renderer dùng chính component/font firmware,
  xuất 11 cảnh sáng/tối, cuộn và dialog; kiểm tra vùng nội dung không che footer.
  Tham khảo `tools/destination_ui_preview/README.md`, ảnh ở `output/destination-ui/`.
- Cần người dùng xác nhận giao diện/chạm trên board, tự ẩn menu và kết thúc
  chuyến thật; ảnh render và build chưa chứng minh các thao tác thiết bị đã đạt.

### Xác nhận giao diện Maps và kết thúc chuyến (03/11/2026)

Người dùng xác nhận trên board: tìm địa điểm → cuộn danh sách → xem tuyến có
tên/địa chỉ, kính lúp và nút BẮT ĐẦU rõ, không chồng lấn. Trong Maps đang dẫn
đường, menu → icon dừng → KẾT THÚC đã dừng chuyến và bỏ bản đồ cũ. Hai hạng mục
này đã được kiểm tra thực tế, ngoài các kiểm tra render/build trước đó. Chưa
suy rộng thành xác nhận chạy đường dài, mất mạng hoặc tái kết nối nền.

## Trang chủ Maps khi chưa có chuyến đi (03/11/2026)

Yêu cầu mới: thay trang chờ bản đồ bằng lịch sử điểm đến và nút giữ để nói lớn.

- Đồng bộ tối đa 5 điểm đến gần đây từ `hudRecentPlaces` trong app, mới nhất
  trước. Lịch sử lưu bền ở iPhone; ESP32 giữ cache trong RAM và nhận lại sau kết
  nối. Địa chỉ gốc được giữ khi dùng lại một điểm đã lưu.
- Trang Maps không có chuyến: danh sách tên/địa chỉ cuộn riêng và nút cố định
  “GIỮ ĐỂ NÓI TÌM ĐIỂM ĐẾN” cao 44 px. Có trạng thái rỗng/mất kết nối.
- Giữ nút thu mic purpose=1, không qua AI; chỉ mở panel tìm kiếm sau khi thả,
  tránh hide nút đang được giữ làm phát PRESS_LOST và hủy ghi âm.
- Chạm lịch sử tạo session đích mới, app lấy đúng địa điểm lưu để tính tuyến,
  vẫn phải xem và xác nhận Bắt đầu. Catalog token ngăn chọn nhầm thứ tự cũ;
  phiên chờ phản hồi có hạn 15 giây, không đứng chờ khi app không phản hồi.
- Lịch sử cập nhật lúc kênh voice sẵn sàng, khi vào Maps/yêu cầu đồng bộ và
  khi bắt đầu/dừng chuyến. Gói 0x54 = token/u32, index/u8, total/u8, UTF-8;
  gói 0x55 = action/u8, session/u32, token/u32, index/u8, tổng 11 byte;
  gói 0x56 + boolean báo chuyến đang chạy. Không dùng session mic để chặn
  catalog; chọn lại lịch sử mới tạo session có thể nhận preview qua 0x50.
- Theo dõi trạng thái chuyến riêng với độ mới ảnh bản đồ: mất ảnh tạm thời
  không bị coi là không có chuyến và thay bằng trang lịch sử. Kết thúc chuyến
  quay về lịch sử; menu dừng vẫn truy cập được khi ảnh bản đồ đã cũ.
- Renderer LVGL/font thực thêm 6 cảnh trang chủ, kiểm tra danh sách không che
  nút mic. Swift Core checks kiểm tra gói refresh/select, token, session,
  index; C++ ASan/UBSan kiểm tra giới hạn payload và chỉ số catalog.
- Cần người dùng xác nhận danh sách thật, giữ/thả mic và chọn lịch sử ra tuyến
  trên board. Build/render chưa thay cho kiểm tra đầy đủ trên thiết bị.

Đồng bộ lịch sử dùng tác vụ nền hữu hạn riêng, giữ đến khi hàng đợi voice/BLE
rỗng; không kết thúc hay thay tác vụ MapKit đang chạy. Khi nhận sự kiện
`@Published ready/voiceReady`, các thao tác đọc lại trạng thái được chuyển sang
lượt MainActor kế tiếp vì publisher phát trong willSet. Điều này tránh bỏ qua
đồng bộ lịch sử và phiên GPS nền ngay lần kết nối đầu.

### Xác nhận trang chủ Maps trên thiết bị (03/11/2026)

Người dùng xác nhận sau đồng bộ app: HUD đã hiện lịch sử và chạm một mục mở
được xem tuyến/xác nhận. Nút lớn giữ để nói trên trang chủ nghe đến lúc thả
và hiện kết quả tìm kiếm. Hai luồng chính của trang chủ Maps đã được kiểm tra
trên thiết bị thật. Chưa suy rộng thành xác nhận cache sau mất kết nối, reboot
không có điện thoại hoặc khóa máy lâu ở mọi tình huống.
