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
        if (content.mode != null && w >= h * 1.15f && w >= 1200f) {
            drawWideBoard(canvas, content, RectF(margin, y, w - margin, h - margin * 1.5f),
                panel, text, secondary, body, meta)
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
            val tagWidth = if (row.group == null) 0f else minOf(190f, w * .22f)
            val maxWidth = w - margin * 2 - 34f - tagWidth
            canvas.drawText(ellipsize(row.title, body, maxWidth), margin + 24f, y + rowH * .38f, body)
            row.group?.let { drawMagicTag(canvas, it, w - margin - tagWidth, y + rowH * .18f, tagWidth - 12f, meta) }
            canvas.drawText(ellipsize(row.subtitle, meta, maxWidth), margin + 24f, y + rowH * .66f, meta)
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

    private fun drawWideBoard(
        canvas: Canvas,
        content: BackgroundContent,
        bounds: RectF,
        panelColor: Int,
        textColor: Int,
        secondaryColor: Int,
        body: Paint,
        meta: Paint,
    ) {
        val groups = content.groups.ifEmpty { content.rows.mapNotNull { it.group }.distinct() }
        if (groups.isEmpty()) return
        val covey = content.mode?.equals("Covey", ignoreCase = true) == true
        val visibleGroups = if (covey) groups.take(4) else groups.take(7)
        val gap = 18f
        visibleGroups.forEachIndexed { index, group ->
            val rect = if (covey) {
                val cellW = (bounds.width() - gap) / 2f
                val cellH = (bounds.height() - gap) / 2f
                val column = index % 2
                val row = index / 2
                RectF(bounds.left + column * (cellW + gap), bounds.top + row * (cellH + gap),
                    bounds.left + column * (cellW + gap) + cellW,
                    bounds.top + row * (cellH + gap) + cellH)
            } else {
                val columnW = (bounds.width() - gap * (visibleGroups.size - 1)) / visibleGroups.size
                RectF(bounds.left + index * (columnW + gap), bounds.top,
                    bounds.left + index * (columnW + gap) + columnW, bounds.bottom)
            }
            drawBoardPanel(canvas, group, content.rows.filter { it.group?.equals(group, ignoreCase = true) == true },
                rect, panelColor, textColor, secondaryColor, body, meta)
        }
    }

    private fun drawBoardPanel(
        canvas: Canvas,
        group: String,
        rows: List<BackgroundRow>,
        rect: RectF,
        panelColor: Int,
        textColor: Int,
        secondaryColor: Int,
        body: Paint,
        meta: Paint,
    ) {
        canvas.drawRoundRect(rect, 18f, 18f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = panelColor })
        drawMagicTag(canvas, group, rect.left + 16f, rect.top + 18f, rect.width() - 70f, meta)
        canvas.drawText(rows.size.toString(), rect.right - 42f, rect.top + 39f,
            Paint(meta).apply { color = secondaryColor })
        val available = maxOf(1, ((rect.height() - 88f) / 68f).toInt())
        var y = rect.top + 65f
        rows.take(available).forEach { row ->
            val rowRect = RectF(rect.left + 12f, y, rect.right - 12f, y + 56f)
            canvas.drawRoundRect(rowRect, 10f, 10f, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = if (row.overdue) OVERDUE else 0x332C4163
            })
            val rowBody = Paint(body).apply { color = textColor; textSize *= .72f }
            val rowMeta = Paint(meta).apply { color = secondaryColor; textSize *= .78f }
            canvas.drawText(ellipsize(row.title, rowBody, rowRect.width() - 20f), rowRect.left + 10f, y + 24f, rowBody)
            canvas.drawText(ellipsize(row.subtitle, rowMeta, rowRect.width() - 20f), rowRect.left + 10f, y + 46f, rowMeta)
            y += 66f
        }
        if (rows.size > available) canvas.drawText("+${rows.size - available} more", rect.left + 16f,
            rect.bottom - 14f, Paint(meta).apply { color = secondaryColor })
    }

    private fun drawMagicTag(canvas: Canvas, value: String, x: Float, y: Float, width: Float, textPaint: Paint) {
        val tagPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x33F2B35D }
        canvas.drawRoundRect(RectF(x, y, x + width, y + 32f), 16f, 16f, tagPaint)
        val labelPaint = Paint(textPaint).apply { color = TASK; typeface = android.graphics.Typeface.DEFAULT_BOLD }
        canvas.drawText(ellipsize("#${value.uppercase()}", labelPaint, width - 18f), x + 9f, y + 22f, labelPaint)
    }

    private fun ellipsize(value: String, paint: Paint, maxWidth: Float): String {
        if (paint.measureText(value) <= maxWidth) return value
        var end = value.length
        while (end > 1 && paint.measureText(value.substring(0, end) + "…") > maxWidth) end--
        return value.substring(0, end) + "…"
    }
}
