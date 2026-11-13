import Foundation
import MapKit
import CoreLocation
import UIKit
import Combine
import UserNotifications

@MainActor final class NavigationModel: NSObject, ObservableObject, @preconcurrency CLLocationManagerDelegate {
    @Published var results: [MKMapItem] = []
    @Published var status = "Cho phép vị trí để tìm đường"
    @Published var navigating = false
    @Published private(set) var simulating = false
    @Published private(set) var simulationFinished = false
    @Published private(set) var simulationCoordinate: CLLocationCoordinate2D?
    @Published var simulationRate = 1
    @Published private(set) var followingTurn: HUDFollowingTurn?
    @Published var preview: UIImage?
    @Published var instruction = "Chưa bắt đầu dẫn đường"
    @Published var transport = "Ô tô"
    @Published var gpsStatus = "Đang chờ vị trí iPhone"
    @Published var backgroundGPS=UserDefaults.standard.object(forKey:"hudBackgroundGPS") as? Bool ?? true
    private var hudLocationActivity: CLBackgroundActivitySession?
    private var hudGPSWaitTimer: Timer?
    /// iOS location permission is "Always": GPS may be started from the background, e.g. when
    /// a route is started by voice on the HUD while the iPhone is locked.
    @Published var locationAlways = false

    /// Keep a Core Location background activity while the connected HUD needs to start a
    /// search or route with the iPhone locked.  iOS requires that this session is created
    /// while the app is foregrounded; it then keeps GPS/MapKit available after lock.
    func updateHUDLocationActivity() {
        let authorization=locations.authorizationStatus
        let authorized=authorization == .authorizedAlways || authorization == .authorizedWhenInUse
        locationAlways=authorization == .authorizedAlways
        if backgroundGPS && bluetooth.ready && authorized {
            if hudLocationActivity == nil && UIApplication.shared.applicationState == .active {
                hudLocationActivity=CLBackgroundActivitySession()
                HUDDiagnostics.record("HUD location activity: started in foreground")
            }
        } else if hudLocationActivity != nil {
            hudLocationActivity?.invalidate(); hudLocationActivity=nil
            HUDDiagnostics.record("HUD location activity: stopped")
        }
        locations.desiredAccuracy=kCLLocationAccuracyBestForNavigation
        locations.distanceFilter=kCLDistanceFilterNone
        let needsBackgroundLocation=navigating || preparing || waitingToStart || hudSearchSession != nil || hudLocationActivity != nil
        locations.allowsBackgroundLocationUpdates=needsBackgroundLocation
        locations.showsBackgroundLocationIndicator=needsBackgroundLocation
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
    /// Short rain outlook for the next 3 hours, e.g. "Mưa 70% lúc 17h", empty when unknown.
    /// Where the HUD link last dropped on its own: usually where the bike is parked.
    @Published var parking: ParkingSpot? = ParkingSpot.load()
    @Published var forgetAlert = UserDefaults.standard.object(forKey: "hudForgetAlert") as? Bool ?? true {
        didSet {
            UserDefaults.standard.set(forgetAlert, forKey: "hudForgetAlert")
            if forgetAlert { Self.requestAlertPermission() }
        }
    }
    @Published var rainForecast = ""
    @Published var rainLikely = false
    private var weatherPayload: Data?
    private var sunriseMinute = 6 * 60, sunsetMinute = 18 * 60   // updated from the weather feed

    /// Light map only for a light HUD theme on a colour background during the day; a photo
    /// background (dark UI on the HUD) or night gives a dark map so it matches the screen.
    private func mapUsesLightStyle() -> Bool {
        guard bluetooth.settings.theme >= 4, bluetooth.wallpaperBackground != true else { return false }
        let now = Calendar.current.dateComponents([.hour, .minute], from: Date())
        let minute = (now.hour ?? 12) * 60 + (now.minute ?? 0)
        return minute >= sunriseMinute && minute < sunsetMinute
    }
    private var weatherFetchedAt = Date.distantPast
    private var weatherLocation: CLLocation?
    private var weatherLoading = false
    private var weatherSentAt = Date.distantPast
    private var tripSentAt = Date.distantPast
    private var tripSentKey = ""
    // Distance ridden today (GPS), shown on the HUD faces.
    @Published var todayMetres = UserDefaults.standard.double(forKey: "hudTodayMetres")
    private var todayKey = UserDefaults.standard.integer(forKey: "hudTodayKey")
    private var distanceFix: CLLocation?
    private var todaySent = -1.0
    private var todaySentAt = Date.distantPast
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
    /// Vietnamese results often have an empty or name-only `placemark.title`,
    /// so build "số nhà đường, phường, quận, tỉnh" from the components.
    static func address(of item: MKMapItem) -> String {
        // iOS 26 moved addresses to `address`/`addressRepresentations`; the deprecated
        // placemark comes back without street components there.
        if #available(iOS 26.0, *) {
            let full = item.addressRepresentations?.fullAddress(includingRegion: false, singleLine: true)
                ?? item.address?.shortAddress ?? item.address?.fullAddress
            if var full, !full.isEmpty {
                if let name = item.name, full.hasPrefix(name + ", ") { full.removeFirst(name.count + 2) }
                if full != item.name { return full }
            }
        }
        let p = item.placemark
        let street = [p.subThoroughfare, p.thoroughfare].compactMap { $0 }.joined(separator: " ")
        var parts: [String] = []
        for part in [street, p.subLocality ?? "", p.locality ?? "", p.subAdministrativeArea ?? "", p.administrativeArea ?? ""]
            where !part.isEmpty && !parts.contains(part) && part != item.name { parts.append(part) }
        if parts.isEmpty, let title = p.title, title != item.name { return title }
        return parts.joined(separator: ", ")
    }
    /// "1,2 km" from the latest fix, or "" without GPS.
    /// Places saved by older builds have no address; look them up once from the coordinate.
    func fillRecentAddresses() async {
        guard #available(iOS 26.0, *) else { return }
        var changed = false
        for (index, place) in recentPlaces.enumerated() where place.address.isEmpty || place.address == place.name {
            let location = CLLocation(latitude: place.latitude, longitude: place.longitude)
            guard let request = MKReverseGeocodingRequest(location: location),
                  let item = try? await request.mapItems.first else { continue }
            let address = Self.address(of: item)
            guard !address.isEmpty, index < recentPlaces.count, recentPlaces[index].id == place.id else { continue }
            recentPlaces[index] = SavedPlace(name: place.name, address: address, latitude: place.latitude, longitude: place.longitude)
            changed = true
        }
        if changed, let data = try? JSONEncoder().encode(recentPlaces) { UserDefaults.standard.set(data, forKey: "hudRecentPlaces") }
    }
    func distanceText(to item: MKMapItem) -> String {
        guard let latest, let target = item.placemark.location else { return "" }
        let metres = latest.distance(from: target)
        return metres < 1000 ? "\(Int(metres.rounded())) m" : String(format: "%.1f km", metres / 1000).replacingOccurrences(of: ".", with: ",")
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
    private var simulationFix: CLLocation?
    private var simulationTickAt: TimeInterval = 0
    private var simulationSpeedSentAt: TimeInterval = 0
    private static let simulationSpeed = 10.0  // 36 km/h at 1×
    var canSimulate: Bool { (geometry?.cumulative.last ?? 0) > 0 }
    private var destination: MKMapItem?
    private var route: MKRoute?
    private var geometry: RouteProgress?
    private var steps: [HUDRouteStep] = []
    /// Maneuver code of the upcoming step (see `Maneuver`), for the app banner.
    @Published var currentManeuver = 0
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
    private var tileTask: Task<Void, Never>?
    private var mapRequest: UUID?
    private var cachedBase: HUDMapBase?
    private var lastAppleBase: HUDMapBase?
    private var cachedRoutePath: CGPath?
    private var snapshotLocation: CLLocation?
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
    private var sentCameraEpoch = -1
    private var markerState = HUDLocationMarkerState()
    private var renderedMarker: HUDLocationMarker = .dot
    private var sentMarker: HUDLocationMarker = .dot
    private var latestSpeed: Double?
    private struct FrameContext {
        let id: UInt32
        let base: HUDMapBase
        let center: CLLocationCoordinate2D
        let heading: Double
        let scale: Double
        let hasMotion: Bool
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
    private var mapSnapshotBackgroundTask: UIBackgroundTaskIdentifier = .invalid
    private var hudPreparationTimer: Timer?
    private func endHUDPreparationLocation() {
        hudPreparationTimer?.invalidate(); hudPreparationTimer=nil
        hudGPSWaitTimer?.invalidate(); hudGPSWaitTimer=nil
        updateHUDLocationActivity()   // single place that decides GPS on/off
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
    private func endMapSnapshotBackground() {
        guard mapSnapshotBackgroundTask != .invalid else { return }
        let task = mapSnapshotBackgroundTask
        mapSnapshotBackgroundTask = .invalid
        UIApplication.shared.endBackgroundTask(task)
    }

    private func cancelMapRequest() {
        mapRequest = nil
        snapshotter?.cancel()
        snapshotter = nil
        tileTask?.cancel()
        tileTask = nil
        rendering = false
        endMapSnapshotBackground()
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
            "results":hudSearchResults.enumerated().map { ["number":$0.offset+1,"name":$0.element.name ?? "Điểm đến","address":String(Self.address(of: $0.element).prefix(180))] as [String:Any] }]
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
                else {
                    UNUserNotificationCenter.current().removePendingNotificationRequests(withIdentifiers: [Self.forgotID])
                    self.clearParking() // back in range of the HUD: you are at the bike
                    self.bluetooth.sendSpeed(self.simulating ? self.simulationFix.map { $0.speed * 3.6 } : self.latestSpeed)
                    self.weatherSentAt = .distantPast; self.tripSentAt = .distantPast; self.todaySentAt = .distantPast; self.todaySent = -1
                }
                self.updateHUDLocationActivity()
            }
        }
        if let data = UserDefaults.standard.data(forKey: "hudRecentPlaces"),
           let saved = try? JSONDecoder().decode([SavedPlace].self, from: data) { recentPlaces = Array(saved.prefix(5)) }
        Task { await fillRecentAddresses() }
        historyObserver=bluetooth.$voiceReady.removeDuplicates().sink { [weak self] ready in
            if ready { Task { @MainActor [weak self] in self?.publishHUDHistory() } }
        }
        bluetooth.onLinkLost = { [weak self] in Task { @MainActor in self?.hudLinkLost() } }
        if forgetAlert { Self.requestAlertPermission() }
        locations.delegate = self
        locations.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        locations.distanceFilter = kCLDistanceFilterNone
        locations.activityType = .automotiveNavigation
        locations.pausesLocationUpdatesAutomatically = false
        // A background relaunch (Bluetooth restore or significant location change) must
        // bring the location session back without waiting for the app to be opened.
        DispatchQueue.main.async { [weak self] in self?.updateHUDLocationActivity() }
        timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                self.advanceSimulation()
                self.sendRideInfo()
                self.renderIfNeeded()
            }
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
        // Route planning needs a precise fix even while the HUD is away (coarse keep-alive mode).
        locations.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        locations.distanceFilter = kCLDistanceFilterNone
        locations.startUpdatingLocation()
    }
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        print("HUD GPS: authorization=\(manager.authorizationStatus.rawValue)")
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            locationDenied = false
            manager.startUpdatingLocation()
            updateHUDLocationActivity()
            // Ask once to upgrade to "Always" so GPS and voice keep working after iOS closes
            // the app; iOS shows its own prompt at a moment it chooses.
            if manager.authorizationStatus == .authorizedWhenInUse
                && !UserDefaults.standard.bool(forKey:"hudAskedAlways") {
                UserDefaults.standard.set(true,forKey:"hudAskedAlways")
                manager.requestAlwaysAuthorization()
            }
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
        if navigating && UIApplication.shared.applicationState != .active && Date().timeIntervalSince(lastBackgroundFixLog) > 30 {
            lastBackgroundFixLog = Date()
            HUDDiagnostics.record("HUD GPS background fix: acc=\(Int(fix.horizontalAccuracy)); age=\(Int(-fix.timestamp.timeIntervalSinceNow))")
        }
        latestSpeed = HUDWire.speedKmh(metresPerSecond: fix.speed, accuracy: fix.horizontalAccuracy,
            speedAccuracy: fix.speedAccuracy, age: fix.timestamp.timeIntervalSinceNow)
        if !simulating { bluetooth.sendSpeed(latestSpeed) }
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
        warmMapSnapshotIfNeeded(from: fix)
        refreshWeatherIfNeeded(fix)
        addTodayDistance(fix)
        if waitingToStart {
            hudGPSWaitTimer?.invalidate(); hudGPSWaitTimer=nil
            waitingToStart = false; plan(from: fix); return
        }
        if !navigating || simulating { return }
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
                let items = response?.mapItems ?? []
                if items.isEmpty, request.region.span.latitudeDelta > 0 && request.region.span.latitudeDelta < 1 {
                    // Nothing nearby: search the whole country before giving up.
                    let wide = MKLocalSearch.Request(); wide.naturalLanguageQuery = query
                    wide.region = MKCoordinateRegion(center: request.region.center, latitudinalMeters: 1_500_000, longitudinalMeters: 1_500_000)
                    let retry = MKLocalSearch(request: wide); self.search = retry
                    retry.start { [weak self, weak retry] response, error in
                        Task { @MainActor in
                            guard let self, self.search === retry else { return }
                            self.searching = false
                            self.results = response?.mapItems ?? []
                            self.status = self.results.isEmpty ? "Không tìm thấy địa điểm" : "Chọn điểm đến"
                        }
                    }
                    return
                }
                self.searching = false
                self.results = items
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
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.count >= 2 else {
            sendHUDSearch(session, phase: 3, index: 0, total: 0, text: "Không nghe rõ địa điểm. Hãy nói lại."); return
        }
        guard beginHUDBackground() else {
            sendHUDSearch(session,phase:3,index:0,total:0,text:"Mở HUD Connect rồi thử tìm lại."); return
        }
        sendHUDSearch(session, phase: 0, index: 0, total: 0, text: "Đang tìm \"\(trimmed)\"…")
        searching=true
        hudSearchTask?.cancel()
        hudSearchTask = Task { @MainActor [weak self] in
            guard let self else { return }
            // Same engine as the app's search box: Apple Maps suggestions first, then
            // a plain-language search nearby and nationwide.
            let (items, source) = await self.placeSearch(trimmed)
            guard !Task.isCancelled, self.hudSearchSession == session else { return }
            do {
                defer { self.finishHUDBackgroundIfIdle() }
                self.searching=false; self.hudSearchResults=Array(items.prefix(5)); self.results=self.hudSearchResults
                HUDDiagnostics.record("HUD destination search: query=\(trimmed); results=\(self.hudSearchResults.count); source=\(source); appState=\(UIApplication.shared.applicationState.rawValue)")
                guard !self.hudSearchResults.isEmpty else {
                    self.sendHUDSearch(session, phase: 3, index: 0, total: 0, text: "Không tìm thấy \"\(trimmed)\". Hãy nói tên khác."); return
                }
                for (index,item) in self.hudSearchResults.enumerated() {
                    let name=item.name ?? "Điểm đến"
                    let address=Self.address(of: item)
                    let distance: Int?
                    if let latest=self.latest, let location=item.placemark.location { distance=Int(location.distance(from:latest).rounded()) }
                    else { distance=nil }
                    let prefix=distance.map { $0>=1000 ? String(format:"%.1f km · ",Double($0)/1000) : "\($0) m · " } ?? ""
                    self.sendHUDSearch(session, phase: 1, index: index, total: self.hudSearchResults.count, text: "\(name)\n\(prefix)\(address)")
                }
            }
        }
    }
    private var hudSearchTask: Task<Void, Never>?
    private var lastBackgroundFixLog = Date.distantPast
    private var loggedFirstFrame = false
    private var lastViewHeading = 0.0

    /// Places for a spoken/typed name, the way the app's search box finds them:
    /// 1. MKLocalSearchCompleter suggestions (what Apple Maps autocompletes), each resolved
    ///    to a map item; 2. plain-language search within 20 km; 3. the same nationwide.
    func placeSearch(_ query: String) async -> (items: [MKMapItem], source: String) {
        let near = latest?.coordinate
        let suggestions = await OneShotCompleter().results(for: query, near: near)
        var items: [MKMapItem] = []
        for completion in suggestions.prefix(6) where !completion.title.isEmpty {
            if Task.isCancelled { break }
            if let item = try? await MKLocalSearch(request: MKLocalSearch.Request(completion: completion)).start().mapItems.first {
                let duplicate = items.contains { existing in
                    existing.name == item.name &&
                    (existing.placemark.location?.distance(from: item.placemark.location ?? CLLocation()) ?? 1e9) < 50
                }
                if !duplicate { items.append(item) }
            }
        }
        if !items.isEmpty { return (items, "suggestions") }
        for metres in [20_000.0, 1_500_000.0] {
            let request = MKLocalSearch.Request(); request.naturalLanguageQuery = query
            if let near { request.region = MKCoordinateRegion(center: near, latitudinalMeters: metres, longitudinalMeters: metres) }
            if let found = try? await MKLocalSearch(request: request).start().mapItems, !found.isEmpty {
                return (found, metres < 100_000 ? "nearby" : "nationwide")
            }
        }
        return ([], "none")
    }

    func handleHUDSearchControl(session: UInt32, action: UInt8, index: UInt8) {
        HUDDiagnostics.record("HUD search control: action=\(action); sessionMatch=\(hudSearchSession == session); previewReady=\(hudPreviewReady); route=\(route != nil); preparing=\(preparing); appState=\(UIApplication.shared.applicationState.rawValue)")
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
        if simulating { clearSimulation(); bluetooth.sendInfo(Data([2])); tripSentKey = "" }
        if navigating { navigating=false; bluetooth.stopNavigation() }
        resetCamera()
        externalGuidance = false
        search?.cancel(); search = nil; searching = false
        directions?.cancel(); resetMapRenderer(); revision += 1
        planning = false; rendering = false; preparing = true; guidanceRequested = false
        plannedRoute = nil; route = nil; geometry = nil
        destination = item
        destinationName = item.name ?? "Điểm đến"
        destinationAddress = Self.address(of: item)
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
        // A route's start point need not be live: plan at once from the last good fix (up to
        // 30 min old, ~100 m) and let live GPS take over once guidance runs. This matters when
        // the HUD starts a route while the iPhone is locked and GPS cannot start right away.
        let candidates: [CLLocation] = [latest, locations.location].compactMap { $0 }
        let usable: [CLLocation] = candidates.filter { fix in
            let age: Double = abs(fix.timestamp.timeIntervalSinceNow)
            return fix.horizontalAccuracy >= 0 && fix.horizontalAccuracy <= 100 && age < 1800
        }
        let known: CLLocation? = usable.max { $0.timestamp < $1.timestamp }
        if let known, abs(known.timestamp.timeIntervalSinceNow) >= 30 {
            HUDDiagnostics.record("HUD destination prepare: using last fix, age=\(Int(abs(known.timestamp.timeIntervalSinceNow)))s")
            waitingToStart = false
            requestLocation()          // refresh in the background if iOS allows it
            plan(from: known); return
        }
        let backgroundNoGPS = UIApplication.shared.applicationState != .active
            && locations.authorizationStatus != .authorizedAlways
        let latestFresh: Bool = latest.map { abs($0.timestamp.timeIntervalSinceNow) < 30 } ?? false
        if !latestFresh, backgroundNoGPS, let session=hudSearchSession {
            // iOS will not start GPS from the background with "When In Use": say so now instead of waiting.
            preparing=false; hudPreviewReady=false
            status=Self.alwaysHint
            endHUDPreparationLocation()
            sendHUDSearch(session,phase:3,index:0,total:0,text:status)
            finishHUDBackgroundIfIdle()
            return
        }
        guard let latest, abs(latest.timestamp.timeIntervalSinceNow) < 30 else {
            waitingToStart = true
            status = "Đang lấy vị trí mới; có GPS sẽ tự tính tuyến"
            if let session=hudSearchSession {
                hudGPSWaitTimer=Timer.scheduledTimer(withTimeInterval:12,repeats:false) { [weak self] _ in
                    Task { @MainActor in
                        guard let self,self.waitingToStart,self.hudSearchSession==session else { return }
                        self.waitingToStart=false; self.preparing=false; self.hudPreviewReady=false
                        self.status="Chưa lấy được GPS. Mở HUD Connect trên iPhone rồi thử lại."
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
    static let alwaysHint = "Chưa lấy được vị trí khi iPhone khóa. Mở HUD Connect, hoặc vào Cài đặt › HUD Connect › Vị trí › Luôn luôn."

    func beginGuidance() {
        guard route != nil, !preparing, !navigating, bluetooth.ready else { return }
        if UIApplication.shared.applicationState != .active && locations.authorizationStatus != .authorizedAlways {
            // Guidance still starts, but live GPS needs the app opened once or "Always".
            HUDDiagnostics.record("HUD guidance: started in background without Always permission")
            if let session=hudSearchSession { sendHUDSearch(session,phase:6,index:0,total:0,text:"GPS sẽ chạy khi mở HUD Connect. Bật vị trí “Luôn luôn” để không cần mở.") }
        }
        guidanceRequested = true; navigating = true; loggedFirstFrame = false
        hudPreparationTimer?.invalidate(); hudPreparationTimer=nil
        updateHUDLocationActivity()   // navigating: background GPS on and running, even if started from the HUD
        HUDDiagnostics.record("HUD guidance: started; appState=\(UIApplication.shared.applicationState.rawValue); allowsBackground=\(locations.allowsBackgroundLocationUpdates); auth=\(locations.authorizationStatus.rawValue)")
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

    func beginSimulation() {
        guard canSimulate, route != nil, !preparing, !planning, !navigating, bluetooth.ready else { return }
        hudPreparationTimer?.invalidate(); hudPreparationTimer = nil
        hudSearchSession = nil; hudPreviewReady = false; hudChoiceContext = UUID().uuidString
        resetCamera()
        progress = 0; segment = 0
        simulationTickAt = ProcessInfo.processInfo.systemUptime - 0.25
        simulationSpeedSentAt = 0
        simulating = true; navigating = true; loggedFirstFrame = false
        status = "Đang chạy thử"
        updateHUDLocationActivity()
        advanceSimulation(starting: true)
        publishHUDHistory()
        tripSentKey = ""
        sendRideInfo()
        lastRender = .distantPast
        renderIfNeeded()
        HUDDiagnostics.record("HUD route simulation: started")
    }

    private func advanceSimulation(starting: Bool = false) {
        guard simulating, let geometry, let total = geometry.cumulative.last else { return }
        let uptime = ProcessInfo.processInfo.systemUptime
        let elapsed = uptime - simulationTickAt
        guard starting || elapsed >= 0.08 else { return }
        simulationTickAt = uptime
        if simulationFinished { sendSimulationSpeed(0, at: uptime); return }
        // Bound the step after suspension so a paused iPhone cannot skip through the route.
        let travelled = starting ? 0 : min(elapsed, 1) * Self.simulationSpeed * Double(simulationRate)
        let next = min(total, progress + travelled)
        guard let sample = geometry.sample(at: next) else { return }
        let coordinate = CLLocationCoordinate2D(latitude: sample.point.latitude, longitude: sample.point.longitude)
        let finished = next >= total
        let speed = finished ? 0 : Self.simulationSpeed
        simulationFix = CLLocation(coordinate: coordinate, altitude: 0, horizontalAccuracy: 1, verticalAccuracy: -1,
            course: sample.course, courseAccuracy: 1, speed: speed, speedAccuracy: 0, timestamp: Date())
        simulationCoordinate = coordinate
        progress = next; segment = sample.segment
        updateGuidanceInstruction()
        sendSimulationSpeed(speed, at: uptime, immediate: finished)
        if finished {
            simulationFinished = true
            status = "Đã chạy thử xong"
            instruction = "Đã đến nơi"
        }
    }

    private func sendSimulationSpeed(_ metresPerSecond: Double, at uptime: TimeInterval, immediate: Bool = false) {
        if (immediate || uptime - simulationSpeedSentAt >= 0.5), bluetooth.sendSpeed(metresPerSecond * 3.6) {
            simulationSpeedSentAt = uptime
        }
    }

    private func clearSimulation() {
        guard simulating else { return }
        simulating = false; simulationFinished = false
        simulationFix = nil; simulationCoordinate = nil
        bluetooth.sendSpeed(latestSpeed)
        HUDDiagnostics.record("HUD route simulation: stopped")
    }
    private var transportFallback = false
    func changeTransport() {
        // Ignore the change made by the walking->car fallback itself.
        if transportFallback { transportFallback = false; return }
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
                if response?.routes.first == nil, request.transportType == .walking {
                    // Apple Maps has no walking directions in many places: fall back to the car route.
                    HUDDiagnostics.record("HUD route: walking unavailable, retrying by car")
                    self.transportFallback = true; self.transport = "Ô tô"
                    self.plan(from: location)
                    self.status = "Không có tuyến đi bộ ở đây · dùng tuyến ô tô"
                    return
                }
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
                self.rebuildCachedRoutePath()
                self.steps = []
                var lastSegment = 0
                // An MKRoute step's instruction happens at the END of its polyline ("in 200 m,
                // turn left"). The turn angle there comes from the geometry: heading into the
                // end of this step versus heading out along the next one.
                let usable = route.steps.filter { $0.distance > 1 && !$0.instructions.isEmpty && $0.polyline.pointCount > 0 }
                var logged = 0
                for (index, step) in usable.enumerated() {
                    let points = (0..<step.polyline.pointCount).map { step.polyline.points()[$0].coordinate }
                    let coordinate = points[points.count - 1]
                    var turn: Double?
                    if index + 1 < usable.count {
                        let next = usable[index + 1].polyline
                        let after = (0..<next.pointCount).map { next.points()[$0].coordinate }
                        turn = Maneuver.turnAngle(into: points, outOf: after)
                    }
                    let code = Maneuver.classify(step.instructions, turn: turn, last: index + 1 == usable.count)
                    if logged < 12 {   // real Apple Maps wording, to refine the keywords later
                        HUDDiagnostics.record("HUD step: maneuver=\(code); turn=\(turn.map { Int($0) } ?? 999); text=\(step.instructions)")
                        logged += 1
                    }
                    if let projected = geometry.project(GeoPoint(coordinate.latitude, coordinate.longitude), startingAt: lastSegment) {
                        self.steps.append(HUDRouteStep(end: projected.metres, text: step.instructions, maneuver: UInt8(code)))
                        lastSegment = projected.segment
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
                    if let session=self.hudSearchSession { self.sendHUDSearch(session, phase: self.hudSelectionFromAI ? 5 : 2, index: 0, total: 0, text: "\(self.destinationName)\n\(self.tripSummary)\(self.rainLikely ? "\n\(self.rainForecast) · mang áo mưa" : "")") }
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
        updateGuidanceInstruction()
        if let destination,
           location.distance(from: CLLocation(latitude: destination.placemark.coordinate.latitude,
                                              longitude: destination.placemark.coordinate.longitude)) < 20,
           (geometry.cumulative.last ?? 0) - progress < 40 {
            status = "Đã đến điểm đến"; instruction = "Đã đến nơi"
        }
    }

    private func updateGuidanceInstruction() {
        let upcoming = HUDRouteStep.upcoming(in: steps, progress: progress)
        let text = upcoming.first?.text ?? "Đến gần điểm đến"
        let maneuver = upcoming.first.map { Int($0.maneuver) } ?? Maneuver.arrive
        if instruction != text { instruction = text }
        if currentManeuver != maneuver { currentManeuver = maneuver }
        if followingTurn != upcoming.following { followingTurn = upcoming.following }
    }
    // MARK: Anti-theft: parking spot and "forgot the HUD" alert

    nonisolated static let forgotID = "hud-forgot"

    nonisolated static func requestAlertPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    /// The HUD dropped out of range (or was switched off). Remember where, and if it does
    /// not come back within 40 s, ask whether it was left on the bike.
    private func hudLinkLost() {
        let fix = latest.flatMap { abs($0.timestamp.timeIntervalSinceNow) < 300 ? $0 : nil }
        if let coordinate = fix?.coordinate ?? currentCoordinate {
            let spot = ParkingSpot(latitude: coordinate.latitude, longitude: coordinate.longitude, savedAt: Date())
            parking = spot; spot.save()
        }
        HUDDiagnostics.record("HUD link lost: parking saved=\(parking != nil)")
        guard forgetAlert else { return }
        let content = UNMutableNotificationContent()
        content.title = "Bạn quên tháo HUD trên xe?"
        content.body = parking != nil ? "Mất kết nối với HUD. Đã lưu chỗ đỗ xe trong HUD Connect." : "Mất kết nối với HUD."
        content.sound = .default
        let request = UNNotificationRequest(identifier: Self.forgotID, content: content,
            trigger: UNTimeIntervalNotificationTrigger(timeInterval: 40, repeats: false))
        UNUserNotificationCenter.current().add(request)
    }

    func clearParking() { parking = nil; ParkingSpot.clear() }

    /// Walking route back to the bike (falls back to car where Apple has no walking data).
    /// Returns false when you are already at the spot.
    func routeToParking() -> Bool {
        guard let parking else { return false }
        if let here = currentCoordinate,
           CLLocation(latitude: here.latitude, longitude: here.longitude)
            .distance(from: CLLocation(latitude: parking.latitude, longitude: parking.longitude)) < 30 {
            importStatus = "Bạn đang ở ngay chỗ đỗ xe"
            Task { try? await Task.sleep(for: .seconds(3)); if importStatus == "Bạn đang ở ngay chỗ đỗ xe" { importStatus = "" } }
            return false
        }
        transport = "Đi bộ" // the transport picker is not on screen here, so no extra re-plan fires
        prepare(to: parking.mapItem)
        return true
    }

    // MARK: Today's distance

    private static func dayKey(_ date: Date = Date()) -> Int {
        let c = Calendar.current.dateComponents([.year, .month, .day], from: date)
        return (c.year ?? 0) * 10000 + (c.month ?? 0) * 100 + (c.day ?? 0)
    }

    /// Adds movement between good fixes; ignores GPS jitter while standing still and gaps/jumps.
    private func addTodayDistance(_ fix: CLLocation) {
        let key = Self.dayKey(fix.timestamp)
        if key != todayKey { todayKey = key; todayMetres = 0; distanceFix = nil; saveToday() }
        guard fix.horizontalAccuracy <= 25 else { return }
        guard let last = distanceFix else { distanceFix = fix; return }
        let gap = fix.timestamp.timeIntervalSince(last.timestamp)
        let step = fix.distance(from: last)
        guard gap > 0, gap < 120, step < 2000 else { distanceFix = fix; return }  // gap or jump: restart
        let moving = fix.speed >= 0 ? fix.speed >= 1.4 : step > 15   // ~5 km/h
        guard moving, step >= 3 else { return }  // standing still: keep the anchor, ignore jitter
        distanceFix = fix
        todayMetres += step
        if Int(todayMetres) / 100 != Int(todayMetres - step) / 100 { saveToday() }
    }

    private func saveToday() {
        UserDefaults.standard.set(todayMetres, forKey: "hudTodayMetres")
        UserDefaults.standard.set(todayKey, forKey: "hudTodayKey")
    }

    private func sendToday() {
        let key = Self.dayKey()
        let metres = todayKey == key ? todayMetres : 0
        guard abs(metres - todaySent) >= 50 || Date().timeIntervalSince(todaySentAt) > 60 else { return }
        var data = Data([4])
        for v in [UInt32(max(0, min(Double(UInt32.max), metres.rounded()))), UInt32(key)] {
            data.append(contentsOf: [UInt8(v & 255), UInt8(v >> 8 & 255), UInt8(v >> 16 & 255), UInt8(v >> 24)])
        }
        if bluetooth.sendInfo(data) { todaySent = metres; todaySentAt = Date() }
    }

    // MARK: Ride info and weather for the HUD

    /// Turn, distance left and ETA, sent every 2 s (or on change) while guiding on an Apple Maps route.
    private func sendRideInfo() {
        if Date().timeIntervalSince(weatherSentAt) > 60, let weatherPayload, bluetooth.sendInfo(weatherPayload) { weatherSentAt = Date() }
        sendToday()
        guard navigating, !externalGuidance, let route, let geometry else { return }
        let total = geometry.cumulative.last ?? route.distance
        let left = max(0, total - progress)
        let upcoming = HUDRouteStep.upcoming(in: steps, progress: progress)
        let next = upcoming.first
        let toTurn = max(0, (next?.end ?? total) - progress)
        let seconds = simulating ? left / (Self.simulationSpeed * Double(simulationRate))
            : route.distance > 0 ? route.expectedTravelTime * left / route.distance : 0
        let eta = Date().addingTimeInterval(seconds)
        let parts = Calendar.current.dateComponents([.hour, .minute], from: eta)
        let etaMinute = UInt16((parts.hour ?? 0) * 60 + (parts.minute ?? 0))
        let text = next?.text ?? "Đến điểm đến"
        let maneuver = next?.maneuver ?? UInt8(Maneuver.arrive)
        let following = upcoming.following
        let key = "\(Int(toTurn / 10))|\(Int(left / 50))|\(etaMinute)|\(maneuver)|\(text)|\(following?.maneuver ?? 255)|\(following?.metresAfter ?? 0)|\(simulating)"
        guard key != tripSentKey || Date().timeIntervalSince(tripSentAt) > 2 else { return }
        func metres(_ value: Double) -> UInt32 { UInt32(max(0, min(Double(UInt32.max), value.rounded()))) }
        guard let data = HUDWire.trip(remaining: metres(left), toTurn: metres(toTurn), etaMinute: etaMinute,
            maneuver: maneuver, instruction: text, following: following, simulating: simulating,
            maxWrite: bluetooth.infoPayloadLimit) else { return }
        if bluetooth.sendInfo(data) { tripSentAt = Date(); tripSentKey = key }
    }

    /// Text-only classification (no geometry), kept for callers without a route.
    nonisolated static func maneuver(_ instruction: String) -> Int { Maneuver.classify(instruction, turn: nil, last: false) }

    private func refreshWeatherIfNeeded(_ fix: CLLocation) {
        let moved = weatherLocation.map { fix.distance(from: $0) > 5000 } ?? true
        guard !weatherLoading, moved || Date().timeIntervalSince(weatherFetchedAt) > 20 * 60 else { return }
        weatherLoading = true
        Task { await loadWeather(fix) }
    }

    /// Open-Meteo (free, no key): rain chance for the next 3 hours plus sunrise/sunset for HUD auto-brightness.
    private func loadWeather(_ fix: CLLocation) async {
        defer { weatherLoading = false }
        let c = fix.coordinate
        let url = URL(string: String(format: "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f&current=temperature_2m,precipitation&hourly=precipitation_probability,precipitation&daily=sunrise,sunset&timezone=auto&forecast_days=2", c.latitude, c.longitude))!
        struct Reply: Decodable {
            struct Current: Decodable { let temperature_2m: Double; let precipitation: Double? }
            struct Hourly: Decodable { let time: [String]; let precipitation_probability: [Int?]; let precipitation: [Double?] }
            struct Daily: Decodable { let sunrise: [String]; let sunset: [String] }
            let utc_offset_seconds: Int; let current: Current; let hourly: Hourly; let daily: Daily
        }
        guard let (data, response) = try? await URLSession.shared.data(from: url),
              (response as? HTTPURLResponse)?.statusCode == 200,
              let reply = try? JSONDecoder().decode(Reply.self, from: data) else {
            HUDDiagnostics.record("HUD weather: fetch failed"); return
        }
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX"); formatter.dateFormat = "yyyy-MM-dd'T'HH:mm"
        formatter.timeZone = TimeZone(secondsFromGMT: reply.utc_offset_seconds)
        let now = Date()
        var chance = 0, rainIn: Int? = nil
        for (i, stamp) in reply.hourly.time.enumerated() {
            guard let start = formatter.date(from: stamp) else { continue }
            let end = start.addingTimeInterval(3600)
            if end <= now || start > now.addingTimeInterval(3 * 3600) { continue }
            let p = i < reply.hourly.precipitation_probability.count ? reply.hourly.precipitation_probability[i] ?? 0 : 0
            let mm = i < reply.hourly.precipitation.count ? reply.hourly.precipitation[i] ?? 0 : 0
            chance = max(chance, p)
            if rainIn == nil && (p >= 40 || mm >= 0.3) { rainIn = max(0, Int(start.timeIntervalSince(now) / 60)) }
        }
        if (reply.current.precipitation ?? 0) >= 0.1 { rainIn = 0; chance = max(chance, 60) }
        func minuteOfDay(_ s: String) -> UInt16 {
            let hm = s.suffix(5).split(separator: ":").compactMap { Int($0) }
            return hm.count == 2 ? UInt16(hm[0] * 60 + hm[1]) : 0xFFFF
        }
        let rise = reply.daily.sunrise.first.map(minuteOfDay) ?? 0xFFFF
        let set = reply.daily.sunset.first.map(minuteOfDay) ?? 0xFFFF
        if rise < 1440 && set < 1440 && rise < set { sunriseMinute = Int(rise); sunsetMinute = Int(set) }
        let temp = Int16(max(-999, min(999, (reply.current.temperature_2m * 10).rounded())))
        let wait = UInt16(rainIn.map { min($0, 0xFFFE) } ?? 0xFFFF)
        var payload = Data([3, UInt8(min(100, chance))])
        for v in [wait, UInt16(bitPattern: temp), rise, set, 0] { payload.append(contentsOf: [UInt8(v & 255), UInt8(v >> 8)]) }
        weatherPayload = payload; weatherFetchedAt = now; weatherLocation = fix; weatherSentAt = .distantPast
        rainLikely = rainIn != nil && chance >= 40
        let degrees = Int((reply.current.temperature_2m).rounded())
        if !rainLikely { rainForecast = "\(degrees)° · Không mưa trong 3 giờ tới" }
        else if let rainIn, rainIn < 30 { rainForecast = "Đang/sắp mưa (\(chance)%)" }
        else if let rainIn {
            let hour = Calendar.current.component(.hour, from: now.addingTimeInterval(Double(rainIn) * 60))
            rainForecast = "Mưa \(chance)% lúc \(hour)h"
        }
        HUDDiagnostics.record("HUD weather: chance=\(chance) rainIn=\(rainIn ?? -1) sunrise=\(rise) sunset=\(set)")
        if bluetooth.sendInfo(payload) { weatherSentAt = Date() }
    }

    func stop() {
        clearSimulation()
        endHUDBackground()
        endHUDPreparationLocation()
        hudChoiceContext=UUID().uuidString; hudPreviewReady=false; hudSearchSession=nil; hudSearchResults=[]
        resetCamera()
        externalGuidance = false
        waitingToStart = false
        guidanceRequested = false; preparing = false; plannedRoute = nil
        revision += 1; directions?.cancel(); resetMapRenderer()
        planning = false; rendering = false; navigating = false
        route = nil; geometry = nil; steps = []; preview = nil; followingTurn = nil
        // Keep the optional background location session ready for a later HUD search.
        status = "Đã dừng dẫn đường"; instruction = "Chưa bắt đầu dẫn đường"
        bluetooth.stopNavigation()
        bluetooth.sendInfo(Data([2])); tripSentKey = ""
        updateHUDLocationActivity()
        publishHUDHistory()
    }
    func mapApplicationStateChanged() {
        cancelMapRequest()
        snapshotAttemptAt = .distantPast
        if navigating { renderIfNeeded() }
        else if let latest { warmMapSnapshotIfNeeded(from: latest) }
    }

    private func renderIfNeeded() {
        guard navigating, bluetooth.ready, !bluetooth.voiceActive, !bluetooth.speakerActive, !planning,
              externalGuidance || route != nil else { return }
        let now = Date()
        let fix = simulating ? simulationFix : latest.flatMap { abs($0.timestamp.timeIntervalSince(now)) < 10 ? $0 : nil }
        let predicted = simulating ? fix : fix.map { predictedLocation($0, now: now) }
        // Browsing a fixed location does not depend on GPS continuing to report.
        guard let center = browseCenter ?? predicted?.coordinate else { return }
        let light = mapUsesLightStyle()
        if let fix {
            renderedMarker = markerState.update(speed: fix.speed, horizontalAccuracy: fix.horizontalAccuracy,
                course: fix.course, courseAccuracy: fix.courseAccuracy,
                age: fix.timestamp.timeIntervalSince(now), now: now)
        }
        let heading = browseCenter != nil ? browseHeading :
            fix.map { $0.course >= 0 && $0.speed > 1.5 ? $0.course : lastViewHeading } ?? lastViewHeading
        if browseCenter == nil { lastViewHeading = heading }

        let appState = UIApplication.shared.applicationState
        let inBackground = appState == .background
        // Restore the Apple image when browsing back into its coverage. Keep the last one
        // even after a background tile request replaces the current base.
        let maxAppleZoom = 2.25
        if inBackground, cachedBase?.isRaster == true, let apple = lastAppleBase {
            let zoom = apple.distance / cameraDistance
            if zoom <= maxAppleZoom,
               Self.roomInBase(apple, center: center, heading: heading, scale: zoom) >= 12 {
                cancelMapRequest()
                cachedBase = apple
                rebuildCachedRoutePath()
                forceFrame = true
                HUDDiagnostics.record("HUD map: restored Apple base in background")
            }
        }

        // Keep composing from the current base while a replacement loads.
        let base = cachedBase
        let scale = base.map { $0.distance / cameraDistance } ?? 1
        let room = base.map { Self.roomInBase($0, center: center, heading: heading, scale: scale) } ?? -.infinity
        let moving = browseCenter == nil && (fix?.speed ?? 0) >= 1
        let needsBase: Bool
        if inBackground, let base, !base.isRaster {
            // Start refreshing before the moving rider reaches the edge. Panning and modest
            // zoom can reuse the existing Apple projection until the requested view leaves it.
            let verticalMargin = (base.image.size.height - Double(HUDWire.imageHeight) / scale) / 2
            let refreshMargin = min(80, max(0, verticalMargin * 0.7))
            needsBase = room < (moving ? refreshMargin : 0) || scale > maxAppleZoom
        } else {
            needsBase = base == nil || base?.isRaster != inBackground || base?.light != light ||
                abs(scale - 1) > 0.001 || room < 40
        }
        if rendering && now.timeIntervalSince(snapshotAttemptAt) > 18 {
            cancelMapRequest()
            snapshotAttemptAt = now
        }
        if needsBase, !rendering, appState != .inactive, now.timeIntervalSince(snapshotAttemptAt) >= 2 {
            requestMapBase(center: center, heading: heading, light: light, inBackground: inBackground)
        }

        let changed = sentBaseVersion != baseVersion || sentInstruction != instruction || sentMarker != renderedMarker
        let wantFrame = forceFrame || sentCameraEpoch != cameraEpoch || (moving
            ? now.timeIntervalSince(lastRender) >= 0.1
            : HUDMapCadence.sendFrame(age: now.timeIntervalSince(lastRender),
                movement: fix.flatMap { current in sentLocation.map { current.distance(from: $0) } } ?? 0,
                changed: changed, speed: 0))
        guard let base, room >= 0, !bluetooth.busy, !encoding, wantFrame else { return }
        var motion: MapFrameMotion?
        let availableMargin = min(Double(HUDWire.maxMotionMargin), room * scale)
        if moving, !externalGuidance, !base.isRaster, let fix, fix.speed >= 2, fix.course >= 0,
           fix.courseAccuracy >= 0, fix.courseAccuracy <= 25, fix.horizontalAccuracy <= 25,
           abs(fix.timestamp.timeIntervalSince(now)) < 2, availableMargin >= Double(HUDWire.minMotionMargin) {
            let speed = fix.speed * (simulating ? Double(simulationRate) : 1)
            let ahead = Self.ahead(of: center, heading: heading, metres: speed)
            let a = base.point(for: center), b = base.point(for: ahead)
            let pixels = hypot(b.x - a.x, b.y - a.y) * scale
            if pixels.isFinite && pixels >= 1 {
                // Budget for the in-flight JPEG and the wait for the following one.
                let wantedMargin = max(24, ceil(pixels * 0.9 / 8) * 8)
                var previousFrame: UInt32 = 0
                var advance: Int16 = 0
                if let previous = frameContexts.last, previous.hasMotion, sentBaseVersion == baseVersion,
                   sentCameraEpoch == cameraEpoch, abs(previous.scale - scale) < 0.001 {
                    let turn = (heading - previous.heading + 540).truncatingRemainder(dividingBy: 360) - 180
                    let point = base.point(for: previous.center)
                    let angle = (heading - base.heading) * .pi / 180
                    let dy = ((point.y - a.y) * cos(angle) - (point.x - a.x) * sin(angle)) * scale
                    if abs(turn) <= 5, abs(dy) <= Double(HUDWire.maxMotionMargin) * 2 {
                        previousFrame = previous.id
                        advance = Int16(dy.rounded())
                    }
                }
                motion = MapFrameMotion(speed: UInt16(min(2000, (pixels * 10).rounded())),
                    marker: renderedMarker, light: base.light, margin: UInt8(min(availableMargin, wantedMargin)),
                    previousFrame: previousFrame, advance: advance)
            }
        }
        let job = MapRenderJob(base: base, location: predicted, marker: renderedMarker, viewHeading: heading,
            scale: scale, center: center, motion: motion, epoch: cameraEpoch, baseVersion: baseVersion, routePath: cachedRoutePath,
            instruction: instruction, externalGuidance: externalGuidance, revision: revision)
        encoding = true
        lastRender = now
        Task.detached(priority: .userInitiated) { [weak self] in
            let result = Self.renderFrame(job)
            await self?.finishRender(job, result: result)
        }
    }

    nonisolated private static func roomInBase(_ base: HUDMapBase, center: CLLocationCoordinate2D,
                                              heading: Double, scale: Double) -> Double {
        let point = base.point(for: center)
        let radians = (heading - base.heading) * .pi / 180
        let cosine = abs(cos(radians)), sine = abs(sin(radians))
        let width = Double(HUDWire.imageWidth) / (2 * scale)
        let height = Double(HUDWire.imageHeight) / (2 * scale)
        let roomX = min(point.x, base.image.size.width - point.x) - (width * cosine + height * sine)
        let roomY = min(point.y, base.image.size.height - point.y) - (width * sine + height * cosine)
        return min(roomX, roomY)
    }

    private func finishRender(_ job: MapRenderJob, result: (image: UIImage, jpeg: Data)?) {
        encoding = false
        guard let result, navigating, job.revision == revision, job.epoch == cameraEpoch,
              job.baseVersion == baseVersion else { return }
        // A JPEG can fill the BLE write queue. Give pending turn information a slot first.
        sendRideInfo()
        guard let id = bluetooth.send(jpeg: result.jpeg, externalGuidance: job.externalGuidance, motion: job.motion) else { return }
        preview = result.image
        forceFrame = false
        sentLocation = job.location
        sentInstruction = job.instruction
        sentBaseVersion = job.baseVersion
        sentCameraEpoch = job.epoch
        sentMarker = job.marker
        if !loggedFirstFrame {
            loggedFirstFrame = true
            HUDDiagnostics.record("HUD map: first frame; raster=\(job.base.isRaster); appState=\(UIApplication.shared.applicationState.rawValue)")
        }
        frameContexts.append(FrameContext(id: id, base: job.base, center: job.center, heading: job.viewHeading,
            scale: job.scale, hasMotion: job.motion != nil))
        if frameContexts.count > 24 { frameContexts.removeFirst(frameContexts.count - 24) }
    }

    private func resetMapRenderer() {
        cancelMapRequest()
        cachedRoutePath = nil
        snapshotAttemptAt = .distantPast
        baseVersion &+= 1
        sentBaseVersion = -1
        sentLocation = nil
        sentInstruction = ""
        lastRender = .distantPast
        sentCameraEpoch = -1
    }

    private func warmMapSnapshotIfNeeded(from location: CLLocation) {
        guard UIApplication.shared.applicationState == .active, bluetooth.ready,
              !navigating, !preparing, !rendering, Date().timeIntervalSince(snapshotAttemptAt) >= 3 else { return }
        let light = mapUsesLightStyle()
        let stale = Date().timeIntervalSince(snapshotAt) > 180
        let moved = snapshotLocation.map { location.distance(from: $0) > 150 } ?? true
        guard cachedBase == nil || cachedBase?.isRaster == true || cachedBase?.light != light || stale || moved else { return }
        requestMapBase(center: location.coordinate, heading: 0, light: light, inBackground: false)
    }
    private func rebuildCachedRoutePath() {
        guard let base = cachedBase else { return }
        let path = CGMutablePath()
        if let route, !externalGuidance {
            for index in 0..<route.polyline.pointCount {
                let point = base.point(for: route.polyline.points()[index].coordinate)
                if index == 0 { path.move(to: point) } else { path.addLine(to: point) }
            }
        }
        cachedRoutePath = path
        baseVersion &+= 1
    }
    private func resetCamera() {
        cameraDistance = 800; browseCenter = nil; browseHeading = 0
        cameraEpoch &+= 1; frameContexts = []
    }
    private func handleMapControl(_ control: HUDMapControl) {
        if control.command == .stop {
            stop()
            HUDDiagnostics.record("HUD map: trip stopped from HUD")
            return
        }
        guard navigating else { return }
        let frame = frameContexts.last { $0.id == control.frameID }
        switch control.command {
        case .zoomIn: cameraDistance = max(180, cameraDistance / 1.5)
        case .zoomOut: cameraDistance = min(5000, cameraDistance * 1.5)
        case .follow: browseCenter = nil
        case .transform:
            cameraDistance = min(5000, max(180, cameraDistance / (Double(control.dx) / 100)))
            if abs(control.dy) >= 4, let frame {
                if browseCenter == nil { browseCenter = frame.center; browseHeading = frame.heading }
                browseHeading = (browseHeading - Double(control.dy) + 360).truncatingRemainder(dividingBy: 360)
            }
        case .browse:
            guard let frame else { return }
            browseCenter = frame.center
            browseHeading = frame.heading
        case .pan:
            guard let frame else { return }
            let origin = MKMapPoint(frame.center)
            let point = frame.base.point(for: frame.center)
            let x = frame.base.point(for: MKMapPoint(x: origin.x + 1000, y: origin.y).coordinate)
            let y = frame.base.point(for: MKMapPoint(x: origin.x, y: origin.y + 1000).coordinate)
            // Invert the same rotation and zoom used to draw the frame.
            let radians = (frame.heading - frame.base.heading) * .pi / 180
            let dx = (Double(control.dx) * cos(radians) - Double(control.dy) * sin(radians)) / frame.scale
            let dy = (Double(control.dx) * sin(radians) + Double(control.dy) * cos(radians)) / frame.scale
            guard let offset = HUDMapControl.mapOffset(dx: dx, dy: dy,
                axisX: (x.x - point.x, x.y - point.y), axisY: (y.x - point.x, y.y - point.y)) else { return }
            let world = MKMapSize.world.width
            let shiftedX = origin.x + offset.x * 1000
            browseCenter = MKMapPoint(x: (shiftedX.truncatingRemainder(dividingBy: world) + world)
                .truncatingRemainder(dividingBy: world), y: max(0, min(world, origin.y + offset.y * 1000))).coordinate
            browseHeading = frame.heading
        case .stop: return
        }
        cameraEpoch &+= 1
        forceFrame = true
        cancelMapRequest()
        snapshotAttemptAt = .distantPast
        HUDDiagnostics.record("HUD map: control=\(control.command.rawValue); appState=\(UIApplication.shared.applicationState.rawValue)")
        renderIfNeeded()
    }

    private func requestMapBase(center: CLLocationCoordinate2D, heading: Double, light: Bool, inBackground: Bool) {
        let request = UUID()
        mapRequest = request
        rendering = true
        snapshotAttemptAt = Date()
        let distance = cameraDistance
        if inBackground {
            mapSnapshotBackgroundTask = UIApplication.shared.beginBackgroundTask(withName: "HUD map refresh") { [weak self] in
                guard let self, self.mapRequest == request else { return }
                self.cancelMapRequest()
                self.snapshotAttemptAt = Date()
            }
            // MapKit can deliver while backgrounded on the tested device. Give it time to
            // produce an Apple image, then let the street tiles cover a slow or failed request.
            tileTask = Task { [weak self] in
                do { try await Task.sleep(for: .seconds(8)) }
                catch { return }
                self?.startStreetTileFallback(request: request, center: center, distance: distance, light: light)
            }
            HUDDiagnostics.record("HUD map: requested Apple base in background")
        }
        let options = MKMapSnapshotter.Options()
        // Use the compact capture from the original app for background requests.
        options.size = inBackground ? CGSize(width: 300, height: 200)
            : CGSize(width: HUDWire.imageWidth + 448, height: HUDWire.imageHeight + 448)
        options.scale = 1
        options.traitCollection = UITraitCollection(userInterfaceStyle: light ? .light : .dark)
        options.mapType = .standard
        options.showsBuildings = false
        options.pointOfInterestFilter = .excludingAll
        let captureDistance = inBackground ? distance : distance * options.size.height / Double(HUDWire.imageHeight + 240)
        // Enlarge the compact image before cropping so it still has room for panning.
        let baseDistance = inBackground ? distance * 1.6 : distance
        options.camera = MKMapCamera(lookingAtCenter: center, fromDistance: captureDistance, pitch: 0, heading: heading)
        let operation = MKMapSnapshotter(options: options)
        snapshotter = operation
        operation.start { [weak self, weak operation] snapshot, error in
            guard let self, let operation, self.mapRequest == request, self.snapshotter === operation else { return }
            if let snapshot {
                self.tileTask?.cancel()
                if inBackground {
                    let elapsed = Int(Date().timeIntervalSince(self.snapshotAttemptAt) * 1000)
                    HUDDiagnostics.record("HUD map: Apple base completed; ms=\(elapsed); appState=\(UIApplication.shared.applicationState.rawValue)")
                }
                let base = HUDMapBase(image: snapshot.image, projection: .apple(snapshot),
                    distance: baseDistance, heading: heading, light: light)
                self.completeMapRequest(request, center: center, base: base, error: nil)
            } else if inBackground {
                self.startStreetTileFallback(request: request, center: center, distance: distance, light: light)
            } else {
                self.completeMapRequest(request, center: center, base: nil, error: error)
            }
        }
    }

    private func startStreetTileFallback(request: UUID, center: CLLocationCoordinate2D, distance: Double, light: Bool) {
        guard mapRequest == request else { return }
        let operation = snapshotter
        snapshotter = nil
        operation?.cancel()
        tileTask?.cancel()
        let size = CGSize(width: HUDWire.imageWidth + 240, height: HUDWire.imageHeight + 240)
        HUDDiagnostics.record("HUD map: Apple base unavailable, loading street tiles")
        tileTask = Task { [weak self] in
            do {
                let base = try await HUDBackgroundMap.shared.load(center: center, distance: distance, size: size, light: light)
                self?.completeMapRequest(request, center: center, base: base, error: nil)
            } catch {
                self?.completeMapRequest(request, center: center, base: nil, error: error)
            }
        }
    }

    private func completeMapRequest(_ request: UUID, center: CLLocationCoordinate2D,
                                    base: HUDMapBase?, error: Error?) {
        guard request == mapRequest else { return }
        mapRequest = nil
        snapshotter = nil
        tileTask = nil
        rendering = false
        snapshotAttemptAt = Date()
        defer { endMapSnapshotBackground() }
        guard let base else {
            let error = error as NSError?
            HUDDiagnostics.record("HUD map: tiles failed; domain=\(error?.domain ?? "none"); code=\(error?.code ?? -1); appState=\(UIApplication.shared.applicationState.rawValue)")
            return
        }
        cachedBase = base
        if !base.isRaster { lastAppleBase = base }
        snapshotLocation = CLLocation(latitude: center.latitude, longitude: center.longitude)
        snapshotAt = Date()
        rebuildCachedRoutePath()
        HUDDiagnostics.record("HUD map: street base ready; raster=\(base.isRaster); appState=\(UIApplication.shared.applicationState.rawValue)")
        renderIfNeeded()
    }
    /// Set by HUD map controls: send a frame on the next tick even when nothing moved.
    private var forceFrame = false

    /// iPhone GPS updates about once a second; between fixes, advance the bike along its
    /// course at its speed (up to 1.5 s) so every frame shows fresh motion.
    private func predictedLocation(_ fix: CLLocation, now: Date) -> CLLocation {
        guard browseCenter == nil, fix.speed >= 1, fix.course >= 0 else { return fix }
        let dt = min(1.5, max(0, now.timeIntervalSince(fix.timestamp)))
        let c = Self.ahead(of: fix.coordinate, heading: fix.course, metres: fix.speed * dt)
        return CLLocation(coordinate: c, altitude: fix.altitude, horizontalAccuracy: fix.horizontalAccuracy,
                          verticalAccuracy: fix.verticalAccuracy, course: fix.course, speed: fix.speed, timestamp: now)
    }

    nonisolated private static func ahead(of c: CLLocationCoordinate2D, heading: Double, metres: Double) -> CLLocationCoordinate2D {
        let r = heading * .pi / 180, earth = 6_371_000.0
        let dLat = metres * cos(r) / earth
        let dLon = metres * sin(r) / (earth * cos(c.latitude * .pi / 180))
        return CLLocationCoordinate2D(latitude: c.latitude + dLat * 180 / .pi, longitude: c.longitude + dLon * 180 / .pi)
    }
    nonisolated private static func renderFrame(_ job: MapRenderJob) -> (image: UIImage, jpeg: Data)? {
        let image = compose(job)
        // Smaller frames cross BLE faster: the link, not drawing, limits the frame rate.
        var jpeg = image.jpegData(compressionQuality: job.motion != nil ? 0.36 : 0.45)
        if let size = jpeg?.count, size > 14 * 1024 { jpeg = image.jpegData(compressionQuality: 0.35) }
        if let size = jpeg?.count, size > 20 * 1024 { jpeg = image.jpegData(compressionQuality: 0.28) }
        guard let jpeg, jpeg.count <= HUDWire.maxImage else { return nil }
        return (image, jpeg)
    }
    nonisolated private static func compose(_ job: MapRenderJob) -> UIImage {
        let base = job.base
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true
        let ink: UIColor = base.light ? .systemBlue : .white
        let arrowInk = base.light
            ? UIColor(red: 185 / 255.0, green: 77 / 255.0, blue: 0, alpha: 1)
            : UIColor(red: 1, green: 158 / 255.0, blue: 40 / 255.0, alpha: 1)
        let margin = Int(job.motion?.margin ?? 0)
        let size = CGSize(width: HUDWire.imageWidth, height: HUDWire.imageHeight + 2 * margin)
        return UIGraphicsImageRenderer(size: size, format: format).image { renderer in
            let context = renderer.cgContext
            let focus = base.point(for: job.center)
            context.saveGState()
            context.translateBy(x: size.width / 2, y: size.height / 2)
            context.rotate(by: -(job.viewHeading - base.heading) * .pi / 180)
            context.scaleBy(x: job.scale, y: job.scale)
            context.translateBy(x: -focus.x, y: -focus.y)
            base.image.draw(at: .zero)
            if let path = job.routePath {
                context.addPath(path)
                context.setStrokeColor(ink.cgColor)
                context.setLineWidth(5 / job.scale)
                context.setLineCap(.round)
                context.setLineJoin(.round)
                context.strokePath()
            }
            if job.motion == nil, let location = job.location {
                let point = base.point(for: location.coordinate)
                context.translateBy(x: point.x, y: point.y)
                context.scaleBy(x: 1 / job.scale, y: 1 / job.scale)
                context.setFillColor((job.marker == .arrow ? arrowInk : ink).cgColor)
                context.setStrokeColor((base.light ? UIColor.white : UIColor.black).cgColor)
                context.setLineWidth(2)
                if job.marker == .arrow {
                    context.rotate(by: (location.course - base.heading) * .pi / 180)
                    context.move(to: CGPoint(x: 0, y: -11))
                    context.addLine(to: CGPoint(x: -8, y: 9))
                    context.addLine(to: CGPoint(x: 0, y: 5))
                    context.addLine(to: CGPoint(x: 8, y: 9))
                    context.closePath()
                } else {
                    context.addEllipse(in: CGRect(x: -7, y: -7, width: 14, height: 14))
                }
                context.drawPath(using: .fillStroke)
            }
            context.restoreGState()
            if let credit = base.attribution {
                let attribution = credit as NSString
                let style: [NSAttributedString.Key: Any] = [
                    .font: UIFont.systemFont(ofSize: 11, weight: .semibold), .foregroundColor: UIColor.white
                ]
                let bounds = attribution.size(withAttributes: style)
                UIColor.black.withAlphaComponent(0.8).setFill()
                renderer.fill(CGRect(x: 4, y: 82, width: bounds.width + 8, height: bounds.height + 4))
                attribution.draw(at: CGPoint(x: 8, y: 84), withAttributes: style)
            }
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
private struct MapRenderJob: @unchecked Sendable {
    let base: HUDMapBase
    let location: CLLocation?
    let marker: HUDLocationMarker
    let viewHeading: Double
    let scale: Double
    let center: CLLocationCoordinate2D
    let motion: MapFrameMotion?
    let epoch: Int
    let baseVersion: Int
    let routePath: CGPath?
    let instruction: String
    let externalGuidance: Bool
    let revision: Int
}

struct MapFrameMotion {
    let speed: UInt16                 // tenths of a screen pixel per second
    let marker: HUDLocationMarker
    let light: Bool
    let margin: UInt8
    let previousFrame: UInt32
    let advance: Int16
}

struct ParkingSpot: Codable, Equatable {
    let latitude: Double, longitude: Double, savedAt: Date
    var coordinate: CLLocationCoordinate2D { CLLocationCoordinate2D(latitude: latitude, longitude: longitude) }
    var mapItem: MKMapItem {
        let item = MKMapItem(placemark: MKPlacemark(coordinate: coordinate)); item.name = "Chỗ đỗ xe"; return item
    }
    static func load() -> ParkingSpot? {
        UserDefaults.standard.data(forKey: "hudParking").flatMap { try? JSONDecoder().decode(ParkingSpot.self, from: $0) }
    }
    func save() { if let data = try? JSONEncoder().encode(self) { UserDefaults.standard.set(data, forKey: "hudParking") } }
    static func clear() { UserDefaults.standard.removeObject(forKey: "hudParking") }
}

/// Turn icons shared with the HUD (byte in the trip message). Mirrors the Apple Maps set.
enum Maneuver {
    static let straight = 0, left = 1, right = 2, slightLeft = 3, slightRight = 4, uTurn = 5, arrive = 6
    static let sharpLeft = 7, sharpRight = 8, keepLeft = 9, keepRight = 10, merge = 11
    static let rampLeft = 12, rampRight = 13, roundaboutStraight = 14, roundaboutLeft = 15, roundaboutRight = 16, ferry = 17

    static func bearing(_ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> Double {
        let p1 = a.latitude * .pi / 180, p2 = b.latitude * .pi / 180, dl = (b.longitude - a.longitude) * .pi / 180
        let y = sin(dl) * cos(p2), x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl)
        return (atan2(y, x) * 180 / .pi + 360).truncatingRemainder(dividingBy: 360)
    }
    /// Point about `metres` along a polyline from its start (or from its end when `backwards`).
    static func point(along points: [CLLocationCoordinate2D], metres: Double, backwards: Bool) -> CLLocationCoordinate2D? {
        let path = backwards ? Array(points.reversed()) : points
        guard path.count >= 2 else { return nil }
        var walked = 0.0
        for i in 1..<path.count {
            walked += CLLocation(latitude: path[i-1].latitude, longitude: path[i-1].longitude)
                .distance(from: CLLocation(latitude: path[i].latitude, longitude: path[i].longitude))
            if walked >= metres { return path[i] }
        }
        return path.last
    }
    /// Signed turn in degrees at the joint (positive = right), from ~25 m either side.
    static func turnAngle(into before: [CLLocationCoordinate2D], outOf after: [CLLocationCoordinate2D]) -> Double? {
        guard let joint = before.last, let back = point(along: before, metres: 25, backwards: true),
              let ahead = point(along: after, metres: 25, backwards: false) else { return nil }
        var turn = bearing(joint, ahead) - bearing(back, joint)
        while turn > 180 { turn -= 360 }
        while turn <= -180 { turn += 360 }
        return turn
    }
    static func classify(_ instruction: String, turn: Double?, last: Bool) -> Int {
        let t = instruction.lowercased()
        func has(_ words: String...) -> Bool { words.contains { t.contains($0) } }
        let textLeft = has("trái", "left"), textRight = has("phải", "right")
        let side: Int? = turn.map { $0 < 0 ? -1 : 1 } ?? (textLeft ? -1 : textRight ? 1 : nil)
        if last || has("điểm đến", "đích", "đã đến", "arrive", "destination") { return arrive }
        if has("phà", "ferry") { return ferry }
        if has("vòng xuyến", "bùng binh", "vòng xoay", "roundabout", "rotary") {
            if let turn, abs(turn) < 35 { return roundaboutStraight }
            if let s = side { return s < 0 ? roundaboutLeft : roundaboutRight }
            return roundaboutStraight
        }
        if has("quay đầu", "quay lại", "u-turn", "make a u") { return uTurn }
        if has("nhập vào", "nhập làn", "hợp nhất", "merge") { return merge }
        if has("lối ra", "lối vào", "đường dẫn", "đường nhánh", "nhánh", "exit", "ramp") {
            return (side ?? 1) < 0 ? rampLeft : rampRight
        }
        if has("giữ bên", "đi về bên", "giữ làn", "keep", "fork") {
            return textLeft ? keepLeft : textRight ? keepRight : (side ?? 1) < 0 ? keepLeft : keepRight
        }
        if let turn {
            let a = abs(turn), l = turn < 0
            if a < 20 { return straight }
            if a < 45 { return l ? slightLeft : slightRight }
            if a < 135 { return l ? left : right }
            if a < 165 { return l ? sharpLeft : sharpRight }
            return uTurn
        }
        // No geometry: fall back to the wording.
        let slight = has("chếch", "hơi", "slight"), sharp = has("gắt", "ngoặt", "sharp")
        if textLeft { return slight ? slightLeft : sharp ? sharpLeft : left }
        if textRight { return slight ? slightRight : sharp ? sharpRight : right }
        return straight
    }
}

/// MKLocalSearchCompleter as a single async call (first result batch, 4 s timeout).
@MainActor final class OneShotCompleter: NSObject, MKLocalSearchCompleterDelegate {
    private let completer = MKLocalSearchCompleter()
    private var continuation: CheckedContinuation<[MKLocalSearchCompletion], Never>?
    func results(for query: String, near: CLLocationCoordinate2D?) async -> [MKLocalSearchCompletion] {
        completer.delegate = self
        completer.resultTypes = [.address, .pointOfInterest]
        if let near { completer.region = MKCoordinateRegion(center: near, latitudinalMeters: 50_000, longitudinalMeters: 50_000) }
        return await withCheckedContinuation { continuation in
            self.continuation = continuation
            completer.queryFragment = query
            Task { @MainActor [weak self] in
                try? await Task.sleep(for: .seconds(4)); self?.finish([])
            }
        }
    }
    private func finish(_ results: [MKLocalSearchCompletion]) {
        continuation?.resume(returning: results); continuation = nil
        completer.cancel()
    }
    nonisolated func completerDidUpdateResults(_ completer: MKLocalSearchCompleter) {
        MainActor.assumeIsolated { self.finish(completer.results) }
    }
    nonisolated func completer(_ completer: MKLocalSearchCompleter, didFailWithError error: Error) {
        MainActor.assumeIsolated { self.finish([]) }
    }
}
