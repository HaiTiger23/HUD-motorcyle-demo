import Foundation
import CoreBluetooth
import Combine

// Small, device-local diagnostic trace for testing without a debugger keeping
// the app awake. Only lifecycle events and counters belong here, never content.
@MainActor enum HUDDiagnostics {
    static func record(_ message: String) {
        print(message)
        guard let cache=FileManager.default.urls(for:.cachesDirectory,in:.userDomainMask).first else { return }
        let url=cache.appendingPathComponent("HUDRuntime.log")
        var data=Data()
        if let previous=try? Data(contentsOf:url) { data.append(contentsOf:previous.suffix(24000)) }
        data.append(Data("\(Date().ISO8601Format()) \(message)\n".utf8))
        try? data.write(to:url,options:[.atomic,.completeFileProtectionUntilFirstUserAuthentication])
    }
}

struct HUDSettings: Equatable {
    var theme = 0
    var face = 0
    var timeout = 30
    var allowNewApps = true
}
struct HUDHardwareSettings: Equatable {
    var brightness=100
    var speakerVolume=30
    var rotation=0
}
struct HUDAppRule: Identifiable {
    var id: String
    var enabled: Bool
    var displayName = ""
    var name: String {
        if !displayName.isEmpty { return displayName }
        switch id {
        case "com.apple.MobileSMS": return "Tin nhắn"
        case "com.apple.mobilephone": return "Điện thoại"
        case "com.facebook.Messenger": return "Messenger"
        case "net.whatsapp.WhatsApp": return "WhatsApp"
        case "com.vng.zalo": return "Zalo"
        case "com.google.Maps": return "Google Maps"
        case "com.apple.Maps": return "Apple Maps"
        case "com.google.Gmail": return "Gmail"
        case "com.vietinbank.mobilebanking": return "VietinBank"
        case "com.beeasy.shopee.vn": return "Shopee"
        default: return id
        }
    }
}

// CoreBluetooth delegates use the explicitly selected main queue.
@MainActor final class BluetoothLink: NSObject, ObservableObject, @preconcurrency CBCentralManagerDelegate, @preconcurrency CBPeripheralDelegate {
    static let service = CBUUID(string: "ee43a100-1b32-4d0f-9ab8-2248b0a01000")
    static let rxID = CBUUID(string: "ee43a101-1b32-4d0f-9ab8-2248b0a01000")
    static let txID = CBUUID(string: "ee43a102-1b32-4d0f-9ab8-2248b0a01000")
    static let settingsID = CBUUID(string: "ee43a103-1b32-4d0f-9ab8-2248b0a01000")
    static let speedID = CBUUID(string: "ee43a104-1b32-4d0f-9ab8-2248b0a01000")
    static let voiceID = CBUUID(string: "ee43a105-1b32-4d0f-9ab8-2248b0a01000")
    @Published var voiceReady = false
    @Published var voiceActive = false
    @Published var speakerActive=false
    var onVoicePacket: ((Data) -> Void)?
    var onVoiceDisconnected: (() -> Void)?
    var onVoiceQueueDrained: (() -> Void)?
    var voicePending: Bool { voiceWriting || !voiceQueue.isEmpty }
    var destinationTextLimit: Int { min(180,max(0,(device?.maximumWriteValueLength(for:.withResponse) ?? 20)-8)) }
    private var voiceChannel: CBCharacteristic?
    private var voiceQueue: [Data] = []
    private var voiceWriting = false
    private var voiceTimer: Timer?
    private var speedChannel: CBCharacteristic?
    var onMapControl: ((HUDMapControl) -> Void)?
    @Published var settings = HUDSettings()
    @Published var hardware = HUDHardwareSettings()
    @Published var hardwareReady=false
    @Published var apps: [HUDAppRule] = []
    @Published var settingsReady = false
    @Published var settingsBusy = false
    @Published var settingsStatus = "Đang chờ HUD"
    private var settingsChannel: CBCharacteristic?
    private var settingsQueue: [Data] = []
    private var settingsRequest: Data?
    private var settingsSequence: UInt8 = 0
    private var settingsTimer: Timer?
    private var catalogRefreshNeeded=false
    @Published var status = "Chưa kết nối HUD"
    @Published var ready = false
    @Published var connecting = false
    @Published var busy = false
    private var central: CBCentralManager!
    private var device: CBPeripheral?
    private var rx: CBCharacteristic?
    private var packets: [Data] = []
    private var packetIndex = 0
    private var waitingWrite = false
    private var frameAcknowledged = false
    private var transferStarted = Date.distantPast
    private var transferBytes = 0
    private var frameID: UInt32 = 0
    private var timeout: Timer?
    private var connectionTimeout: Timer?
    private var ownsConnection = false
    private var reconnect = true
    private var scannedAfterTimeout = false
    private var retryTimer: Timer?
    private var retryDelay: TimeInterval = 2
    private var appActive = true
    private var resubscribing=Set<CBUUID>()
    override init() {
        super.init()
        let extensionProcess = Bundle.main.bundleURL.pathExtension == "appex"
        central = CBCentralManager(delegate: self, queue: .main,
            options: extensionProcess ? nil : [CBCentralManagerOptionRestoreIdentifierKey: "HUDConnect.central.v2"])
    }
    func connect() {
        guard central.state == .poweredOn else { status = "Bật Bluetooth trên iPhone"; return }
        if ready || connecting { return }
        scannedAfterTimeout = false
        reconnect = true
        retryTimer?.invalidate()
        // Settings/ANCS may already own the BLE connection, in which case the
        // board is no longer advertising and a fresh scan cannot find it.
        if let peripheral = systemConnectedHUD() {
            attach(peripheral); return
        }
        if let saved = UserDefaults.standard.string(forKey: "hudPeripheral"), let id = UUID(uuidString: saved),
           let peripheral = central.retrievePeripherals(withIdentifiers: [id]).first {
            attach(peripheral)
            // Also discover a replacement identifier instead of waiting only
            // on a cached peripheral that may no longer represent this board.
            if peripheral.state != .connected { central.scanForPeripherals(withServices: nil) }
            return
        }
        status = "Đang tìm HUD iPhone…"
        connecting = true
        watchConnection()
        // Preserve existing ANCS/AMS advertising. Initial discovery is foreground-only.
        central.scanForPeripherals(withServices: nil)
    }
    private func systemConnectedHUD() -> CBPeripheral? {
        let connected=central.retrieveConnectedPeripherals(withServices:[Self.service, CBUUID(string:"180A")])
        let saved=UserDefaults.standard.string(forKey:"hudPeripheral").flatMap(UUID.init(uuidString:))
        return connected.first(where:{ $0.identifier == saved }) ?? connected.first(where:{ $0.name == "HUD iPhone" })
    }
    func resume() {
        HUDDiagnostics.record("HUD BLE: foreground resume; ready=\(ready)")
        appActive=true
        guard central.state == .poweredOn else { return }
        if let device, ownsConnection, device.state == .connected {
            if ready {
                if !voiceReady && !voiceActive && !busy {
                    print("HUD BLE: foreground restoring channels")
                    device.discoverServices([Self.service])
                }
                return
            }
            if !connecting { attach(device) }
            else { watchConnection() }
            return
        }
        if !connecting { ready=false; connect() }
        else { watchConnection() }
    }
    func enterBackground() {
        HUDDiagnostics.record("HUD BLE: background; ready=\(ready)")
        appActive=false; retryTimer?.invalidate()
    }
    private func scheduleRetry() {
        guard reconnect, appActive, central.state == .poweredOn,
              Bundle.main.bundleURL.pathExtension != "appex" else { return }
        retryTimer?.invalidate()
        let delay=retryDelay; retryDelay=min(10,retryDelay+2)
        retryTimer=Timer.scheduledTimer(withTimeInterval:delay,repeats:false) { [weak self] _ in
            Task { @MainActor in self?.connect() }
        }
        print("HUD BLE: automatic retry in \(Int(delay))s")
    }
    private func attach(_ peripheral: CBPeripheral, restored: Bool = false) {
        let previous=device
        reconnect = true
        device = peripheral; peripheral.delegate = self
        if let previous, previous.identifier != peripheral.identifier {
            ready=false; ownsConnection=false; resetSettings(); rx=nil; clearTransfer()
            central.cancelPeripheralConnection(previous)
        }
        central.stopScan()
        connecting = true
        watchConnection()
        status = "Đang kết nối HUD…"
        if (restored || ownsConnection) && peripheral.state == .connected {
            ownsConnection = true
            peripheral.discoverServices([Self.service])
        }
        else { central.connect(peripheral) }
    }
    private func subscribe(_ characteristic: CBCharacteristic,on peripheral: CBPeripheral) {
        guard !resubscribing.contains(characteristic.uuid) else { return }
        // iOS may restore isNotifying=true after a firmware restart while the
        // board has lost its CCCD. Force a real disable/enable handshake.
        if characteristic.isNotifying {
            resubscribing.insert(characteristic.uuid)
            peripheral.setNotifyValue(false,for:characteristic)
        } else { peripheral.setNotifyValue(true,for:characteristic) }
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        print("HUD BLE: central state=\(central.state.rawValue)")
        if central.state == .poweredOn {
            if let device, ownsConnection, device.state == .connected { attach(device,restored:true) }
            else { connect() }
        }
        else {
            ready = false; connecting = false; ownsConnection = false
            retryTimer?.invalidate()
            resetSettings()
            connectionTimeout?.invalidate(); clearTransfer()
            status = central.state == .unauthorized ? "Chưa cho phép Bluetooth" : "Bluetooth chưa sẵn sàng"
        }
    }
    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let peripheral = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.first {
            device=peripheral; peripheral.delegate=self
            ownsConnection=peripheral.state == .connected
            // Restoration can precede poweredOn. Wait before issuing GATT requests.
            if central.state == .poweredOn { attach(peripheral,restored:true) }
        }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? ""
        if name == "HUD iPhone", !(connecting && device?.identifier == peripheral.identifier) { attach(peripheral) }
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard device?.identifier == peripheral.identifier else { return }
        HUDDiagnostics.record("HUD BLE: app connection established")
        central.stopScan()
        ownsConnection = true
        watchConnection()
        UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: "hudPeripheral")
        peripheral.discoverServices([Self.service])
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard device?.identifier == peripheral.identifier else { return }
        print("HUD BLE: connect failed code=\((error as NSError?)?.code ?? 0)")
        connecting = false; ownsConnection = false; connectionTimeout?.invalidate()
        status = "Không kết nối được HUD. Chạm Kết nối để thử lại."
        scheduleRetry()
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard device?.identifier == peripheral.identifier else { return }
        ready = false; connecting = false; ownsConnection = false
        resetSettings()
        rx = nil; clearTransfer(); connectionTimeout?.invalidate()
        status = "Mất kết nối HUD"
        if reconnect { scheduleRetry(); if !appActive { attach(peripheral) } }
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard device?.identifier == peripheral.identifier else { return }
        print("HUD BLE: services callback error=\((error as NSError?)?.code ?? 0)")
        guard error == nil, let service = peripheral.services?.first(where: { $0.uuid == Self.service }) else {
            connecting = false; connectionTimeout?.invalidate()
            status = error == nil ? "HUD cần firmware có nhận bản đồ" : "Chưa đọc được dịch vụ HUD"
            if error != nil { scheduleRetry() }
            return
        }
        print("HUD BLE: navigation service discovered")
        watchConnection()
        peripheral.discoverCharacteristics(nil, for: service)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard device?.identifier == peripheral.identifier else { return }
        print("HUD BLE: characteristics callback error=\((error as NSError?)?.code ?? 0)")
        print("HUD BLE: service=\(service.uuid.uuidString), channels=\(service.characteristics?.map { $0.uuid.uuidString } ?? [])")
        guard service.uuid == Self.service else { return }
        guard error == nil, let chars = service.characteristics else {
            connecting = false; connectionTimeout?.invalidate(); status = "Chưa đọc được kênh bản đồ"; scheduleRetry(); return
        }
        rx = chars.first { $0.uuid == Self.rxID }
        speedChannel = chars.first { $0.uuid == Self.speedID }
        voiceChannel = chars.first { $0.uuid == Self.voiceID }
        if Bundle.main.bundleURL.pathExtension == "appex" { voiceChannel = nil }
        if let voiceChannel {
            HUDDiagnostics.record("HUD speaker transport: fast=\(voiceChannel.properties.contains(.writeWithoutResponse))")
            voiceReady=false; subscribe(voiceChannel,on:peripheral)
        }
        settingsChannel = chars.first { $0.uuid == Self.settingsID }
        if Bundle.main.bundleURL.pathExtension == "appex" { settingsChannel = nil }
        if let channel = settingsChannel {
            subscribe(channel,on:peripheral)
        } else { settingsStatus = "Cần cập nhật firmware để cài đặt" }
        guard rx != nil, let tx = chars.first(where: { $0.uuid == Self.txID }) else {
            connecting = false; connectionTimeout?.invalidate(); status = "HUD thiếu kênh bản đồ"; return
        }
        print("HUD BLE: notification already active=\(tx.isNotifying)")
        watchConnection(); subscribe(tx,on:peripheral)
    }
    func peripheral(_ peripheral: CBPeripheral, didModifyServices invalidatedServices: [CBService]) {
        guard device?.identifier == peripheral.identifier else { return }
        guard invalidatedServices.contains(where: { $0.uuid == Self.service }) else { return }
        ready = false; rx = nil; connecting = true; watchConnection()
        resetSettings()
        peripheral.discoverServices([Self.service])
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        HUDDiagnostics.record("HUD notify: channel=\(characteristic.uuid); on=\(characteristic.isNotifying); error=\((error as NSError?)?.code ?? 0)")
        guard device?.identifier == peripheral.identifier else { return }
        if resubscribing.remove(characteristic.uuid) != nil && error == nil {
            peripheral.setNotifyValue(true,for:characteristic); return
        }
        if characteristic.uuid == Self.voiceID {
            voiceReady = error == nil && characteristic.isNotifying
            return
        }
        if characteristic.uuid == Self.settingsID {
            if error == nil && characteristic.isNotifying { refreshSettings() }
            else { settingsStatus = "Không mở được kênh cài đặt" }
            return
        }
        guard characteristic.uuid == Self.txID else { return }
        ready = error == nil && characteristic.isNotifying && rx != nil
        connecting = false; connectionTimeout?.invalidate()
        HUDDiagnostics.record("HUD BLE: navigation ready=\(ready)")
        status = ready ? "HUD đã sẵn sàng" : "Chưa đăng ký được nhận bản đồ"
        if ready { retryDelay=2; retryTimer?.invalidate() }
        else { scheduleRetry() }
    }
    private func markReady() {
        ready = true; connecting = false; connectionTimeout?.invalidate()
        status = "HUD đã sẵn sàng"
        retryDelay=2; retryTimer?.invalidate()
        HUDDiagnostics.record("HUD BLE: navigation ready=true (restored subscription)")
    }
    private func watchConnection() {
        connectionTimeout?.invalidate()
        connectionTimeout = Timer.scheduledTimer(withTimeInterval: 15, repeats: false) { [weak self] _ in
            Task { @MainActor in
                guard let self, !self.ready else { return }
                if !self.scannedAfterTimeout && self.central.state == .poweredOn {
                    self.scannedAfterTimeout = true
                    if let connected=self.systemConnectedHUD(), connected.identifier != self.device?.identifier {
                        self.attach(connected); return
                    }
                    if let device=self.device, self.ownsConnection, device.state == .connected {
                        device.discoverServices([Self.service])
                    } else {
                        self.status = "Đang tìm lại HUD iPhone…"; self.connecting = true
                        self.central.scanForPeripherals(withServices: nil)
                    }
                    self.watchConnection()
                    print("HUD BLE: recovering existing connection and scanning")
                    return
                }
                self.connecting = false; self.ownsConnection=false
                self.central.stopScan()
                self.status = "Đang thử kết nối lại HUD…"
                print("HUD BLE: connection phase timed out")
                let old=self.device; self.device=nil
                self.resetSettings(); self.rx=nil; self.clearTransfer()
                if let old { self.central.cancelPeripheralConnection(old) }
                self.scheduleRetry()
            }
        }
    }
    @discardableResult func send(jpeg: Data, externalGuidance: Bool = false) -> UInt32? {
        guard ready, !busy, let device, rx != nil else { return nil }
        frameID &+= 1
        guard let newPackets = HUDWire.packets(jpeg: jpeg, id: frameID,
            maxWrite: device.maximumWriteValueLength(for: .withoutResponse)) else { status = "Khung bản đồ quá lớn"; return nil }
        packets = [Data([5, externalGuidance ? 1 : 0])] + newPackets; packetIndex = 0; busy = true
        frameAcknowledged = false; transferStarted = Date(); transferBytes = jpeg.count
        timeout = Timer.scheduledTimer(withTimeInterval: 12, repeats: false) { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                self.clearTransfer(); self.ready = false
                self.status = "Gửi bản đồ hết thời gian"
                if let device = self.device { self.central.cancelPeripheralConnection(device) }
            }
        }
        pump()
        return frameID
    }
    func sendSpeed(_ kmh: Double?) {
        guard ready, let device, let speedChannel, device.canSendWriteWithoutResponse else { return }
        let value: UInt16 = kmh.map { UInt16(max(0, min(3000, ($0*10).rounded()))) } ?? 65535
        device.writeValue(Data([UInt8(value & 255), UInt8(value >> 8)]), for: speedChannel, type: .withoutResponse)
    }
    private func pump() {
        guard !waitingWrite, let device, let rx else { return }
        while packetIndex < packets.count {
            let packet = packets[packetIndex]
            if packet.first == 1 || packet.first == 3 || packet.first == 5 {
                waitingWrite = true; packetIndex += 1
                device.writeValue(packet, for: rx, type: .withResponse)
                return
            }
            guard device.canSendWriteWithoutResponse else { return }
            packetIndex += 1
            device.writeValue(packet, for: rx, type: .withoutResponse)
        }
    }
    func peripheralIsReady(toSendWriteWithoutResponse peripheral: CBPeripheral) { pumpVoice(); pump() }
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if characteristic.uuid == Self.voiceID {
            voiceTimer?.invalidate(); voiceWriting = false
            if error != nil { voiceQueue=[]; onVoiceDisconnected?() }
            else { pumpVoice() }
            return
        }
        if characteristic.uuid == Self.settingsID {
            HUDDiagnostics.record("HUD settings: writeACK error=\((error as NSError?)?.code ?? 0)")
            if error != nil { failSettings("Không gửi được cài đặt. Hãy thử lại.") }
            else if let request=settingsRequest { confirmSettingsByRead(request) }
            return
        }
        guard characteristic.uuid == Self.rxID else { return }
        waitingWrite = false
        if let error { status = error.localizedDescription; clearTransfer() }
        else { pump(); finishTransferIfAcknowledged() }
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let value=characteristic.value, characteristic.uuid==Self.settingsID || (value.first==0x94) {
            HUDDiagnostics.record("HUD incoming: channel=\(characteristic.uuid); first=\(value.first ?? 0); bytes=\(value.count); currentPeer=\(device?.identifier == peripheral.identifier)")
        }
        guard device?.identifier == peripheral.identifier else { return }
        if characteristic.uuid == Self.voiceID {
            if let value=characteristic.value, value.first==0xB1, value.count>5, value[5] != 1 {
                HUDDiagnostics.record("HUD speaker ACK: phase=\(value.count>5 ? value[5] : 255); bytes=\(value.count); voiceReady=\(voiceReady)")
            }
            if voiceReady, error == nil, let data=characteristic.value { onVoicePacket?(data) }
            return
        }
        if characteristic.uuid == Self.settingsID {
            if error == nil, let data = characteristic.value { receiveSettings(data) }
            else { failSettings("Không đọc được cài đặt từ HUD") }
            return
        }
        guard characteristic.uuid == Self.txID else { return }
        if error == nil, ready, let data = characteristic.value, let control = HUDMapControl(data) {
            onMapControl?(control); return
        }
        guard error == nil, let data = characteristic.value, data.count == 5, busy else { return }
        let id = (0..<4).reduce(UInt32(0)) { $0 | UInt32(data[$1 + 1]) << ($1 * 8) }
        guard id == frameID else { return }
        status = data[0] == 0 ? "HUD đã nhận bản đồ" : "HUD từ chối khung bản đồ"
        frameAcknowledged = true
        print("HUD map: bytes=\(transferBytes), bleMs=\(Int(Date().timeIntervalSince(transferStarted) * 1000)), accepted=\(data[0] == 0)")
        finishTransferIfAcknowledged()
    }
    private func finishTransferIfAcknowledged() {
        // Notification can arrive before didWriteValue for the final packet.
        // Keep that callback from accidentally advancing the next frame.
        if frameAcknowledged && !waitingWrite && packetIndex == packets.count { clearTransfer() }
    }
    private func clearTransfer() {
        timeout?.invalidate(); timeout = nil; busy = false; waitingWrite = false
        frameAcknowledged = false
        packets = []; packetIndex = 0
    }
    func stopNavigation() {
        clearTransfer()
        if ready, let device, let rx { device.writeValue(Data([4]), for: rx, type: .withResponse) }
    }
    func refreshSettings() {
        guard !settingsBusy, let device, let settingsChannel else { return }
        settingsStatus = "Đang đọc từ HUD…"
        device.readValue(for: settingsChannel)
    }
    func saveSettings(_ value: HUDSettings) {
        enqueueSettings(0x10, [UInt8(value.theme), UInt8(value.face), UInt8(value.timeout & 255),
                              UInt8(value.timeout >> 8), value.allowNewApps ? 1 : 0])
    }
    func saveHardware(_ value: HUDHardwareSettings) {
        guard (5...100).contains(value.brightness), (0...100).contains(value.speakerVolume), [0,2].contains(value.rotation) else { return }
        enqueueSettings(0x15,[UInt8(value.brightness),UInt8(value.speakerVolume),UInt8(value.rotation)])
    }
    func testSpeaker() { enqueueSettings(0x16,[]) }
    func setApp(_ id: String, enabled: Bool) {
        enqueueSettings(0x12, [enabled ? 1 : 0] + Array(id.utf8))
    }
    private func resetSettings() {
        resubscribing=[]
        hardwareReady=false
        voiceReady=false; voiceActive=false; speakerActive=false; voiceChannel=nil
        voiceTimer?.invalidate(); voiceQueue=[]; voiceWriting=false; onVoiceDisconnected?()
        catalogRefreshNeeded=false
        settingsReady = false; settingsChannel = nil; settingsQueue = []; settingsRequest = nil
        settingsBusy = false; settingsTimer?.invalidate(); settingsStatus = "Đang chờ HUD"
    }
    private func failSettings(_ message: String) {
        settingsQueue = []; settingsRequest = nil; settingsTimer?.invalidate()
        settingsBusy = false; settingsStatus = message
    }
    func sendVoice(_ packets: [Data]) {
        guard voiceReady, let device,
              packets.allSatisfy({ $0.count<=device.maximumWriteValueLength(for:.withResponse) }),
              voiceQueue.count+packets.count<=100 else { return }
        voiceQueue += packets; pumpVoice()
    }
    func sendVoiceText(_ text: String, session: UInt32, field: UInt8) {
        guard let device else { return }
        sendVoice(HUDVoiceWire.text(text,session:session,field:field,maxWrite:device.maximumWriteValueLength(for:.withResponse)))
    }
    func discardQueuedVoice() { voiceQueue=[] }
    private func pumpVoice() {
        guard !voiceWriting else { return }
        guard voiceReady, let device, let voiceChannel else { return }
        while let packet=voiceQueue.first, packet.first==0x41 {
            guard device.canSendWriteWithoutResponse else { return }
            guard packet.count<=device.maximumWriteValueLength(for:.withoutResponse) else {
                voiceQueue=[]; onVoiceDisconnected?(); return
            }
            voiceQueue.removeFirst(); device.writeValue(packet,for:voiceChannel,type:.withoutResponse)
        }
        guard !voiceQueue.isEmpty else { onVoiceQueueDrained?(); return }
        voiceWriting=true; device.writeValue(voiceQueue.removeFirst(),for:voiceChannel,type:.withResponse)
        voiceTimer?.invalidate()
        voiceTimer=Timer.scheduledTimer(withTimeInterval:5,repeats:false) { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                self.voiceQueue=[]; self.voiceWriting=false; self.voiceActive=false; self.onVoiceDisconnected?()
            }
        }
    }
    private func enqueueSettings(_ opcode: UInt8, _ payload: [UInt8]) {
        guard let device, let settingsChannel, settingsChannel.isNotifying else { return }
        settingsSequence &+= 1
        let command = Data([opcode, settingsSequence] + payload)
        guard command.count <= device.maximumWriteValueLength(for: .withResponse) else {
            failSettings("Kết nối có gói tin quá nhỏ. Hãy kết nối lại."); return
        }
        settingsQueue.append(command); settingsBusy = true; pumpSettings()
    }
    private func pumpSettings() {
        guard settingsRequest == nil, let device, let settingsChannel else { return }
        guard !settingsQueue.isEmpty else {
            settingsBusy = false
            if catalogRefreshNeeded { catalogRefreshNeeded=false; refreshSettings() }
            return
        }
        let request = settingsQueue.removeFirst(); settingsRequest = request
        HUDDiagnostics.record("HUD settings: send=\(request[0]); seq=\(request[1])")
        device.writeValue(request, for: settingsChannel, type: .withResponse)
        settingsTimer?.invalidate()
        settingsTimer = Timer.scheduledTimer(withTimeInterval: 6, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.failSettings("HUD chưa xác nhận. Bấm làm mới để kiểm tra.") }
        }
    }
    private func decodeSettings(_ data: Data) -> Bool {
        guard data.count == 7, data[0] == 1, data[1] < 8, data[2] < 8,
              [0,15,30,60,120,300].contains(Int(data[3]) | Int(data[4]) << 8), data[5] < 2, data[6] <= 24 else { return false }
        let value = HUDSettings(theme: Int(data[1]), face: Int(data[2]),
            timeout: Int(data[3]) | Int(data[4]) << 8, allowNewApps: data[5] == 1)
        if settings != value { settings = value }
        settingsReady = true
        return true
    }
    private func confirmSettingsByRead(_ request: Data) {
        Task { @MainActor [weak self] in
            try? await Task.sleep(for:.milliseconds(100))
            guard let self, self.settingsRequest==request, let device=self.device, let channel=self.settingsChannel else { return }
            device.readValue(for:channel)
        }
    }
    private func decodeHardware(_ data: Data) -> Bool {
        guard data.count==3, (5...100).contains(data[0]), data[1]<=100, [0,2].contains(data[2]) else { return false }
        let value=HUDHardwareSettings(brightness:Int(data[0]),speakerVolume:Int(data[1]),rotation:Int(data[2]))
        if hardware != value { hardware=value }
        hardwareReady=true; return true
    }
    private func receiveSettings(_ data: Data) {
        HUDDiagnostics.record("HUD settings: response=\(data.first ?? 0); bytes=\(data.count); busy=\(settingsBusy); hardwareReady=\(hardwareReady)")
        if data.first == 1 {
            let extended=data.count>=11 && data.count==11+Int(data[10])
            guard (data.count==7 || extended), decodeSettings(Data(data.prefix(7))) else {
                HUDDiagnostics.record("HUD settings: invalid snapshot=\(Array(data.prefix(7)))")
                failSettings("Phiên bản cài đặt chưa tương thích"); return
            }
            if extended, !decodeHardware(Data(data[7..<10])) { failSettings("Cài đặt màn hình/loa không hợp lệ"); return }
            if settingsBusy {
                if extended, let request=settingsRequest, data[10]>=3 {
                    let reply=Data(data.dropFirst(11))
                    if reply[0] == request[0] | 0x80 && reply[1]==request[1] {
                        receiveSettings(reply); return
                    }
                    confirmSettingsByRead(request)
                } else if extended, let request=settingsRequest { confirmSettingsByRead(request) }
                else { catalogRefreshNeeded=true }
                return
            }
            apps = []
            if !extended { enqueueSettings(0x14,[]) }
            for index in 0..<Int(data[6]) { enqueueSettings(0x13, [UInt8(index)]) }
            settingsStatus = "Đã đọc cài đặt từ HUD"
            return
        }
        guard let request = settingsRequest, data.count >= 3, data[0] == request[0] | 0x80,
              data[1] == request[1] else { return }
        settingsTimer?.invalidate(); settingsRequest = nil
        guard data[2] == 0 else { failSettings(data[0]==0x96 ? "Loa đang bận hoặc chưa sẵn sàng. Thả mic AI rồi thử lại." : data[2] == 2 ? "Bộ lọc đã đầy (24 ứng dụng)" : "HUD chưa áp dụng thay đổi. Hãy thử lại."); return }
        if data[0]==0x94 || data[0]==0x95 {
            guard data.count==7, data[3]==1, (5...100).contains(data[4]), data[5]<=100, [0,2].contains(data[6]) else { failSettings("Cài đặt màn hình/loa không hợp lệ"); return }
            let value=HUDHardwareSettings(brightness:Int(data[4]),speakerVolume:Int(data[5]),rotation:Int(data[6]))
            if hardware != value { hardware=value }
            hardwareReady=true
            settingsStatus=data[0]==0x95 ? "Đã áp dụng màn hình và loa trên HUD" : "Đã đọc cài đặt màn hình và loa"
        } else if data[0]==0x96 { settingsStatus="Đã phát âm thử trên loa HUD" }
        else if data[0] == 0x90 {
            guard decodeSettings(Data(data.dropFirst(3))) else { failSettings("Phản hồi cài đặt không hợp lệ"); return }
            settingsStatus = "HUD đã áp dụng cài đặt"
        } else if data[0] == 0x93, data.count > 5 {
            let fields=Data(data.dropFirst(5)).split(separator: 0, maxSplits: 1, omittingEmptySubsequences: false)
            if let first=fields.first, let id=String(data: first, encoding: .utf8) {
                let name=fields.count>1 ? String(data: fields[1], encoding: .utf8) ?? "" : ""
                apps.removeAll { $0.id == id }; apps.append(HUDAppRule(id: id, enabled: data[4] == 1, displayName: name))
            }
        } else if data[0] == 0x92, let id = String(data: request.dropFirst(3), encoding: .utf8) {
            if let index = apps.firstIndex(where: { $0.id == id }) { apps[index].enabled = request[2] == 1 }
            settingsStatus = "HUD đã cập nhật bộ lọc"
        }
        pumpSettings()
    }
    #if DEBUG
    // Invoked only by an explicit development launch argument; restores the user's settings.
    func runSettingsSmokeTest() async {
        func idle() async -> Bool {
            for _ in 0..<200 {
                if settingsReady && !settingsBusy { return true }
                try? await Task.sleep(for: .milliseconds(100))
            }
            return false
        }
        guard await idle() else { print("HUD SETTINGS CHECK: FAIL initial connection"); return }
        let original = settings
        let app = apps.first
        var passed = true
        for theme in 0..<8 {
            var test = original; test.face = theme; test.theme = theme; test.timeout = 15
            saveSettings(test)
            let completed = await idle()
            passed = passed && completed && settings == test
        }
        if let app {
            setApp(app.id, enabled: !app.enabled)
            let completed = await idle()
            passed = passed && completed && apps.first(where: { $0.id == app.id })?.enabled == !app.enabled
            setApp(app.id, enabled: app.enabled)
            let restored = await idle(); passed = passed && restored
        }
        saveSettings(original)
        let restored = await idle(); passed = passed && restored && settings == original
        refreshSettings()
        try? await Task.sleep(for: .seconds(1))
        let reread = await idle(); passed = passed && reread && settings == original
        print("HUD SETTINGS CHECK: \(passed ? "PASS" : "FAIL") eight faces/eight colors, timeout, filter, restore and reread")
    }
    #endif
}
