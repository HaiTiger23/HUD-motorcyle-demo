import Foundation
import MapKit
import CoreLocation
import UIKit
import Combine

@MainActor final class NavigationModel: NSObject, ObservableObject, @preconcurrency CLLocationManagerDelegate {
    @Published var results: [MKMapItem] = []
    @Published var status = "Cho phép vị trí để tìm đường"
    @Published var navigating = false
    @Published var preview: UIImage?
    @Published var instruction = "Chưa bắt đầu dẫn đường"
    @Published var transport = "Ô tô"
    @Published var gpsStatus = "Đang chờ vị trí iPhone"
    @Published var backgroundGPS=UserDefaults.standard.object(forKey:"hudBackgroundGPS") as? Bool ?? true
    private var hudLocationActivity: CLBackgroundActivitySession?
    private var hudGPSWaitTimer: Timer?
    func updateHUDLocationActivity() {
        let authorized=locations.authorizationStatus == .authorizedAlways || locations.authorizationStatus == .authorizedWhenInUse
        if backgroundGPS && bluetooth.ready && authorized {
            if hudLocationActivity == nil && UIApplication.shared.applicationState == .active {
                hudLocationActivity=CLBackgroundActivitySession()
                HUDDiagnostics.record("HUD location activity: started in foreground")
            }
        } else {
            hudLocationActivity?.invalidate(); hudLocationActivity=nil
            HUDDiagnostics.record("HUD location activity: stopped")
        }
        locations.allowsBackgroundLocationUpdates=navigating || preparing || hudLocationActivity != nil
        locations.showsBackgroundLocationIndicator=locations.allowsBackgroundLocationUpdates
        if authorized { locations.startUpdatingLocation() }
    }
    func saveBackgroundGPS() {
        UserDefaults.standard.set(backgroundGPS,forKey:"hudBackgroundGPS")
        updateHUDLocationActivity()
    }
    @Published var locationDenied = false
    @Published var currentCoordinate: CLLocationCoordinate2D?
    @Published var externalGuidance = false
    @Published var importVersion = 0
    @Published var importQuery = ""
    @Published var importStatus = ""
    @Published var importing = false
    @Published var searching = false
    @Published var preparing = false
    @Published var plannedRoute: MKRoute?
    @Published var destinationName = ""
    @Published var destinationAddress = ""
    @Published var recentPlaces: [SavedPlace] = []
    private var guidanceRequested = false
    struct SavedPlace: Codable, Identifiable, Equatable {
        let name: String
        let address: String
        let latitude: Double
        let longitude: Double
        var id: String { "\(latitude),\(longitude)" }
        var mapItem: MKMapItem {
            let item = MKMapItem(placemark: MKPlacemark(coordinate: CLLocationCoordinate2D(latitude: latitude, longitude: longitude)))
            item.name = name; return item
        }
    }
    var tripSummary: String {
        guard let route = plannedRoute else { return "" }
        let distance = route.distance < 1000 ? "\(Int(route.distance.rounded())) m" : String(format: "%.1f km", route.distance / 1000)
        let minutes = max(1, Int(ceil(route.expectedTravelTime / 60)))
        let duration = minutes < 60 ? "\(minutes) phút" : "\(minutes / 60) giờ \(minutes % 60) phút"
        return "\(distance) · \(duration)"
    }
    let bluetooth: BluetoothLink
    private let locations = CLLocationManager()
    private var latest: CLLocation?
    private var destination: MKMapItem?
    private var route: MKRoute?
    private var geometry: RouteProgress?
    private var steps: [(end: Double, text: String)] = []
    private var segment = 0
    private var progress = 0.0
    private var lastRender = Date.distantPast
    private var lastPlan = Date.distantPast
    private var planning = false
    private var rendering = false
    private var encoding = false
    private var directions: MKDirections?
    private var search: MKLocalSearch?
    private var snapshotter: MKMapSnapshotter?
    private var cachedSnapshot: MKMapSnapshotter.Snapshot?
    private var cachedRoutePath: CGPath?
    private var snapshotLocation: CLLocation?
    private var snapshotHeading = 0.0
    private var snapshotLight = false
    private var snapshotAt = Date.distantPast
    private var snapshotAttemptAt = Date.distantPast
    private var baseVersion = 0
    private var sentBaseVersion = -1
    private var sentLocation: CLLocation?
    private var sentInstruction = ""
    private var cameraDistance = 800.0
    private var browseCenter: CLLocationCoordinate2D?
    private var browseHeading = 0.0
    private var cameraEpoch = 0
    private var snapshotEpoch = -1
    private var sentCameraEpoch = -1
    private var snapshotCenter = CLLocationCoordinate2D()
    private var markerState = HUDLocationMarkerState()
    private var renderedMarker: HUDLocationMarker = .dot
    private var sentMarker: HUDLocationMarker = .dot
    private var latestSpeed: Double?
    private struct FrameContext {
        let id: UInt32
        let snapshot: MKMapSnapshotter.Snapshot
        let center: CLLocationCoordinate2D
        let heading: Double
    }
    private var frameContexts: [FrameContext] = []
    private var connectionObserver: AnyCancellable?
    private var timer: Timer?
    private var revision = 0
    private var waitingToStart = false
    private var hudSearchSession: UInt32?
    private var hudChoiceContext=UUID().uuidString
    private var hudChoiceAt=Date.distantPast
    private var hudPreviewReady=false
    private var hudSelectionFromAI=false
    private var hudBackgroundTask: UIBackgroundTaskIdentifier = .invalid
    private var hudBackgroundEpoch=UUID()
    private var hudPreparationTimer: Timer?
    private func endHUDPreparationLocation() {
        hudPreparationTimer?.invalidate(); hudPreparationTimer=nil
        hudGPSWaitTimer?.invalidate(); hudGPSWaitTimer=nil
        if !navigating { locations.allowsBackgroundLocationUpdates=hudLocationActivity != nil }
    }
    func finishHUDBackgroundIfIdle() {
        finishHistoryDelivery()
        guard !searching, !planning, !preparing, !bluetooth.voicePending else { return }
        endHUDBackground()
    }
    private func endHUDBackground() {
        hudBackgroundEpoch=UUID()
        guard hudBackgroundTask != .invalid else { return }
        let task=hudBackgroundTask; hudBackgroundTask = .invalid
        UIApplication.shared.endBackgroundTask(task)
        HUDDiagnostics.record("HUD navigation background: ended")
    }
    private func beginHUDBackground() -> Bool {
        endHUDBackground()
        let epoch=hudBackgroundEpoch
        hudBackgroundTask=UIApplication.shared.beginBackgroundTask(withName:"HUD destination search/route") { [weak self] in
            guard let self, self.hudBackgroundEpoch==epoch else { return }
            let session=self.hudSearchSession
            self.endHUDBackground()
            self.search?.cancel(); self.search=nil; self.searching=false
            self.directions?.cancel(); self.revision += 1
            self.planning=false; self.preparing=false; self.waitingToStart=false
            self.endHUDPreparationLocation()
            self.hudPreviewReady=false; self.hudChoiceContext=UUID().uuidString
            self.status="iOS hết thời gian xử lý nền. Mở HUD Connect rồi thử lại."
            if let session { self.sendHUDSearch(session,phase:3,index:0,total:0,text:self.status) }
            HUDDiagnostics.record("HUD navigation background: expired")
        }
        HUDDiagnostics.record("HUD navigation background: started=\(hudBackgroundTask != .invalid); appState=\(UIApplication.shared.applicationState.rawValue)")
        return hudBackgroundTask != .invalid || UIApplication.shared.applicationState != .background
    }
    var assistantNavigationContext: String {
        let state: String = searching ? "searching" : hudPreviewReady ? "preview" : preparing ? "planning" : hudSearchResults.isEmpty ? "none" : "results"
        let data: [String:Any] = ["context":hudChoiceContext,"state":state,
            "valid":hudSearchSession != nil && Date().timeIntervalSince(hudChoiceAt)<300,
            "destination":destinationName,
            "results":hudSearchResults.enumerated().map { ["number":$0.offset+1,"name":$0.element.name ?? "Điểm đến","address":String(($0.element.placemark.title ?? "").prefix(180))] as [String:Any] }]
        guard let json=try? JSONSerialization.data(withJSONObject:data),let text=String(data:json,encoding:.utf8) else { return "{}" }
        return text
    }
    func performAssistantNavigation(_ intent: HUDNavigationIntent, voiceSession: UInt32) throws -> String {
        func rejected(_ text: String) -> NSError { NSError(domain:"HUD.Navigation",code:1,userInfo:[NSLocalizedDescriptionKey:text]) }
        guard intent.isAllowed(currentContext:hudChoiceContext,age:Date().timeIntervalSince(hudChoiceAt),
              hasSession:hudSearchSession != nil,resultsCount:hudSearchResults.count,searching:searching,
              previewReady:hudPreviewReady) else { throw rejected("Chưa có kết quả/tuyến phù hợp, hoặc lựa chọn đã thay đổi/hết hạn. Hãy tìm hoặc chọn lại địa điểm.") }
        guard bluetooth.voiceReady, voiceSession != 0 else { throw rejected("Hãy dùng mic trên HUD để chọn và xác nhận chuyến đi.") }
        switch intent {
        case .select(let index,_):
            guard !searching, hudSearchResults.indices.contains(index) else { throw rejected("Chưa có kết quả đó. Hãy tìm địa điểm trước.") }
            hudSearchSession=voiceSession
            let item=hudSearchResults[index]
            sendHUDSearch(voiceSession,phase:0,index:0,total:0,text:"Đang tính tuyến…")
            prepare(to:item)
            hudSelectionFromAI=true
            return "Đã chọn \(item.name ?? "địa điểm"). Đang tính tuyến; hãy chờ màn hình xác nhận rồi nói Bắt đầu hoặc bấm nút trên HUD."
        case .start:
            guard hudPreviewReady, route != nil, !preparing, !planning, bluetooth.ready,
                  let latest, abs(latest.timestamp.timeIntervalSinceNow)<10, latest.horizontalAccuracy>=0,
                  latest.horizontalAccuracy<=50 else { throw rejected("Chưa sẵn sàng bắt đầu. Hãy chờ tuyến và vị trí GPS mới, rồi xác nhận lại.") }
            hudSearchSession=voiceSession
            beginGuidance()
            guard navigating else { throw rejected("Chưa bắt đầu được chuyến đi.") }
            hudChoiceContext=UUID().uuidString; hudPreviewReady=false; hudSearchSession=nil
            sendHUDSearch(voiceSession,phase:4,index:0,total:0,text:"Đã bắt đầu dẫn đường")
            return "Đã bắt đầu dẫn đường đến \(destinationName)."
        }
    }
    private var hudSearchResults: [MKMapItem] = []
    private var historyObserver: AnyCancellable?
    private var hudHistoryToken=UInt32.random(in:1...UInt32.max)
    private var hudHistoryPlaces: [SavedPlace]=[]
    private var historyBackground: UIBackgroundTaskIdentifier = .invalid
    private var historyBackgroundEpoch=UUID()
    private func finishHistoryDelivery() {
        guard !bluetooth.voicePending,historyBackground != .invalid else { return }
        historyBackgroundEpoch=UUID()
        let task=historyBackground; historyBackground = .invalid
        UIApplication.shared.endBackgroundTask(task)
    }
    func publishHUDHistory() {
        guard bluetooth.voiceReady else { return }
        if historyBackground == .invalid {
            let epoch=historyBackgroundEpoch
            historyBackground=UIApplication.shared.beginBackgroundTask(withName:"HUD recent destinations") { [weak self] in
                guard let self,self.historyBackgroundEpoch==epoch,self.historyBackground != .invalid else { return }
                let task=self.historyBackground; self.historyBackground = .invalid; self.historyBackgroundEpoch=UUID()
                UIApplication.shared.endBackgroundTask(task)
            }
        }
        if hudHistoryPlaces != recentPlaces {
            hudHistoryPlaces=Array(recentPlaces.prefix(5)); hudHistoryToken=UInt32.random(in:1...UInt32.max)
        }
        let token=(0..<4).map { UInt8(truncatingIfNeeded:hudHistoryToken >> ($0*8)) }
        var packets=[Data([0x56,navigating ? 1 : 0])]
        if hudHistoryPlaces.isEmpty { packets.append(Data([0x54]+token+[0,0])) }
        for (index,place) in hudHistoryPlaces.enumerated() {
            let text=HUDVoiceWire.bounded("\(place.name)\n\(place.address)",bytes:bluetooth.destinationTextLimit)
            packets.append(Data([0x54]+token+[UInt8(index),UInt8(hudHistoryPlaces.count)]+Array(text.utf8)))
        }
        bluetooth.sendVoice(packets)
        finishHistoryDelivery()
    }
    func handleHUDHistory(_ request: HUDHistoryRequest) {
        if request.action==0 { publishHUDHistory(); return }
        HUDDiagnostics.record("HUD history selection: index=\(request.index); tokenMatch=\(request.token==hudHistoryToken)")
        guard request.token==hudHistoryToken,hudHistoryPlaces.indices.contains(request.index),!navigating,!preparing,!planning else {
            sendHUDSearch(request.session,phase:3,index:0,total:0,text:"Lịch sử đã thay đổi hoặc đang tính tuyến. Hãy chọn lại.")
            publishHUDHistory(); return
        }
        hudSearchSession=request.session; hudSearchResults=hudHistoryPlaces.map { $0.mapItem }
        prepare(to:hudSearchResults[request.index])
        destinationAddress=hudHistoryPlaces[request.index].address
    }

    init(bluetooth: BluetoothLink) {
        self.bluetooth = bluetooth
        super.init()
        bluetooth.onMapControl = { [weak self] control in self?.handleMapControl(control) }
        connectionObserver = bluetooth.$ready.dropFirst().removeDuplicates().sink { [weak self] ready in
            // @Published emits in willSet; wait until the new readiness value
            // is committed before helpers read it from BluetoothLink.
            Task { @MainActor [weak self] in
                guard let self,self.bluetooth.ready==ready else { return }
                if !ready { self.resetCamera(); self.resetMapRenderer() }
                else { self.bluetooth.sendSpeed(self.latestSpeed) }
                self.updateHUDLocationActivity()
            }
        }
        if let data = UserDefaults.standard.data(forKey: "hudRecentPlaces"),
           let saved = try? JSONDecoder().decode([SavedPlace].self, from: data) { recentPlaces = Array(saved.prefix(5)) }
        historyObserver=bluetooth.$voiceReady.removeDuplicates().sink { [weak self] ready in
            if ready { Task { @MainActor [weak self] in self?.publishHUDHistory() } }
        }
        locations.delegate = self
        locations.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        locations.distanceFilter = kCLDistanceFilterNone
        locations.activityType = .automotiveNavigation
        locations.pausesLocationUpdatesAutomatically = false
        timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.renderIfNeeded() }
        }
    }
    func requestLocation() {
        if locations.authorizationStatus == .denied || locations.authorizationStatus == .restricted {
            locationDenied = true; waitingToStart = false; preparing = false
            status = "Cho phép vị trí trong Cài đặt để tìm đường"
            if let session=hudSearchSession {
                sendHUDSearch(session,phase:3,index:0,total:0,text:status)
                endHUDPreparationLocation(); finishHUDBackgroundIfIdle()
            }
            return
        }
        locations.requestWhenInUseAuthorization()
        locations.startUpdatingLocation()
    }
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        print("HUD GPS: authorization=\(manager.authorizationStatus.rawValue)")
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            locationDenied = false
            manager.startUpdatingLocation()
            updateHUDLocationActivity()
            if !navigating && !preparing { status = "Đang lấy vị trí…" }
        case .denied, .restricted:
            locationDenied = true; waitingToStart = false; preparing = false
            updateHUDLocationActivity()
            status = "Bật quyền vị trí trong Cài đặt"; gpsStatus = "Chưa được phép dùng vị trí"
        default: break
        }
    }
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        status = "Chưa lấy được GPS: \(error.localizedDescription)"
    }
    func locationManager(_ manager: CLLocationManager, didUpdateLocations fixes: [CLLocation]) {
        guard let fix = fixes.last, fix.horizontalAccuracy >= 0 else { return }
        latestSpeed = HUDWire.speedKmh(metresPerSecond: fix.speed, accuracy: fix.horizontalAccuracy,
            speedAccuracy: fix.speedAccuracy, age: fix.timestamp.timeIntervalSinceNow)
        bluetooth.sendSpeed(latestSpeed)
        gpsStatus = "GPS ±\(Int(fix.horizontalAccuracy.rounded())) m"
        if locations.accuracyAuthorization == .reducedAccuracy {
            gpsStatus = "Bật Vị trí chính xác trong Cài đặt"
        }
        guard fix.horizontalAccuracy <= 50, abs(fix.timestamp.timeIntervalSinceNow) < 30 else {
            if waitingToStart { status = "Đang chờ GPS rõ hơn · \(gpsStatus)" }
            return
        }
        latest = fix
        currentCoordinate = fix.coordinate
        if waitingToStart {
            hudGPSWaitTimer?.invalidate(); hudGPSWaitTimer=nil
            waitingToStart = false; plan(from: fix); return
        }
        if !navigating { return }
        if externalGuidance { renderIfNeeded(); return }
        if geometry == nil && !planning && Date().timeIntervalSince(lastPlan) > 30 { plan(from: fix) }
        updateProgress(fix)
        renderIfNeeded()
    }
    func find(_ query: String) {
        guard query.trimmingCharacters(in: .whitespacesAndNewlines).count >= 2 else { return }
        search?.cancel(); results = []
        let request = MKLocalSearch.Request()
        request.naturalLanguageQuery = query
        if let latest {
            request.region = MKCoordinateRegion(center: latest.coordinate, latitudinalMeters: 20000, longitudinalMeters: 20000)
        }
        let operation = MKLocalSearch(request: request)
        search = operation; searching = true; status = "Đang tìm địa điểm…"
        operation.start { [weak self, weak operation] response, error in
            Task { @MainActor in
                guard let self, self.search === operation else { return }
                self.searching = false
                self.results = response?.mapItems ?? []
                self.status = error?.localizedDescription ?? (self.results.isEmpty ? "Không tìm thấy địa điểm" : "Chọn điểm đến")
            }
        }
    }
    func clearSearch() {
        search?.cancel(); search = nil; searching = false; results = []
        status = "Nhập địa điểm hoặc địa chỉ"
    }
    // Voice search is independent of the AI gateway.  The session is echoed
    // in every packet so an old MapKit response cannot open a newer search.
    func findFromHUDVoice(_ query: String, session: UInt32) {
        endHUDPreparationLocation()
        endHUDBackground()
        hudChoiceContext=UUID().uuidString; hudChoiceAt=Date(); hudPreviewReady=false
        revision += 1; directions?.cancel(); planning=false; preparing=false
        waitingToStart=false
        hudSearchSession=session; hudSearchResults=[]; search?.cancel(); search=nil; searching=false; results=[]
        guard query.trimmingCharacters(in: .whitespacesAndNewlines).count >= 2 else {
            sendHUDSearch(session, phase: 3, index: 0, total: 0, text: "Không nghe rõ địa điểm. Hãy nói lại."); return
        }
        guard beginHUDBackground() else {
            sendHUDSearch(session,phase:3,index:0,total:0,text:"Mở HUD Connect rồi thử tìm lại."); return
        }
        sendHUDSearch(session, phase: 0, index: 0, total: 0, text: "Đang tìm địa điểm…")
        let request = MKLocalSearch.Request(); request.naturalLanguageQuery = query
        if let latest { request.region = MKCoordinateRegion(center: latest.coordinate, latitudinalMeters: 20000, longitudinalMeters: 20000) }
        let operation = MKLocalSearch(request: request); search=operation; searching=true
        operation.start { [weak self, weak operation] response, error in
            Task { @MainActor in
                guard let self, self.search === operation, self.hudSearchSession == session else { return }
                defer { self.finishHUDBackgroundIfIdle() }
                self.searching=false; self.hudSearchResults=Array((response?.mapItems ?? []).prefix(5)); self.results=self.hudSearchResults
                HUDDiagnostics.record("HUD destination search: results=\(self.hudSearchResults.count); errorDomain=\((error as NSError?)?.domain ?? "none"); errorCode=\((error as NSError?)?.code ?? 0); appState=\(UIApplication.shared.applicationState.rawValue)")
                guard error == nil, !self.hudSearchResults.isEmpty else {
                    self.sendHUDSearch(session, phase: 3, index: 0, total: 0, text: error?.localizedDescription ?? "Không tìm thấy địa điểm"); return
                }
                for (index,item) in self.hudSearchResults.enumerated() {
                    let name=item.name ?? "Điểm đến"
                    let address=item.placemark.title ?? ""
                    let distance: Int?
                    if let latest=self.latest, let location=item.placemark.location { distance=Int(location.distance(from:latest).rounded()) }
                    else { distance=nil }
                    let prefix=distance.map { $0>=1000 ? String(format:"%.1f km · ",Double($0)/1000) : "\($0) m · " } ?? ""
                    self.sendHUDSearch(session, phase: 1, index: index, total: self.hudSearchResults.count, text: "\(name)\n\(prefix)\(address)")
                }
            }
        }
    }
    func handleHUDSearchControl(session: UInt32, action: UInt8, index: UInt8) {
        guard hudSearchSession == session else { return }
        switch action {
        case 0:
            endHUDBackground(); revision += 1; directions?.cancel()
            planning=false; preparing=false; waitingToStart=false
            endHUDPreparationLocation()
            hudChoiceContext=UUID().uuidString; hudPreviewReady=false
            search?.cancel(); search=nil; hudSearchResults=[]; hudSearchSession=nil; status="Đã hủy tìm trên HUD"
        case 1:
            guard !preparing, !planning, Int(index)<hudSearchResults.count else { return }
            prepare(to:hudSearchResults[Int(index)])
        case 2:
            guard hudPreviewReady, route != nil, !preparing else { return }; beginGuidance()
            if navigating { hudSearchSession=nil; hudPreviewReady=false; hudChoiceContext=UUID().uuidString }
        default: break
        }
    }
    private func sendHUDSearch(_ session: UInt32, phase: UInt8, index: Int, total: Int, text: String) {
        HUDDiagnostics.record("HUD destination TX: phase=\(phase); index=\(index); total=\(total); voiceReady=\(bluetooth.voiceReady)")
        let bounded=HUDVoiceWire.bounded(text,bytes:bluetooth.destinationTextLimit)
        let id=[UInt8](0..<4).map { UInt8(truncatingIfNeeded: session >> ($0*8)) }
        bluetooth.sendVoice([Data([0x50] + id + [phase,UInt8(index),UInt8(total)] + Array(bounded.utf8))])
    }
    func importShared(_ text: String) async {
        guard !importing else { return }
        importing = true; importStatus = "Đang đọc điểm đến được chia sẻ…"
        defer { importing = false }
        guard var url = MapShare.firstURL(in: text), MapShare.allowed(url) else {
            importStatus = "Chưa nhận ra đường dẫn địa điểm. Hãy chia sẻ từ Google Maps hoặc Apple Maps."; return
        }
        if MapShare.place(from: url) == nil {
            do {
                let config = URLSessionConfiguration.ephemeral; config.timeoutIntervalForRequest = 15
                let session = URLSession(configuration: config, delegate: MapLinkRedirectGuard(), delegateQueue: nil)
                defer { session.invalidateAndCancel() }
                // Only resolve redirects; do not download the full Maps web page.
                let (_, response) = try await session.bytes(from: url)
                if let resolved = response.url, MapShare.allowed(resolved) { url = resolved }
            } catch { importStatus = "Chưa mở được đường dẫn. Kiểm tra mạng rồi chia sẻ lại."; return }
        }
        guard let place = MapShare.place(from: url) else {
            importStatus = "Đường dẫn này chưa chứa vị trí đọc được. Bạn có thể tìm tên địa điểm hoặc chọn trên bản đồ."; return
        }
        if navigating { stop() }
        if let lat = place.latitude, let lon = place.longitude {
            let item = MKMapItem(placemark: MKPlacemark(coordinate: CLLocationCoordinate2D(latitude: lat, longitude: lon)))
            item.name = place.query
            prepare(to: item); importVersion += 1
            importStatus = "Đã nhận điểm đến. Xem tuyến trước khi bắt đầu."
        } else {
            importQuery = place.query; find(place.query)
            importStatus = "Chọn địa điểm đúng trong kết quả tìm kiếm."
        }
    }
    func startGoogleGuidance() {
        stop(); destination = nil
        externalGuidance = true; navigating = true; destinationName = "Theo Google Maps"
        instruction = "Chỉ dẫn từ thông báo Google Maps"
        status = "Nền bản đồ theo GPS · mở Google Maps để chỉ đường"
        locations.allowsBackgroundLocationUpdates = true
        locations.showsBackgroundLocationIndicator = true
        requestLocation(); lastRender = .distantPast; renderIfNeeded()
    }
    func prepare(to item: MKMapItem) {
        endHUDPreparationLocation()
        endHUDBackground()
        hudSelectionFromAI=false
        hudChoiceContext=UUID().uuidString; hudChoiceAt=Date(); hudPreviewReady=false
        if navigating { navigating=false; bluetooth.stopNavigation() }
        resetCamera()
        externalGuidance = false
        search?.cancel(); search = nil; searching = false
        directions?.cancel(); resetMapRenderer(); revision += 1
        planning = false; rendering = false; preparing = true; guidanceRequested = false
        plannedRoute = nil; route = nil; geometry = nil
        destination = item
        destinationName = item.name ?? "Điểm đến"
        destinationAddress = item.placemark.title ?? ""
        if let session=hudSearchSession, !beginHUDBackground() {
            preparing=false; status="Mở HUD Connect rồi thử chọn lại địa điểm."
            sendHUDSearch(session,phase:3,index:0,total:0,text:status); return
        }
        if let session=hudSearchSession {
            locations.allowsBackgroundLocationUpdates=true
            locations.showsBackgroundLocationIndicator=true
            HUDDiagnostics.record("HUD destination prepare: gpsAge=\(latest.map { Int(abs($0.timestamp.timeIntervalSinceNow)) } ?? -1); authorization=\(locations.authorizationStatus.rawValue); appState=\(UIApplication.shared.applicationState.rawValue)")
            sendHUDSearch(session,phase:6,index:0,total:0,text:"Đang lấy vị trí và tính tuyến…")
            hudPreparationTimer=Timer.scheduledTimer(withTimeInterval:300,repeats:false) { [weak self] _ in
                Task { @MainActor in
                    guard let self, !self.navigating else { return }
                    self.handleHUDSearchControl(session:session,action:0,index:0)
                    self.sendHUDSearch(session,phase:3,index:0,total:0,text:"Tuyến đã hết hạn. Hãy tìm lại điểm đến.")
                }
            }
        }
        guard let latest, abs(latest.timestamp.timeIntervalSinceNow) < 30 else {
            waitingToStart = true
            status = "Đang lấy vị trí mới; có GPS sẽ tự tính tuyến"
            if let session=hudSearchSession {
                hudGPSWaitTimer=Timer.scheduledTimer(withTimeInterval:20,repeats:false) { [weak self] _ in
                    Task { @MainActor in
                        guard let self,self.waitingToStart,self.hudSearchSession==session else { return }
                        self.waitingToStart=false; self.preparing=false; self.hudPreviewReady=false
                        self.status="Chưa lấy được GPS mới. Mở HUD Connect, bật GPS nền rồi thử lại."
                        self.endHUDPreparationLocation()
                        self.sendHUDSearch(session,phase:3,index:0,total:0,text:self.status)
                        self.finishHUDBackgroundIfIdle()
                    }
                }
            }
            requestLocation(); return
        }
        waitingToStart = false
        plan(from: latest)
    }
    func beginGuidance() {
        guard route != nil, !preparing, bluetooth.ready else { return }
        guidanceRequested = true; navigating = true
        hudPreparationTimer?.invalidate(); hudPreparationTimer=nil
        locations.allowsBackgroundLocationUpdates = true
        locations.showsBackgroundLocationIndicator = true
        status = "Đang dẫn đường · tuyến \(transport.lowercased())"
        if let coordinate = destination?.placemark.coordinate {
            let place = SavedPlace(name: destinationName, address: destinationAddress, latitude: coordinate.latitude, longitude: coordinate.longitude)
            recentPlaces.removeAll { $0.id == place.id }; recentPlaces.insert(place, at: 0)
            recentPlaces = Array(recentPlaces.prefix(5))
            if let data = try? JSONEncoder().encode(recentPlaces) { UserDefaults.standard.set(data, forKey: "hudRecentPlaces") }
        }
        publishHUDHistory()
        if let latest { updateProgress(latest) }
        lastRender = .distantPast; renderIfNeeded()
    }
    func changeTransport() {
        guard !navigating, let destination else { return }
        prepare(to: destination)
    }
    private func plan(from location: CLLocation) {
        guard !planning, let destination else { return }
        resetMapRenderer()
        planning = true; preparing = !navigating; lastPlan = Date(); revision += 1
        let token = revision
        let request = MKDirections.Request()
        request.source = MKMapItem(placemark: MKPlacemark(coordinate: location.coordinate))
        request.destination = destination
        request.transportType = transport == "Đi bộ" ? .walking : .automobile
        let operation = MKDirections(request: request)
        directions = operation
        status = navigating ? "Đang tính lại tuyến…" : "Đang tính tuyến…"
        if let session=hudSearchSession { sendHUDSearch(session,phase:6,index:0,total:0,text:status) }
        print("HUD route: planning started")
        operation.calculate { [weak self] response, error in
            Task { @MainActor in
                guard let self, token == self.revision else { return }
                defer { self.finishHUDBackgroundIfIdle() }
                HUDDiagnostics.record("HUD route result: routes=\(response?.routes.count ?? 0); errorDomain=\((error as NSError?)?.domain ?? "none"); errorCode=\((error as NSError?)?.code ?? 0)")
                self.planning = false
                self.preparing = false
                guard let route = response?.routes.first else {
                    print("HUD route: planning failed, code=\((error as NSError?)?.code ?? -1)")
                    self.status = error?.localizedDescription ?? "Không tìm được tuyến ở khu vực này"
                    // Do not keep broadcasting the old route after rerouting failed.
                    self.route = nil; self.geometry = nil; self.plannedRoute = nil
                    self.endHUDPreparationLocation()
                    if let session=self.hudSearchSession { self.sendHUDSearch(session, phase: 3, index: 0, total: 0, text: self.status) }
                    return
                }
                print("HUD route: ready, points=\(route.polyline.pointCount), steps=\(route.steps.count)")
                let coordinates = (0..<route.polyline.pointCount).map { route.polyline.points()[$0].coordinate }
                let geometry = RouteProgress(points: coordinates.map { GeoPoint($0.latitude, $0.longitude) })
                self.route = route; self.geometry = geometry; self.plannedRoute = route
                self.steps = []
                var lastSegment = 0
                for step in route.steps where step.distance > 1 && !step.instructions.isEmpty && step.polyline.pointCount > 0 {
                    let coordinate = step.polyline.points()[step.polyline.pointCount - 1].coordinate
                    if let projected = geometry.project(GeoPoint(coordinate.latitude, coordinate.longitude), startingAt: lastSegment) {
                        self.steps.append((projected.metres, step.instructions)); lastSegment = projected.segment
                    }
                }
                self.progress = 0; self.segment = 0
                if self.guidanceRequested {
                    self.navigating = true
                    self.status = "Đang dẫn đường · tuyến \(self.transport.lowercased())"
                    self.updateProgress(location); self.lastRender = .distantPast; self.renderIfNeeded()
                } else {
                    self.status = "Sẵn sàng bắt đầu"
                    self.hudPreviewReady = self.hudSearchSession != nil
                    if let session=self.hudSearchSession { self.sendHUDSearch(session, phase: self.hudSelectionFromAI ? 5 : 2, index: 0, total: 0, text: "\(self.destinationName)\n\(self.tripSummary)") }
                }
            }
        }
    }
    private func updateProgress(_ location: CLLocation) {
        guard let geometry,
              let match = geometry.project(GeoPoint(location.coordinate.latitude, location.coordinate.longitude), startingAt: max(0, segment - 3)) else { return }
        if match.error > 70 {
            status = "Bạn đang lệch tuyến"
            if Date().timeIntervalSince(lastPlan) > 30 { plan(from: location) }
            return
        }
        segment = match.segment; progress = match.metres
        let next = steps.first { $0.end > progress + 3 }
        instruction = next?.text ?? "Đến gần điểm đến"
        if let destination,
           location.distance(from: CLLocation(latitude: destination.placemark.coordinate.latitude,
                                              longitude: destination.placemark.coordinate.longitude)) < 20,
           (geometry.cumulative.last ?? 0) - progress < 40 {
            status = "Đã đến điểm đến"; instruction = "Đã đến nơi"
        }
    }
    func stop() {
        endHUDBackground()
        endHUDPreparationLocation()
        hudChoiceContext=UUID().uuidString; hudPreviewReady=false; hudSearchSession=nil; hudSearchResults=[]
        resetCamera()
        externalGuidance = false
        waitingToStart = false
        guidanceRequested = false; preparing = false; plannedRoute = nil
        revision += 1; directions?.cancel(); resetMapRenderer()
        planning = false; rendering = false; navigating = false
        route = nil; geometry = nil; steps = []; preview = nil
        locations.allowsBackgroundLocationUpdates = hudLocationActivity != nil
        status = "Đã dừng dẫn đường"; instruction = "Chưa bắt đầu dẫn đường"
        bluetooth.stopNavigation()
        publishHUDHistory()
    }
    private func renderIfNeeded() {
        guard navigating, bluetooth.ready, !bluetooth.voiceActive, !bluetooth.speakerActive, !planning,
              let latest, externalGuidance || route != nil, abs(latest.timestamp.timeIntervalSinceNow) < 10,
              latest.horizontalAccuracy <= 50 else { return }
        let now = Date()
        let light = bluetooth.settings.theme >= 4
        renderedMarker = markerState.update(speed: latest.speed, horizontalAccuracy: latest.horizontalAccuracy,
            course: latest.course, courseAccuracy: latest.courseAccuracy,
            age: latest.timestamp.timeIntervalSinceNow, now: now)
        let heading = browseCenter != nil ? browseHeading :
            latest.course >= 0 && latest.speed > 1.5 ? latest.course : snapshotHeading
        if rendering && now.timeIntervalSince(snapshotAttemptAt) > 8 {
            snapshotter?.cancel(); snapshotter = nil; rendering = false
            snapshotAttemptAt = now
        }
        let needsBase = cachedSnapshot == nil || snapshotEpoch != cameraEpoch || snapshotLight != light || HUDMapCadence.refreshBase(
            age: now.timeIntervalSince(snapshotAt),
            movement: browseCenter != nil ? 0 : snapshotLocation.map { latest.distance(from: $0) } ?? .infinity,
            headingChange: HUDMapCadence.headingChange(heading, snapshotHeading))
        if needsBase && !rendering && now.timeIntervalSince(snapshotAttemptAt) >= 1.5 {
            requestSnapshot(location: latest, heading: heading, light: light)
        }
        // A pending tile fetch must not hold up the latest position/instruction.
        guard !bluetooth.busy, !encoding, let snapshot = cachedSnapshot, snapshotLight == light, snapshotEpoch == cameraEpoch,
              sentCameraEpoch != snapshotEpoch || HUDMapCadence.sendFrame(age: now.timeIntervalSince(lastRender),
                movement: sentLocation.map { latest.distance(from: $0) } ?? .infinity,
                changed: sentBaseVersion != baseVersion || sentInstruction != instruction || sentMarker != renderedMarker,
                speed: max(0, latest.speed)) else { return }
        // Drawing and JPEG encoding take tens of milliseconds; run them off the
        // main actor so the UI, GPS callbacks and BLE pump are never blocked.
        let remaining = max(0, (steps.first { $0.end > progress + 3 }?.end ?? progress) - progress)
        let job = MapRenderJob(snapshot: snapshot, location: latest, marker: renderedMarker, light: light,
            heading: snapshotHeading, center: snapshotCenter, epoch: snapshotEpoch, baseVersion: baseVersion,
            routePath: cachedRoutePath, instruction: instruction, remaining: remaining,
            externalGuidance: externalGuidance, revision: revision, maxBytes: HUDWire.maxImage)
        encoding = true
        lastRender = now
        Task.detached(priority: .userInitiated) { [weak self] in
            let started = Date()
            let result = Self.renderFrame(job)
            let ms = Int(Date().timeIntervalSince(started) * 1000)
            await self?.finishRender(job, result: result, renderMs: ms)
        }
    }
    private func finishRender(_ job: MapRenderJob, result: (image: UIImage, jpeg: Data)?, renderMs: Int) {
        encoding = false
        // Drop frames made for a camera, route or session that has since changed.
        guard let result, navigating, job.revision == revision, job.epoch == cameraEpoch,
              job.epoch == snapshotEpoch, !bluetooth.busy else { return }
        print("HUD map: renderMs=\(renderMs); jpegBytes=\(result.jpeg.count)")
        preview = result.image
        sentLocation = job.location; sentInstruction = job.instruction; sentBaseVersion = job.baseVersion
        if let id = bluetooth.send(jpeg: result.jpeg, externalGuidance: job.externalGuidance) {
            sentCameraEpoch = job.epoch; sentMarker = job.marker
            frameContexts.append(FrameContext(id: id, snapshot: job.snapshot, center: job.center, heading: job.heading))
            if frameContexts.count > 6 { frameContexts.removeFirst(frameContexts.count - 6) }
        }
    }
    private func resetMapRenderer() {
        snapshotter?.cancel(); snapshotter = nil; rendering = false
        cachedSnapshot = nil; cachedRoutePath = nil; snapshotLocation = nil
        snapshotAt = .distantPast; snapshotAttemptAt = .distantPast
        baseVersion = 0; sentBaseVersion = -1; sentLocation = nil; sentInstruction = ""
        lastRender = .distantPast
        snapshotEpoch = -1; sentCameraEpoch = -1
    }
    private func resetCamera() {
        cameraDistance = 800; browseCenter = nil; browseHeading = 0
        cameraEpoch &+= 1; frameContexts = []
    }
    private func handleMapControl(_ control: HUDMapControl) {
        if control.command == .stop {
            stop(); HUDDiagnostics.record("HUD map: trip stopped from HUD"); return
        }
        guard navigating, let frame = frameContexts.last(where: { $0.id == control.frameID }) else { return }
        switch control.command {
        case .zoomIn: cameraDistance = max(180, cameraDistance / 1.5)
        case .zoomOut: cameraDistance = min(5000, cameraDistance * 1.5)
        case .follow: browseCenter = nil
        case .browse:
            browseCenter = frame.center; browseHeading = frame.heading
        case .pan:
            let origin = MKMapPoint(frame.center)
            let point = frame.snapshot.point(for: frame.center)
            let x = frame.snapshot.point(for: MKMapPoint(x: origin.x + 1000, y: origin.y).coordinate)
            let y = frame.snapshot.point(for: MKMapPoint(x: origin.x, y: origin.y + 1000).coordinate)
            guard let offset = HUDMapControl.mapOffset(dx: Double(control.dx), dy: Double(control.dy),
                axisX: (x.x - point.x, x.y - point.y), axisY: (y.x - point.x, y.y - point.y)) else { return }
            let world = MKMapSize.world.width
            let shiftedX = origin.x + offset.x * 1000
            browseCenter = MKMapPoint(x: (shiftedX.truncatingRemainder(dividingBy: world) + world)
                .truncatingRemainder(dividingBy: world), y: max(0, min(world, origin.y + offset.y * 1000))).coordinate
            browseHeading = frame.heading
        case .stop: return
        }
        cameraEpoch &+= 1
        snapshotter?.cancel(); snapshotter = nil; rendering = false; snapshotAttemptAt = .distantPast
        print("HUD map: control=\(control.command.rawValue)")
        renderIfNeeded()
    }
    private func requestSnapshot(location: CLLocation, heading: Double, light: Bool) {
        rendering = true; snapshotAttemptAt = Date()
        let token = revision
        let epoch = cameraEpoch
        let center = browseCenter ?? location.coordinate
        let distance = cameraDistance
        let options = MKMapSnapshotter.Options()
        options.size = CGSize(width: HUDWire.imageWidth, height: HUDWire.imageHeight); options.scale = 1
        options.traitCollection = UITraitCollection(userInterfaceStyle: light ? .light : .dark)
        options.mapType = .standard
        options.showsBuildings = false
        options.pointOfInterestFilter = .excludingAll
        options.camera = MKMapCamera(lookingAtCenter: center, fromDistance: distance, pitch: 0, heading: heading)
        let operation = MKMapSnapshotter(options: options)
        snapshotter = operation
        operation.start { [weak self, weak operation] snapshot, error in
            Task { @MainActor in
                guard let self, let operation, self.snapshotter === operation, token == self.revision, epoch == self.cameraEpoch else { return }
                self.rendering = false; self.snapshotter = nil
                guard let snapshot else {
                    if self.cachedSnapshot == nil { self.status = error?.localizedDescription ?? "Chưa tải được nền bản đồ" }
                    return
                }
                self.cachedSnapshot = snapshot; self.snapshotLocation = location
                self.snapshotHeading = heading; self.snapshotLight = light; self.snapshotAt = Date()
                self.snapshotEpoch = epoch; self.snapshotCenter = center
                let path = CGMutablePath()
                if let route = self.route, !self.externalGuidance {
                    for i in 0..<route.polyline.pointCount {
                        let point = snapshot.point(for: route.polyline.points()[i].coordinate)
                        if i == 0 { path.move(to: point) } else { path.addLine(to: point) }
                    }
                }
                self.cachedRoutePath = path; self.baseVersion &+= 1
                print("HUD map: baseMs=\(Int(Date().timeIntervalSince(self.snapshotAttemptAt) * 1000))")
                self.renderIfNeeded()
            }
        }
    }
    nonisolated private static func renderFrame(_ job: MapRenderJob) -> (image: UIImage, jpeg: Data)? {
        let image = compose(job)
        var jpeg = image.jpegData(compressionQuality: 0.55)
        if let size = jpeg?.count, size > 12 * 1024 { jpeg = image.jpegData(compressionQuality: 0.42) }
        guard let jpeg, jpeg.count <= job.maxBytes else { return nil }
        return (image, jpeg)
    }
    nonisolated private static func compose(_ job: MapRenderJob) -> UIImage {
        let snapshot = job.snapshot, location = job.location, marker = job.marker
        let instruction = job.instruction, externalGuidance = job.externalGuidance
        let instructionText = instruction
        let format = UIGraphicsImageRendererFormat(); format.scale = 1; format.opaque = true
        let light = job.light
        let mapInk: UIColor = light ? .systemBlue : .white
        let textInk: UIColor = light ? .black : .white
        return UIGraphicsImageRenderer(size: CGSize(width: HUDWire.imageWidth, height: HUDWire.imageHeight), format: format).image { renderer in
            snapshot.image.draw(at: .zero)
            let context = renderer.cgContext
            context.saveGState(); context.clip(to: CGRect(x: 0, y: 0, width: 300, height: 180))
            context.setStrokeColor(mapInk.cgColor); context.setLineWidth(5); context.setLineCap(.round); context.setLineJoin(.round)
            if let path = job.routePath { context.addPath(path) }
            context.strokePath()
            let point = snapshot.point(for: location.coordinate)
            context.translateBy(x: point.x, y: point.y)
            context.setFillColor(mapInk.cgColor); context.setStrokeColor((light ? UIColor.white : UIColor.black).cgColor); context.setLineWidth(2)
            if marker == .arrow {
                context.rotate(by: (location.course - job.heading) * .pi / 180)
                context.move(to: CGPoint(x: 0, y: -11))
                context.addLine(to: CGPoint(x: -8, y: 9))
                context.addLine(to: CGPoint(x: 0, y: 5))
                context.addLine(to: CGPoint(x: 8, y: 9)); context.closePath(); context.drawPath(using: .fillStroke)
            } else {
                context.addEllipse(in: CGRect(x: -7, y: -7, width: 14, height: 14)); context.drawPath(using: .fillStroke)
            }
            context.restoreGState()
            if externalGuidance { return }
            let remaining = job.remaining
            let distance = remaining >= 1000 ? String(format: "%.1f km", remaining / 1000) : "\(Int(remaining.rounded())) m"
            let paragraph = NSMutableParagraphStyle()
            paragraph.lineBreakMode = .byWordWrapping
            paragraph.lineSpacing = 1
            var font = UIFont.systemFont(ofSize: 14, weight: .semibold)
            var textRect = CGRect(x: 134, y: 10, width: 153, height: 0)
            func textHeight() -> CGFloat {
                ceil((instructionText as NSString).boundingRect(with: CGSize(width: textRect.width, height: .greatestFiniteMagnitude),
                    options: [.usesLineFragmentOrigin, .usesFontLeading],
                    attributes: [.font: font, .paragraphStyle: paragraph], context: nil).height)
            }
            // Short turns wrap alongside the distance; longer instructions get
            // the full width below it rather than losing the destination road.
            if textHeight() > 52 { textRect = CGRect(x: 12, y: 39, width: 276, height: 0) }
            while textHeight() > 132 - textRect.minY && font.pointSize > 10 {
                font = UIFont.systemFont(ofSize: font.pointSize - 1, weight: .semibold)
            }
            textRect.size.height = min(textHeight(), 132 - textRect.minY)
            let bannerHeight = max(44, textRect.maxY + 3)
            // Keep controls and MapKit attribution below the banner visible.
            (light ? UIColor.white : UIColor.black).withAlphaComponent(0.9).setFill()
            UIBezierPath(roundedRect: CGRect(x: 5, y: 5, width: 290, height: bannerHeight), cornerRadius: 7).fill()
            let words = instruction.lowercased()
            let symbol = words.contains("quay đầu") || words.contains("u-turn") ? "arrow.uturn.down" :
                words.contains("trái") || words.contains("left") ? "arrow.turn.up.left" :
                words.contains("phải") || words.contains("right") ? "arrow.turn.up.right" : "arrow.up"
            UIImage(systemName: symbol)?.withTintColor(textInk, renderingMode: .alwaysOriginal)
                .draw(in: CGRect(x: 12, y: 14, width: 22, height: 24))
            (distance as NSString).draw(in: CGRect(x: 42, y: 9, width: 88, height: 30), withAttributes: [
                .font: UIFont.boldSystemFont(ofSize: 22), .foregroundColor: textInk])
            (instructionText as NSString).draw(with: textRect, options: [.usesLineFragmentOrigin, .usesFontLeading],
                attributes: [.font: font, .foregroundColor: textInk, .paragraphStyle: paragraph], context: nil)
        }
    }
}

private final class MapLinkRedirectGuard: NSObject, URLSessionTaskDelegate {
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
        completionHandler(request.url.map { MapShare.allowed($0) } == true ? request : nil)
    }
}

/// Everything the off-main renderer needs, captured on the main actor.
/// MKMapSnapshotter.Snapshot is immutable once delivered, so sharing it is safe.
private struct MapRenderJob: @unchecked Sendable {
    let snapshot: MKMapSnapshotter.Snapshot
    let location: CLLocation
    let marker: HUDLocationMarker
    let light: Bool
    let heading: Double
    let center: CLLocationCoordinate2D
    let epoch: Int
    let baseVersion: Int
    let routePath: CGPath?
    let instruction: String
    let remaining: Double
    let externalGuidance: Bool
    let revision: Int
    let maxBytes: Int
}
