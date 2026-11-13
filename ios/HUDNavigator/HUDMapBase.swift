import MapKit
import UIKit

/// An image and its projection, shared by the renderer and HUD gesture handling.
struct HUDMapBase: @unchecked Sendable {
    enum Projection {
        case apple(MKMapSnapshotter.Snapshot)
        case raster(origin: CGPoint, scale: Double)
    }
    let image: UIImage
    let projection: Projection
    let distance: Double
    let heading: Double
    let light: Bool
    var attribution: String? = nil

    var isRaster: Bool {
        if case .raster = projection { return true }
        return false
    }

    func point(for coordinate: CLLocationCoordinate2D) -> CGPoint {
        switch projection {
        case .apple(let snapshot):
            return snapshot.point(for: coordinate)
        case .raster(let origin, let scale):
            let point = MKMapPoint(coordinate)
            let world = MKMapSize.world.width * scale
            var x = (point.x * scale - origin.x).truncatingRemainder(dividingBy: world)
            if x < -world / 2 { x += world }
            if x > world / 2 { x -= world }
            return CGPoint(x: x, y: point.y * scale - origin.y)
        }
    }
}

/// Fetch just the raster tiles intersecting the HUD view and its small scroll margin.
/// URLCache honors the tile server's HTTP cache policy across pans, rides and app launches.
actor HUDBackgroundMap {
    static let shared = HUDBackgroundMap()

    private enum Source {
        case standard, france

        func tileURL(zoom: Int, x: Int, y: Int) -> URL {
            let root = self == .standard
                ? "https://tile.openstreetmap.org" : "https://a.tile.openstreetmap.fr/osmfr"
            return URL(string: "\(root)/\(zoom)/\(x)/\(y).png")!
        }

        var attribution: String {
            self == .standard ? "© OpenStreetMap contributors"
                : "© OpenStreetMap contributors · OSM France"
        }
    }

    private var preferFranceUntil = Date.distantPast
    private static let session: URLSession = {
        let config = URLSessionConfiguration.default
        config.urlCache = URLCache(memoryCapacity: 8 << 20, diskCapacity: 48 << 20, diskPath: "HUDStreetTiles")
        config.timeoutIntervalForRequest = 4
        config.timeoutIntervalForResource = 5
        config.httpAdditionalHeaders = ["User-Agent": "HUDConnect/0.1 (https://github.com/HaiHai-17/HUD-motorcyle-demo)"]
        config.httpCookieStorage = nil
        return URLSession(configuration: config)
    }()

    private struct Tile: Sendable {
        let x: Int
        let y: Int
        let data: Data
    }

    func load(center: CLLocationCoordinate2D, distance: Double, size: CGSize, light: Bool) async throws -> HUDMapBase {
        if preferFranceUntil > Date() {
            return try await Self.load(center: center, distance: distance, size: size, light: light, source: .france)
        }
        do {
            return try await Self.load(center: center, distance: distance, size: size, light: light, source: .standard)
        } catch {
            try Task.checkCancellation()
            let base = try await Self.load(center: center, distance: distance, size: size, light: light, source: .france)
            preferFranceUntil = Date().addingTimeInterval(300)
            return base
        }
    }

    private nonisolated static func load(center: CLLocationCoordinate2D, distance: Double,
                                         size: CGSize, light: Bool, source: Source) async throws -> HUDMapBase {
        let latitude = min(85.05, max(-85.05, center.latitude))
        let point = MKMapPoint(CLLocationCoordinate2D(latitude: latitude, longitude: center.longitude))
        let scale = Double(HUDWire.imageWidth) / (distance * MKMapPointsPerMeterAtLatitude(latitude))
        let world = MKMapSize.world.width
        let zoom = min(19, max(0, Int(log2(scale * world / 256).rounded())))
        let tileCount = 1 << zoom
        let tileSize = world * scale / Double(tileCount)
        let origin = CGPoint(x: point.x * scale - size.width / 2, y: point.y * scale - size.height / 2)
        let firstX = Int(floor(origin.x / tileSize))
        let lastX = Int(ceil((origin.x + size.width) / tileSize))
        let firstY = max(0, Int(floor(origin.y / tileSize)))
        let lastY = min(tileCount, Int(ceil((origin.y + size.height) / tileSize)))

        let tiles = try await withThrowingTaskGroup(of: Tile.self) { group in
            for y in firstY..<lastY {
                for x in firstX..<lastX {
                    let wrappedX = (x % tileCount + tileCount) % tileCount
                    let url = source.tileURL(zoom: zoom, x: wrappedX, y: y)
                    group.addTask {
                        let (data, response) = try await session.data(from: url)
                        guard (response as? HTTPURLResponse)?.statusCode == 200,
                              response.mimeType == "image/png", data.count <= 1_048_576 else {
                            throw URLError(.badServerResponse)
                        }
                        return Tile(x: x, y: y, data: data)
                    }
                }
            }
            var tiles: [Tile] = []
            for try await tile in group { tiles.append(tile) }
            return tiles
        }
        try Task.checkCancellation()
        guard !tiles.isEmpty else { throw URLError(.cannotDecodeContentData) }
        let images = try tiles.map { tile in
            guard let image = UIImage(data: tile.data), image.size == CGSize(width: 256, height: 256) else {
                throw URLError(.cannotDecodeContentData)
            }
            return (tile, image)
        }
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true
        let image = UIGraphicsImageRenderer(size: size, format: format).image { renderer in
            UIColor(white: 0.94, alpha: 1).setFill()
            renderer.fill(CGRect(origin: .zero, size: size))
            for (tile, image) in images {
                let x = (Double(tile.x) * tileSize - origin.x).rounded()
                let y = (Double(tile.y) * tileSize - origin.y).rounded()
                let right = (Double(tile.x + 1) * tileSize - origin.x).rounded()
                let bottom = (Double(tile.y + 1) * tileSize - origin.y).rounded()
                image.draw(in: CGRect(x: x, y: y, width: right - x, height: bottom - y))
            }
            if !light {
                renderer.cgContext.setBlendMode(.difference)
                UIColor.white.setFill()
                renderer.fill(CGRect(origin: .zero, size: size))
                renderer.cgContext.setBlendMode(.screen)
                UIColor(red: 0.06, green: 0.09, blue: 0.14, alpha: 1).setFill()
                renderer.fill(CGRect(origin: .zero, size: size))
            }
        }
        return HUDMapBase(image: image, projection: .raster(origin: origin, scale: scale),
                          distance: distance, heading: 0, light: light, attribution: source.attribution)
    }
}
