import AppKit

@MainActor
enum DesktopBackgroundRenderer {
    static func render(_ content: DesktopBackgroundContent, pixels: CGSize, dark: Bool) -> Data? {
        let width = max(1200, Int(pixels.width)), height = max(800, Int(pixels.height))
        guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width, pixelsHigh: height,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: false, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
              let graphics = NSGraphicsContext(bitmapImageRep: rep) else { return nil }
        NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
        let canvas = NSRect(x: 0, y: 0, width: width, height: height)
        (dark ? NSColor(hex: 0x0B1220) : NSColor(hex: 0xF2F6FA)).setFill()
        NSBezierPath(rect: canvas).fill()
        let text = dark ? NSColor(hex: 0xEAF0F7) : NSColor(hex: 0x172235)
        let secondary = dark ? NSColor(hex: 0x9AABC2) : NSColor(hex: 0x52647A)
        let panel = dark ? NSColor(hex: 0x18243A) : .white
        let margin = CGFloat(min(width, height)) * 0.065
        draw(content.title, at: NSPoint(x: margin, y: height - margin - 92), size: 72, color: text, bold: true, width: width - Int(margin * 2))
        draw(content.filters + (content.mode.map { "  ·  \($0)" } ?? "") + "  ·  \(content.rows.count) visible", at: NSPoint(x: margin, y: height - margin - 135), size: 24, color: secondary, width: width - Int(margin * 2))
        let stamp = content.refreshedAt.formatted(date: .omitted, time: .shortened)
        draw("UPDATED \(stamp)", at: NSPoint(x: CGFloat(width) - margin - 300, y: height - margin - 82), size: 18, color: secondary, width: 300)
        let visible = Array(content.rows.prefix(width > height ? 12 : 8)); var y = CGFloat(height) - margin - 245
        if visible.isEmpty { draw("Nothing matches this snapshot", at: NSPoint(x: margin, y: y), size: 32, color: text, width: width - Int(margin * 2)) }
        for row in visible {
            let rect = NSRect(x: margin, y: y, width: CGFloat(width) - margin * 2, height: 76)
            panel.setFill(); NSBezierPath(roundedRect: rect, xRadius: 12, yRadius: 12).fill()
            (row.overdue ? NSColor(hex: 0xF27C7C) : row.kind == 0 ? NSColor(hex: 0x65C7D0) : NSColor(hex: 0xF2B35D)).setFill()
            NSRect(x: margin, y: y, width: 6, height: 76).fill()
            draw(row.title, at: NSPoint(x: margin + 22, y: y + 38), size: 25, color: text, bold: true, width: width - Int(margin * 2) - 44)
            let detail = [row.group, row.subtitle].compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: "  ·  ")
            draw(detail, at: NSPoint(x: margin + 22, y: y + 12), size: 17, color: secondary, width: width - Int(margin * 2) - 44)
            y -= 90
        }
        var footer: [String] = []
        if content.rows.count > visible.count { footer.append("+\(content.rows.count - visible.count) more") }
        if content.totalMinutes > 0 {
            footer.append("\(content.totalMinutes / 60)h \(content.totalMinutes % 60)m estimated")
        }
        if !footer.isEmpty { draw(footer.joined(separator: "  ·  "), at: NSPoint(x: margin, y: margin), size: 19, color: secondary, width: 800) }
        graphics.flushGraphics(); NSGraphicsContext.restoreGraphicsState()
        return rep.representation(using: .png, properties: [:])
    }

    private static func draw(_ value: String, at point: NSPoint, size: CGFloat, color: NSColor, bold: Bool = false, width: Int) {
        let style = NSMutableParagraphStyle(); style.lineBreakMode = .byTruncatingTail
        (value as NSString).draw(in: NSRect(x: point.x, y: point.y, width: CGFloat(width), height: size * 1.3),
            withAttributes: [.font: bold ? NSFont.systemFont(ofSize: size, weight: .semibold) : NSFont.systemFont(ofSize: size), .foregroundColor: color, .paragraphStyle: style])
    }
}

private extension NSColor {
    convenience init(hex: Int) { self.init(srgbRed: CGFloat((hex >> 16) & 255) / 255, green: CGFloat((hex >> 8) & 255) / 255, blue: CGFloat(hex & 255) / 255, alpha: 1) }
}
