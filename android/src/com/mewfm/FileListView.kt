package com.mewfm

import android.app.AlertDialog
import android.content.Context
import android.graphics.*
import android.os.SystemClock
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.widget.EditText
import android.widget.OverScroller
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

class FileListView(context: Context) : View(context) {
    val fm = FileManager()

    private val d = context.resources.displayMetrics.density
    private fun dp(v: Float) = v * d

    private val cBg = Color.rgb(30, 30, 30)
    private val cTop = Color.rgb(22, 22, 22)
    private val cStatus = Color.rgb(20, 20, 20)
    private val cHeader = Color.rgb(34, 34, 34)
    private val cStripe = Color.argb(10, 255, 255, 255)
    private val cSel = Color.rgb(45, 74, 115)
    private val cPress = Color.rgb(48, 48, 48)
    private val cText = Color.rgb(230, 230, 230)
    private val cTextDim = Color.rgb(160, 160, 160)
    private val cSep = Color.rgb(18, 18, 18)
    private val cFolder = Color.rgb(230, 180, 80)
    private val cFile = Color.rgb(150, 180, 220)
    private val cScrim = Color.argb(140, 0, 0, 0)
    private val cSidebar = Color.rgb(26, 26, 26)

    private val topBarH = dp(56f)
    private val statusBarH = dp(24f)
    private val headerH = dp(26f)
    private val rowH = dp(44f)
    private val sidebarW = dp(240f)
    private val padX = dp(12f)
    private val btnSize = dp(44f)

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val text = Paint(Paint.ANTI_ALIAS_FLAG)
    private val bold = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.DEFAULT_BOLD }
    private val small = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = dp(11f) }
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }

    private val scroller = OverScroller(context)
    private var scrollY = 0f
    private var vt: VelocityTracker? = null
    private var lastY = 0f

    private var selIdx = -1
    private var pressIdx = -1
    private var lastTapT = 0L
    private var lastTapIdx = -1
    private var downX = 0f
    private var downY = 0f
    private var downT = 0L

    private var rippleX = 0f
    private var rippleY = 0f
    private var rippleT = 0L
    private var rippleIdx = -1

    private var sidebarOpen = false
    private var sidebarP = 0f
    private var toastMsg = ""
    private var toastT = 0L
    private var dirFadeT = 0L

    private val sidebarItems = listOf(
        "Root" to "/",
        "Home" to "/storage/emulated/0",
        "Pictures" to "/storage/emulated/0/Pictures",
        "Downloads" to "/storage/emulated/0/Download",
        "DCIM" to "/storage/emulated/0/DCIM"
    )

    init {
        setBackgroundColor(cBg)
        isClickable = true
    }

    fun showToast(msg: String) { toastMsg = msg; toastT = SystemClock.uptimeMillis(); invalidate() }

    fun navigateTo(path: String) {
        fm.setPathAsync(path) { success ->
            if (success) {
                selIdx = -1; scrollY = 0f; dirFadeT = SystemClock.uptimeMillis(); invalidate()
            } else {
                showToast("Cannot open $path")
            }
        }
    }

    fun goUp() { fm.goUpAsync { success -> if (success) { selIdx = -1; scrollY = 0f; dirFadeT = SystemClock.uptimeMillis(); invalidate() } } }
    fun goBack() { fm.goBackAsync { success -> if (success) { selIdx = -1; scrollY = 0f; dirFadeT = SystemClock.uptimeMillis(); invalidate() } } }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (vt == null) vt = VelocityTracker.obtain()
        vt?.addMovement(e)
        val x = e.x; val y = e.y; val now = SystemClock.uptimeMillis()
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = x; downY = y; downT = now; lastY = y
                if (!scroller.isFinished) scroller.abortAnimation()
                if (sidebarP > 0.5f && x > sidebarW * sidebarP) {
                    sidebarOpen = false; postInvalidateOnAnimation(); return true
                }
                val idx = rowAt(y)
                if (idx >= 0) {
                    pressIdx = idx; rippleIdx = idx
                    rippleX = x; rippleY = y; rippleT = now
                    invalidate()
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dy = y - lastY
                if (abs(y - downY) > dp(8f) && pressIdx >= 0) { pressIdx = -1; rippleIdx = -1 }
                if (abs(y - downY) > dp(8f) && sidebarP < 0.01f) { scrollY -= dy; clampScroll(); invalidate() }
                lastY = y
                return true
            }
            MotionEvent.ACTION_UP -> {
                val dx = abs(x - downX); val dy = abs(y - downY)
                val wasScroll = dy > dp(8f) && sidebarP < 0.01f
                if (!wasScroll && dx < dp(10f) && now - downT < 350) handleTap(x, y)
                else if (wasScroll) {
                    vt?.computeCurrentVelocity(1000)
                    val vy = vt?.yVelocity ?: 0f
                    if (abs(vy) > 100f) startFling(-vy)
                }
                if (pressIdx >= 0) { pressIdx = -1; invalidate() }
                vt?.recycle(); vt = null
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                pressIdx = -1; rippleIdx = -1
                vt?.recycle(); vt = null
                invalidate(); return true
            }
        }
        return super.onTouchEvent(e)
    }

    private fun startFling(vel: Float) {
        val listH = (height - topBarH - headerH - statusBarH).toInt()
        val maxY = max(0, (fm.entries.size * rowH).toInt() - listH)
        scroller.fling(0, scrollY.toInt(), 0, vel.toInt(), 0, 0, 0, maxY)
        postInvalidateOnAnimation()
    }

    override fun computeScroll() {
        if (scroller.computeScrollOffset()) { scrollY = scroller.currY.toFloat(); postInvalidateOnAnimation() }
    }

    private fun clampScroll() {
        val maxS = max(0f, fm.entries.size * rowH - (height - topBarH - headerH - statusBarH))
        scrollY = scrollY.coerceIn(0f, maxS)
    }

    private fun rowAt(y: Float): Int {
        if (y < topBarH + headerH) return -1
        val i = ((y - topBarH - headerH + scrollY) / rowH).toInt()
        return if (i in fm.entries.indices) i else -1
    }

    private fun handleTap(x: Float, y: Float) {
        if (sidebarP > 0.5f) {
            val px = -sidebarW + sidebarW * sidebarP
            if (x in px..(px + sidebarW)) {
                for ((i, item) in sidebarItems.withIndex()) {
                    val iy = dp(90f) + i * dp(40f)
                    if (y in (iy - dp(20f))..(iy + dp(20f))) { navigateTo(item.second); sidebarOpen = false; return }
                }
            }
            sidebarOpen = false
            return
        }
        if (y < topBarH) {
            if (x < padX + btnSize) { sidebarOpen = !sidebarOpen; postInvalidateOnAnimation(); return }
            if (x < padX + 2 * btnSize + dp(4f)) { goBack(); return }
            if (x < padX + 3 * btnSize + dp(8f)) { goUp(); return }
            return
        }
        
        val fabCx = width - dp(36f)
        val fabCy = height - statusBarH - dp(36f)
        val dx = x - fabCx
        val dy = y - fabCy
        if (dx * dx + dy * dy <= dp(30f) * dp(30f)) {
            showFabMenu()
            return
        }
        
        val idx = rowAt(y)
        if (idx < 0) return
        val now = SystemClock.uptimeMillis()
        if (idx == lastTapIdx && now - lastTapT < 350) { openEntry(idx); lastTapIdx = -1 }
        else { selIdx = idx; lastTapIdx = idx; lastTapT = now }
        invalidate()
    }

    private fun openEntry(idx: Int) {
        val e = fm.entries[idx]
        if (e.isDirectory) navigateTo(fm.currentPath.trimEnd('/') + "/" + e.name)
        else showToast("Opening ${e.name}")
    }

    override fun onDraw(c: Canvas) {
        val now = SystemClock.uptimeMillis()
        val target = if (sidebarOpen) 1f else 0f
        if (sidebarP != target) {
            sidebarP = if (sidebarP < target) min(1f, sidebarP + 0.18f) else max(0f, sidebarP - 0.18f)
            postInvalidateOnAnimation()
        }
        text.textSize = dp(14f)
        text.color = cText

        c.drawColor(cBg)
        drawList(c, now)
        drawHeader(c)
        drawTopBar(c)
        drawStatusBar(c)

        if (rippleIdx >= 0) {
            val p = (now - rippleT) / 280f
            if (p < 1f) {
                fill.color = Color.argb((60 * (1f - p)).toInt(), 255, 255, 255)
                c.drawCircle(rippleX, rippleY, dp(60f) * p, fill)
                postInvalidateOnAnimation()
            } else { rippleIdx = -1 }
        }

        if (dirFadeT > 0 && now - dirFadeT < 220) postInvalidateOnAnimation()
        if (sidebarP > 0.01f) drawSidebar(c)
        if (!sidebarOpen) drawFab(c)
        if (toastMsg.isNotEmpty()) drawToast(c, now)
    }

    private fun drawTopBar(c: Canvas) {
        fill.color = cTop; c.drawRect(0f, 0f, width.toFloat(), topBarH, fill)
        fill.color = cSep; c.drawRect(0f, topBarH - 1f, width.toFloat(), topBarH, fill)
        val hy = topBarH / 2
        stroke.color = cText; stroke.strokeWidth = dp(2f)
        val hx = padX + btnSize / 2
        for (i in -1..1) c.drawLine(hx - dp(8f), hy + i * dp(5f), hx + dp(8f), hy + i * dp(5f), stroke)
        val bx = padX + btnSize + dp(2f) + btnSize / 2
        stroke.color = if (fm.canGoBack()) cText else Color.rgb(80, 80, 80)
        val p1 = Path()
        p1.moveTo(bx - dp(6f), hy); p1.lineTo(bx + dp(6f), hy - dp(8f))
        p1.moveTo(bx - dp(6f), hy); p1.lineTo(bx + dp(6f), hy + dp(8f))
        c.drawPath(p1, stroke)
        val ux = padX + 2 * btnSize + dp(8f) + btnSize / 2
        val p2 = Path()
        p2.moveTo(ux, hy - dp(8f)); p2.lineTo(ux - dp(8f), hy + dp(2f)); p2.lineTo(ux + dp(8f), hy + dp(2f)); p2.close()
        fill.color = cText; c.drawPath(p2, fill)
        val pathX = padX + 3 * btnSize + dp(16f)
        c.save(); c.clipRect(pathX, 0f, width - padX, topBarH)
        c.drawText(fm.currentPath, pathX, hy + dp(5f), text)
        c.restore()
    }

    private fun drawHeader(c: Canvas) {
        fill.color = cHeader; c.drawRect(0f, topBarH, width.toFloat(), topBarH + headerH, fill)
        small.color = cTextDim
        c.drawText("NAME", padX, topBarH + headerH / 2 + dp(4f), small)
        val szW = small.measureText("SIZE")
        c.drawText("SIZE", width - padX - szW, topBarH + headerH / 2 + dp(4f), small)
    }

    private fun drawList(c: Canvas, now: Long) {
        val listTop = topBarH + headerH
        val listH = height - listTop - statusBarH
        c.save()
        c.clipRect(0f, listTop, width.toFloat(), listTop + listH)
        val fade = if (dirFadeT > 0 && now - dirFadeT < 220) ((now - dirFadeT) / 220f).coerceIn(0f, 1f) else 1f
        for (i in fm.entries.indices) {
            val y = listTop + i * rowH - scrollY
            if (y + rowH < listTop) continue
            if (y > listTop + listH) break
            val e = fm.entries[i]
            if (i % 2 == 0) { fill.color = cStripe; c.drawRect(0f, y, width.toFloat(), y + rowH, fill) }
            if (i == selIdx) { fill.color = cSel; c.drawRect(0f, y, width.toFloat(), y + rowH, fill) }
            else if (i == pressIdx) { fill.color = cPress; c.drawRect(0f, y, width.toFloat(), y + rowH, fill) }
            val ix = padX + dp(6f); val iy = y + rowH / 2
            fill.alpha = (255 * fade).toInt()
            drawIcon(c, ix, iy - dp(8f), e.isDirectory)
            fill.alpha = 255
            text.color = cText
            val nameX = ix + dp(22f)
            c.save(); c.clipRect(nameX, y, width - padX - dp(90f), y + rowH)
            c.drawText(e.name, nameX, iy + dp(5f), text)
            c.restore()
            small.color = cTextDim
            val szW = small.measureText(e.sizeText)
            c.drawText(e.sizeText, width - padX - szW, iy + dp(4f), small)
            fill.color = cSep
            c.drawRect(0f, y + rowH - 1f, width.toFloat(), y + rowH, fill)
        }
        c.restore()
    }

    private fun drawIcon(c: Canvas, x: Float, y: Float, isDir: Boolean) {
        if (isDir) {
            fill.color = cFolder
            val w = dp(16f); val h = dp(12f); val tab = dp(5f)
            val p = Path()
            p.moveTo(x, y); p.lineTo(x + tab, y); p.lineTo(x + tab, y + dp(2f))
            p.lineTo(x + w, y + dp(2f)); p.lineTo(x + w, y + h); p.lineTo(x, y + h); p.close()
            c.drawPath(p, fill)
        } else {
            fill.color = cFile
            val w = dp(12f); val h = dp(15f); val fold = dp(4f)
            val p = Path()
            p.moveTo(x, y); p.lineTo(x + w - fold, y); p.lineTo(x + w, y + fold)
            p.lineTo(x + w, y + h); p.lineTo(x, y + h); p.close()
            c.drawPath(p, fill)
            stroke.color = cFile; stroke.strokeWidth = dp(1f)
            c.drawLine(x + w - fold, y, x + w - fold, y + fold, stroke)
            c.drawLine(x + w - fold, y + fold, x + w, y + fold, stroke)
        }
    }

    private fun drawFab(c: Canvas) {
        val cx = width - dp(36f)
        val cy = height - statusBarH - dp(36f)
        fill.color = Color.rgb(76, 175, 80)
        c.drawCircle(cx, cy, dp(24f), fill)
        
        fill.color = Color.WHITE
        c.drawCircle(cx - dp(8f), cy, dp(2.5f), fill)
        c.drawCircle(cx, cy, dp(2.5f), fill)
        c.drawCircle(cx + dp(8f), cy, dp(2.5f), fill)
    }

    private fun showFabMenu() {
        val options = arrayOf("New Folder", "New File", "Refresh")
        AlertDialog.Builder(context)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> promptForName("New Folder") { fm.createDirectory(it); invalidate() }
                    1 -> promptForName("New File") { fm.createFile(it); invalidate() }
                    2 -> { fm.refresh(); invalidate() }
                }
            }
            .show()
    }

    private fun promptForName(title: String, onOk: (String) -> Unit) {
        val input = EditText(context)
        AlertDialog.Builder(context)
            .setTitle(title)
            .setView(input)
            .setPositiveButton("OK") { _, _ -> onOk(input.text.toString()) }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun drawStatusBar(c: Canvas) {
        fill.color = cStatus
        c.drawRect(0f, height - statusBarH, width.toFloat(), height.toFloat(), fill)
        small.color = cTextDim
        val n = fm.entries.size
        c.drawText(if (n == 1) "1 item" else "$n items", padX, height - statusBarH / 2 + dp(4f), small)
    }

    private fun drawSidebar(c: Canvas) {
        val p = sidebarP
        fill.color = Color.argb((140 * p).toInt(), 0, 0, 0)
        c.drawRect(0f, 0f, width.toFloat(), height.toFloat(), fill)
        val px = -sidebarW + sidebarW * p
        fill.color = cSidebar
        c.drawRect(px, 0f, px + sidebarW, height.toFloat(), fill)
        bold.textSize = dp(16f); bold.color = cText
        c.drawText("mewFM", px + dp(16f), dp(34f), bold)
        small.color = cTextDim
        c.drawText(fm.currentPath, px + dp(16f), dp(58f), small)
        text.color = cText
        for ((i, item) in sidebarItems.withIndex()) {
            val iy = dp(90f) + i * dp(40f)
            fill.color = cFolder
            c.drawCircle(px + dp(24f), iy - dp(4f), dp(5f), fill)
            c.drawText(item.first, px + dp(40f), iy, text)
        }
    }

    private fun drawToast(c: Canvas, now: Long) {
        val elapsed = now - toastT
        if (elapsed > 2400) { toastMsg = ""; return }
        val fade = when {
            elapsed < 200 -> elapsed / 200f
            elapsed > 2000 -> 1f - (elapsed - 2000) / 400f
            else -> 1f
        }
        text.textSize = dp(13f)
        val tw = text.measureText(toastMsg)
        val bw = tw + dp(32f); val bh = dp(36f)
        val bx = (width - bw) / 2
        val by = height - statusBarH - bh - dp(24f) + (1f - fade) * dp(20f)
        fill.color = Color.argb((200 * fade).toInt(), 0, 0, 0)
        c.drawRoundRect(RectF(bx, by, bx + bw, by + bh), dp(8f), dp(8f), fill)
        text.color = Color.argb((255 * fade).toInt(), 255, 255, 255)
        c.drawText(toastMsg, bx + dp(16f), by + bh / 2 + dp(5f), text)
        postInvalidateOnAnimation()
    }
}
