# Thử vector hóa ảnh bản đồ bằng Vision

Đây là thí nghiệm độc lập trên Mac, chưa tích hợp vào app hay firmware.
ML chỉ dùng ở bước Apple Vision OCR để xác định vùng chữ. Phần còn lại là
lọc theo bảng màu, dò contour và Ramer–Douglas–Peucker, **không phải mô hình
AI đã huấn luyện nhận biết mạng đường phố**. Các nét chỉ là đồ họa, không có
đồ thị giao thông, tên đường hay thông tin cấm đường để tính tuyến.

Biên dịch bằng Xcode Command Line Tools trên macOS:

```sh
xcrun swiftc -O experiments/map_vectorization/main.swift -o /tmp/hud-map-vector-probe
/tmp/hud-map-vector-probe input.png /tmp/hud-map-vector-output crop-x crop-y crop-width crop-height
```

Input là ảnh thử do người dùng cung cấp. Vùng crop được thu về300×200. Script
không upload ảnh; chỉ dùng bounding box OCR, không lưu hay log nội dung chữ.
Output: ảnh đầu vào, ảnh che chữ, mask tương phản, ảnh vector và so sánh,
`scene.bin` chứa điểm16 bit, `metrics.json` chứa số điểm/byte và thời gian.
Giới hạn64 contour/768 điểm; geometry không mang chữ, màu phân lớp hay georeference.

Kết quả một mẫu ảnh MapKit tối từ screenshot:

- OCR khoảng660 ms, contour khoảng23 ms, tổng khoảng693 ms **trên Mac**.
- 16 contour/203 điểm, geometry854 byte, JPEG so sánh10.798 byte ở quality0,55.
- Các nét đường nhỏ còn thiếu; xuất hiện biên của vùng che chữ và vùng không
  phải đường. Kết quả không có chất lượng tương đương JPEG, nên không dùng tỉ
  lệ byte này để tuyên bố giảm băng thông mà vẫn giữ đủ thông tin.
- Chưa có phép đo trên iPhone, thử nhiều khu vực/chế độ sáng, hay kiểm chứng
  topology. Chưa đủ bằng chứng để thay nền bản đồ đang hoạt động bằng mẫu này.

Muốn tiếp tục theo hướng ML nhận đường cần mô hình phân đoạn phù hợp ảnh bản
đồ và tập mẫu có nhãn để đo phần đường bị bỏ sót/biến dạng; OCR không thay thế
bước đó. Tuyến dẫn đường và tiến độ phải lấy từ dữ liệu tọa độ gốc.
