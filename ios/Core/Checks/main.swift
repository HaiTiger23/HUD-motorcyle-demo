import Foundation
import HUDNavCore
precondition(HUDWire.crc32(Data("123456789".utf8)) == 0xcbf43926)
let bytes = Data((0..<5000).map { UInt8(truncatingIfNeeded: $0) })
for mtu in [20, 64, 244, 512] {
    let packets = HUDWire.packets(jpeg: bytes, id: 123, maxWrite: mtu)!
    precondition(packets.allSatisfy { $0.count <= min(mtu, 244) })
    precondition(packets.dropFirst().dropLast().reduce(Data()) { $0 + $1.dropFirst(9) } == bytes)
    precondition(packets.first?.count == 13)
    precondition(packets.last == Data([3, 123, 0, 0, 0]))
}
precondition(HUDWire.packets(jpeg: Data(), id: 1, maxWrite: 244) == nil)
precondition(HUDWire.packets(jpeg: Data(count: 50000), id: 1, maxWrite: 244) == nil)
let start = GeoPoint(10.0, 106.0), end = GeoPoint(10.001, 106.0)
let route = RouteProgress(points: [start, end, GeoPoint(10.001, 106.001)])
let halfway = route.project(GeoPoint(10.0005, 106.00001))!
precondition(abs(halfway.metres - 55.6) < 1 && halfway.error < 2 && halfway.segment == 0)
precondition(route.project(GeoPoint(10.002, 106))!.error > 100)
precondition(RouteProgress(points: []).project(start) == nil)
print("Swift CRC32, BLE fragmentation and route projection checks passed")
let apple = MapShare.place(from: URL(string: "https://maps.apple.com/?ll=10.77,106.69&q=Test")!)!
precondition(apple.latitude == 10.77 && apple.longitude == 106.69)
let google = MapShare.place(from: URL(string: "https://www.google.com/maps/place/Test/@1,2,15z/data=!3d10.78!4d106.70")!)!
precondition(google.latitude == 10.78 && google.longitude == 106.70)
precondition(MapShare.place(from: URL(string: "https://www.google.com/maps/@1,2,15z")!) == nil)
precondition(MapShare.place(from: URL(string: "https://maps.apple.com/?coordinate=999,106")!) == nil)
precondition(!MapShare.allowed(URL(string: "https://google.com.evil.example/maps?q=abc")!))
precondition(!MapShare.allowed(URL(string: "file:///maps?q=abc")!))
precondition(MapShare.firstURL(in: "Tên địa điểm\nhttps://maps.app.goo.gl/abc")?.host == "maps.app.goo.gl")
precondition(MapShare.place(from: URL(string: "https://www.google.com/maps/dir/?api=1&destination=10.1,106.2")!)?.latitude == 10.1)
print("Shared map links: Apple, Google, destination vs camera, bounds and host checks passed")
precondition(HUDWire.speedKmh(metresPerSecond: 10, accuracy: 5, speedAccuracy: 0.5, age: 1) == 36)
precondition(HUDWire.speedKmh(metresPerSecond: 0.2, accuracy: 5, speedAccuracy: 0.5, age: 1) == 0)
precondition(HUDWire.speedKmh(metresPerSecond: -1, accuracy: 5, speedAccuracy: 0.5, age: 1) == nil)
precondition(HUDWire.speedKmh(metresPerSecond: 10, accuracy: 50, speedAccuracy: 0.5, age: 1) == nil)
precondition(HUDWire.speedKmh(metresPerSecond: 10, accuracy: 5, speedAccuracy: -1, age: 1) == nil)
precondition(HUDWire.speedKmh(metresPerSecond: 10, accuracy: 5, speedAccuracy: 0.5, age: 10) == nil)
print("GPS speed: conversion, stationary noise, invalid and stale fixes passed")
var marker = HUDLocationMarkerState()
let t0 = Date(timeIntervalSinceReferenceDate: 0)
precondition(marker.update(speed: 3, horizontalAccuracy: 5, course: 10, courseAccuracy: 5, age: 0, now: t0) == .dot)
precondition(marker.update(speed: 3, horizontalAccuracy: 5, course: 10, courseAccuracy: 5, age: 0, now: t0.addingTimeInterval(2)) == .arrow)
precondition(marker.update(speed: 1.5, horizontalAccuracy: 5, course: 10, courseAccuracy: 5, age: 0, now: t0.addingTimeInterval(10)) == .arrow)
precondition(marker.update(speed: 0.5, horizontalAccuracy: 5, course: 10, courseAccuracy: 5, age: 0, now: t0.addingTimeInterval(11)) == .arrow)
precondition(marker.update(speed: 0.5, horizontalAccuracy: 5, course: 10, courseAccuracy: 5, age: 0, now: t0.addingTimeInterval(14)) == .dot)
precondition(marker.update(speed: 3, horizontalAccuracy: 5, course: -1, courseAccuracy: -1, age: 0, now: t0.addingTimeInterval(20)) == .dot)
print("Location marker: GPS-course arrow, hysteresis, dwell time and unreliable-course dot passed")
precondition(HUDMapCadence.headingChange(359, 1) == 2)
precondition(HUDMapCadence.headingChange(10, 350) == 20)
precondition(!HUDMapCadence.refreshBase(age: 1, movement: 40, headingChange: 90))
precondition(HUDMapCadence.refreshBase(age: 1.5, movement: 18, headingChange: 0))
precondition(HUDMapCadence.refreshBase(age: 2, movement: 0, headingChange: 15))
precondition(!HUDMapCadence.refreshBase(age: 4, movement: 0, headingChange: 0))
precondition(HUDMapCadence.refreshBase(age: 8, movement: 0, headingChange: 0))
precondition(!HUDMapCadence.sendFrame(age: 0.5, movement: 20, changed: true))
precondition(HUDMapCadence.sendFrame(age: 1, movement: 2, changed: false))
precondition(HUDMapCadence.sendFrame(age: 1, movement: 0, changed: true))
precondition(!HUDMapCadence.sendFrame(age: 4, movement: 0, changed: false))
precondition(HUDMapCadence.sendFrame(age: 5, movement: 0, changed: false))
print("Map cadence: live GPS overlays, stationary keepalive, base throttling and north crossing passed")
let pan = HUDMapControl(Data([0x80,4,0x78,0x56,0x34,0x12,0xce,0xff,40,0]))!
precondition(pan.command == .pan && pan.frameID == 0x12345678 && pan.dx == -50 && pan.dy == 40)
precondition(HUDMapControl(Data([0x80,1,1,0,0,0,0,0,0,0]))?.command == .zoomIn)
precondition(HUDMapControl(Data([0x80,3,1,0,0,0,0,0,0,0]))?.command == .follow)
precondition(HUDMapControl(Data([0x80,1,1,0,0,0,1,0,0,0])) == nil)
precondition(HUDMapControl(Data([0x80,4,1,0,0,0,0x2d,1,0,0])) == nil)
precondition(HUDMapControl(Data([0x80,6,1,0,0,0,0,0,0,0]))?.command == .stop)
precondition(HUDMapControl(Data([0x80,6,1,0,0,0,1,0,0,0])) == nil)
precondition(HUDMapControl(Data([0x80,7,1,0,0,0,0,0,0,0])) == nil)
precondition(HUDMapControl(Data([0,1,0,0,0])) == nil)
let northPan = HUDMapControl.mapOffset(dx: 50, dy: -20, axisX: (2,0), axisY: (0,2))!
precondition(northPan.x == -25 && northPan.y == 10)
let eastPan = HUDMapControl.mapOffset(dx: 50, dy: 0, axisX: (0,-2), axisY: (2,0))!
precondition(eastPan.x == 0 && eastPan.y == -25)
precondition(HUDMapControl.mapOffset(dx: 50, dy: 0, axisX: (0,0), axisY: (0,0)) == nil)
print("Map controls: frame binding, signed pan, bounds, invalid packets and rotated map inversion passed")
let voice = Data([0xA1,1,0,0,0,0,0,0,0,0,4,0,0xF7,0x07])
precondition(HUDVoiceWire.decode(voice)!.samples == [0,11,-19,44])
precondition(HUDVoiceWire.session(voice) == 1)
var invalidVoice=voice; invalidVoice[9]=89
precondition(HUDVoiceWire.decode(invalidVoice) == nil)
precondition(HUDVoiceWire.decode(Data(voice.dropLast())) == nil)
invalidVoice=voice; invalidVoice[10]=0
precondition(HUDVoiceWire.decode(invalidVoice) == nil)
let vietnamese="Đường Việt Nam 🛵 — thử nhận dạng tiếng nói"
let voiceText=HUDVoiceWire.text(vietnamese,session:123,field:2,maxWrite:20)
precondition(voiceText.allSatisfy { $0.count<=20 && String(data:$0.dropFirst(9),encoding:.utf8) != nil })
precondition(voiceText.reduce(Data()) { $0+$1.dropFirst(9) } == Data(vietnamese.utf8))
precondition(voiceText.first![6]==0 && voiceText.dropFirst().allSatisfy { $0[6]==1 })
precondition(HUDVoiceWire.bounded("a🛵",bytes:3)=="a")
precondition(HUDVoiceWire.bounded("Tiếng Việt",bytes:0).isEmpty)
print("Voice wire: cross-language ADPCM golden vector, malformed packets, UTF-8 fragments and bounds passed")

if case .action(let action)=try HUDAssistantPlan.parse("{\"action\":\"music_next\"}") {
    precondition(action.code==3 && action.value==3)
    precondition(action.packet(id:0x12345678)==Data([0x30,0x78,0x56,0x34,0x12,3,3,0]))
} else { preconditionFailure() }
if case .reply(let reply)=try HUDAssistantPlan.parse("```json\n{\"reply\":\"Xin chào\"}\n```") { precondition(reply=="Xin chào") }
else { preconditionFailure() }
if case .destination(let destination)=try HUDAssistantPlan.parse("{\"action\":\"navigate_to\",\"query\":\"Trống Đồng Palace\"}") { precondition(destination=="Trống Đồng Palace") }
else { preconditionFailure() }
for invalid in ["{\"action\":\"delete_all\"}","{\"action\":\"set_theme\"}","{\"action\":\"set_theme\",\"value\":8}",
                "{\"action\":\"set_timeout\",\"value\":45}","{\"action\":\"set_face\",\"value\":true}",
                "{\"action\":\"set_face\",\"value\":1.5}","{\"action\":\"show_page\",\"value\":-1}",
                "{\"action\":\"navigate_to\",\"query\":\"\"}","{\"action\":\"navigate_to\",\"query\":3}",
                "{\"action\":\"music_next\",\"value\":3}","{\"action\":\"music_next\",\"reply\":\"Đã đổi\"}",
                "[{\"action\":\"music_next\"}]","{\"reply\":\"\"}"] {
    do { _=try HUDAssistantPlan.parse(invalid); preconditionFailure("Unsafe action accepted") } catch {}
}
precondition(HUDAssistantAction.make("set_timeout",value:0)?.value==0)
precondition(HUDAssistantAction.make("set_face",value:7)?.value==7)
precondition(HUDAssistantAction.make("set_face",value:8)==nil)
var toolAssembly=HUDToolAssembly(id:1)
let split=Array("ẽ".utf8)
let partialTool=try toolAssembly.feed(Data([0xB0,1,0,0,0,0,0,0,0,split[0]]))
precondition(partialTool==nil)
let assembled=try toolAssembly.feed(Data([0xB0,1,0,0,0,0,1,0,1,split[1],split[2]]))
precondition(assembled?.text=="ẽ" && assembled?.success==true)
for invalid in [Data([0xB0,2,0,0,0,0,0,0,1]),Data([0xB0,1,0,0,0,0,1,0,1]),Data([0xB0,1,0,0,0,0,0,0,1,0xe1])] {
    var assembly=HUDToolAssembly(id:1)
    do { _=try assembly.feed(invalid); preconditionFailure("Invalid tool fragment accepted") } catch {}
}
print("Assistant actions: whitelist, strict JSON, argument bounds, wire encoding, fragmented Vietnamese and invalid replies passed")
var playback=HUDVoiceWire.playback([100,-100,100],id:1,sequence:0)!
precondition(playback[0]==0x41 && playback.count==13)
playback[0]=0xA1
let playbackDecoded=HUDVoiceWire.decode(playback)!
precondition(playbackDecoded.samples==[100,89,99])
precondition(playback[12]==0x2f)
precondition(HUDVoiceWire.playback([],id:1,sequence:0)==nil)
precondition(HUDVoiceWire.playback(Array(repeating:0,count:321),id:1,sequence:0)==nil)
print("Speaker ADPCM: independent playback blocks, golden reconstruction and bounds passed")
var carriedIndex=0,carriedError=0.0,resetError=0.0
for block in 0..<10 {
    let source=(0..<320).map { Int16(12000*sin(Double(block*320+$0)*2*Double.pi*440/8000)) }
    var continuous=HUDVoiceWire.playback(source,id:1,sequence:block,index:&carriedIndex)!
    var reset=HUDVoiceWire.playback(source,id:1,sequence:block)!
    continuous[0]=0xA1; reset[0]=0xA1
    let good=HUDVoiceWire.decode(continuous)!.samples,bad=HUDVoiceWire.decode(reset)!.samples
    for i in source.indices {
        carriedError+=pow(Double(source[i])-Double(good[i]),2)
        resetError+=pow(Double(source[i])-Double(bad[i]),2)
    }
}
precondition(carriedError<resetError)
print("Speaker ADPCM: carrying quantizer state reduces block-boundary distortion")

// AI navigation: one-based speech selection, strict context and confirmation.
let navContext="15BD271D-2170-4CE8-99C2-AB83C8B0BD87"
if case .navigation(.select(let index,let context))=try HUDAssistantPlan.parse("{\"action\":\"navigation_select\",\"value\":1,\"context\":\"\(navContext)\"}") {
    precondition(index==0 && context==navContext)
} else { preconditionFailure("Missing selection intent") }
if case .navigation(.start(let context))=try HUDAssistantPlan.parse("{\"action\":\"navigation_start\",\"context\":\"\(navContext)\"}") { precondition(context==navContext) }
else { preconditionFailure("Missing start intent") }
for invalid in [
    "{\"action\":\"navigation_select\",\"value\":0,\"context\":\"\(navContext)\"}",
    "{\"action\":\"navigation_select\",\"value\":6,\"context\":\"\(navContext)\"}",
    "{\"action\":\"navigation_select\",\"value\":true,\"context\":\"\(navContext)\"}",
    "{\"action\":\"navigation_select\",\"value\":1.5,\"context\":\"\(navContext)\"}",
    "{\"action\":\"navigation_select\",\"value\":1}",
    "{\"action\":\"navigation_start\",\"context\":\"made-up\"}",
    "{\"action\":\"navigation_start\",\"context\":\"\(navContext)\",\"value\":1}"
] { do { _=try HUDAssistantPlan.parse(invalid); preconditionFailure("Invalid navigation accepted") } catch {} }
let chooseFirst=HUDNavigationIntent.select(index:0,context:navContext)
let beginTrip=HUDNavigationIntent.start(context:navContext)
func permits(_ intent: HUDNavigationIntent,context: String=navContext,age: Double=10,session: Bool=true,count: Int=3,searching: Bool=false,preview: Bool=false) -> Bool {
    intent.isAllowed(currentContext:context,age:age,hasSession:session,resultsCount:count,searching:searching,previewReady:preview)
}
precondition(permits(chooseFirst))
precondition(!permits(chooseFirst,count:0))
precondition(!permits(chooseFirst,context:UUID().uuidString))
precondition(!permits(chooseFirst,age:300))
precondition(!permits(chooseFirst,age:-1))
precondition(!permits(chooseFirst,session:false))
precondition(!permits(chooseFirst,searching:true))
precondition(!permits(beginTrip)) // Selecting/search results never authorizes start.
precondition(permits(beginTrip,preview:true))
precondition(!permits(beginTrip,context:UUID().uuidString,preview:true))
precondition(!permits(beginTrip,session:false,preview:true))
precondition(!permits(beginTrip,age:301,preview:true))
precondition(!permits(beginTrip,searching:true,preview:true))
precondition(!permits(.select(index:3,context:navContext),count:3))
print("AI navigation: strict intents, first-result selection, stale/cancelled/expired contexts and separate start confirmation passed")

let historySelection=HUDHistoryRequest(Data([0x55,1,2,0,0,0,0x78,0x56,0x34,0x12,4]))!
precondition(historySelection.session==2 && historySelection.token==0x12345678 && historySelection.index==4)
precondition(HUDHistoryRequest(Data([0x55,0,0,0,0,0,0,0,0,0,0]))?.action==0)
for invalid in [
    Data([0x55,1,0,0,0,0,1,0,0,0,0]), // Selection needs a new session.
    Data([0x55,1,1,0,0,0,0,0,0,0,0]), // And the actual catalog token.
    Data([0x55,1,1,0,0,0,1,0,0,0,5]),
    Data([0x55,0,1,0,0,0,0,0,0,0,0]),
    Data([0x55,2,1,0,0,0,1,0,0,0,0]),Data([0x55,1])
] { precondition(HUDHistoryRequest(invalid)==nil) }
print("HUD history requests: refresh/selection, catalog token, session and index bounds passed")
