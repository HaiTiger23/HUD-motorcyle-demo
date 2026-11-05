import SwiftUI
import MapKit
import UniformTypeIdentifiers

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
                ContentView(bluetooth: bluetooth)
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

@MainActor struct ContentView: View {
    @ObservedObject var bluetooth: BluetoothLink
    @Environment(\.scenePhase) private var scenePhase
    @State private var draft = HUDSettings()
    @State private var hardwareDraft=HUDHardwareSettings()
    private let colors: [Color] = [Color(red: 0.25, green: 0.85, blue: 0.96), .mint, .orange, .pink,
        Color(red: 0, green: 0.41, blue: 0.66), Color(red: 0, green: 0.48, blue: 0.31),
        Color(red: 0.6, green: 0.31, blue: 0), Color(red: 0.64, green: 0.15, blue: 0.4)]
    private let themeNames = ["Đại dương", "Rừng xanh", "Hoàng hôn", "Anh đào", "Trắng xanh", "Bạc hà", "Kem", "Hồng sáng"]
    private var editable: Bool { bluetooth.settingsReady && !bluetooth.settingsBusy }
    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack(spacing: 12) {
                        Image(systemName: "display").font(.title).foregroundStyle(colors[draft.theme])
                        VStack(alignment: .leading, spacing: 4) {
                            Text("HUD iPhone").font(.headline)
                            Label(bluetooth.status, systemImage: bluetooth.ready ? "circle.fill" : "circle")
                                .font(.caption).foregroundStyle(bluetooth.ready ? Color.green : Color.secondary)
                        }
                        Spacer()
                        if bluetooth.connecting { ProgressView() }
                    }
                    if !bluetooth.ready {
                        Button("Kết nối màn hình") { bluetooth.connect() }.disabled(bluetooth.connecting)
                    }
                }
                Section("Giao diện màn hình") {
                    facePreview
                    Picker("Mặt đồng hồ", selection: $draft.face) {
                        Text("Rõ nét").tag(0); Text("Sắc màu").tag(1); Text("Kim hiện đại").tag(2)
                        Text("Có giây").tag(3); Text("Ngày lớn").tag(4); Text("Kim cổ điển").tag(5)
                        Text("Tốc độ số").tag(6); Text("Tốc độ kim").tag(7)
                    }.pickerStyle(.menu)
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 4), spacing: 16) {
                        ForEach(0..<8) { index in
                            Button { draft.theme = index } label: {
                                VStack(spacing: 6) {
                                    ZStack {
                                        Circle().fill(colors[index]).frame(width: 38, height: 38)
                                        if draft.theme == index { Image(systemName: "checkmark").bold().foregroundStyle(index >= 4 ? .white : .black) }
                                    }
                                    Text(themeNames[index]).font(.caption2).foregroundStyle(.primary)
                                }.frame(maxWidth: .infinity)
                            }.buttonStyle(.plain).accessibilityLabel(themeNames[index])
                        }
                    }.padding(.vertical, 8)
                }.disabled(!editable)
                Section {
                    HStack { Label("Độ sáng HUD",systemImage:"sun.max.fill"); Spacer(); Text("\(hardwareDraft.brightness)%").monospacedDigit() }
                    Slider(value:Binding(get:{Double(hardwareDraft.brightness)},set:{hardwareDraft.brightness=Int($0.rounded())}),in:5...100,step:5)
                        .accessibilityLabel("Độ sáng màn hình HUD")
                    HStack { Label("Âm lượng loa HUD",systemImage:hardwareDraft.speakerVolume==0 ? "speaker.slash.fill" : "speaker.wave.2.fill"); Spacer(); Text("\(hardwareDraft.speakerVolume)%").monospacedDigit() }
                    Slider(value:Binding(get:{Double(hardwareDraft.speakerVolume)},set:{hardwareDraft.speakerVolume=Int($0.rounded())}),in:0...100,step:5)
                        .accessibilityLabel("Âm lượng loa ESP32")
                    Picker("Xoay màn hình HUD",selection:$hardwareDraft.rotation) {
                        Text("Bình thường · 0°").tag(0)
                        Text("Lật ngược · 180°").tag(2)
                    }
                    Button("Áp dụng màn hình và loa") { bluetooth.saveHardware(hardwareDraft) }
                        .disabled(hardwareDraft==bluetooth.hardware)
                    Button("Nghe thử loa",systemImage:"speaker.wave.2") { bluetooth.testSpeaker() }
                        .disabled(bluetooth.voiceActive || hardwareDraft != bluetooth.hardware)
                } header: { Text("Độ sáng, loa và góc xoay") }
                footer: { Text("Bấm áp dụng để lưu trên ESP32. Âm lượng này dành cho loa board, không đổi nhạc iPhone. Nghe thử phát một âm ngắn. Lật 180° giữ nguyên bố cục và đổi tọa độ cảm ứng theo.") }
                    .disabled(!editable || !bluetooth.hardwareReady)
                Section {
                    Picker("Tắt màn hình sau", selection: $draft.timeout) {
                        Text("15 giây").tag(15); Text("30 giây").tag(30)
                        Text("1 phút").tag(60); Text("2 phút").tag(120)
                        Text("5 phút").tag(300); Text("Luôn sáng").tag(0)
                    }
                    Toggle("Cho phép app mới gửi thông báo", isOn: $draft.allowNewApps)
                } header: { Text("Màn hình và thông báo mới") }
                footer: { Text("Chạm để bật lại màn hình. Lần chạm đầu chỉ đánh thức màn hình. Cài đặt được lưu trên ESP32.") }
                    .disabled(!editable)
                Section {
                    Button { bluetooth.saveSettings(draft) } label: {
                        HStack {
                            Spacer()
                            if bluetooth.settingsBusy { ProgressView() }
                            Text("Áp dụng lên HUD").bold()
                            Spacer()
                        }
                    }.disabled(!editable || draft == bluetooth.settings)
                    Text(bluetooth.settingsStatus).font(.footnote).foregroundStyle(.secondary)
                }
                Section {
                    ForEach(bluetooth.apps) { app in
                        Toggle(isOn: Binding(get: { app.enabled }, set: { bluetooth.setApp(app.id, enabled: $0) })) {
                            Text(app.name)
                        }.disabled(!editable)
                    }
                    Button("Làm mới danh sách app", systemImage: "arrow.clockwise") { bluetooth.refreshSettings() }
                        .disabled(!editable)
                } header: { Text("Ứng dụng được hiện trên HUD") }
                footer: { Text("Tắt một app sẽ ẩn thông báo của app đó trên HUD, không đổi cài đặt iPhone. Danh sách bổ sung khi HUD nhận thông báo mới; bấm làm mới để xem. Tối đa 24 app. Trên iPhone vẫn cần cho phép chia sẻ thông báo hệ thống và bản xem trước.") }
            }
            .navigationTitle("Cài đặt HUD")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Đọc lại", systemImage: "arrow.clockwise") { bluetooth.refreshSettings() }.disabled(!editable)
                }
            }
            .onReceive(bluetooth.$settings) { draft = $0 }
            .onChange(of: scenePhase) { _, value in if value == .active { bluetooth.refreshSettings() } }
            .onChange(of: bluetooth.hardware) { _, value in hardwareDraft=value }
            .onAppear { hardwareDraft=bluetooth.hardware }
        }.tint(colors[draft.theme])
    }
    private var facePreview: some View {
        VStack(spacing: 8) {
            HStack {
                Text("HUD CONNECT").font(.system(size: 10, weight: .bold))
                Spacer(); Circle().fill(.green).frame(width: 6, height: 6)
            }.foregroundStyle(colors[draft.theme])
            HStack(spacing: 12) {
                if draft.face == 2 || draft.face == 5 || draft.face == 7 {
                    ZStack {
                        Circle().stroke(.gray, lineWidth: 2)
                        ForEach(0..<(draft.face == 7 ? 17 : 12)) { index in
                            Capsule().fill(.gray).frame(width: 2, height: 6).offset(y: -35).rotationEffect(.degrees(Double(index) * 30))
                        }
                        Capsule().fill(draft.theme >= 4 ? .black : .white).frame(width: 3, height: 28).offset(y: -12).rotationEffect(.degrees(305))
                        Capsule().fill(colors[draft.theme]).frame(width: 2, height: 34).offset(y: -15).rotationEffect(.degrees(60))
                        if draft.face == 7 { Text("42\nkm/h").multilineTextAlignment(.center).font(.system(size: 12, weight: .bold)).foregroundStyle(draft.theme >= 4 ? .black : .white) }
                    }.frame(width: draft.face == 7 ? 120 : 84, height: draft.face == 7 ? 120 : 84)
                }
                if draft.face == 6 { VStack(spacing: 4) {
                    Text("42").font(.system(size: 72, weight: .bold, design: .rounded)).foregroundStyle(draft.theme >= 4 ? .black : .white)
                    Text("km/h").font(.headline).foregroundStyle(colors[draft.theme])
                    Text("10:08 · Thứ bảy, 31/10/2026").font(.caption).foregroundStyle(.secondary)
                }
                } else if draft.face != 5 && draft.face != 7 { VStack(spacing: 6) {
                    Text("10:08").font(.system(size: draft.face == 2 || draft.face == 4 ? 32 : 64, weight: .bold, design: .rounded))
                        .foregroundStyle(draft.face == 1 ? colors[draft.theme] : draft.theme >= 4 ? .black : .white)
                    if draft.face == 3 || draft.face == 4 {
                        Text(draft.face == 4 ? "31" : "24").font(.system(size: draft.face == 4 ? 56 : 30, weight: .bold)).foregroundStyle(colors[draft.theme])
                    }
                    Text("Thứ bảy, 31/10/2026").font(.system(size: draft.face == 2 ? 12 : 18, weight: .semibold))
                        .foregroundStyle(draft.face == 1 ? (draft.theme >= 4 ? .black : .white) : colors[draft.theme])
                }.minimumScaleFactor(0.7).lineLimit(1)
                }
            }.frame(maxWidth: .infinity, minHeight: 150)
            if draft.face == 5 || draft.face == 7 { Text(draft.face == 7 ? "10:08" : "Thứ bảy, 31/10/2026").font(.caption).foregroundStyle(colors[draft.theme]) }
        }.padding(14).background(draft.theme >= 4 ? Color(red: 0.95, green: 0.97, blue: 0.99) : Color(red: 0.04, green: 0.06, blue: 0.09), in: RoundedRectangle(cornerRadius: 18))
            .padding(.vertical, 6).accessibilityLabel("Xem trước mặt đồng hồ")
    }
}

@MainActor struct NavigationHome: View {
    @ObservedObject var bluetooth: BluetoothLink
    @ObservedObject var navigation: NavigationModel
    @State private var query = ""
    @State private var showingRoute = false
    @State private var confirmingStop = false
    @State private var showingMapPicker = false
    @State private var pickedItem: MKMapItem?
    @FocusState private var searchFocused: Bool
    @Environment(\.openURL) private var openURL
    private func choose(_ item: MKMapItem) {
        searchFocused = false
        navigation.prepare(to: item)
        showingRoute = true
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
    var body: some View {
        NavigationStack {
            List {
                Section {
                    HStack {
                        Label(bluetooth.ready ? "HUD sẵn sàng" : bluetooth.status,
                              systemImage: bluetooth.ready ? "checkmark.circle.fill" : "antenna.radiowaves.left.and.right")
                            .foregroundStyle(bluetooth.ready ? Color.green : Color.secondary)
                            .font(.subheadline)
                        Spacer()
                        if bluetooth.connecting { ProgressView() }
                        else if !bluetooth.ready { Button("Kết nối") { bluetooth.connect() } }
                    }
                    Label(navigation.gpsStatus, systemImage: "location").font(.caption).foregroundStyle(.secondary)
                    Toggle("Giữ GPS khi kết nối HUD",isOn:$navigation.backgroundGPS)
                        .onChange(of:navigation.backgroundGPS) { _,_ in navigation.saveBackgroundGPS() }
                    Text("Giúp tìm tuyến và cập nhật tốc độ khi iPhone khóa. iOS hiện chỉ báo vị trí; chế độ này dùng thêm pin. Mở app và kết nối HUD trước khi khóa máy.")
                        .font(.caption).foregroundStyle(.secondary)
                    if navigation.locationDenied {
                        Button("Cho phép vị trí trong Cài đặt") { openURL(URL(string: UIApplication.openSettingsURLString)!) }
                    }
                }
                if !navigation.importStatus.isEmpty {
                    Section {
                        HStack {
                            if navigation.importing { ProgressView() }
                            Text(navigation.importStatus).font(.subheadline).foregroundStyle(.secondary)
                        }
                    }
                }
                if navigation.navigating {
                    Section {
                        Label(navigation.destinationName, systemImage: "mappin.circle.fill").font(.title3.bold())
                        Text(navigation.instruction).font(.title2.bold())
                        Text(navigation.status).foregroundStyle(.secondary)
                        if let preview = navigation.preview {
                            Image(uiImage: preview).resizable().aspectRatio(1.5, contentMode: .fit)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                                .accessibilityLabel("Bản đồ đang gửi lên HUD")
                        }
                        Text("Trên ESP32, vuốt sang màn hình Bản đồ để xem.").font(.footnote).foregroundStyle(.secondary)
                        Button("Kết thúc chuyến đi", systemImage: "stop.circle.fill", role: .destructive) { confirmingStop = true }
                    } header: { Text("Đang dẫn đường") }
                } else {
                    Section {
                        HStack {
                            Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                            TextField("Bạn muốn đi đâu?", text: $query)
                                .focused($searchFocused).submitLabel(.search)
                                .onSubmit { searchFocused = false; navigation.find(query) }
                                .autocorrectionDisabled()
                            if !query.isEmpty {
                                Button { query = ""; navigation.clearSearch() } label: { Image(systemName: "xmark.circle.fill") }
                                    .buttonStyle(.borderless).accessibilityLabel("Xóa tìm kiếm")
                            }
                        }
                        Button {
                            searchFocused = false; navigation.find(query)
                        } label: {
                            HStack { Text("Tìm địa điểm"); Spacer(); if navigation.searching { ProgressView() } }
                        }.disabled(query.trimmingCharacters(in: .whitespacesAndNewlines).count < 2 || navigation.searching)
                        Button("Chọn điểm trên bản đồ", systemImage: "map.fill") {
                            searchFocused = false; showingMapPicker = true
                        }
                    }
                    Section {
                        HStack {
                            Label("Dán liên kết địa điểm", systemImage: "link")
                            Spacer()
                            PasteButton(supportedContentTypes: [.url, .plainText], payloadAction: pasteDestination)
                                .labelStyle(.titleAndIcon).disabled(navigation.importing)
                        }
                    } footer: {
                        Text("Google Maps hoặc Apple Maps → Chia sẻ → Sao chép liên kết, rồi dán tại đây. HUD tính tuyến bằng Apple Maps.")
                    }
                    if !navigation.results.isEmpty {
                        Section("Kết quả tìm kiếm") {
                            ForEach(Array(navigation.results.enumerated()), id: \.offset) { _, item in
                                Button { choose(item) } label: {
                                    placeRow(name: item.name ?? "Điểm đến", address: item.placemark.title ?? "")
                                }.buttonStyle(.plain)
                            }
                        }
                    } else if navigation.searching || !query.isEmpty {
                        Section { Text(navigation.status).foregroundStyle(.secondary) }
                    }
                    if !navigation.recentPlaces.isEmpty {
                        Section("Điểm đến gần đây") {
                            ForEach(navigation.recentPlaces) { place in
                                Button { choose(place.mapItem) } label: { placeRow(name: place.name, address: place.address) }
                                    .buttonStyle(.plain)
                            }
                        }
                    }
                    Section {
                        Label("Tìm địa điểm, xem tuyến rồi bấm bắt đầu. Bản đồ sẽ tự gửi lên HUD.", systemImage: "display")
                            .font(.subheadline).foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle("Chỉ đường")
            .scrollDismissesKeyboard(.interactively)
            .task { navigation.requestLocation() }
            .sheet(isPresented: $showingRoute, onDismiss: {
                if !navigation.navigating { navigation.stop() }
            }) { routePreview }
            .sheet(isPresented: $showingMapPicker, onDismiss: {
                if let item = pickedItem { pickedItem = nil; choose(item) }
            }) {
                DestinationMapPicker(coordinate: navigation.currentCoordinate) { item in
                    pickedItem = item; showingMapPicker = false
                }
            }
            .onChange(of: navigation.importVersion) { _, _ in showingRoute = true }
            .onChange(of: navigation.importQuery) { _, value in query = value }
            .confirmationDialog("Kết thúc chuyến đi này?", isPresented: $confirmingStop, titleVisibility: .visible) {
                Button("Kết thúc chuyến đi", role: .destructive) { navigation.stop() }
            }
        }
    }
    private func placeRow(name: String, address: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: "mappin.circle.fill").font(.title2).foregroundStyle(.cyan)
            VStack(alignment: .leading, spacing: 5) {
                Text(name).font(.headline).foregroundStyle(.primary)
                if !address.isEmpty { Text(address).font(.caption).foregroundStyle(.secondary).lineLimit(2) }
            }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right").font(.caption.bold()).foregroundStyle(.secondary)
        }.padding(.vertical, 5)
    }
    private var routePreview: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    Text(navigation.destinationName).font(.title2.bold())
                    if !navigation.destinationAddress.isEmpty { Text(navigation.destinationAddress).foregroundStyle(.secondary) }
                    Picker("Loại tuyến", selection: $navigation.transport) {
                        Text("Ô tô").tag("Ô tô"); Text("Đi bộ").tag("Đi bộ")
                    }.pickerStyle(.segmented).onChange(of: navigation.transport) { _, _ in navigation.changeTransport() }
                    if let route = navigation.plannedRoute {
                        Map {
                            MapPolyline(route.polyline).stroke(.cyan, lineWidth: 5)
                            UserAnnotation()
                            if route.polyline.pointCount > 0 {
                                Marker(navigation.destinationName, coordinate: route.polyline.points()[route.polyline.pointCount - 1].coordinate)
                            }
                        }.frame(height: 235).clipShape(RoundedRectangle(cornerRadius: 18))
                        Text(navigation.tripSummary).font(.title3.bold())
                    } else {
                        VStack(spacing: 12) {
                            if navigation.preparing { ProgressView() }
                            Text(navigation.status).multilineTextAlignment(.center).foregroundStyle(.secondary)
                            if navigation.locationDenied {
                                Button("Mở Cài đặt vị trí") { openURL(URL(string: UIApplication.openSettingsURLString)!) }
                            }
                            if !navigation.preparing { Button("Thử lại") { navigation.changeTransport() } }
                        }.frame(maxWidth: .infinity, minHeight: 180)
                    }
                    Text("Tuyến Apple Maps hiện có ô tô và đi bộ, chưa có tuyến xe máy riêng. Tuyến ô tô có thể đi qua đường cấm xe máy.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if !bluetooth.ready {
                        Text(bluetooth.status).font(.subheadline).foregroundStyle(.secondary)
                        Button("Kết nối HUD") { bluetooth.connect() }.disabled(bluetooth.connecting)
                    }
                    Button {
                        navigation.beginGuidance()
                        if navigation.navigating { showingRoute = false }
                    } label: {
                        Label("Bắt đầu trên HUD", systemImage: "location.north.fill")
                            .font(.headline).frame(maxWidth: .infinity).padding(.vertical, 8)
                    }.buttonStyle(.borderedProminent).controlSize(.large)
                        .disabled(navigation.preparing || navigation.plannedRoute == nil || !bluetooth.ready)
                }.padding(20)
            }
            .navigationTitle("Xem tuyến").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("Đóng") { showingRoute = false } } }
        }
    }
}

private struct DestinationMapPicker: View {
    let coordinate: CLLocationCoordinate2D?
    let selected: (MKMapItem) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var picked: CLLocationCoordinate2D?
    @State private var position: MapCameraPosition = .userLocation(followsHeading: false, fallback: .automatic)
    var body: some View {
        NavigationStack {
            MapReader { proxy in
                Map(position: $position) {
                    UserAnnotation()
                    if let picked { Marker("Điểm đến", coordinate: picked) }
                }
                .onTapGesture { point in
                    if let coordinate = proxy.convert(point, from: .local) { picked = coordinate }
                }
                .mapControls { MapUserLocationButton(); MapCompass(); MapScaleView() }
                .overlay(alignment: .top) {
                    Text("Di chuyển bản đồ rồi chạm vào điểm muốn đến")
                        .font(.subheadline).padding(12).background(.regularMaterial, in: Capsule()).padding()
                        .allowsHitTesting(false)
                }
                .safeAreaInset(edge: .bottom) {
                    Button {
                        guard let picked else { return }
                        let item = MKMapItem(placemark: MKPlacemark(coordinate: picked)); item.name = "Điểm đã chọn"
                        selected(item)
                    } label: { Label("Chọn điểm này", systemImage: "mappin.and.ellipse").font(.headline).frame(maxWidth: .infinity).padding(8) }
                        .buttonStyle(.borderedProminent).controlSize(.large).disabled(picked == nil)
                        .padding().background(.regularMaterial)
                }
                .onAppear {
                    if let coordinate { position = .region(MKCoordinateRegion(center: coordinate, latitudinalMeters: 3000, longitudinalMeters: 3000)) }
                }
            }
            .navigationTitle("Chọn trên bản đồ").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("Đóng") { dismiss() } } }
        }
    }
}

