import AppKit
import SwiftUI
import SwiftMath

struct MathView: NSViewRepresentable {
    let latex: String
    var displayMode: Bool = true
    var fontSize: CGFloat = 16
    var textAlignment: MTTextAlignment = .left

    func makeNSView(context: Context) -> MTMathUILabel {
        let view = MTMathUILabel()
        view.setContentHuggingPriority(.required, for: .vertical)
        view.setContentCompressionResistancePriority(.required, for: .vertical)
        return view
    }

    func updateNSView(_ view: MTMathUILabel, context: Context) {
        view.latex = latex
        view.fontSize = fontSize
        view.textAlignment = textAlignment
        view.labelMode = displayMode ? .display : .text
        view.textColor = MTColor(Color.primary)
        view.invalidateIntrinsicContentSize()
    }
}
