import Foundation
import Security

enum SharedDestinationInbox {
    private static var group: String { Bundle.main.object(forInfoDictionaryKey: "HUDSharedKeychainGroup") as? String ?? "" }
    private static var query: [String: Any] {
        [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: "HUD.sharedDestination",
         kSecAttrAccount as String: "pending", kSecAttrAccessGroup as String: group]
    }
    static func save(_ text: String) -> Bool {
        guard !group.isEmpty, text.utf8.count <= 32768,
              let data = try? JSONSerialization.data(withJSONObject: ["text": text, "date": Date().timeIntervalSince1970]) else { return false }
        let result = SecItemUpdate(query as CFDictionary, [kSecValueData as String: data] as CFDictionary)
        if result == errSecSuccess { return true }
        guard result == errSecItemNotFound else { return false }
        var item = query; item[kSecValueData as String] = data
        item[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        return SecItemAdd(item as CFDictionary, nil) == errSecSuccess
    }
    static func take() -> String? {
        guard !group.isEmpty else { return nil }
        var request = query; request[kSecReturnData as String] = true
        var result: CFTypeRef?
        guard SecItemCopyMatching(request as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data,
              let value = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
        SecItemDelete(query as CFDictionary)
        guard let date = value["date"] as? Double, Date().timeIntervalSince1970 - date < 86400 else { return nil }
        return value["text"] as? String
    }
}
