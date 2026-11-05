import Foundation

public struct SharedMapPlace {
    public let latitude: Double?
    public let longitude: Double?
    public let query: String
}

public enum MapShare {
    public static func firstURL(in text: String) -> URL? {
        guard text.utf8.count <= 32768,
              let expression = try? NSRegularExpression(pattern: "https?://[^\\s<>]+"),
              let match = expression.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)),
              let range = Range(match.range, in: text) else { return nil }
        return URL(string: String(text[range]))
    }
    public static func allowed(_ url: URL) -> Bool {
        guard url.scheme == "https" || url.scheme == "http", let host = url.host?.lowercased(), url.user == nil else { return false }
        return host == "maps.apple.com" || host == "maps.app.goo.gl" || host == "goo.gl" && url.path.hasPrefix("/maps") ||
            host == "maps.google.com" || (host == "google.com" || host.hasSuffix(".google.com")) && url.path.hasPrefix("/maps")
    }
    public static func place(from url: URL) -> SharedMapPlace? {
        guard allowed(url), let components = URLComponents(url: url, resolvingAgainstBaseURL: false) else { return nil }
        let items = components.queryItems ?? []
        func value(_ name: String) -> String? { items.first { $0.name == name }?.value }
        func coordinate(_ text: String) -> (Double, Double)? {
            let values = text.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
            guard values.count == 2, let lat = Double(values[0]), let lon = Double(values[1]),
                  lat.isFinite, lon.isFinite, abs(lat) <= 90, abs(lon) <= 180 else { return nil }
            return (lat, lon)
        }
        let apple = url.host?.lowercased() == "maps.apple.com"
        let name = value("name") ?? value("q") ?? value("query") ?? value("address") ?? "Điểm đã chia sẻ"
        let keys = apple ? ["coordinate", "daddr", "ll"] : ["destination", "daddr", "query", "q"]
        for key in keys {
            if let raw = value(key), let (lat, lon) = coordinate(raw) {
                return SharedMapPlace(latitude: lat, longitude: lon, query: name)
            }
        }
        // Google's /@lat,lon is a camera centre; only !3d/!4d identifies a place here.
        let decoded = url.absoluteString.removingPercentEncoding ?? url.absoluteString
        if !apple, let regex = try? NSRegularExpression(pattern: "!3d(-?[0-9.]+)!4d(-?[0-9.]+)"),
           let match = regex.matches(in: decoded, range: NSRange(decoded.startIndex..., in: decoded)).last,
           let latRange = Range(match.range(at: 1), in: decoded), let lonRange = Range(match.range(at: 2), in: decoded),
           let pair = coordinate("\(decoded[latRange]),\(decoded[lonRange])") {
            let parts = components.path.components(separatedBy: "/place/")
            let label = parts.count > 1 ? String(parts[1].split(separator: "/").first ?? "").replacingOccurrences(of: "+", with: " ") : name
            return SharedMapPlace(latitude: pair.0, longitude: pair.1, query: label)
        }
        for key in ["destination", "daddr", "query", "q", "address", "name"] {
            if let text = value(key), !text.isEmpty { return SharedMapPlace(latitude: nil, longitude: nil, query: text) }
        }
        if components.path.contains("/place/") {
            let text = String(components.path.components(separatedBy: "/place/")[1].split(separator: "/").first ?? "")
                .replacingOccurrences(of: "+", with: " ")
            if !text.isEmpty { return SharedMapPlace(latitude: nil, longitude: nil, query: text) }
        }
        return nil
    }
}
