import AppKit
import CoreImage

@MainActor
enum DesktopBackgroundRenderer {
    static func render(_ content: DesktopBackgroundContent, pixels: CGSize, dark: Bool,
        appearance: DesktopBackgroundAppearance = .init(imageURL: nil, glassOpacity: 0.8, imageFit: .fill),
        accent: NSColor = .controlAccentColor) -> Data? {
        let pixelWidth = max(1200, Int(pixels.width))
        let pixelHeight = max(800, Int(pixels.height))
        guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixelWidth, pixelsHigh: pixelHeight,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
              let graphics = NSGraphicsContext(bitmapImageRep: rep) else { return nil }
        NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
        let width = CGFloat(pixelWidth)
        let height = CGFloat(pixelHeight)
        let canvas = NSRect(x: 0, y: 0, width: width, height: height)
        (dark ? NSColor.black : NSColor.white).setFill()
        NSBezierPath(rect: canvas).fill()
        let backdrop = appearance.imageURL.flatMap { NSImage(contentsOf: $0) }
        if let backdrop { drawBackdrop(backdrop, in: canvas, fit: appearance.imageFit) }
        let blurredBackdrop = backdrop.flatMap { blurredImage($0) }
        let text = dark ? NSColor(hex: 0xEAF0F7) : NSColor(hex: 0x172235)
        let secondary = dark ? NSColor(hex: 0x9AABC2) : NSColor(hex: 0x52647A)
        let glassTint: NSColor = dark ? .black : .white
        let margin = min(width, height) * 0.065
        drawGlass(in: NSRect(x: margin - 22, y: height - margin - 155,
            width: width - margin * 2 + 44, height: 130), radius: 18,
            backdrop: blurredBackdrop, canvas: canvas, fit: appearance.imageFit,
            tint: glassTint, opacity: appearance.glassOpacity)
        draw(content.title, at: NSPoint(x: margin, y: height - margin - 92), size: 72, color: text, bold: true, width: width - margin * 2)
        draw(content.filters + (content.mode.map { "  ·  \($0)" } ?? "") + "  ·  \(content.rows.count) visible", at: NSPoint(x: margin, y: height - margin - 135), size: 24, color: secondary, width: width - margin * 2)
        let stamp = content.refreshedAt.formatted(date: .omitted, time: .shortened)
        draw("UPDATED \(stamp)", at: NSPoint(x: width - margin - 300, y: height - margin - 82), size: 18, color: secondary, width: 300)
        if content.mode != nil && width >= height * 1.15 && width >= 1_400 {
            drawWideBoard(content, in: NSRect(x: margin, y: margin * 1.6,
                width: width - margin * 2, height: height - margin * 2.6 - 190),
                text: text, secondary: secondary, backdrop: blurredBackdrop, canvas: canvas,
                fit: appearance.imageFit, tint: glassTint, opacity: appearance.glassOpacity,
                accent: accent)
            drawFooter(content, visibleCount: content.rows.count, at: NSPoint(x: margin, y: margin),
                color: secondary)
            graphics.flushGraphics(); NSGraphicsContext.restoreGraphicsState()
            return rep.representation(using: .png, properties: [:])
        }
        let visible = Array(content.rows.prefix(pixelWidth > pixelHeight ? 12 : 8)); var y = height - margin - 245
        if visible.isEmpty { draw("Nothing matches this snapshot", at: NSPoint(x: margin, y: y), size: 32, color: text, width: width - margin * 2) }
        for row in visible {
            let rect = NSRect(x: margin, y: y, width: width - margin * 2, height: 76)
            drawGlass(in: rect, radius: 12, backdrop: blurredBackdrop, canvas: canvas,
                fit: appearance.imageFit, tint: glassTint, opacity: appearance.glassOpacity)
            (row.overdue ? NSColor(hex: 0xF27C7C) : accent).setFill()
            NSRect(x: margin, y: y, width: 6, height: 76).fill()
            let tagWidth: CGFloat = row.group == nil ? 0 : 175
            draw(row.title, at: NSPoint(x: margin + 22, y: y + 38), size: 25, color: text, bold: true, width: width - margin * 2 - 44 - tagWidth)
            if let group = row.group { drawMagicTag(group, at: NSPoint(x: width - margin - 165, y: y + 39), width: 145, accent: accent) }
            let detail = row.subtitle
            draw(detail, at: NSPoint(x: margin + 22, y: y + 12), size: 17, color: secondary, width: width - margin * 2 - 44)
            y -= 90
        }
        drawFooter(content, visibleCount: visible.count, at: NSPoint(x: margin, y: margin), color: secondary)
        graphics.flushGraphics(); NSGraphicsContext.restoreGraphicsState()
        return rep.representation(using: .png, properties: [:])
    }

    private static func draw(_ value: String, at point: NSPoint, size: CGFloat, color: NSColor, bold: Bool = false, width: CGFloat) {
        let style = NSMutableParagraphStyle(); style.lineBreakMode = .byTruncatingTail
        (value as NSString).draw(in: NSRect(x: point.x, y: point.y, width: width, height: size * 1.3),
            withAttributes: [.font: bold ? NSFont.systemFont(ofSize: size, weight: .semibold) : NSFont.systemFont(ofSize: size), .foregroundColor: color, .paragraphStyle: style])
    }

    private static func drawWideBoard(_ content: DesktopBackgroundContent, in bounds: NSRect,
        text: NSColor, secondary: NSColor, backdrop: NSImage?, canvas: NSRect,
        fit: DesktopBackgroundImageFit, tint: NSColor, opacity: CGFloat, accent: NSColor) {
        var groups = content.groups
        if groups.isEmpty {
            for group in content.rows.compactMap(\.group) where !groups.contains(group) {
                groups.append(group)
            }
        }
        guard !groups.isEmpty else { return }
        if content.mode == "Covey" {
            let gap: CGFloat = 22
            let cellWidth = (bounds.width - gap) / 2
            let cellHeight = (bounds.height - gap) / 2
            for (index, group) in groups.prefix(4).enumerated() {
                let column = CGFloat(index % 2), row = CGFloat(index / 2)
                let rect = NSRect(x: bounds.minX + column * (cellWidth + gap),
                    y: bounds.maxY - (row + 1) * cellHeight - row * gap,
                    width: cellWidth, height: cellHeight)
                drawBoardPanel(group, rows: content.rows.filter { $0.group == group }, in: rect,
                    text: text, secondary: secondary, backdrop: backdrop, canvas: canvas,
                    fit: fit, tint: tint, opacity: opacity, accent: accent)
            }
        } else {
            let visibleGroups = Array(groups.prefix(7))
            let gap: CGFloat = 18
            let columnWidth = (bounds.width - gap * CGFloat(max(0, visibleGroups.count - 1))) /
                CGFloat(max(1, visibleGroups.count))
            for (index, group) in visibleGroups.enumerated() {
                let rect = NSRect(x: bounds.minX + CGFloat(index) * (columnWidth + gap), y: bounds.minY,
                    width: columnWidth, height: bounds.height)
                drawBoardPanel(group, rows: content.rows.filter { $0.group?.caseInsensitiveCompare(group) == .orderedSame },
                    in: rect, text: text, secondary: secondary, backdrop: backdrop, canvas: canvas,
                    fit: fit, tint: tint, opacity: opacity, accent: accent)
            }
        }
    }

    private static func drawBoardPanel(_ group: String, rows: [DesktopBackgroundRow], in rect: NSRect,
        text: NSColor, secondary: NSColor, backdrop: NSImage?, canvas: NSRect,
        fit: DesktopBackgroundImageFit, tint: NSColor, opacity: CGFloat, accent: NSColor) {
        drawGlass(in: rect, radius: 18, backdrop: backdrop, canvas: canvas, fit: fit,
            tint: tint, opacity: opacity)
        drawMagicTag(group, at: NSPoint(x: rect.minX + 20, y: rect.maxY - 48),
            width: min(220, rect.width - 40), accent: accent)
        draw("\(rows.count)", at: NSPoint(x: rect.maxX - 55, y: rect.maxY - 45), size: 18,
            color: secondary, width: 35)
        let available = max(1, Int((rect.height - 92) / 72))
        var y = rect.maxY - 92
        for row in rows.prefix(available) {
            let rowRect = NSRect(x: rect.minX + 16, y: y - 52, width: rect.width - 32, height: 58)
            (row.overdue ? NSColor(hex: 0xF27C7C) : accent).withAlphaComponent(0.18).setFill()
            NSBezierPath(roundedRect: rowRect, xRadius: 10, yRadius: 10).fill()
            draw(row.title, at: NSPoint(x: rowRect.minX + 12, y: rowRect.minY + 29), size: 18,
                color: text, bold: true, width: rowRect.width - 24)
            draw(row.subtitle, at: NSPoint(x: rowRect.minX + 12, y: rowRect.minY + 8), size: 13,
                color: secondary, width: rowRect.width - 24)
            y -= 70
        }
        if rows.count > available {
            draw("+\(rows.count - available) more", at: NSPoint(x: rect.minX + 20, y: rect.minY + 15),
                size: 14, color: secondary, width: rect.width - 40)
        }
    }

    private static func drawMagicTag(_ value: String, at point: NSPoint, width: CGFloat, accent: NSColor) {
        accent.withAlphaComponent(0.18).setFill()
        NSBezierPath(roundedRect: NSRect(x: point.x, y: point.y - 5, width: width, height: 30),
            xRadius: 15, yRadius: 15).fill()
        draw("#\(value.uppercased())", at: NSPoint(x: point.x + 11, y: point.y + 2), size: 14,
            color: accent, bold: true, width: width - 22)
    }

    private static func drawGlass(in rect: NSRect, radius: CGFloat, backdrop: NSImage?, canvas: NSRect,
        fit: DesktopBackgroundImageFit, tint: NSColor, opacity: CGFloat) {
        NSGraphicsContext.saveGraphicsState()
        NSBezierPath(roundedRect: rect, xRadius: radius, yRadius: radius).addClip()
        if let backdrop { drawBackdrop(backdrop, in: canvas, fit: fit) }
        tint.withAlphaComponent(min(1, max(0.5, opacity))).setFill()
        NSBezierPath(rect: rect).fill()
        NSGraphicsContext.restoreGraphicsState()
    }

    private static func drawBackdrop(_ image: NSImage, in rect: NSRect, fit: DesktopBackgroundImageFit) {
        if fit == .stretch {
            image.draw(in: rect, from: .zero, operation: .sourceOver, fraction: 1)
            return
        }
        let source = image.size
        guard source.width > 0, source.height > 0 else { return }
        let scaleX = rect.width / source.width, scaleY = rect.height / source.height
        let scale = fit == .fill ? max(scaleX, scaleY) : min(scaleX, scaleY)
        let size = NSSize(width: source.width * scale, height: source.height * scale)
        image.draw(in: NSRect(x: rect.midX - size.width / 2, y: rect.midY - size.height / 2,
            width: size.width, height: size.height), from: .zero, operation: .sourceOver, fraction: 1)
    }

    private static func blurredImage(_ image: NSImage) -> NSImage? {
        guard let data = image.tiffRepresentation, let input = CIImage(data: data),
              let filter = CIFilter(name: "CIGaussianBlur") else { return nil }
        filter.setValue(input, forKey: kCIInputImageKey)
        filter.setValue(18, forKey: kCIInputRadiusKey)
        guard let output = filter.outputImage?.cropped(to: input.extent),
              let cgImage = CIContext().createCGImage(output, from: input.extent) else { return nil }
        return NSImage(cgImage: cgImage, size: image.size)
    }

    private static func drawFooter(_ content: DesktopBackgroundContent, visibleCount: Int,
        at point: NSPoint, color: NSColor) {
        var footer: [String] = []
        if content.rows.count > visibleCount { footer.append("+\(content.rows.count - visibleCount) more") }
        if content.totalMinutes > 0 {
            footer.append("\(content.totalMinutes / 60)h \(content.totalMinutes % 60)m estimated")
        }
        if !footer.isEmpty { draw(footer.joined(separator: "  ·  "), at: point, size: 19,
            color: color, width: 900) }
    }
}

private extension NSColor {
    convenience init(hex: Int) { self.init(srgbRed: CGFloat((hex >> 16) & 255) / 255, green: CGFloat((hex >> 8) & 255) / 255, blue: CGFloat(hex & 255) / 255, alpha: 1) }
}
