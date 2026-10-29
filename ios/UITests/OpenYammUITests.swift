import XCTest
import Vision
import ImageIO

// Runs against the already installed game. Rendering has no accessibility tree,
// so scene preconditions use screenshot OCR and gameplay results use device logs/saves.
@MainActor
final class OpenYammUITests: XCTestCase {
    private struct Step: Decodable {
        let op: String
        let name: String?
        let x: Double?
        let y: Double?
        let toX: Double?
        let toY: Double?
        let text: String?
        let timeout: Double?
        let duration: Double?
        let hold: Double?
    }

    private enum PlanError: Error {
        case invalid(String)
    }

    private func validate(_ steps: [Step]) throws {
        guard !steps.isEmpty else { throw PlanError.invalid("Empty UI plan") }
        for step in steps {
            guard ["capture", "tap", "tapButton", "tapKey", "tapCandidate", "doubleTap", "drag",
                   "text", "key", "waitText", "background"].contains(step.op) else {
                throw PlanError.invalid("Unknown operation: \(step.op)")
            }
            if ["tap", "doubleTap", "drag"].contains(step.op) {
                guard let x = step.x, let y = step.y, (0...1).contains(x), (0...1).contains(y) else {
                    throw PlanError.invalid("Tap coordinates must be normalized to the app frame")
                }
            }
            if step.op == "drag" {
                guard let x = step.toX, let y = step.toY, (0...1).contains(x), (0...1).contains(y) else {
                    throw PlanError.invalid("Drag destination is missing or out of range")
                }
            }
            if ["text", "key", "waitText", "tapButton", "tapKey", "tapCandidate"].contains(step.op),
               step.text?.isEmpty != false {
                throw PlanError.invalid("Operation requires nonempty text")
            }
            let maximumDuration = step.op == "background" ? 60.0 : 5.0
            guard (0...30).contains(step.timeout ?? 10),
                  (0...maximumDuration).contains(step.duration ?? 0.1),
                  (0...30).contains(step.hold ?? 0) else {
                throw PlanError.invalid("Timeout or gesture duration is out of range")
            }
        }
    }

    private func attach(_ screenshot: XCUIScreenshot, name: String) {
        let attachment = XCTAttachment(screenshot: screenshot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
    }

    private func recognizedText(_ screenshot: XCUIScreenshot) throws -> String {
        let request = VNRecognizeTextRequest()
        request.recognitionLevel = .accurate
        request.recognitionLanguages = ["zh-Hans", "en-US"]
        request.usesLanguageCorrection = false
        guard let bitmap = screenshot.image.cgImage else {
            throw PlanError.invalid("Screenshot has no bitmap")
        }
        let orientation: CGImagePropertyOrientation
        switch screenshot.image.imageOrientation {
        case .up: orientation = .up
        case .down: orientation = .down
        case .left: orientation = .left
        case .right: orientation = .right
        case .upMirrored: orientation = .upMirrored
        case .downMirrored: orientation = .downMirrored
        case .leftMirrored: orientation = .leftMirrored
        case .rightMirrored: orientation = .rightMirrored
        @unknown default: throw PlanError.invalid("Unknown screenshot orientation")
        }
        // iPad captures can store a portrait bitmap with landscape orientation.
        try VNImageRequestHandler(cgImage: bitmap, orientation: orientation).perform([request])
        return (request.results ?? []).compactMap { $0.topCandidates(1).first?.string }.joined(separator: "\n")
    }

    private func waitForText(_ expected: String, app: XCUIApplication, timeout: Double) throws {
        let deadline = Date().addingTimeInterval(timeout)
        var recognized = ""
        repeat {
            let screenshot = XCUIScreen.main.screenshot()
            recognized = try recognizedText(screenshot)
            let compact = recognized.components(separatedBy: .whitespacesAndNewlines).joined()
            let wanted = expected.components(separatedBy: .whitespacesAndNewlines).joined()
            if compact.contains(wanted) {
                let attachment = XCTAttachment(string: recognized)
                attachment.name = "OCR: \(expected)"
                attachment.lifetime = .keepAlways
                add(attachment)
                return
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.3))
        } while Date() < deadline
        attach(XCUIScreen.main.screenshot(), name: "Missing expected text: \(expected)")
        XCTFail("Expected screenshot text '\(expected)'; recognized: \(recognized)")
        throw PlanError.invalid("Scene precondition failed; remaining actions were not executed")
    }

    func testDevicePlan() throws {
        continueAfterFailure = false
        guard let json = ProcessInfo.processInfo.environment["OPENYAMM_UI_STEPS"],
              let data = json.data(using: .utf8) else {
            throw PlanError.invalid("Set OPENYAMM_UI_STEPS in the test runner environment")
        }
        let steps = try JSONDecoder().decode([Step].self, from: data)
        try validate(steps)

        let app = XCUIApplication(bundleIdentifier: "org.openyamm.ipados")
        guard app.state != .notRunning else {
            throw PlanError.invalid("Start the game first; the UI plan does not reset or launch a new game")
        }
        app.activate()
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: 15))
        let geometry = XCTAttachment(string: "Screen image: \(XCUIScreen.main.screenshot().image.size), "
            + "scale: \(XCUIScreen.main.screenshot().image.scale), "
            + "orientation: \(XCUIScreen.main.screenshot().image.imageOrientation.rawValue)\n\(app.debugDescription)")
        geometry.name = "Application geometry and accessibility"
        geometry.lifetime = .keepAlways
        add(geometry)
        for (index, step) in steps.enumerated() {
            let name = step.name ?? "\(index)-\(step.op)"
            try XCTContext.runActivity(named: name) { _ in
                XCTAssertEqual(app.state, .runningForeground)
                switch step.op {
                case "capture":
                    break
                case "background":
                    XCUIDevice.shared.press(.home)
                    XCTAssertTrue(app.wait(for: .runningBackground, timeout: step.timeout ?? 10))
                    attach(XCUIScreen.main.screenshot(), name: "\(name)-background")
                    RunLoop.current.run(until: Date().addingTimeInterval(step.duration ?? 2))
                    app.activate()
                    XCTAssertTrue(app.wait(for: .runningForeground, timeout: step.timeout ?? 10))
                case "waitText":
                    try waitForText(step.text!, app: app, timeout: step.timeout ?? 10)
                case "tapButton":
                    let button = app.buttons[step.text!]
                    XCTAssertTrue(button.waitForExistence(timeout: step.timeout ?? 10))
                    XCTAssertTrue(button.isEnabled)
                    XCTAssertTrue(button.isHittable)
                    button.tap()
                case "tapKey", "tapCandidate":
                    // Tap the real software keyboard/IME. SDL's hidden UITextField
                    // is not exposed as a focused accessibility text element.
                    let element = step.op == "tapKey"
                        ? app.keyboards.keys[step.text!] : app.cells[step.text!]
                    XCTAssertTrue(element.waitForExistence(timeout: step.timeout ?? 10))
                    XCTAssertTrue(element.isHittable)
                    element.tap()
                case "tap", "doubleTap", "drag":
                    // The application proxy can report an uncropped portrait frame
                    // on iPad. The main UIWindow has the actual scene geometry.
                    let window = app.windows.firstMatch
                    XCTAssertTrue(window.exists)
                    let start = window.coordinate(withNormalizedOffset: CGVector(dx: step.x!, dy: step.y!))
                    if step.op == "tap" {
                        start.tap()
                    } else if step.op == "doubleTap" {
                        start.doubleTap()
                    } else {
                        let end = window.coordinate(withNormalizedOffset: CGVector(dx: step.toX!, dy: step.toY!))
                        start.press(forDuration: step.duration ?? 0.1, thenDragTo: end,
                                    withVelocity: .default, thenHoldForDuration: step.hold ?? 0)
                    }
                case "text":
                    app.typeText(step.text!)
                case "key":
                    let special: [String: XCUIKeyboardKey] = [
                        "return": .return, "escape": .escape, "space": .space,
                        "tab": .tab, "delete": .delete, "up": .upArrow, "down": .downArrow,
                        "left": .leftArrow, "right": .rightArrow
                    ]
                    app.typeKey(special[step.text!] ?? XCUIKeyboardKey(rawValue: step.text!), modifierFlags: [])
                default:
                    throw PlanError.invalid(step.op)
                }
                attach(XCUIScreen.main.screenshot(), name: name)
            }
        }
    }
}
