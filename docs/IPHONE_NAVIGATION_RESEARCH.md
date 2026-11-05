# Đối chiếu chỉ đường từ iPhone

Đã đọc tài liệu và phần giao thức/mã liên quan ngày 30/10/2026.

| Dự án | iPhone / chỉ đường | Áp dụng cho ES3C28P 2.8 inch |
| --- | --- | --- |
| [SteedPilot](https://github.com/deanthecoder/SteedPilot) | Có mã app SwiftUI, MapKit và truyền trạng thái dẫn đường qua BLE. Giao thức có mũi tên, khoảng cách, tiến độ và heartbeat. | Phần giao tiếp có thể làm nền tảng; cần chuyển driver và bố cục từ màn tròn 360×360 sang ILI9341V 320×240. App này tự dẫn đường; không lấy luồng rẽ từ ứng dụng Google Maps có sẵn. |
| [moto-gps-waveshare](https://github.com/mx3353672833-debug/moto-gps-waveshare) | Có app iOS, lõi dẫn đường, BLE và nền đường/bản đồ. Tìm địa điểm/tính tuyến dùng AMap qua gateway HTTPS; bản đồ nền dùng OSM/Protomaps. | Cần chuyển BSP CO5300 QSPI 466×466, touch CST9217, quản lý nguồn và UI sang board hiện tại. Phức tạp hơn việc chỉ đưa mũi tên/khoảng cách lên màn hình. Không phải bộ lấy chỉ đường từ Google Maps iPhone. |
| [esp32-bike-computer-main](https://github.com/m-gracia/esp32-bike-computer-main) | Kết nối điện thoại được mô tả cho Android. Chỉ đường từng chặng vẫn nằm trong danh sách cải tiến chưa có. | Hợp tham khảo TPMS/GPS/thông tin xe; không phải giải pháp có sẵn cho chỉ đường iPhone. |

SteedPilot công bố giấy phép MIT; moto-gps-waveshare tự mô tả nguồn có điều kiện
ghi công/phi thương mại; dự án thứ ba công bố GPL-3.0. Chưa nhập mã nguồn từ ba
dự án này vào firmware. Đánh giá khả năng chuyển đổi là nhận định kỹ thuật,
chưa phải kết quả chạy chúng trên board của người dùng.

## Yêu cầu đã chọn

Ban đầu người dùng muốn dùng Google Maps đang có trên iPhone. Ngày 31/10/2026,
sau khi xác nhận muốn bản đồ và hướng rẽ đầy đủ như MOTO GPS, người dùng đã
chọn **dùng app iPhone đồng hành**, đồng ý cài Xcode và cho biết chỉ có Apple
Account thường. Do đó hiện có prototype [HUD Navigator](../ios/README.md) dùng
MapKit và dịch vụ nhận khung bản đồ riêng trên ESP. Đã build toàn bộ target iOS
bằng Xcode 27.2 và cài bản Debug ký Personal Team lên iPhone 14 Pro ngày 31/10/2026;
đã kiểm tra app tính tuyến và gửi bản đồ BLE trên máy thật; người dùng xác nhận
thấy bản đồ và tuyến trên ESP. Chưa có kết quả thử di chuyển. Không có link TestFlight.

Bản thử MapKit có tuyến ô tô/đi bộ, chưa có tuyến xe máy riêng. Không tuyên bố
đã có tính năng/nguồn dữ liệu giống hoàn toàn MOTO GPS hoặc Google Maps.

- Đồng hồ: BLE Current Time Service; có đặt giờ USB khi cần.
- Thông báo: Apple Notification Center Service (ANCS), cần ghép đôi và quyền iPhone.
- Google Maps: chỉ thử hiển thị nguyên văn thông báo của Maps nếu ANCS cung cấp.
  Chưa đảm bảo có hướng dẫn rẽ, khoảng cách và cập nhật liên tục.

Repo MOTO GPS hiện đã có release `v0.3.1-sideload` chứa IPA **chưa ký**,
khác với ghi chú README gốc còn nói chưa có bản dựng sẵn. Đã xác minh release
ngày 31/10/2026. IPA vẫn cần người dùng ký/cài và gateway riêng; app đó không
tự tương thích với firmware của board hiện tại. Ảnh bìa là render sản phẩm;
repo ghi rõ dữ liệu giới hạn tốc độ và đếm đèn giao thông thật chưa được nối.

Google Navigation Connect mới vẫn ghi rõ không hỗ trợ Google Maps trên iOS.
Google Navigation SDK có luồng turn-by-turn cho màn hình nhỏ trong **app tự
xây**, nhưng cần cấu hình dịch vụ riêng, không phải đọc màn hình app Maps có sẵn.
Nguồn: [Navigation Connect](https://developers.google.com/maps/documentation/navigation/connect/overview),
[data feed](https://developers.google.com/maps/documentation/navigation/ios-sdk/nav-only-feed),
[IPA release](https://github.com/mx3353672833-debug/moto-gps-waveshare/releases/tag/v0.3.1-sideload).

ANCS là luồng thông báo, không phải API truy cập toàn bộ trạng thái app khác.
Không coi Live Activity/CarPlay hay quyền Bluetooth là một giao thức xuất tuyến
mặc định cho ESP32. Không hiển thị mũi tên/khoảng cách giả như dữ liệu thật.

## Yêu cầu cập nhật: app chỉ cài đặt, Maps dùng như thường

Người dùng yêu cầu Google Maps/Apple Maps đang có tự đẩy hình sang ESP. Không
có API công khai đã xác minh để app đồng hành tự lấy tuyến hoặc tự bật ghi
màn hình app khác. Navigation Connect hiện ghi rõ Google Maps iOS không hỗ trợ.
Sau khi được giải thích giới hạn, người dùng chấp nhận thử bật chia sẻ màn
hình trước mỗi chuyến. Đã thêm target ReplayKit HUDScreen và đổi giao diện app
chính thành settings. Phải thử luồng mới riêng, không dùng kết quả MapKit cũ
để kết luận ReplayKit hoạt động. Không hứa hoạt động khi khóa màn hình.

Nguồn: [Apple ReplayKit security](https://support.apple.com/guide/security/replaykit-security-seca5fc039dd/web),
[Google requirements](https://developers.google.com/maps/documentation/navigation/connect/setup-overview-requirements).

### Kết quả thử ReplayKit

Người dùng gửi ảnh IMG_1818.HEIC: board thực sự nhận được hình Google Maps,
nhưng toàn ảnh dọc bị thu nhỏ giữa panel ngang; rời Maps vẫn chia sẻ màn hình
app khác. Người dùng bác bỏ phương án này. Đã gỡ picker và không nhúng HUDScreen
trong app chính. Không coi crop/phóng to ảnh là giải pháp tách riêng Google Maps.
Luồng tự tính/vẽ MapKit chỉ triển khai lại khi người dùng chấp nhận chọn điểm
đến và bắt đầu chuyến đi trong HUD; tuyến có thể khác Google Maps.

### Quyết định sau cùng: đọc thông báo Maps

Người dùng chọn giữ đọc thông báo Google Maps và hỏi hỗ trợ tương tự Apple Maps.
Tab mũi tên hiện thông báo mới nhất từ `com.google.Maps`/`com.google.maps` hoặc
`com.apple.Maps`, tiêu đề và nội dung 24 px có cuộn. Giữ nguyên nội dung gốc,
không suy diễn tuyến, bản đồ, hướng rẽ hay khoảng cách khi thông báo không có.
Xóa thông báo mới nhất, làm mới ANCS, ngắt kết nối hoặc tắt bộ lọc app sẽ bỏ
nội dung cũ. Cần thử thông báo chỉ đường thực tế từ từng app trên iPhone;
nhận được thông báo chat chưa chứng minh Maps cung cấp cập nhật dẫn đường.

## Tài liệu gốc

- [SteedPilot BLE protocol](https://github.com/deanthecoder/SteedPilot/blob/main/docs/protocol.md)
- [SteedPilot iPhone BLE sender](https://github.com/deanthecoder/SteedPilot/blob/main/ios/SteedPilot/SteedPilot/BluetoothNavSender.swift)
- [MOTO GPS README](https://github.com/mx3353672833-debug/moto-gps-waveshare/blob/main/README.en.md)
- [MOTO GPS iOS setup](https://github.com/mx3353672833-debug/moto-gps-waveshare/blob/main/platforms/ios/README.en.md)
- [MOTO GPS known issues](https://github.com/mx3353672833-debug/moto-gps-waveshare/blob/main/docs/KNOWN_ISSUES.en.md)
- [Bike computer improvements](https://github.com/m-gracia/esp32-bike-computer-main#improvements-list)
- [Apple ANCS specification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html)
- [NimBLE ANCS example](https://github.com/h2zero/NimBLE-Arduino/blob/2.5.2/examples/ANCS/ANCS.ino)
- [Google Maps URL Scheme for iOS](https://developers.google.com/maps/documentation/urls/ios-urlscheme)
- [Google Navigation SDK for iOS](https://developers.google.com/maps/documentation/navigation/ios-sdk)
