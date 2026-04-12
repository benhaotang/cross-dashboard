import SwiftUI
import AppKit
import CrossDashboardKit

// ─── PomodoroBarView ──────────────────────────────────────────────────────────
// Shown inside the NSPopover attached to the menu bar status item.
// Also used as the floating overlay panel at the bottom-right of the main window.

struct PomodoroBarView: View {

    @Environment(PomodoroViewModel.self) private var vm
    @State private var showModal = false

    var body: some View {
        VStack(spacing: 0) {
            // Phase colour strip
            phaseStrip

            VStack(spacing: 12) {
                // Task name
                if !vm.state.itemTitle.isEmpty {
                    Text(vm.state.itemTitle)
                        .font(.headline)
                        .lineLimit(1)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .accessibilityLabel("Tracking: \(vm.state.itemTitle)")
                }

                // Countdown
                Text(vm.timerLabel)
                    .font(.system(size: 36, weight: .bold, design: .monospaced))
                    .accessibilityLabel("Time remaining: \(vm.timerLabel)")

                // Phase label + session dots
                HStack(spacing: 8) {
                    Text(vm.state.phase.label)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                    Spacer()
                    sessionDots
                }

                // Controls
                HStack(spacing: 16) {
                    Button {
                        if vm.state.running { vm.pause() } else { vm.resume() }
                    } label: {
                        Image(systemName: vm.state.running ? "pause.fill" : "play.fill")
                            .font(.title2)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(phaseSwiftUIColor)
                    .accessibilityLabel(vm.state.running ? "Pause" : "Resume")

                    Button { vm.skip() } label: {
                        Image(systemName: "forward.fill")
                            .font(.title2)
                    }
                    .buttonStyle(.bordered)
                    .accessibilityLabel("Skip phase")

                    Button { vm.stop() } label: {
                        Image(systemName: "stop.fill")
                            .font(.title2)
                    }
                    .buttonStyle(.bordered)
                    .accessibilityLabel("Stop timer")

                    Spacer()

                    Button {
                        showModal = true
                    } label: {
                        Image(systemName: "arrow.up.left.and.arrow.down.right")
                            .font(.title2)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Expand Pomodoro")
                }
            }
            .padding()
        }
        .frame(width: 300)
        .sheet(isPresented: $showModal) {
            PomodoroModalView()
                .environment(vm)
        }
    }

    private var phaseStrip: some View {
        Rectangle()
            .fill(phaseSwiftUIColor)
            .frame(height: 4)
    }

    private var sessionDots: some View {
        HStack(spacing: 4) {
            let total = vm.state.settings.sessionsUntilLongBreak
            ForEach(0..<total, id: \.self) { i in
                Circle()
                    .fill(i < vm.state.completedSessions % total ? phaseSwiftUIColor : Color(.separatorColor))
                    .frame(width: 8, height: 8)
            }
        }
    }

    private var phaseSwiftUIColor: Color {
        switch vm.state.phase {
        case .work:       return .red
        case .shortBreak: return .green
        case .longBreak:  return .blue
        }
    }
}

// ─── PomodoroModalView ────────────────────────────────────────────────────────
// Full expanded Pomodoro view; shown as a sheet from PomodoroBarView
// or opened by a dedicated Window scene in CrossDashboardApp.

struct PomodoroModalView: View {

    @Environment(PomodoroViewModel.self) private var vm
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 24) {
            // Header
            HStack {
                Text("Pomodoro")
                    .font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
                    .keyboardShortcut(.cancelAction)
            }
            .padding([.top, .horizontal])

            Divider()

            // Large countdown
            VStack(spacing: 8) {
                Text(vm.state.phase.label)
                    .font(.headline)
                    .foregroundStyle(.secondary)
                    .textCase(.uppercase)
                    .tracking(1.5)

                Text(vm.timerLabel)
                    .font(.system(size: 72, weight: .bold, design: .monospaced))
                    .accessibilityLabel("Time remaining: \(vm.timerLabel)")

                // Session progress dots
                HStack(spacing: 8) {
                    let total = vm.state.settings.sessionsUntilLongBreak
                    ForEach(0..<total, id: \.self) { i in
                        Circle()
                            .fill(i < vm.state.completedSessions % total ? phaseColor : Color(.separatorColor))
                            .frame(width: 12, height: 12)
                    }
                }
            }
            .padding()
            .frame(maxWidth: .infinity)
            .background(phaseColor.opacity(0.08), in: RoundedRectangle(cornerRadius: 16))
            .padding(.horizontal)

            // Task name
            if !vm.state.itemTitle.isEmpty {
                Label(vm.state.itemTitle, systemImage: "checkmark.circle")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            // Controls
            HStack(spacing: 24) {
                Button {
                    if vm.state.running { vm.pause() } else { vm.resume() }
                } label: {
                    Image(systemName: vm.state.running ? "pause.circle.fill" : "play.circle.fill")
                        .font(.system(size: 48))
                        .foregroundStyle(phaseColor)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(vm.state.running ? "Pause" : "Resume")

                Button { vm.skip() } label: {
                    Image(systemName: "forward.circle.fill")
                        .font(.system(size: 32))
                        .foregroundStyle(.secondary)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Skip phase")

                Button { vm.stop(); dismiss() } label: {
                    Image(systemName: "stop.circle.fill")
                        .font(.system(size: 32))
                        .foregroundStyle(.red)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Stop timer")
            }

            Spacer()
        }
        .frame(width: 380, height: 420)
    }

    private var phaseColor: Color {
        switch vm.state.phase {
        case .work:       return .red
        case .shortBreak: return .green
        case .longBreak:  return .blue
        }
    }
}

// ─── PomodoroFloatingBar ──────────────────────────────────────────────────────
// Floating overlay shown in the bottom-right corner of the main WindowGroup
// when the timer is active and the menu bar popover is not visible.
// Attach via `.overlay(alignment: .bottomTrailing) { PomodoroFloatingBar() }`.

struct PomodoroFloatingBar: View {

    @Environment(PomodoroViewModel.self) private var vm
    @State private var showModal = false

    var body: some View {
        if vm.state.active && !AppPreferences.shared.showPomodoroInMenuBar {
            HStack(spacing: 10) {
                Circle()
                    .fill(phaseColor)
                    .frame(width: 10, height: 10)

                Text(vm.timerLabel)
                    .font(.system(.subheadline, design: .monospaced).weight(.semibold))

                Text("·")
                    .foregroundStyle(.secondary)

                Text(vm.state.phase.label)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)

                Button {
                    if vm.state.running { vm.pause() } else { vm.resume() }
                } label: {
                    Image(systemName: vm.state.running ? "pause.fill" : "play.fill")
                        .font(.caption.weight(.semibold))
                }
                .buttonStyle(.plain)
                .accessibilityLabel(vm.state.running ? "Pause" : "Resume")

                Button { showModal = true } label: {
                    Image(systemName: "arrow.up.backward.and.arrow.down.forward")
                        .font(.caption)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Expand Pomodoro")
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 8)
            .background(.regularMaterial, in: Capsule())
            .shadow(color: Color(.shadowColor).opacity(0.2), radius: 6, y: 2)
            .padding([.bottom, .trailing], 16)
            .sheet(isPresented: $showModal) {
                PomodoroModalView()
                    .environment(vm)
            }
        }
    }

    private var phaseColor: Color {
        switch vm.state.phase {
        case .work:       return .red
        case .shortBreak: return .green
        case .longBreak:  return .blue
        }
    }
}
