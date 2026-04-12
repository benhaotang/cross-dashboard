import SwiftUI
import LocalAuthentication
import CryptoKit
import CrossDashboardKit

/// Full-screen lock screen shown when `AppViewModel.isLocked == true`.
/// Attempts Touch ID on appear; shows a 6-digit PIN numpad as fallback.
/// PIN is compared to a SHA-256 hash stored in KeychainStore under `biometric_pin_hash`.
/// Mirrors BiometricLockScreen on Android.
struct BiometricLockView: View {

    @Environment(AppViewModel.self) private var appViewModel

    @State private var pinDigits: [Int] = []
    @State private var showPinPad = false
    @State private var errorMessage: String? = nil
    @State private var shakeOffset: CGFloat = 0
    @State private var isEvaluating = false

    private let maxDigits = 6
    private let pinHashKey = "biometric_pin_hash"

    var body: some View {
        ZStack {
            Color(.windowBackgroundColor).ignoresSafeArea()

            VStack(spacing: 32) {
                Spacer()

                // Lock icon + app name
                VStack(spacing: 12) {
                    Image(systemName: "lock.fill")
                        .font(.system(size: 52, weight: .light))
                        .foregroundStyle(.secondary)
                    Text("CrossDashboard")
                        .font(.title2.weight(.semibold))
                    Text("Authenticate to continue")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }

                if showPinPad {
                    pinPadSection
                        .offset(x: shakeOffset)
                } else {
                    biometricButton
                }

                Spacer()

                if showPinPad {
                    Button("Use Touch ID") {
                        showPinPad = false
                        evaluateBiometrics()
                    }
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(.bottom, 20)
                } else {
                    Button("Use PIN instead") {
                        showPinPad = true
                    }
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(.bottom, 20)
                }
            }
        }
        .onAppear {
            evaluateBiometrics()
        }
        .accessibilityLabel("App locked. Authenticate to unlock.")
    }

    // ─── Biometric button ─────────────────────────────────────────────────────

    private var biometricButton: some View {
        VStack(spacing: 12) {
            Button {
                evaluateBiometrics()
            } label: {
                Label("Unlock with Touch ID", systemImage: "touchid")
                    .padding(.horizontal, 24)
                    .padding(.vertical, 12)
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .disabled(isEvaluating)
            .accessibilityLabel("Unlock with Touch ID")
            .accessibilityHint("Authenticates using your fingerprint")

            if let msg = errorMessage {
                Text(msg)
                    .font(.caption)
                    .foregroundStyle(.red)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal)
                    .accessibilityLabel("Authentication error: \(msg)")
            }
        }
    }

    // ─── PIN pad ──────────────────────────────────────────────────────────────

    private var pinPadSection: some View {
        VStack(spacing: 20) {
            // PIN dots
            HStack(spacing: 16) {
                ForEach(0..<maxDigits, id: \.self) { i in
                    Circle()
                        .fill(i < pinDigits.count ? Color.primary : Color(.separatorColor))
                        .frame(width: 14, height: 14)
                }
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("PIN entry: \(pinDigits.count) of \(maxDigits) digits entered")

            if let msg = errorMessage {
                Text(msg)
                    .font(.caption)
                    .foregroundStyle(.red)
                    .accessibilityLabel("Error: \(msg)")
            }

            // Number grid
            LazyVGrid(columns: Array(repeating: GridItem(.fixed(72)), count: 3), spacing: 12) {
                ForEach(1...9, id: \.self) { digit in
                    PinButton(digit: digit) { appendDigit(digit) }
                }
                // Bottom row: clear, 0, delete
                PinButton(systemImage: "delete.left", label: "Delete") {
                    if !pinDigits.isEmpty { pinDigits.removeLast() }
                    errorMessage = nil
                }
                PinButton(digit: 0) { appendDigit(0) }
                PinButton(systemImage: "checkmark", label: "Confirm", isPrimary: true) {
                    submitPin()
                }
                .disabled(pinDigits.count < maxDigits)
            }
            .frame(maxWidth: 260)
        }
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    private func appendDigit(_ digit: Int) {
        guard pinDigits.count < maxDigits else { return }
        errorMessage = nil
        pinDigits.append(digit)
        if pinDigits.count == maxDigits {
            submitPin()
        }
    }

    private func submitPin() {
        let enteredPin = pinDigits.map(String.init).joined()
        let enteredHash = sha256(enteredPin)
        let storedHash = KeychainStore.shared.get(pinHashKey) ?? ""

        if storedHash.isEmpty {
            // No PIN stored yet — this is first-time setup; set the PIN and unlock
            KeychainStore.shared.set(pinHashKey, value: enteredHash)
            appViewModel.unlock()
        } else if enteredHash == storedHash {
            appViewModel.unlock()
        } else {
            errorMessage = "Incorrect PIN. Try again."
            pinDigits = []
            triggerShake()
        }
    }

    private func evaluateBiometrics() {
        guard !isEvaluating else { return }
        let ctx = LAContext()
        var error: NSError?
        guard ctx.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error) else {
            errorMessage = error?.localizedDescription ?? "Touch ID not available."
            showPinPad = true
            return
        }
        isEvaluating = true
        ctx.evaluatePolicy(
            .deviceOwnerAuthenticationWithBiometrics,
            localizedReason: "Unlock CrossDashboard"
        ) { success, evaluationError in
            DispatchQueue.main.async {
                isEvaluating = false
                if success {
                    appViewModel.unlock()
                } else {
                    errorMessage = evaluationError?.localizedDescription ?? "Authentication failed."
                    showPinPad = true
                }
            }
        }
    }

    private func sha256(_ input: String) -> String {
        let digest = SHA256.hash(data: Data(input.utf8))
        return digest.map { String(format: "%02x", $0) }.joined()
    }

    private func triggerShake() {
        withAnimation(.spring(response: 0.3, dampingFraction: 0.3)) {
            shakeOffset = 12
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
            withAnimation(.spring(response: 0.3, dampingFraction: 0.3)) {
                shakeOffset = 0
            }
        }
    }
}

// ─── PIN Button ───────────────────────────────────────────────────────────────

private struct PinButton: View {
    var digit: Int? = nil
    var systemImage: String? = nil
    var label: String = ""
    var isPrimary = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Group {
                if let digit {
                    Text(String(digit))
                        .font(.title2.monospacedDigit())
                } else if let icon = systemImage {
                    Image(systemName: icon)
                        .font(.body.weight(.medium))
                }
            }
            .frame(width: 72, height: 44)
            .background(
                RoundedRectangle(cornerRadius: 8)
                    .fill(isPrimary ? Color.accentColor : Color(.controlBackgroundColor))
            )
            .foregroundStyle(isPrimary ? .white : .primary)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(digit.map(String.init) ?? label)
    }
}
