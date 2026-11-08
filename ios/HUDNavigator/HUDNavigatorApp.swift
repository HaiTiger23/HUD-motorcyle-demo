import SwiftUI
import MapKit
import UniformTypeIdentifiers
import PhotosUI

@main @MainActor struct HUDNavigatorApp: App {
    @StateObject private var bluetooth: BluetoothLink
    @StateObject private var navigation: NavigationModel
    @StateObject private var ai: AIChatModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var selectedTab = 0
    init() {
        let link = BluetoothLink()
        _bluetooth = StateObject(wrappedValue: link)
        let nav = NavigationModel(bluetooth: link)
        _navigation = StateObject(wrappedValue: nav)
        _ai = StateObject(wrappedValue: AIChatModel(bluetooth:link,navigation:nav))
    }
    var body: some Scene {
        WindowGroup {
            TabView(selection: $selectedTab) {
                NavigationHome(bluetooth: bluetooth, navigation: navigation)
                    .tabItem { Label("Chỉ đường", systemImage: "location.north.fill") }.tag(0)
                ContentView(bluetooth: bluetooth, navigation: navigation)
                    .tabItem { Label("Cài đặt", systemImage: "slider.horizontal.3") }.tag(1)
                AIChatView(ai:ai,bluetooth:bluetooth)
                    .tabItem { Label("AI",systemImage:"sparkles") }.tag(2)
            }
            .task {
                ai.refreshAuthorization()
                bluetooth.resume()
                await receiveDestination()
                #if DEBUG
                if ProcessInfo.processInfo.arguments.contains("--hud-settings-smoke") { await bluetooth.runSettingsSmokeTest() }
                #endif
            }
            .onChange(of: scenePhase) { _, phase in
                if phase == .active {
                    ai.refreshAuthorization()
                    bluetooth.resume()
                    navigation.updateHUDLocationActivity()
                    Task { await receiveDestination() }
                } else if phase == .background { bluetooth.enterBackground() }
            }
        }
    }
    private func receiveDestination() async {
        if let text = SharedDestinationInbox.take() {
            selectedTab = 0
            await navigation.importShared(text)
        }
    }
}

// MARK: - Settings

/// One list, every control applies immediately (no save buttons).
@MainActor struct ContentView: View {
    @ObservedObject var bluetooth: BluetoothLink
    @ObservedObject var navigation: NavigationModel
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.openURL) private var openURL
    @State private var draft = HUDSettings()
    @State private var hardwareDraft = HUDHardwareSettings()
    @State private var enteringPIN = false
    @State private var pinText = ""
    private let colors: [Color] = [Color(red: 0.25, green: 0.85, blue: 0.96), .mint, .orange, .pink,
        Color(red: 0, green: 0.41, blue: 0.66), Color(red: 0, green: 0.48, blue: 0.31),
        Color(red: 0.6, green: 0.31, blue: 0), Color(red: 0.64, green: 0.15, blue: 0.4)]
    private let themeNames = ["Đại dương", "Rừng xanh", "Hoàng hôn", "Anh đào", "Trắng xanh", "Bạc hà", "Kem", "Hồng sáng"]
    /// HUD face ids (firmware numbering) in gallery order.
    private let faces: [(id: Int, name: String)] = [(0, "Số lớn"), (2, "Kim hiện đại"), (5, "Kim cổ điển"), (6, "Đi xe"), (8, "iPhone")]
    private var photoBackground: Bool { bluetooth.wallpaperBackground ?? false }
    /// Over a photo (and on the iPhone face) the HUD uses the dark twin of a light theme.
    private func previewTheme(_ face: Int) -> Int { photoBackground || face == 8 ? draft.theme % 4 : draft.theme }
    private let clockSwatches: [Int] = [0xFFFFFF, 0xFFD60A, 0x64D2FF, 0xFF6482, 0x30D158, 0xBF5AF2]
    @State private var photoItem: PhotosPickerItem?
    @State private var wallpaperImage: UIImage? = WallpaperStore.load()
    @State private var clockColor = Color.white
    @State private var colorSend: Task<Void, Never>?
    private var editable: Bool { bluetooth.settingsReady }
    private var hardwareEditable: Bool { bluetooth.settingsReady && bluetooth.hardwareReady }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HUDConnectionRow(bluetooth: bluetooth)
                } footer: {
                    if bluetooth.ready { Text(bluetooth.settingsStatus) }
                }

                Section("Mặt đồng hồ") {
                    faceGallery
                    backgroundControls
                    HStack(spacing: 0) {
                        ForEach(colors.indices, id: \.self) { index in
                            Button { draft.theme = index } label: {
                                ZStack {
                                    Circle().fill(colors[index]).frame(width: 30, height: 30)
                                    if draft.theme == index {
                                        Circle().stroke(Color.primary, lineWidth: 2).frame(width: 38, height: 38)
                                    }
                                }.frame(maxWidth: .infinity, minHeight: 44)
                            }.buttonStyle(.plain).accessibilityLabel(themeNames[index])
                        }
                    }
                }.disabled(!editable)

                Section("Màn hình HUD") {
                    sliderRow("Độ sáng", icon: "sun.max.fill", value: $hardwareDraft.brightness, range: 5...100)
                    Picker("Tự tắt sau", selection: $draft.timeout) {
                        Text("15 giây").tag(15); Text("30 giây").tag(30)
                        Text("1 phút").tag(60); Text("2 phút").tag(120)
                        Text("5 phút").tag(300); Text("Luôn sáng").tag(0)
                    }
                    Toggle("Lật ngược 180°", isOn: Binding(get: { hardwareDraft.rotation == 2 },
                                                          set: { hardwareDraft.rotation = $0 ? 2 : 0; commitHardware() }))
                }.disabled(!hardwareEditable)

                Section("Loa HUD") {
                    sliderRow("Âm lượng", icon: hardwareDraft.speakerVolume == 0 ? "speaker.slash.fill" : "speaker.wave.2.fill",
                              value: $hardwareDraft.speakerVolume, range: 0...100)
                    Button("Nghe thử", systemImage: "play.circle") { bluetooth.testSpeaker() }
                        .disabled(bluetooth.voiceActive)
                }.disabled(!hardwareEditable)

                Section {
                    NavigationLink {
                        AppFilterView(bluetooth: bluetooth, allowNewApps: $draft.allowNewApps)
                    } label: {
                        LabeledContent("Ứng dụng hiện trên HUD",
                                       value: "\(bluetooth.apps.filter(\.enabled).count)/\(bluetooth.apps.count)")
                    }
                } header: { Text("Thông báo") }
                .disabled(!editable)

                Section {
                    Toggle("Báo khi quên HUD trên xe", isOn: $navigation.forgetAlert)
                    LabeledContent("Mã PIN chủ xe", value: bluetooth.ownerPinSet == nil ? "—" : bluetooth.ownerPinSet == true ? "Đã đặt" : "Chưa đặt")
                    Button(bluetooth.ownerPinSet == true ? "Đổi mã PIN" : "Đặt mã PIN") { pinText = ""; enteringPIN = true }
                        .disabled(bluetooth.ownerPinSet == nil)
                    if bluetooth.ownerPinSet == true {
                        Button("Xóa mã PIN", role: .destructive) { bluetooth.setOwnerPIN(nil) }
                    }
                } header: { Text("Chống mất") }
                footer: { Text("Mất kết nối iPhone thì HUD tự khóa, chỉ còn đồng hồ. iPhone của bạn kết nối lại là tự mở. Mã PIN dùng để mở khóa không cần iPhone và chặn iPhone lạ ghép với HUD.") }
                .alert("Mã PIN 4 số", isPresented: $enteringPIN) {
                    TextField("0000", text: $pinText).keyboardType(.numberPad)
                    Button("Lưu") { bluetooth.setOwnerPIN(String(pinText.filter(\.isNumber).prefix(4))) }
                        .disabled(pinText.filter(\.isNumber).count != 4)
                    Button("Hủy", role: .cancel) {}
                } message: { Text("Nhập trên HUD khi mở khóa hoặc khi ghép iPhone mới. Hãy nhớ mã này.") }

                Section {
                    Toggle("Giữ GPS khi khóa máy", isOn: $navigation.backgroundGPS)
                        .onChange(of: navigation.backgroundGPS) { _, _ in navigation.saveBackgroundGPS() }
                    if navigation.locationDenied {
                        Button("Cho phép vị trí trong Cài đặt") { openURL(URL(string: UIApplication.openSettingsURLString)!) }
                    }
                } header: { Text("Vị trí") }
                footer: { Text("Cập nhật tốc độ và tuyến khi iPhone khóa; dùng thêm pin.") }
            }
            .navigationTitle("Cài đặt")
            .refreshable { bluetooth.refreshSettings() }
            // Sync only when the HUD reports a new value. (onReceive on $settings re-delivered
            // the old value on every redraw and overwrote the user's pick before it was sent.)
            .onChange(of: bluetooth.settings) { _, value in draft = value }
            .onChange(of: draft) { _, value in
                if editable && value != bluetooth.settings { bluetooth.saveSettings(value) }
            }
            .onChange(of: scenePhase) { _, value in if value == .active { bluetooth.refreshSettings() } }
            .onChange(of: bluetooth.hardware) { _, value in hardwareDraft = value }
            .onChange(of: photoItem) { _, item in
                guard let item else { return }
                Task { await pickWallpaper(item); photoItem = nil }
            }
            .onChange(of: bluetooth.wallpaperColor) { _, value in
                if let value, value != clockColor.rgb { clockColor = Color(rgb: value) }
            }
            .onChange(of: clockColor) { _, value in
                // ColorPicker reports every drag step; send once it settles.
                colorSend?.cancel()
                let rgb = value.rgb
                guard rgb != bluetooth.wallpaperColor else { return }
                colorSend = Task { try? await Task.sleep(for: .milliseconds(400)); if !Task.isCancelled { bluetooth.setWallpaperColor(rgb) } }
            }
            .onAppear {
                draft = bluetooth.settings; hardwareDraft = bluetooth.hardware
                if let c = bluetooth.wallpaperColor { clockColor = Color(rgb: c) }
            }
        }
    }

    private func commitHardware() {
        if hardwareEditable && hardwareDraft != bluetooth.hardware { bluetooth.saveHardware(hardwareDraft) }
    }

    /// Sliders send once when the finger lifts, so dragging does not flood the BLE queue.
    private func sliderRow(_ title: String, icon: String, value: Binding<Int>, range: ClosedRange<Double>) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Label(title, systemImage: icon)
                Spacer()
                Text("\(value.wrappedValue)%").monospacedDigit().foregroundStyle(.secondary)
            }
            Slider(value: Binding(get: { Double(value.wrappedValue) }, set: { value.wrappedValue = Int($0.rounded()) }),
                   in: range, step: 5) { editing in if !editing { commitHardware() } }
                .accessibilityLabel(title)
        }
    }

    // MARK: Background (all faces) and the iPhone face

    @ViewBuilder private var backgroundControls: some View {
        Picker("Nền", selection: Binding(get: { photoBackground }, set: { bluetooth.setWallpaperBackground($0) })) {
            Text("Màu").tag(false); Text("Ảnh").tag(true)
        }
        .pickerStyle(.segmented)
        .disabled(bluetooth.wallpaperBackground == nil)
        if photoBackground {
            PhotosPicker(selection: $photoItem, matching: .images) {
                Label(wallpaperImage == nil ? "Chọn ảnh nền" : "Đổi ảnh nền", systemImage: "photo.on.rectangle.angled")
            }
        }
        if draft.face == 8 {
            VStack(alignment: .leading, spacing: 10) {
                Text("Màu giờ").font(.subheadline)
                HStack(spacing: 0) {
                    ForEach(clockSwatches, id: \.self) { rgb in
                        Button { clockColor = Color(rgb: rgb) } label: {
                            ZStack {
                                Circle().fill(Color(rgb: rgb)).frame(width: 28, height: 28)
                                    .overlay(Circle().stroke(Color(.separator), lineWidth: 0.5))
                                if clockColor.rgb == rgb { Circle().stroke(Color.primary, lineWidth: 2).frame(width: 36, height: 36) }
                            }.frame(maxWidth: .infinity, minHeight: 40)
                        }.buttonStyle(.plain)
                    }
                    ColorPicker("", selection: $clockColor, supportsOpacity: false).labelsHidden()
                        .frame(maxWidth: .infinity)
                }
            }
        }
        if !bluetooth.wallpaperStatus.isEmpty {
            Text(bluetooth.wallpaperStatus).font(.footnote).foregroundStyle(Color(.secondaryLabel))
        }
        if photoBackground && (bluetooth.wallpaperHasImage == true || wallpaperImage != nil) {
            Button("Xóa ảnh nền", role: .destructive) {
                bluetooth.clearWallpaper(); WallpaperStore.clear()
                wallpaperImage = nil; bluetooth.wallpaperHasImage = false; bluetooth.wallpaperBackground = false
                bluetooth.wallpaperStatus = ""
            }
        }
    }

    /// Photo (dimmed for classic faces) or the theme colour behind a preview.
    @ViewBuilder private func previewBackground(_ face: Int, dim: Double) -> some View {
        if photoBackground, let wallpaperImage {
            Image(uiImage: wallpaperImage).resizable().scaledToFill().overlay(Color.black.opacity(dim))
        } else if face == 8 {
            LinearGradient(colors: [themeBackground(previewTheme(face)), Color(rgb: 0x05070C)], startPoint: .top, endPoint: .bottom)
        } else {
            themeBackground(previewTheme(face))
        }
    }
    private func themeBackground(_ theme: Int) -> Color {
        [0x080E18, 0x071510, 0x181008, 0x150C19, 0xEDF3FA, 0xECF6F0, 0xFFF5E8, 0xFFF0F5].map { Color(rgb: $0) }[theme]
    }

    /// The "Đi xe" face without a trip: speed, clock, today's distance, hint card.
    private var ridePreview: some View {
        let theme = previewTheme(6), ink: Color = theme >= 4 ? .black : .white
        return VStack(spacing: 10) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 0) {
                    Text("42").font(.system(size: 92, weight: .bold, design: .rounded)).foregroundStyle(ink)
                    Text("KM/H").font(.system(size: 13, weight: .semibold)).foregroundStyle(.gray)
                }
                Spacer()
                VStack(alignment: .trailing, spacing: 3) {
                    Text("10:08").font(.system(size: 36, weight: .semibold, design: .rounded)).foregroundStyle(ink)
                    Text("Chủ nhật, 08/11").font(.system(size: 12)).foregroundStyle(.gray)
                    Text("28° · Không mưa").font(.system(size: 12)).foregroundStyle(.gray)
                    Text("Hôm nay").font(.system(size: 12)).foregroundStyle(.gray).padding(.top, 6)
                    Text("12,4 km").font(.system(size: 20, weight: .semibold)).foregroundStyle(ink)
                }
            }
            HStack(spacing: 10) {
                Image(systemName: "location.fill").font(.system(size: 18)).foregroundStyle(.gray)
                    .frame(width: 42, height: 42).background(Color.gray.opacity(0.25), in: RoundedRectangle(cornerRadius: 11))
                VStack(alignment: .leading, spacing: 2) {
                    Text("Chưa có chuyến đi").font(.system(size: 17, weight: .semibold)).foregroundStyle(ink)
                    Text("Vuốt sang Bản đồ để chọn điểm đến").font(.system(size: 12)).foregroundStyle(.gray)
                }
                Spacer(minLength: 0)
            }
            .padding(9).background(Color.gray.opacity(photoBackground ? 0.28 : 0.18), in: RoundedRectangle(cornerRadius: 14))
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .frame(width: 320, height: 240)
        .background { previewBackground(6, dim: 0.43) }
        .clipped()
    }

    private func pickWallpaper(_ item: PhotosPickerItem) async {
        bluetooth.wallpaperStatus = "Đang chuẩn bị ảnh…"
        guard let data = try? await item.loadTransferable(type: Data.self), let image = UIImage(data: data),
              let jpeg = WallpaperStore.makeJPEG(image) else { bluetooth.wallpaperStatus = "Không mở được ảnh này"; return }
        WallpaperStore.save(jpeg); wallpaperImage = UIImage(data: jpeg)
        bluetooth.wallpaperStatus = "Đang gửi ảnh lên HUD…"
        // The map stream may be mid-frame; retry for a few seconds.
        for _ in 0..<30 {
            if bluetooth.sendWallpaper(jpeg) { return }
            try? await Task.sleep(for: .milliseconds(400))
        }
        bluetooth.wallpaperStatus = bluetooth.ready ? "HUD đang bận, thử lại sau" : "Kết nối HUD để gửi ảnh"
    }

    /// Mirrors the HUD "iPhone" face: background, soft shades, date, big clock, glass widgets.
    private var phonePreview: some View {
        ZStack {
            VStack(spacing: 0) {
                LinearGradient(colors: [.black.opacity(0.6), .clear], startPoint: .top, endPoint: .bottom).frame(height: 110)
                Spacer()
                LinearGradient(colors: [.clear, .black.opacity(0.75)], startPoint: .top, endPoint: .bottom).frame(height: 110)
            }
            VStack(spacing: 0) {
                Text("Chủ Nhật, 8 tháng 11").font(.system(size: 15, weight: .semibold)).foregroundStyle(.white.opacity(0.9))
                    .padding(.top, 18)
                Text("10:08").font(.system(size: 78, weight: .bold, design: .rounded)).foregroundStyle(clockColor)
                    .padding(.top, 8)
                Spacer()
                HStack(spacing: 6) {
                    ForEach([("Thời tiết", "28°"), ("Âm lịch", "18/9"), ("Hôm nay", "12,4 km")], id: \.0) { item in
                        VStack(spacing: 2) {
                            Text(item.0).font(.system(size: 10)).foregroundStyle(.white.opacity(0.7))
                            Text(item.1).font(.system(size: 14, weight: .semibold)).foregroundStyle(.white)
                        }
                        .frame(maxWidth: .infinity).padding(.vertical, 6)
                        .background(.white.opacity(0.18), in: RoundedRectangle(cornerRadius: 12))
                        .overlay(RoundedRectangle(cornerRadius: 12).stroke(.white.opacity(0.28), lineWidth: 0.5))
                    }
                }.padding(.horizontal, 8).padding(.bottom, 8)
            }
        }
        .frame(width: 320, height: 240)
        .background { previewBackground(8, dim: 0) }
        .clipped()
    }

    @ViewBuilder private func facePreview(_ face: Int) -> some View {
        switch face {
        case 8: phonePreview
        case 6: ridePreview
        default: classicPreview(face)
        }
    }

    /// Lock-screen style gallery: every face as a live mini HUD; tap a card to use it.
    private var faceGallery: some View {
        ScrollViewReader { proxy in
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 12) {
                    ForEach(faces, id: \.id) { entry in
                        let face = entry.id
                        let selected = draft.face == face
                        Button { withAnimation(.snappy) { draft.face = face } } label: {
                            VStack(spacing: 8) {
                                facePreview(face)
                                    .scaleEffect(0.6, anchor: .topLeading)
                                    .frame(width: 192, height: 144, alignment: .topLeading)
                                    .clipShape(RoundedRectangle(cornerRadius: 16))
                                    .overlay(RoundedRectangle(cornerRadius: 16)
                                        .stroke(selected ? Color.accentColor : Color(.separator), lineWidth: selected ? 3 : 0.5))
                                    .shadow(color: .black.opacity(selected ? 0.25 : 0), radius: 8, y: 4)
                                HStack(spacing: 4) {
                                    if selected { Image(systemName: "checkmark.circle.fill") }
                                    Text(entry.name)
                                }
                                .font(.footnote.weight(selected ? .semibold : .regular))
                                .foregroundStyle(selected ? Color.accentColor : Color(.secondaryLabel))
                            }
                            .scaleEffect(selected ? 1 : 0.94)
                        }
                        .buttonStyle(.plain).id(face)
                        .accessibilityLabel("Mặt đồng hồ \(entry.name)")
                        .accessibilityAddTraits(selected ? .isSelected : [])
                    }
                }
                .padding(.vertical, 10).padding(.horizontal, 4)
            }
            .onAppear { proxy.scrollTo(draft.face, anchor: .center) }
            .onChange(of: draft.face) { _, face in withAnimation(.snappy) { proxy.scrollTo(face, anchor: .center) } }
        }
        .listRowInsets(EdgeInsets(top: 4, leading: 12, bottom: 4, trailing: 12))
    }

    private func classicPreview(_ face: Int) -> some View {
        let theme = previewTheme(face)
        return VStack(spacing: 8) {
            HStack {
                Text("HUD CONNECT").font(.system(size: 10, weight: .bold))
                Spacer(); Circle().fill(.green).frame(width: 6, height: 6)
            }.foregroundStyle(colors[theme])
            HStack(spacing: 12) {
                if face == 2 || face == 5 || face == 7 {
                    ZStack {
                        Circle().stroke(.gray, lineWidth: 2)
                        ForEach(0..<(face == 7 ? 17 : 12), id: \.self) { index in
                            Capsule().fill(.gray).frame(width: 2, height: 6).offset(y: -35).rotationEffect(.degrees(Double(index) * 30))
                        }
                        Capsule().fill(theme >= 4 ? .black : .white).frame(width: 3, height: 28).offset(y: -12).rotationEffect(.degrees(305))
                        Capsule().fill(colors[theme]).frame(width: 2, height: 34).offset(y: -15).rotationEffect(.degrees(60))
                        if face == 7 { Text("42\nkm/h").multilineTextAlignment(.center).font(.system(size: 12, weight: .bold)).foregroundStyle(theme >= 4 ? .black : .white) }
                    }.frame(width: face == 7 ? 120 : 84, height: face == 7 ? 120 : 84)
                }
                if face == 6 {
                    VStack(spacing: 4) {
                        Text("42").font(.system(size: 72, weight: .bold, design: .rounded)).foregroundStyle(theme >= 4 ? .black : .white)
                        Text("km/h").font(.headline).foregroundStyle(colors[theme])
                        Text("10:08 · Thứ bảy, 31/10/2026").font(.caption).foregroundStyle(.secondary)
                    }
                } else if face != 5 && face != 7 {
                    VStack(spacing: 6) {
                        Text("10:08").font(.system(size: face == 2 || face == 4 ? 32 : 64, weight: .bold, design: .rounded))
                            .foregroundStyle(face == 1 ? colors[theme] : theme >= 4 ? .black : .white)
                        if face == 3 || face == 4 {
                            Text(face == 4 ? "31" : "24").font(.system(size: face == 4 ? 56 : 30, weight: .bold)).foregroundStyle(colors[theme])
                        }
                        Text("Thứ bảy, 31/10/2026").font(.system(size: face == 2 ? 12 : 18, weight: .semibold))
                            .foregroundStyle(face == 1 ? (theme >= 4 ? .black : .white) : colors[theme])
                    }.minimumScaleFactor(0.7).lineLimit(1)
                }
            }.frame(maxWidth: .infinity, minHeight: 150)
            if face == 5 || face == 7 { Text(face == 7 ? "10:08" : "Thứ bảy, 31/10/2026").font(.caption).foregroundStyle(colors[theme]) }
            Text("Âm 18/09  ·  Hôm nay 12,4 km").font(.caption).foregroundStyle(.gray)
        }.padding(14).frame(width: 320, height: 240)
            .background { previewBackground(face, dim: 0.43) }
            .clipped()
    }
}

private struct AppFilterView: View {
    @ObservedObject var bluetooth: BluetoothLink
    @Binding var allowNewApps: Bool
    var body: some View {
        Form {
            Section {
                Toggle("Tự thêm app mới", isOn: $allowNewApps)
            } footer: { Text("Chỉ ẩn trên HUD, không đổi cài đặt iPhone. Tối đa 24 app.") }
            Section {
                ForEach(bluetooth.apps) { app in
                    Toggle(app.name, isOn: Binding(get: { app.enabled }, set: { bluetooth.setApp(app.id, enabled: $0) }))
                }
            }
        }
        .navigationTitle("Ứng dụng").navigationBarTitleDisplayMode(.inline)
        .refreshable { bluetooth.refreshSettings() }
        .disabled(!bluetooth.settingsReady)
    }
}

private struct HUDConnectionRow: View {
    @ObservedObject var bluetooth: BluetoothLink
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "display").font(.title2).foregroundStyle(bluetooth.ready ? Color.green : Color.secondary)
            VStack(alignment: .leading, spacing: 2) {
                Text("HUD iPhone").font(.headline)
                Text(bluetooth.ready ? "Đã kết nối" : bluetooth.status).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            if bluetooth.connecting { ProgressView() }
            else if !bluetooth.ready { Button("Kết nối") { bluetooth.connect() }.buttonStyle(.borderedProminent) }
        }
    }
}

// MARK: - Directions (full-screen map)

/// As-you-type suggestions, like the Apple Maps search field.
final class PlaceCompleter: NSObject, ObservableObject, MKLocalSearchCompleterDelegate {
    @Published var completions: [MKLocalSearchCompletion] = []
    private let completer = MKLocalSearchCompleter()
    override init() {
        super.init()
        completer.delegate = self
        completer.resultTypes = [.address, .pointOfInterest]
    }
    func update(_ text: String, near coordinate: CLLocationCoordinate2D?) {
        if let coordinate { completer.region = MKCoordinateRegion(center: coordinate, latitudinalMeters: 50_000, longitudinalMeters: 50_000) }
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty { completer.cancel(); completions = [] } else { completer.queryFragment = trimmed }
    }
    func completerDidUpdateResults(_ completer: MKLocalSearchCompleter) {
        let results = Array(completer.results.prefix(12))
        DispatchQueue.main.async { self.completions = results }
    }
    func completer(_ completer: MKLocalSearchCompleter, didFailWithError error: Error) {}
}

@MainActor struct NavigationHome: View {
    @ObservedObject var bluetooth: BluetoothLink
    @ObservedObject var navigation: NavigationModel
    @State private var query = ""
    @State private var showingRoute = false
    @State private var confirmingStop = false
    @State private var pickedPoint: CLLocationCoordinate2D?
    @State private var selectedFeature: MapFeature?
    @StateObject private var completer = PlaceCompleter()
    @State private var position: MapCameraPosition = .userLocation(followsHeading: false, fallback: .automatic)
    @FocusState private var searchFocused: Bool
    @Environment(\.openURL) private var openURL

    private var destinationCoordinate: CLLocationCoordinate2D? {
        guard let route = navigation.plannedRoute, route.polyline.pointCount > 0 else { return nil }
        return route.polyline.points()[route.polyline.pointCount - 1].coordinate
    }
    private var searchExpanded: Bool { searchFocused || !query.isEmpty }

    var body: some View {
        MapReader { proxy in
            Map(position: $position, selection: $selectedFeature) {
                UserAnnotation()
                if let route = navigation.plannedRoute {
                    MapPolyline(route.polyline).stroke(.blue, style: StrokeStyle(lineWidth: 7, lineCap: .round, lineJoin: .round))
                }
                if let parking = navigation.parking {
                    Marker("Chỗ đỗ xe", systemImage: "parkingsign", coordinate: parking.coordinate).tint(.blue)
                }
                if let destinationCoordinate {
                    Marker(navigation.destinationName, coordinate: destinationCoordinate)
                } else if let pickedPoint {
                    Marker("Điểm đã chọn", coordinate: pickedPoint)
                }
            }
            .mapStyle(.standard)
            .mapControls { MapUserLocationButton(); MapCompass() }
            .onTapGesture { point in
                if searchFocused { searchFocused = false; return }
                guard !navigation.navigating, !showingRoute, let coordinate = proxy.convert(point, from: .local) else { return }
                withAnimation(.snappy) { pickedPoint = coordinate }
            }
        }
        .safeAreaInset(edge: .top) { topBar }
        .safeAreaInset(edge: .bottom) { bottomCard.padding(.horizontal, 10).padding(.bottom, 8) }
        .task { navigation.requestLocation() }
        .onChange(of: navigation.plannedRoute) { _, route in
            guard let route, !navigation.navigating else { return }
            let rect = route.polyline.boundingMapRect
            withAnimation { position = .rect(rect.insetBy(dx: -rect.width * 0.25, dy: -rect.height * 0.45)) }
        }
        .onChange(of: navigation.navigating) { _, active in
            withAnimation { position = .userLocation(followsHeading: active, fallback: .automatic) }
        }
        .onChange(of: query) { _, value in
            if navigation.results.isEmpty == false { navigation.clearSearch() }
            completer.update(value, near: navigation.currentCoordinate)
        }
        // Tapping a place icon on the map routes to it directly.
        .onChange(of: selectedFeature) { _, feature in
            guard let feature else { return }
            selectedFeature = nil
            guard !navigation.navigating, !showingRoute else { return }
            MKMapItemRequest(feature: feature).getMapItem { item, _ in
                Task { @MainActor in if let item { choose(item) } }
            }
        }
        .onChange(of: navigation.importVersion) { _, _ in pickedPoint = nil; showingRoute = true }
        .onChange(of: navigation.importQuery) { _, value in query = value }
        .confirmationDialog("Kết thúc chuyến đi này?", isPresented: $confirmingStop, titleVisibility: .visible) {
            Button("Kết thúc", role: .destructive) { navigation.stop(); showingRoute = false }
        }
    }

    // MARK: Top

    private var topBar: some View {
        VStack(spacing: 8) {
            if navigation.navigating {
                HStack(spacing: 12) {
                    Image(systemName: "arrow.turn.up.right").font(.title.bold())
                    Text(navigation.instruction).font(.title3.bold()).lineLimit(2).minimumScaleFactor(0.8)
                    Spacer(minLength: 0)
                }
                .foregroundStyle(.white).padding(14)
                .background(Color(red: 0.1, green: 0.45, blue: 0.3), in: RoundedRectangle(cornerRadius: 18))
            }
            HStack {
                Button { if !bluetooth.ready && !bluetooth.connecting { bluetooth.connect() } } label: {
                    HStack(spacing: 6) {
                        if bluetooth.connecting { ProgressView().controlSize(.mini) }
                        else { Circle().fill(bluetooth.ready ? Color.green : Color.orange).frame(width: 8, height: 8) }
                        Text(bluetooth.ready ? "HUD" : bluetooth.connecting ? "Đang kết nối" : "Kết nối HUD")
                            .font(.footnote.weight(.semibold))
                    }
                    .padding(.horizontal, 12).padding(.vertical, 7)
                    .background(.regularMaterial, in: Capsule())
                }.buttonStyle(.plain)
                Spacer()
            }
            if !navigation.importStatus.isEmpty && !navigation.navigating {
                HStack {
                    if navigation.importing { ProgressView() }
                    Text(navigation.importStatus).font(.footnote)
                }.padding(10).frame(maxWidth: .infinity).background(.regularMaterial, in: RoundedRectangle(cornerRadius: 14))
            }
        }.padding(.horizontal, 10)
    }

    // MARK: Bottom

    @ViewBuilder private var bottomCard: some View {
        Group {
            if navigation.navigating { navigatingCard }
            else if showingRoute { routeCard }
            else if let pickedPoint { pickedCard(pickedPoint) }
            else { searchCard }
        }
        .padding(14)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 24))
        .shadow(color: .black.opacity(0.12), radius: 12, y: 4)
    }

    private var searchCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(Color(.secondaryLabel))
                TextField("Bạn muốn đi đâu?", text: $query)
                    .focused($searchFocused).submitLabel(.search).autocorrectionDisabled()
                    .onSubmit { navigation.find(query) }
                if navigation.searching { ProgressView() }
                else if !query.isEmpty {
                    Button { query = ""; navigation.clearSearch() } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(Color(.secondaryLabel)) }
                        .buttonStyle(.plain).accessibilityLabel("Xóa tìm kiếm")
                }
            }
            .padding(12).background(Color(.tertiarySystemFill), in: RoundedRectangle(cornerRadius: 14))

            if searchExpanded {
                ScrollView {
                    VStack(spacing: 0) {
                        if !navigation.results.isEmpty {
                            ForEach(Array(navigation.results.enumerated()), id: \.offset) { _, item in
                                placeRow(name: item.name ?? "Điểm đến", address: NavigationModel.address(of: item),
                                         distance: navigation.distanceText(to: item), icon: "mappin.circle.fill") { choose(item) }
                            }
                        } else if !query.isEmpty {
                            ForEach(completer.completions, id: \.self) { completion in
                                placeRow(name: completion.title, address: completion.subtitle,
                                         icon: completion.subtitle.isEmpty ? "magnifyingglass.circle.fill" : "mappin.circle.fill") { choose(completion) }
                            }
                            if completer.completions.isEmpty && !navigation.searching {
                                Text("Bấm Tìm trên bàn phím để tìm rộng hơn").font(.footnote).foregroundStyle(Color(.secondaryLabel)).padding(.vertical, 8)
                            }
                        }
                        if navigation.results.isEmpty && query.isEmpty {
                            ForEach(navigation.recentPlaces) { place in
                                placeRow(name: place.name, address: place.address, distance: navigation.distanceText(to: place.mapItem), icon: "clock.fill") { choose(place.mapItem) }
                            }
                        }
                        HStack {
                            Label("Dán liên kết Google/Apple Maps", systemImage: "link").font(.subheadline).foregroundStyle(Color(.secondaryLabel))
                            Spacer()
                            PasteButton(supportedContentTypes: [.url, .plainText], payloadAction: pasteDestination)
                                .labelStyle(.iconOnly).buttonBorderShape(.capsule).disabled(navigation.importing)
                        }.padding(.top, 10)
                    }
                }.frame(maxHeight: 300)
            } else if let parking = navigation.parking {
                HStack {
                    placeRow(name: "Chỗ đỗ xe", address: "Lưu lúc " + parking.savedAt.formatted(date: .omitted, time: .shortened),
                             distance: navigation.distanceText(to: parking.mapItem), icon: "parkingsign.circle.fill") {
                        searchFocused = false; pickedPoint = nil
                        if navigation.routeToParking() { showingRoute = true }
                    }
                    Button { navigation.clearParking() } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(Color(.secondaryLabel)) }
                        .buttonStyle(.plain).accessibilityLabel("Xóa chỗ đỗ xe")
                }
            } else if let place = navigation.recentPlaces.first {
                placeRow(name: place.name, address: place.address, distance: navigation.distanceText(to: place.mapItem), icon: "clock.fill") { choose(place.mapItem) }
            } else {
                Text("Tìm hoặc chạm vào bản đồ để chọn điểm đến").font(.footnote).foregroundStyle(Color(.secondaryLabel))
            }
        }
    }

    private func pickedCard(_ coordinate: CLLocationCoordinate2D) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Điểm đã chọn").font(.title3.bold())
                    Text(String(format: "%.5f, %.5f", coordinate.latitude, coordinate.longitude)).font(.caption).foregroundStyle(Color(.secondaryLabel))
                }
                Spacer()
                closeButton { withAnimation(.snappy) { pickedPoint = nil } }
            }
            Button {
                let item = MKMapItem(placemark: MKPlacemark(coordinate: coordinate)); item.name = "Điểm đã chọn"
                choose(item)
            } label: { primaryLabel("Chỉ đường", icon: "arrow.triangle.turn.up.right.diamond.fill") }
                .buttonStyle(.borderedProminent)
        }
    }

    private var routeCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(navigation.destinationName).font(.title3.bold()).lineLimit(1)
                    if !navigation.destinationAddress.isEmpty {
                        Text(navigation.destinationAddress).font(.caption).foregroundStyle(Color(.secondaryLabel)).lineLimit(2)
                    }
                }
                Spacer()
                closeButton { navigation.stop(); showingRoute = false }
            }
            Picker("Loại tuyến", selection: $navigation.transport) {
                Label("Ô tô", systemImage: "car.fill").tag("Ô tô")
                Label("Đi bộ", systemImage: "figure.walk").tag("Đi bộ")
            }.pickerStyle(.segmented).onChange(of: navigation.transport) { _, _ in navigation.changeTransport() }

            if navigation.plannedRoute != nil {
                Text(navigation.tripSummary).font(.headline)
                if !navigation.rainForecast.isEmpty {
                    Label(navigation.rainLikely ? "\(navigation.rainForecast) · nhớ mang áo mưa" : navigation.rainForecast,
                          systemImage: navigation.rainLikely ? "cloud.rain.fill" : "sun.max.fill")
                        .font(.subheadline).foregroundStyle(navigation.rainLikely ? Color.orange : Color(.secondaryLabel))
                }
            } else {
                HStack(spacing: 8) {
                    if navigation.preparing { ProgressView() }
                    Text(navigation.status).font(.subheadline).foregroundStyle(Color(.secondaryLabel))
                    Spacer()
                    if !navigation.preparing { Button("Thử lại") { navigation.changeTransport() } }
                }
                if navigation.locationDenied {
                    Button("Cho phép vị trí") { openURL(URL(string: UIApplication.openSettingsURLString)!) }
                }
            }
            if bluetooth.ready {
                Button {
                    navigation.beginGuidance()
                    if navigation.navigating { showingRoute = false }
                } label: { primaryLabel("Bắt đầu", icon: "location.north.fill") }
                    .buttonStyle(.borderedProminent).tint(.green)
                    .disabled(navigation.preparing || navigation.plannedRoute == nil)
            } else {
                Button { bluetooth.connect() } label: { primaryLabel(bluetooth.connecting ? "Đang kết nối HUD…" : "Kết nối HUD để bắt đầu", icon: "display") }
                    .buttonStyle(.borderedProminent).disabled(bluetooth.connecting)
            }
            Text("Apple Maps chưa có tuyến xe máy; tuyến ô tô có thể qua đường cấm xe máy.")
                .font(.caption2).foregroundStyle(Color(.secondaryLabel))
        }
    }

    private var navigatingCard: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(navigation.destinationName).font(.headline).lineLimit(1)
                Text(navigation.tripSummary.isEmpty ? navigation.status : navigation.tripSummary)
                    .font(.subheadline).foregroundStyle(Color(.secondaryLabel)).lineLimit(1)
            }
            Spacer()
            Button(role: .destructive) { confirmingStop = true } label: {
                Text("Kết thúc").font(.headline).padding(.horizontal, 6).padding(.vertical, 4)
            }.buttonStyle(.borderedProminent).tint(.red)
        }
    }

    // MARK: Helpers

    private func choose(_ item: MKMapItem) {
        searchFocused = false
        pickedPoint = nil
        navigation.prepare(to: item)
        showingRoute = true
    }

    private func choose(_ completion: MKLocalSearchCompletion) {
        searchFocused = false
        MKLocalSearch(request: MKLocalSearch.Request(completion: completion)).start { response, _ in
            Task { @MainActor in
                if let item = response?.mapItems.first { choose(item) }
                else { navigation.importStatus = "Không mở được địa điểm này. Thử bấm Tìm." }
            }
        }
    }

    private func pasteDestination(_ providers: [NSItemProvider]) {
        guard let provider = providers.first else { return }
        searchFocused = false
        let type = provider.hasItemConformingToTypeIdentifier(UTType.url.identifier) ? UTType.url : .plainText
        provider.loadItem(forTypeIdentifier: type.identifier, options: nil) { value, _ in
            let text = (value as? URL)?.absoluteString ?? (value as? String) ??
                (value as? Data).flatMap { String(data: $0, encoding: .utf8) }
            Task { @MainActor in
                if let text { await navigation.importShared(text) }
                else { navigation.importStatus = "Chưa đọc được liên kết. Hãy sao chép lại địa điểm từ ứng dụng bản đồ." }
            }
        }
    }

    private func placeRow(name: String, address: String, distance: String = "", icon: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: icon).font(.title2).foregroundStyle(.red, Color(.systemFill))
                VStack(alignment: .leading, spacing: 2) {
                    Text(name).font(.body.weight(.medium)).foregroundStyle(Color(.label)).lineLimit(1)
                    if !address.isEmpty { Text(address).font(.subheadline).foregroundStyle(Color(.secondaryLabel)).lineLimit(2) }
                }
                Spacer(minLength: 0)
                if !distance.isEmpty { Text(distance).font(.caption.monospacedDigit()).foregroundStyle(Color(.secondaryLabel)) }
            }.padding(.vertical, 8).contentShape(Rectangle())
        }.buttonStyle(.plain)
    }

    private func primaryLabel(_ title: String, icon: String) -> some View {
        Label(title, systemImage: icon).font(.headline).frame(maxWidth: .infinity).padding(.vertical, 6)
    }

    private func closeButton(_ action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: "xmark").font(.footnote.bold()).foregroundStyle(Color(.secondaryLabel))
                .frame(width: 30, height: 30).background(Color(.tertiarySystemFill), in: Circle())
        }.buttonStyle(.plain).accessibilityLabel("Đóng")
    }
}

// MARK: - Wallpaper helpers

enum WallpaperStore {
    private static var url: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("hud-wallpaper.jpg")
    }
    static func load() -> UIImage? { (try? Data(contentsOf: url)).flatMap(UIImage.init(data:)) }
    static func save(_ jpeg: Data) {
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? jpeg.write(to: url, options: .atomic)
    }
    static func clear() { try? FileManager.default.removeItem(at: url) }
    /// Centre-crops to the HUD's 320x240 and keeps the JPEG under the 48 KB transfer limit.
    static func makeJPEG(_ image: UIImage) -> Data? {
        let size = CGSize(width: 320, height: 240)
        let format = UIGraphicsImageRendererFormat(); format.scale = 1; format.opaque = true
        let scale = max(size.width / image.size.width, size.height / image.size.height)
        let drawn = CGSize(width: image.size.width * scale, height: image.size.height * scale)
        let rendered = UIGraphicsImageRenderer(size: size, format: format).image { _ in
            image.draw(in: CGRect(x: (size.width - drawn.width) / 2, y: (size.height - drawn.height) / 2,
                                  width: drawn.width, height: drawn.height))
        }
        for quality in stride(from: 0.85, through: 0.35, by: -0.1) {
            if let data = rendered.jpegData(compressionQuality: quality), data.count <= 46 * 1024 { return data }
        }
        return nil
    }
}

extension Color {
    init(rgb: Int) {
        self.init(red: Double(rgb >> 16 & 255) / 255, green: Double(rgb >> 8 & 255) / 255, blue: Double(rgb & 255) / 255)
    }
    var rgb: Int {
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        UIColor(self).getRed(&r, green: &g, blue: &b, alpha: &a)
        func c(_ v: CGFloat) -> Int { Int((min(1, max(0, v)) * 255).rounded()) }
        return c(r) << 16 | c(g) << 8 | c(b)
    }
}
