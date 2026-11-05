import Foundation
import Network

// Read Aloud v2 protocol constants and arithmetic reference:
// https://github.com/hs-CN/msedge-tts/blob/master/src/tts/drm.rs (MIT)
// See ios/ThirdParty/msedge-tts-LICENSE.txt. No user API key is involved.
enum EdgeToken {
    private static func rotate(_ x: UInt32,_ n: UInt32) -> UInt32 { (x >> n) | (x << (32-n)) }
    private static func swap(_ x: UInt32) -> UInt32 { (x << 16) | (x >> 16) }
    private static func s0(_ x: UInt32) -> UInt32 { rotate(x,7) ^ rotate(x,18) ^ (x >> 3) }
    private static func s1(_ x: UInt32) -> UInt32 { rotate(x,17) ^ rotate(x,19) ^ (x >> 10) }
    private static func expand(_ words: inout [UInt32],from start: Int=16) {
        for i in start..<64 { words.append(s1(words[i-2]) &+ words[i-7] &+ s0(words[i-15]) &+ words[i-16]) }
    }
    private static func rounds(_ state: [UInt32],_ words: [UInt32],from start: Int=0) -> [UInt32] {
        var v=state
        for i in start..<64 {
            let e=v[4],a=v[0]
            let constant: UInt32 = start==14 && i==14 ? 0xC9DC06A7 : roundConstants[i]
            let t1=v[7] &+ (rotate(e,6) ^ rotate(e,11) ^ rotate(e,25)) &+ (v[6] ^ (e & (v[5] ^ v[6]))) &+ constant &+ words[i]
            let t2=(rotate(a,2) ^ rotate(a,13) ^ rotate(a,22)) &+ ((a & v[1]) | (v[2] & (a | v[1])))
            v=[t1 &+ t2,a,v[1],v[2],v[3] &+ t1,e,v[5],v[6]]
        }
        return v
    }
    private static func bigWord(_ bytes: [UInt8],_ offset: Int,_ length: Int=4) -> UInt32 {
        bytes[offset..<offset+length].reduce(0) { ($0 << 8) | UInt32($1) }
    }
    private static func digest(_ data: [UInt8]) -> [UInt8] {
        var words=[UInt32](repeating:0,count:14)
        words.append(bigWord(data,0,3)); words.append(bigWord(data,3))
        for i in 16..<30 {
            var value=saltSchedule[i-16]
            if i-2>=14 { value &+= s1(words[i-2]) ^ (i==16 ? 0x000B92C0 : 0) }
            if i-7>=14 { value &+= words[i-7] }
            if i-15>=14 { value &+= s0(words[i-15]) ^ (i==29 ? 0x059C0B80 : 0) }
            words.append(value)
        }
        words.append(s1(words[28]) &+ words[23] &+ s0(words[15]) &+ words[14] &+ 0xC9DC06A7 &- roundConstants[14])
        expand(&words,from:31)
        var state=zip(initial,rounds(saltState,words,from:14)).map { $0 &+ $1 }
        let length=57+data.count
        var padded=Array(data.dropFirst(7))+[0x80]+[UInt8](repeating:0,count:(55-length%64+64)%64)
        let bits=UInt64(length*8)
        padded += (0..<8).reversed().map { UInt8(truncatingIfNeeded:bits >> ($0*8)) }
        for offset in stride(from:0,to:padded.count,by:64) {
            var block=(0..<16).map { bigWord(padded,offset+$0*4) }
            expand(&block)
            state=zip(state,rounds(state,block)).map { $0 &+ $1 }
        }
        return state.flatMap { x in (0..<4).reversed().map { UInt8(truncatingIfNeeded:x >> ($0*8)) } }
    }
    private static func littleWord(_ bytes: [UInt8],_ offset: Int) -> UInt32 {
        (0..<4).reduce(UInt32(0)) { $0 | UInt32(bytes[offset+$1]) << ($1*8) }
    }
    static func generate(at timestamp: Double=Date().timeIntervalSince1970) -> String {
        let epoch=timestamp+11644473600
        let ticks=String(format:"%.0f",floor(epoch/300)*300*10000000)
        let data=Array((ticks+client).utf8),sha=digest(data)
        let keys=[littleWord(sha,0) | 1,littleWord(sha,4) | 1]
        let factors=keys.map { $0 &* 0x82893933 }
        var terms=coefficients.enumerated().map { $0.element.0 &* factors[$0.offset%2] }
        var offsets=coefficients.map { $0.1 }
        var input=[UInt8(0)]+data
        input += [UInt8](repeating:0,count:(8-input.count%8)%8)
        for i in 0..<input.count/4 {
            let word=littleWord(input,i*4)
            var term=word &* keys[i%2],offset=swap(word) &* (i%2==0 ? 0xEF0569FB : 0xC31713DB)
            if i==0 { term &+= 0xF2F90B1A &* factors[0]; offset &+= 0x0B1A0000 }
            terms.append(term); offsets.append(offset)
        }
        var swapped: UInt32=0,last: UInt32=0,state: UInt32=0,acc: UInt32=0
        for i in stride(from:0,to:terms.count,by:2) {
            let q=swap(offsets[i] &+ terms[i]) &* 0x76F50F34
            swapped &+= (swap(q) &* 0xC3EFEA97) &+ q
            let r=swap(offsets[i+1] &+ terms[i+1]) &* 0x1881CE3C
            last=(swap(r) &* 0x35BD1EC9) &+ r; swapped &+= last
            state=state &* keys[0] &+ terms[i]
            for factor: UInt32 in [0xCF98B111,0x87085B9F,0x12CEB96D,0x257E1D83] { state=swap(state) &* factor }
            acc &+= state
            state=state &* keys[1] &+ terms[i+1]
            for factor: UInt32 in [0xA27416F5,0xD38396FF,0x7C932B89,0xBFA49F69] { state=swap(state) &* factor }
            acc &+= state
        }
        let cs=[last ^ state,swapped ^ acc].flatMap { x in (0..<4).map { UInt8(truncatingIfNeeded:x >> ($0*8)) } }
        return cs.map { String(format:"%02X",$0) }.joined()+"-"+sha.map { String(format:"%02X",$0) }.joined()
    }
    static let client="6A5AA1D4EAFF4E9FB37E23D68491D6F4"
    private static let initial: [UInt32] = [0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19]
    private static let saltState: [UInt32] = [0xFD9FBEA2, 0x91243CE7, 0xBE499094, 0xCD2E1E0A, 0x6C006F49, 0x04BD35AB, 0xFEB0A7E4, 0x03EFFBD2]
    private static let saltSchedule: [UInt32] = [0xEB24BF9E, 0x1558691F, 0xA0C09EE2, 0x0CFA32BA, 0xA622EE8C, 0xD1DD9491, 0xC0DD25A0, 0xF9E70AB6, 0xF0C31573, 0x7058CE4E, 0x7188E606, 0x00E5AB33, 0xD258B060, 0x72766564]
    private static let roundConstants: [UInt32] = [0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA, 0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070, 0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2]
    private static let coefficients: [(UInt32,UInt32)] = [(0xE82E91B1,0xB32700D0), (0x44DE24C6,0x88DFF51D), (0x3BEB65BC,0x29514038), (0xC89C5933,0xB328CBDF), (0x8D1EC211,0xD405B4D5), (0x314EF5D5,0x26E55F3C), (0xE92491B1,0x607801C6), (0xF0A5B9D5,0x82B2C1FB), (0x06064CF3,0xFA51A3DA), (0xDA79FA60,0x207CC464), (0xBED20560,0x3C3C74F3), (0x89F9C9F8,0xD2D14B61), (0x35FA38C6,0xBB04A2C1), (0xCB49E3C6,0xC5626167)]
}

@MainActor final class EdgeSpeech {
    private var socket: EdgeSocket?
    func cancel() { socket?.cancel(); socket=nil }
    static func escaped(_ text: String) -> String {
        String(text.unicodeScalars.filter { $0.value>=32 || [9,10,13].contains($0.value) })
            .replacingOccurrences(of:"&",with:"&amp;").replacingOccurrences(of:"<",with:"&lt;")
            .replacingOccurrences(of:">",with:"&gt;").replacingOccurrences(of:"\"",with:"&quot;")
            .replacingOccurrences(of:"'",with:"&apos;")
    }
    static func audio(_ data: Data) throws -> Data {
        guard data.count>=2 else { throw failure("Gói giọng đọc không hợp lệ") }
        let count=Int(data[data.startIndex])*256+Int(data[data.startIndex+1])
        guard count<=data.count-2,let headers=String(data:data.dropFirst(2).prefix(count),encoding:.utf8),
              headers.components(separatedBy:"\r\n").contains("Path:audio") else { throw failure("Gói giọng đọc không hợp lệ") }
        return Data(data.dropFirst(2+count))
    }
    private static func failure(_ text: String) -> NSError { NSError(domain:"HUD.EdgeSpeech",code:1,userInfo:[NSLocalizedDescriptionKey:text]) }
    func synthesize(_ text: String,voice: String) async throws -> Data {
        guard ["vi-VN-HoaiMyNeural","vi-VN-NamMinhNeural"].contains(voice),!text.isEmpty,text.utf8.count<=12000 else { throw Self.failure("Giọng hoặc độ dài câu đọc không hợp lệ") }
        for attempt in 0..<2 {
            do { return try await synthesizeOnce(text,voice:voice) }
            catch {
                try Task.checkCancellation()
                if attempt==1 { throw Self.failure("Không tải được giọng Edge. Vui lòng thử lại hoặc chọn giọng iPhone.") }
                try await Task.sleep(nanoseconds:500_000_000)
            }
        }
        throw CancellationError()
    }
    private func synthesizeOnce(_ text: String,voice: String) async throws -> Data {
        cancel()
        let id=UUID().uuidString.replacingOccurrences(of:"-",with:"")
        let url="wss://speech.platform.bing.com/consumer/speech/synthesize/readaloud/edge/v1?TrustedClientToken=\(EdgeToken.client)&ConnectionId=\(id)&Sec-MS-GEC=\(EdgeToken.generate())&Sec-MS-GEC-Version=2-143.0.3650.75"
        var request=URLRequest(url:URL(string:url)!,timeoutInterval:20)
        request.setValue("chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold",forHTTPHeaderField:"Origin")
        request.setValue("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0",forHTTPHeaderField:"User-Agent")
        request.setValue("no-cache",forHTTPHeaderField:"Cache-Control")
        request.setValue("muid=\(UUID().uuidString.replacingOccurrences(of:"-",with:""));",forHTTPHeaderField:"Cookie")
        let task=EdgeSocket(request:request); socket=task
        let timeout=Task { try await Task.sleep(nanoseconds:45_000_000_000); task.cancel() }
        defer { timeout.cancel(); task.cancel(); if socket===task { socket=nil } }
        return try await withTaskCancellationHandler {
            try await task.start()
            let formatter=DateFormatter(); formatter.locale=Locale(identifier:"en_US_POSIX"); formatter.timeZone=TimeZone(secondsFromGMT:0)
            formatter.dateFormat="EEE MMM dd yyyy HH:mm:ss 'GMT+0000 (Coordinated Universal Time)'"
            let stamp=formatter.string(from:Date())
            try await task.send(.string("X-Timestamp:\(stamp)\r\nContent-Type:application/json; charset=utf-8\r\nPath:speech.config\r\n\r\n{\"context\":{\"synthesis\":{\"audio\":{\"metadataoptions\":{\"sentenceBoundaryEnabled\":\"false\",\"wordBoundaryEnabled\":\"false\"},\"outputFormat\":\"audio-24khz-48kbitrate-mono-mp3\"}}}}\r\n"))
            let ssml="<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='vi-VN'><voice name='\(voice)'><prosody pitch='+0Hz' rate='+0%' volume='+0%'>\(Self.escaped(text))</prosody></voice></speak>"
            try await task.send(.string("X-RequestId:\(id)\r\nContent-Type:application/ssml+xml\r\nX-Timestamp:\(stamp)Z\r\nPath:ssml\r\n\r\n\(ssml)"))
            var result=Data()
            for _ in 0..<2000 {
                try Task.checkCancellation()
                switch try await task.receive() {
                case .data(let bytes):
                    result.append(try Self.audio(bytes))
                    guard result.count<=1024*1024 else { throw Self.failure("Giọng đọc quá dài") }
                case .string(let message):
                    if message.components(separatedBy:"\r\n").contains("Path:turn.end") {
                        guard !result.isEmpty else { throw Self.failure("Edge chưa trả âm thanh") }
                        return result
                    }
                @unknown default: throw Self.failure("Định dạng giọng đọc chưa hỗ trợ")
                }
            }
            throw Self.failure("Quá nhiều gói giọng đọc")
        } onCancel: { Task { @MainActor in task.cancel() } }
    }
}

// TLS + RFC6455 with explicit handshake headers, bounded messages and normal
// certificate verification. Keep this endpoint's connection separate from the
// AI provider and never attach its API key.
@MainActor private final class EdgeSocket {
    private let connection: NWConnection
    private var connecting: CheckedContinuation<Void,Error>?
    private var cancelled=false
    init(request: URLRequest) {
        let options=NWProtocolWebSocket.Options()
        options.autoReplyPing=true; options.maximumMessageSize=256*1024
        options.setAdditionalHeaders((request.allHTTPHeaderFields ?? [:]).map { (name:$0.key,value:$0.value) })
        let tls=NWProtocolTLS.Options()
        sec_protocol_options_set_max_tls_protocol_version(tls.securityProtocolOptions,.TLSv12)
        sec_protocol_options_add_tls_application_protocol(tls.securityProtocolOptions,"http/1.1")
        let parameters=NWParameters(tls:tls,tcp:NWProtocolTCP.Options())
        parameters.defaultProtocolStack.applicationProtocols.insert(options,at:0)
        connection=NWConnection(to:.url(request.url!),using:parameters)
    }
    func start() async throws {
        guard !cancelled else { throw CancellationError() }
        try await withCheckedThrowingContinuation { continuation in
            connecting=continuation
            connection.stateUpdateHandler={ [weak self] state in
                Task { @MainActor in
                    guard let self else { return }
                    switch state {
                    case .ready: self.finishConnect(.success(()))
                    case .failed(let error),.waiting(let error): self.finishConnect(.failure(error)); self.connection.cancel()
                    case .cancelled: self.finishConnect(.failure(CancellationError()))
                    default: break
                    }
                }
            }
            connection.start(queue:.main)
        }
    }
    private func finishConnect(_ result: Result<Void,Error>) {
        let current=connecting; connecting=nil; current?.resume(with:result)
    }
    func cancel() {
        cancelled=true; finishConnect(.failure(CancellationError())); connection.cancel()
    }
    func send(_ message: URLSessionWebSocketTask.Message) async throws {
        guard !cancelled else { throw CancellationError() }
        let bytes: Data,opcode: NWProtocolWebSocket.Opcode
        switch message {
        case .string(let text): bytes=Data(text.utf8); opcode = .text
        case .data(let data): bytes=data; opcode = .binary
        @unknown default: throw CancellationError()
        }
        let metadata=NWProtocolWebSocket.Metadata(opcode:opcode)
        let context=NWConnection.ContentContext(identifier:"edge",metadata:[metadata])
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void,Error>) in
            connection.send(content:bytes,contentContext:context,isComplete:true,completion:.contentProcessed { error in
                if let error { continuation.resume(throwing:error) } else { continuation.resume() }
            })
        }
    }
    func receive() async throws -> URLSessionWebSocketTask.Message {
        guard !cancelled else { throw CancellationError() }
        return try await withCheckedThrowingContinuation { continuation in
            connection.receiveMessage { data,context,complete,error in
                if let error { continuation.resume(throwing:error); return }
                guard complete,let data,let metadata=context?.protocolMetadata(definition:NWProtocolWebSocket.definition) as? NWProtocolWebSocket.Metadata else { continuation.resume(throwing:CancellationError()); return }
                if metadata.opcode == .binary { continuation.resume(returning:.data(data)) }
                else if metadata.opcode == .text,let text=String(data:data,encoding:.utf8) { continuation.resume(returning:.string(text)) }
                else { continuation.resume(throwing:CancellationError()) }
            }
        }
    }
}
