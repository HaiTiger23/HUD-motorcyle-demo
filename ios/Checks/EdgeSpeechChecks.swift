import Foundation

@main struct EdgeSpeechChecks {
    @MainActor static func main() async throws {
        precondition(EdgeToken.generate(at:1700000000)=="2CD23A2CA0B2595A-F3C846C370E05F9CC83ACC5F995C5F5D87A4D0F14CE8936A0F902E64099E38A2")
        precondition(EdgeToken.generate(at:1793491200)=="7B4CE9D537A0C0C7-F040DBE252E495196E8C0E60525F20AF087248354D0C94DEEBA0E0BEE7D95F56")
        precondition(EdgeToken.generate(at:1700000000)==EdgeToken.generate(at:1700000001))
        precondition(EdgeToken.generate(at:1700000000) != EdgeToken.generate(at:1700000300))
        precondition(EdgeSpeech.escaped("Xin <chào> & 'bạn'\u{01}")=="Xin &lt;chào&gt; &amp; &apos;bạn&apos;")
        let header=Data("Path:audio\r\nContent-Type:audio/mpeg\r\n".utf8)
        let packet=Data([UInt8(header.count>>8),UInt8(header.count&255)])+header+Data([1,2,3])
        let audio=try EdgeSpeech.audio(packet)
        precondition(audio==Data([1,2,3]))
        for invalid in [Data(),Data([0]),Data([255,255,1]),Data([0,1,0])] {
            do { _=try EdgeSpeech.audio(invalid); preconditionFailure("Malformed frame accepted") } catch {}
        }
        let client=EdgeSpeech()
        do { _=try await client.synthesize("test",voice:"invalid"); preconditionFailure("Voice injection accepted") } catch {}
        print("Edge token, escaping and audio framing checks passed")
        if CommandLine.arguments.contains("--online") {
            for voice in ["vi-VN-HoaiMyNeural","vi-VN-NamMinhNeural"] {
                let bytes=try await client.synthesize("Xin chào, tôi là trợ lý HUD của bạn.",voice:voice)
                precondition(bytes.count>1000)
                try bytes.write(to:URL(fileURLWithPath:"/tmp/hud-\(voice).mp3"))
                print("\(voice): synthesized \(bytes.count) bytes")
            }
        }
    }
}
