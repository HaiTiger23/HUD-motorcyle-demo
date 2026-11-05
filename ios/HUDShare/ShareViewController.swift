import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {
    private let message = UILabel()
    private let send = UIButton(type: .system)
    private var sharedText: String?
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        let title = UILabel(); title.text = "Gửi điểm đến tới HUD"; title.font = .preferredFont(forTextStyle: .title2)
        message.text = "Đang đọc địa điểm…"; message.numberOfLines = 5
        message.font = .preferredFont(forTextStyle: .body)
        send.setTitle("Gửi điểm đến", for: .normal); send.isEnabled = false
        send.configuration = .filled(); send.addTarget(self, action: #selector(save), for: .touchUpInside)
        let close = UIButton(type: .system); close.setTitle("Đóng", for: .normal)
        close.addTarget(self, action: #selector(done), for: .touchUpInside)
        let stack = UIStackView(arrangedSubviews: [title, message, send, close]); stack.axis = .vertical; stack.spacing = 24
        stack.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 30)])
        let providers = (extensionContext?.inputItems as? [NSExtensionItem] ?? []).flatMap { $0.attachments ?? [] }
        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.url.identifier) }) {
            provider.loadItem(forTypeIdentifier: UTType.url.identifier, options: nil) { [weak self] value, _ in
                let text = (value as? URL)?.absoluteString ?? (value as? String)
                DispatchQueue.main.async { self?.loaded(text) }
            }
        } else if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) }) {
            provider.loadItem(forTypeIdentifier: UTType.plainText.identifier, options: nil) { [weak self] value, _ in
                DispatchQueue.main.async { self?.loaded(value as? String) }
            }
        } else { loaded(nil) }
    }
    private func loaded(_ text: String?) {
        guard let text, text.utf8.count <= 32768, !text.isEmpty else {
            message.text = "Chưa đọc được địa điểm. Hãy chia sẻ đường dẫn địa điểm từ Google Maps hoặc Apple Maps."; return
        }
        sharedText = text; message.text = text; send.isEnabled = true
    }
    @objc private func save() {
        guard let sharedText else { return }
        if SharedDestinationInbox.save(sharedText) {
            message.text = "Đã gửi. Bạn mở HUD Connect để xem điểm đến và chọn tuyến."
            send.isHidden = true
        } else { message.text = "Chưa lưu được điểm đến. Hãy thử lại sau khi mở HUD Connect một lần." }
    }
    @objc private func done() { extensionContext?.completeRequest(returningItems: nil) }
}
