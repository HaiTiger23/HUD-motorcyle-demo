import Foundation
import CoreFoundation

public struct HUDAssistantAction: Equatable {
    public let name: String
    public let code: UInt8
    public let value: UInt16
    public var summarizesNotifications: Bool { name == "notifications_summary" }
    public static func make(_ name: String, value: Int?) -> HUDAssistantAction? {
        let fixed: [String:(UInt8,UInt16)] = [
            "notifications_count":(1,0),"notifications_latest":(1,1),"notifications_list":(1,3),"notifications_summary":(1,3),
            "music_status":(2,0),"music_play":(3,0),"music_pause":(3,1),"music_next":(3,3),
            "music_previous":(3,4),"volume_up":(3,5),"volume_down":(3,6)]
        if let pair=fixed[name], value == nil { return Self(name:name,code:pair.0,value:pair.1) }
        guard let value else { return nil }
        let code: UInt8
        switch name {
        case "set_theme": guard (0..<8).contains(value) else { return nil }; code=4
        case "set_face": guard (0..<8).contains(value) else { return nil }; code=5
        case "set_timeout": guard [0,15,30,60,120,300].contains(value) else { return nil }; code=6
        case "show_page": guard (0..<4).contains(value) else { return nil }; code=7
        default: return nil
        }
        return Self(name:name,code:code,value:UInt16(value))
    }
    public func packet(id: UInt32) -> Data {
        Data([0x30,UInt8(truncatingIfNeeded:id),UInt8(truncatingIfNeeded:id>>8),UInt8(truncatingIfNeeded:id>>16),UInt8(truncatingIfNeeded:id>>24),code,UInt8(truncatingIfNeeded:value),UInt8(value>>8)])
    }
}
public enum HUDAssistantPlan {
    case reply(String), action(HUDAssistantAction), destination(String), navigation(HUDNavigationIntent)
    public static func parse(_ content: String) throws -> HUDAssistantPlan {
        var text=content.trimmingCharacters(in:.whitespacesAndNewlines)
        if text.hasPrefix("```"), text.hasSuffix("```") {
            let lines=text.components(separatedBy:"\n")
            text=lines.dropFirst().dropLast().joined(separator:"\n")
        }
        guard let data=text.data(using:.utf8), let object=try JSONSerialization.jsonObject(with:data) as? [String:Any] else { throw invalid() }
        if Set(object.keys)==["reply"], let reply=object["reply"] as? String,
           !reply.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty { return .reply(reply) }
        guard let name=object["action"] as? String else { throw invalid() }
        if name=="navigation_select" || name=="navigation_start" {
            guard let context=object["context"] as? String, UUID(uuidString:context) != nil else { throw invalid() }
            if name=="navigation_start" {
                guard Set(object.keys)==["action","context"] else { throw invalid() }
                return .navigation(.start(context:context))
            }
            guard Set(object.keys)==["action","context","value"], let number=object["value"] as? NSNumber,
                  CFGetTypeID(number) != CFBooleanGetTypeID(), number.doubleValue.rounded()==number.doubleValue,
                  (1...5).contains(number.doubleValue) else { throw invalid() }
            return .navigation(.select(index:number.intValue-1,context:context))
        }
        if name=="navigate_to", Set(object.keys)==["action","query"], let query=object["query"] as? String {
            let trimmed=query.trimmingCharacters(in:.whitespacesAndNewlines)
            guard (2...160).contains(trimmed.utf8.count), !trimmed.unicodeScalars.contains(where:{ $0.value<32 }) else { throw invalid() }
            return .destination(trimmed)
        }
        guard Set(object.keys).isSubset(of:["action","value"]) else { throw invalid() }
        var value: Int?
        if let raw=object["value"] {
            guard let number=raw as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
                  number.doubleValue.isFinite, number.doubleValue.rounded()==number.doubleValue,
                  (0...300).contains(number.doubleValue) else { throw invalid() }
            value=number.intValue
        }
        guard let action=HUDAssistantAction.make(name,value:value) else { throw invalid() }
        return .action(action)
    }
    private static func invalid() -> NSError {
        NSError(domain:"HUD assistant",code:1,userInfo:[NSLocalizedDescriptionKey:"AI chưa trả đúng định dạng. Chưa thực hiện lệnh; hãy nói lại ngắn gọn."])
    }
}
public enum HUDNavigationIntent {
    case select(index: Int, context: String), start(context: String)
    public var context: String {
        switch self { case .select(_,let context),.start(let context): return context }
    }
    public func isAllowed(currentContext: String, age: TimeInterval, hasSession: Bool,
                          resultsCount: Int, searching: Bool, previewReady: Bool) -> Bool {
        guard context==currentContext, hasSession, age>=0, age<300, !searching else { return false }
        switch self {
        case .select(let index,_): return index>=0 && index<resultsCount && index<5
        case .start: return previewReady
        }
    }
}
public struct HUDHistoryRequest {
    public let action: UInt8, index: Int
    public let session: UInt32, token: UInt32
    public init?(_ data: Data) {
        guard data.count==11,data[0]==0x55,data[1]<=1,data[10]<5 else { return nil }
        action=data[1]; index=Int(data[10])
        session=(0..<4).reduce(UInt32(0)) { $0 | UInt32(data[$1+2]) << ($1*8) }
        token=(0..<4).reduce(UInt32(0)) { $0 | UInt32(data[$1+6]) << ($1*8) }
        guard action==0 ? (session==0 && index==0) : (session != 0 && token != 0) else { return nil }
    }
}
public struct HUDToolAssembly {
    public let id: UInt32
    private var sequence=0
    private var status: UInt8?
    private var bytes=Data()
    public init(id: UInt32) { self.id=id }
    public mutating func feed(_ packet: Data) throws -> (success:Bool,text:String)? {
        func invalid() -> NSError { NSError(domain:"HUD assistant",code:2,userInfo:[NSLocalizedDescriptionKey:"Không xác nhận được kết quả từ HUD. Kiểm tra trạng thái trước khi thử lại."]) }
        guard packet.count>=9, packet.count<=244, packet[0]==0xB0,
              (0..<4).reduce(UInt32(0),{ $0 | UInt32(packet[$1+1]) << ($1*8) })==id,
              packet[5]<=1, packet[8]<=1, Int(packet[6]) | Int(packet[7])<<8 == sequence,
              status == nil || status == packet[5], bytes.count+packet.count-9<=3071 else { throw invalid() }
        status=packet[5]; sequence+=1; bytes.append(packet.dropFirst(9))
        if packet[8]==0 { return nil }
        guard let text=String(data:bytes,encoding:.utf8) else { throw invalid() }
        return (packet[5]==0,text)
    }
}

// Separate cheap GPS overlays from the network-backed map snapshot.
public enum HUDMapCadence {
    public static func headingChange(_ first: Double, _ second: Double) -> Double {
        let delta = abs(first - second).truncatingRemainder(dividingBy: 360)
        return min(delta, 360 - delta)
    }
    public static func refreshBase(age: TimeInterval, movement: Double, headingChange: Double) -> Bool {
        age >= 8 || (age >= 1.5 && (movement >= 18 || headingChange >= 12))
    }
    public static func sendFrame(age: TimeInterval, movement: Double, changed: Bool) -> Bool {
        age >= 1 && (changed || movement >= 1.5 || age >= 5)
    }
}

public struct HUDMapControl {
    public enum Command: UInt8 { case zoomIn = 1, zoomOut = 2, follow = 3, pan = 4, browse = 5, stop = 6 }
    public let command: Command
    public let frameID: UInt32
    public let dx: Int
    public let dy: Int
    public init?(_ data: Data) {
        guard data.count == 10, data[0] == 0x80, let command = Command(rawValue: data[1]) else { return nil }
        let x = Int(Int16(bitPattern: UInt16(data[6]) | UInt16(data[7]) << 8))
        let y = Int(Int16(bitPattern: UInt16(data[8]) | UInt16(data[9]) << 8))
        guard abs(x) <= HUDWire.imageWidth, abs(y) <= HUDWire.imageHeight,
              command == .pan || (x == 0 && y == 0) else { return nil }
        self.command = command; dx = x; dy = y
        frameID = (0..<4).reduce(UInt32(0)) { $0 | UInt32(data[$1 + 2]) << ($1 * 8) }
    }
    // Invert the snapshot's screen axes, including heading. Dragging the map
    // right moves its camera left; the GPS fix itself never changes.
    public static func mapOffset(dx: Double, dy: Double, axisX: (Double, Double), axisY: (Double, Double)) -> (x: Double, y: Double)? {
        let determinant = axisX.0 * axisY.1 - axisY.0 * axisX.1
        guard determinant.isFinite, abs(determinant) > 0.000000001 else { return nil }
        let x = (-dx * axisY.1 + dy * axisY.0) / determinant
        let y = (-dy * axisX.0 + dx * axisX.1) / determinant
        return x.isFinite && y.isFinite ? (x, y) : nil
    }
}

public enum HUDWire {
    public static func speedKmh(metresPerSecond: Double, accuracy: Double, speedAccuracy: Double, age: Double) -> Double? {
        guard metresPerSecond.isFinite, metresPerSecond >= 0, metresPerSecond <= 300/3.6,
              accuracy >= 0, accuracy <= 25, speedAccuracy >= 0, speedAccuracy <= 3, abs(age) <= 5 else { return nil }
        return metresPerSecond < 0.5 ? 0 : metresPerSecond*3.6
    }
    public static let imageWidth = 300
    public static let imageHeight = 200
    public static let maxImage = 48 * 1024
    public static func crc32(_ data: Data) -> UInt32 {
        var crc: UInt32 = 0xffff_ffff
        for byte in data {
            crc ^= UInt32(byte)
            for _ in 0..<8 { crc = (crc >> 1) ^ ((crc & 1) == 1 ? 0xedb8_8320 : 0) }
        }
        return ~crc
    }
    static func word(_ value: UInt32) -> Data {
        Data((0..<4).map { UInt8(truncatingIfNeeded: value >> ($0 * 8)) })
    }
    public static func packets(jpeg: Data, id: UInt32, maxWrite: Int) -> [Data]? {
        guard !jpeg.isEmpty, jpeg.count <= maxImage, maxWrite >= 20 else { return nil }
        var packets = [Data([1]) + word(id) + word(UInt32(jpeg.count)) + word(crc32(jpeg))]
        let chunkSize = min(maxWrite, 244) - 9
        for offset in stride(from: 0, to: jpeg.count, by: chunkSize) {
            packets.append(Data([2]) + word(id) + word(UInt32(offset)) + jpeg.subdata(in: offset..<min(offset + chunkSize, jpeg.count)))
        }
        packets.append(Data([3]) + word(id))
        return packets
    }
}

// The map marker is intentionally derived from the vehicle's GPS course, not
// from the phone's compass.  Separate enter/leave thresholds and dwell times
// prevent the icon from flashing while a rider is waiting at a light.
public enum HUDLocationMarker: Equatable { case dot, arrow }

public struct HUDLocationMarkerState {
    public private(set) var marker: HUDLocationMarker = .dot
    private var candidate: HUDLocationMarker?
    private var candidateSince: Date?

    public init() {}

    public mutating func update(speed: Double, horizontalAccuracy: Double, course: Double,
                                courseAccuracy: Double, age: Double, now: Date = Date()) -> HUDLocationMarker {
        let positionIsCurrent = horizontalAccuracy >= 0 && horizontalAccuracy <= 25 && abs(age) <= 5
        let courseIsReliable = course.isFinite && course >= 0 && course < 360 &&
            courseAccuracy >= 0 && courseAccuracy <= 30
        // Hysteresis: enter at 2 m/s (7.2 km/h), leave at 1 m/s (3.6 km/h).
        let wantsArrow = positionIsCurrent && courseIsReliable && speed.isFinite && speed >= 2
        let wantsDot = !positionIsCurrent || !courseIsReliable || !speed.isFinite || speed <= 1
        let desired: HUDLocationMarker? = wantsArrow ? .arrow : (wantsDot ? .dot : nil)
        guard let desired, desired != marker else {
            candidate = nil; candidateSince = nil; return marker
        }
        if candidate != desired { candidate = desired; candidateSince = now; return marker }
        let dwell: TimeInterval = desired == .arrow ? 2 : 3
        if let candidateSince, now.timeIntervalSince(candidateSince) >= dwell {
            marker = desired; candidate = nil; self.candidateSince = nil
        }
        return marker
    }
}

public enum HUDVoiceWire {
    private static let steps = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
    private static let adjustments = [-1,-1,-1,-1,2,4,6,8]
    public static func playback(_ samples: [Int16], id: UInt32, sequence: Int) -> Data? {
        var index=0
        return playback(samples,id:id,sequence:sequence,index:&index)
    }
    public static func playback(_ samples: [Int16], id: UInt32, sequence: Int,index: inout Int) -> Data? {
        guard (1...320).contains(samples.count), (0...65535).contains(sequence), (0...88).contains(index) else { return nil }
        let first=UInt16(bitPattern:samples[0])
        var packet=command(0x41,session:id,payload:[UInt8(truncatingIfNeeded:sequence),UInt8(sequence>>8),UInt8(truncatingIfNeeded:first),UInt8(first>>8),UInt8(index),UInt8(truncatingIfNeeded:samples.count),UInt8(samples.count>>8)])
        var nibbles=[UInt8](repeating:0,count:samples.count/2)
        var predictor=Int(samples[0])
        for i in 1..<samples.count {
            let step=steps[index],difference=Int(samples[i])-predictor
            var magnitude=abs(difference),code=difference<0 ? 8 : 0,delta=step>>3
            if magnitude>=step { code|=4; magnitude-=step; delta+=step }
            if magnitude>=step>>1 { code|=2; magnitude-=step>>1; delta+=step>>1 }
            if magnitude>=step>>2 { code|=1; delta+=step>>2 }
            predictor=max(-32768,min(32767,predictor+(code&8 != 0 ? -delta : delta)))
            index=max(0,min(88,index+adjustments[code&7]))
            nibbles[(i-1)/2] |= UInt8(code)<<(((i-1)%2)*4)
        }
        packet.append(contentsOf:nibbles); return packet
    }
    public static func session(_ data: Data) -> UInt32? {
        guard data.count >= 5 else { return nil }
        return (0..<4).reduce(UInt32(0)) { $0 | UInt32(data[$1+1]) << ($1*8) }
    }
    public static func command(_ opcode: UInt8, session: UInt32, payload: [UInt8] = []) -> Data {
        Data([opcode] + (0..<4).map { UInt8(truncatingIfNeeded: session >> ($0*8)) } + payload)
    }
    public static func decode(_ data: Data) -> (sequence: Int, samples: [Int16])? {
        guard data.count >= 12, data[0] == 0xA1, data[9] <= 88 else { return nil }
        let count = Int(data[10]) | Int(data[11]) << 8
        guard (1...320).contains(count), data.count == 12 + count/2 else { return nil }
        var predictor = Int(Int16(bitPattern: UInt16(data[7]) | UInt16(data[8]) << 8))
        var index = Int(data[9]), samples = [Int16(predictor)]
        for i in 0..<(count-1) {
            let code = Int((data[12+i/2] >> ((i%2)*4)) & 15)
            let step = steps[index]
            let delta = (step>>3) + (code&4 != 0 ? step : 0) + (code&2 != 0 ? step>>1 : 0) + (code&1 != 0 ? step>>2 : 0)
            predictor = max(-32768,min(32767,predictor + (code&8 != 0 ? -delta : delta)))
            index = max(0,min(88,index+adjustments[code&7]))
            samples.append(Int16(predictor))
        }
        return (Int(data[5]) | Int(data[6])<<8, samples)
    }
    public static func text(_ text: String, session: UInt32, field: UInt8, maxWrite: Int) -> [Data] {
        guard field < 3, maxWrite >= 20 else { return [] }
        let limit = min(maxWrite,244)-9
        var pieces: [[UInt8]] = [], part: [UInt8] = []
        for scalar in text.unicodeScalars {
            let bytes = Array(String(scalar).utf8)
            if part.count+bytes.count > limit { pieces.append(part); part=[] }
            part += bytes
        }
        pieces.append(part)
        return pieces.enumerated().map { index, bytes in
            command(0x20,session:session,payload:[field,index==0 ? 0 : 1,UInt8(truncatingIfNeeded:index),UInt8(truncatingIfNeeded:index>>8)]+bytes)
        }
    }
    public static func bounded(_ text: String, bytes: Int) -> String {
        // Decode a byte prefix, backing off incomplete trailing UTF-8 scalars.
        var data = Data(text.utf8.prefix(max(0,bytes)))
        while !data.isEmpty {
            if let decoded=String(data:data,encoding:.utf8) { return decoded }
            data.removeLast()
        }
        return ""
    }
}

public struct GeoPoint: Equatable {
    public var latitude: Double
    public var longitude: Double
    public init(_ latitude: Double, _ longitude: Double) { self.latitude = latitude; self.longitude = longitude }
    public func distance(to other: GeoPoint) -> Double {
        let rad = Double.pi / 180
        let dlat = (other.latitude - latitude) * rad, dlon = (other.longitude - longitude) * rad
        let a = pow(sin(dlat / 2), 2) + cos(latitude * rad) * cos(other.latitude * rad) * pow(sin(dlon / 2), 2)
        return 6_371_000 * 2 * atan2(sqrt(a), sqrt(max(0, 1 - a)))
    }
}
public struct RouteProgress {
    public let points: [GeoPoint]
    public let cumulative: [Double]
    public init(points: [GeoPoint]) {
        self.points = points
        var lengths = [Double](repeating: 0, count: points.count)
        if points.count > 1 { for i in 1..<points.count { lengths[i] = lengths[i - 1] + points[i - 1].distance(to: points[i]) } }
        cumulative = lengths
    }
    public func project(_ location: GeoPoint, startingAt segment: Int = 0) -> (metres: Double, error: Double, segment: Int)? {
        guard points.count > 1 else { return nil }
        let rad = Double.pi / 180, scale = 6_371_000 * Double.pi / 180
        let xscale = scale * cos(location.latitude * rad)
        var best = (metres: 0.0, error: Double.infinity, segment: 0)
        for i in max(0, min(segment, points.count - 2))..<(points.count - 1) {
            let ax = (points[i].longitude - location.longitude) * xscale
            let ay = (points[i].latitude - location.latitude) * scale
            let bx = (points[i + 1].longitude - location.longitude) * xscale
            let by = (points[i + 1].latitude - location.latitude) * scale
            let dx = bx - ax, dy = by - ay, square = dx * dx + dy * dy
            let t = square > 0 ? max(0, min(1, -(ax * dx + ay * dy) / square)) : 0
            let error = hypot(ax + t * dx, ay + t * dy)
            if error < best.error {
                best = (cumulative[i] + t * (cumulative[i + 1] - cumulative[i]), error, i)
            }
        }
        return best
    }
}
