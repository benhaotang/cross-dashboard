package com.crossdashboard.app.background

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import com.crossdashboard.app.ui.screen.inbox.InboxViewModel
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

object BackgroundCanvasRenderer {
    private const val EVENT = 0xFF65C7D0.toInt()
    private const val TASK = 0xFFF2B35D.toInt()
    private const val OVERDUE = 0xFFF27C7C.toInt()

    fun draw(canvas: Canvas, content: BackgroundContent, preferred: PreferredWallpaperOrientation, dark: Boolean) {
        val ink = if (dark) 0xFF0B1220.toInt() else 0xFFF2F6FA.toInt()
        val panel = if (dark) 0xFF18243A.toInt() else 0xFFFFFFFF.toInt()
        val text = if (dark) 0xFFEAF0F7.toInt() else 0xFF172235.toInt()
        val secondary = if (dark) 0xFF9AABC2.toInt() else 0xFF52647A.toInt()
        canvas.drawColor(ink)
        val w = canvas.width.toFloat()
        val h = canvas.height.toFloat()
        val landscape = w > h
        val margin = (minOf(w, h) * .065f).coerceAtLeast(32f)
        val title = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = text
            textSize = minOf(w, h) * .055f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
        }
        val meta = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = secondary; textSize = title.textSize * .34f }
        val body = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = text; textSize = title.textSize * .36f }
        canvas.drawText(content.title, margin, margin + title.textSize, title)
        canvas.drawText(content.filterLabel + (content.mode?.let { "  ·  $it" } ?: "") + "  ·  ${content.rows.size} visible", margin, margin + title.textSize * 1.55f, meta)
        val stamp = DateTimeFormatter.ofPattern("HH:mm").withZone(ZoneId.systemDefault()).format(Instant.ofEpochMilli(content.refreshedAt))
        canvas.drawText("UPDATED $stamp", w - margin - meta.measureText("UPDATED $stamp"), margin + meta.textSize, meta)
        var y = margin + title.textSize * 2.15f
        if (content.rows.isEmpty()) {
            canvas.drawText("Nothing matches this snapshot", margin, y + body.textSize * 2f, body)
            return
        }
        val preferredMatches = (preferred == PreferredWallpaperOrientation.LANDSCAPE) == landscape
        val limit = when { minOf(w, h) < 700 -> 5; preferredMatches -> 12; else -> 8 }
        val visible = content.rows.take(limit)
        val rowH = ((h - y - margin) / (visible.size + 1)).coerceIn(body.textSize * 2.0f, body.textSize * 3.2f)
        visible.forEach { row ->
            canvas.drawRoundRect(RectF(margin, y, w - margin, y + rowH * .82f), rowH * .14f, rowH * .14f, Paint().apply { color = panel })
            val accent = when { row.overdue -> OVERDUE; row.kind == BackgroundRow.Kind.EVENT -> EVENT; else -> TASK }
            canvas.drawRoundRect(RectF(margin, y, margin + 7f, y + rowH * .82f), 4f, 4f, Paint().apply { color = accent })
            val maxWidth = w - margin * 2 - 34f
            canvas.drawText(ellipsize(row.title, body, maxWidth), margin + 24f, y + rowH * .38f, body)
            val sub = listOfNotNull(row.group, row.subtitle).joinToString("  ·  ")
            canvas.drawText(ellipsize(sub, meta, maxWidth), margin + 24f, y + rowH * .66f, meta)
            y += rowH
        }
        val footer = buildString {
            if (content.rows.size > visible.size) append("+${content.rows.size - visible.size} more")
            if (content.totalMinutes > 0) {
                if (isNotEmpty()) append("  ·  ")
                append(InboxViewModel.formatMinutes(content.totalMinutes))
            }
        }
        if (footer.isNotEmpty()) canvas.drawText(footer, margin, h - margin, meta)
    }

    private fun ellipsize(value: String, paint: Paint, maxWidth: Float): String {
        if (paint.measureText(value) <= maxWidth) return value
        var end = value.length
        while (end > 1 && paint.measureText(value.substring(0, end) + "…") > maxWidth) end--
        return value.substring(0, end) + "…"
    }
}
