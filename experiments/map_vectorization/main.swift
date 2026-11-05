// Offline experiment, deliberately separate from the navigation app/firmware.
// ML: Apple Vision OCR. Geometry: contour tracing + Ramer-Douglas-Peucker.
// Output is display artwork, never a routing graph or a source of maneuvers.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
import Vision

struct Outline {
    let points: [CGPoint]
    let score: Double
}
func bitmap(_ width: Int, _ height: Int) -> CGContext {
    CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4,
        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
}
func save(_ image: CGImage, _ path: URL) {
    let destination = CGImageDestinationCreateWithURL(path as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(destination, image, nil)
    precondition(CGImageDestinationFinalize(destination))
}
func milliseconds(since start: UInt64) -> Double { Double(DispatchTime.now().uptimeNanoseconds - start) / 1_000_000 }
guard CommandLine.arguments.count == 7 else {
    fputs("Usage: map-vector-probe image output-directory crop-x crop-y crop-width crop-height\n", stderr)
    exit(2)
}
let sourceURL = URL(fileURLWithPath: CommandLine.arguments[1])
let output = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
let cropValues = CommandLine.arguments[3...6].compactMap(Double.init)
guard cropValues.count == 4, cropValues.allSatisfy({ $0.isFinite }), cropValues[0] >= 0,
      cropValues[1] >= 0, cropValues[2] > 0, cropValues[3] > 0,
      let source = CGImageSourceCreateWithURL(sourceURL as CFURL, nil),
      let original = CGImageSourceCreateImageAtIndex(source, 0, nil),
      let crop = original.cropping(to: CGRect(x: cropValues[0], y: cropValues[1], width: cropValues[2], height: cropValues[3])) else {
    fputs("Cannot read image or crop\n", stderr); exit(2)
}
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let width = 300, height = 200
let frame = CGRect(x: 0, y: 0, width: width, height: height)
let resized = bitmap(width, height)
resized.interpolationQuality = .high
resized.draw(crop, in: frame)
let input = resized.makeImage()!
save(input, output.appendingPathComponent("input.png"))

// Recognize at the source resolution, before shrinking to the HUD dimensions.
// We use only text bounding boxes; recognized strings are never logged/saved.
let started = DispatchTime.now().uptimeNanoseconds
let ocr = VNRecognizeTextRequest()
ocr.recognitionLevel = .accurate
ocr.usesLanguageCorrection = false
ocr.automaticallyDetectsLanguage = true
try VNImageRequestHandler(cgImage: crop).perform([ocr])
let ocrMs = milliseconds(since: started)

// Estimate a background color from the map interior, excluding the screenshot's
// existing guidance banner and attribution. This is only a display heuristic.
let pixels = resized.data!.assumingMemoryBound(to: UInt8.self)
var histogram = [Int](repeating: 0, count: 4096)
for y in 55..<175 {
    for x in 5..<295 {
        let offset = (y * width + x) * 4
        let key = Int(pixels[offset] >> 4) << 8 | Int(pixels[offset + 1] >> 4) << 4 | Int(pixels[offset + 2] >> 4)
        histogram[key] += 1
    }
}
let dominant = histogram.indices.max(by: { histogram[$0] < histogram[$1] })!
let background = CGColor(red: CGFloat((dominant >> 8) * 16 + 8) / 255,
    green: CGFloat(((dominant >> 4) & 15) * 16 + 8) / 255,
    blue: CGFloat((dominant & 15) * 16 + 8) / 255, alpha: 1)
let cleaned = bitmap(width, height)
cleaned.draw(input, in: frame)
cleaned.setFillColor(background)
// Screenshot-only UI regions; an actual MKMapSnapshotter image has no banner.
cleaned.fill(CGRect(x: 0, y: 150, width: 300, height: 50))
cleaned.fill(CGRect(x: 0, y: 0, width: 300, height: 20))
var maskedTextRegions = 0
for text in ocr.results ?? [] {
    let bounds = text.boundingBox
    let rectangle = CGRect(x: bounds.minX * 300, y: bounds.minY * 200,
        width: bounds.width * 300, height: bounds.height * 200).insetBy(dx: -1, dy: -1)
    guard rectangle.minY > 20, rectangle.maxY < 150 else { continue }
    cleaned.fill(rectangle); maskedTextRegions += 1
}
save(cleaned.makeImage()!, output.appendingPathComponent("ocr-masked.png"))

// Contrast experiment: isolate near-neutral strokes from the dominant map
// color. This is a palette heuristic, NOT an ML road-segmentation model.
let mask = bitmap(width, height)
let maskPixels = mask.data!.assumingMemoryBound(to: UInt8.self)
let cleanPixels = cleaned.data!.assumingMemoryBound(to: UInt8.self)
let backgroundComponents = background.components!
let backgroundLuma = 0.2126 * backgroundComponents[0] + 0.7152 * backgroundComponents[1] + 0.0722 * backgroundComponents[2]
for index in 0..<(width * height) {
    let offset = index * 4
    let r = Double(cleanPixels[offset]) / 255, g = Double(cleanPixels[offset + 1]) / 255, b = Double(cleanPixels[offset + 2]) / 255
    let luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    let nearNeutral = max(r, max(g, b)) - min(r, min(g, b)) < 0.22
    let contrasting = backgroundLuma < 0.5 ? luma > backgroundLuma + 0.018 : luma < backgroundLuma - 0.04
    let value: UInt8 = nearNeutral && contrasting ? 255 : 0
    maskPixels[offset] = value; maskPixels[offset + 1] = value; maskPixels[offset + 2] = value; maskPixels[offset + 3] = 255
}
save(mask.makeImage()!, output.appendingPathComponent("contrast-mask.png"))
let contourStart = DispatchTime.now().uptimeNanoseconds
let request = VNDetectContoursRequest()
request.maximumImageDimension = 300
request.detectsDarkOnLight = false
request.contrastAdjustment = 1.5
try VNImageRequestHandler(cgImage: mask.makeImage()!).perform([request])
guard let observation = request.results?.first else { fatalError("No contour observation") }
var candidates: [Outline] = []
for index in 0..<min(observation.contourCount, 4096) {
    let contour = try observation.contour(at: index)
    let simplified = try contour.polygonApproximation(epsilon: 0.003)
    let points = simplified.normalizedPoints.map { CGPoint(x: CGFloat($0.x) * 300, y: CGFloat($0.y) * 200) }
    guard points.count >= 3, points.count <= 768 else { continue }
    let bounds = contour.normalizedPath.boundingBoxOfPath
    let length = zip(points, points.dropFirst() + points.prefix(1)).reduce(0.0) {
        $0 + hypot($1.0.x - $1.1.x, $1.0.y - $1.1.y)
    }
    guard length > 15, !(bounds.width > 0.98 && bounds.height > 0.98) else { continue }
    candidates.append(Outline(points: points, score: length))
}
// Hard limits describe an ESP32 scene budget, not a claim of complete coverage.
var retained: [Outline] = [], pointCount = 0
for candidate in candidates.sorted(by: { $0.score > $1.score }) {
    guard retained.count < 64, pointCount + candidate.points.count <= 768 else { continue }
    retained.append(candidate); pointCount += candidate.points.count
}
let contourMs = milliseconds(since: contourStart)
let vector = bitmap(width, height)
vector.setFillColor(background); vector.fill(frame)
vector.setStrokeColor(CGColor(gray: 0.9, alpha: 1)); vector.setLineWidth(1)
vector.setLineJoin(.round); vector.setLineCap(.round)
var binary = Data("HVM1".utf8)
func word(_ number: Int) { binary.append(UInt8(truncatingIfNeeded: number)); binary.append(UInt8(truncatingIfNeeded: number >> 8)) }
word(width); word(height); word(retained.count)
for outline in retained {
    word(outline.points.count)
    for (index, point) in outline.points.enumerated() {
        if index == 0 { vector.move(to: point) } else { vector.addLine(to: point) }
        word(Int(point.x.rounded())); word(Int(point.y.rounded()))
    }
    vector.closePath(); vector.strokePath()
}
let rendered = vector.makeImage()!
save(rendered, output.appendingPathComponent("vector.png"))
try binary.write(to: output.appendingPathComponent("scene.bin"))
let comparison = bitmap(width * 2, height)
comparison.draw(input, in: frame)
comparison.draw(rendered, in: CGRect(x: width, y: 0, width: width, height: height))
save(comparison.makeImage()!, output.appendingPathComponent("comparison.png"))
let jpegData = NSMutableData()
let jpeg = CGImageDestinationCreateWithData(jpegData, UTType.jpeg.identifier as CFString, 1, nil)!
CGImageDestinationAddImage(jpeg, input, [kCGImageDestinationLossyCompressionQuality: 0.55] as CFDictionary)
precondition(CGImageDestinationFinalize(jpeg))
let metrics: [String: Any] = ["measuredOn": "Mac, not iPhone", "width": width, "height": height,
    "ocrMs": ocrMs, "contourMs": contourMs, "processingMs": milliseconds(since: started),
    "maskedTextRegions": maskedTextRegions, "candidateContours": candidates.count,
    "retainedContours": retained.count, "retainedPoints": pointCount,
    "geometryBytes": binary.count, "comparisonJpegBytes": jpegData.length,
    "isRoutingData": false, "isRoadSegmentationModel": false,
    "note": "OCR is ML; contour tracing is not ML. Masking can cut roads and retained outlines can include non-road objects."]
let report = try JSONSerialization.data(withJSONObject: metrics, options: [.prettyPrinted, .sortedKeys])
try report.write(to: output.appendingPathComponent("metrics.json"))
print(String(data: report, encoding: .utf8)!)
