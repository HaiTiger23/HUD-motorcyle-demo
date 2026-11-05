import ReplayKit
import CoreImage
import UIKit

// System broadcast UI provides consent. No audio, backend or App Group is used.
final class SampleHandler: RPBroadcastSampleHandler {
    private let context = CIContext(options: [.cacheIntermediates: false])
    private var lastFrame: CFTimeInterval = 0
    private var link: BluetoothLink?
    override func broadcastStarted(withSetupInfo setupInfo: [String: NSObject]?) {
        Task { @MainActor [weak self] in self?.link = BluetoothLink() }
    }
    override func broadcastFinished() {
        Task { @MainActor [weak self] in self?.link?.stopNavigation(); self?.link = nil }
    }
    override func processSampleBuffer(_ sampleBuffer: CMSampleBuffer, with sampleBufferType: RPSampleBufferType) {
        guard sampleBufferType == .video else { return }
        let now = CACurrentMediaTime()
        guard now - lastFrame >= 1, let buffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        lastFrame = now
        // Scale before allocating a CGImage to stay within the extension memory budget.
        var source = CIImage(cvPixelBuffer: buffer)
        if let orientation = CMGetAttachment(sampleBuffer, key: RPVideoSampleOrientationKey as CFString,
                                             attachmentModeOut: nil) as? NSNumber {
            source = source.oriented(forExifOrientation: orientation.int32Value)
        }
        let bounds = source.extent
        let scale = min(300 / bounds.width, 150 / bounds.height)
        source = source.transformed(by: CGAffineTransform(translationX: -bounds.minX, y: -bounds.minY))
            .transformed(by: CGAffineTransform(scaleX: scale, y: scale))
        guard let cg = context.createCGImage(source, from: source.extent) else { return }
        let format = UIGraphicsImageRendererFormat(); format.scale = 1; format.opaque = true
        let image = UIGraphicsImageRenderer(size: CGSize(width: 300, height: 150), format: format).image { canvas in
            UIColor.black.setFill(); canvas.fill(CGRect(x: 0, y: 0, width: 300, height: 150))
            let size = CGSize(width: CGFloat(cg.width), height: CGFloat(cg.height))
            UIImage(cgImage: cg).draw(in: CGRect(x: (300-size.width)/2, y: (150-size.height)/2,
                                                width: size.width, height: size.height))
        }
        let jpeg = image.jpegData(compressionQuality: 0.65)
        Task { @MainActor [weak self] in
            if let jpeg, let link = self?.link, link.ready, !link.busy { link.send(jpeg: jpeg) }
        }
    }
}
