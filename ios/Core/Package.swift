// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "HUDNavCore", products: [.library(name: "HUDNavCore", targets: ["HUDNavCore"])],
    targets: [.target(name: "HUDNavCore"), .executableTarget(name: "HUDCoreChecks", dependencies: ["HUDNavCore"], path: "Checks")])
