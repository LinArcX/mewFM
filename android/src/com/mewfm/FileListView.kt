package com.mewfm

import android.app.AlertDialog
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.graphics.*
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.Editable
import android.text.TextWatcher
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
    private var selectionMode = false
    private val selectedIndices = mutableSetOf<Int>()
    private var pressIdx = -1
    private var lastTapT = 0L
    private var lastTapIdx = -1
    private var downX = 0f
    private var downY = 0f
    private var downT = 0L

    private val longPressHandler = Handler(Looper.getMainLooper())
    private var longPressRunnable: Runnable? = null
    private var longPressFired = false

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
        exitSelectionMode()
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
                longPressFired = false
                if (!scroller.isFinished) scroller.abortAnimation()
                if (sidebarP > 0.5f && x > sidebarW * sidebarP) {
                    sidebarOpen = false; postInvalidateOnAnimation(); return true
                }
                val idx = rowAt(y)
                if (idx >= 0) {
                    pressIdx = idx; rippleIdx = idx
                    rippleX = x; rippleY = y; rippleT = now
                    invalidate()
                    scheduleLongPress(x, y, idx)
                } else if (y >= topBarH + headerH) {
                    scheduleLongPress(x, y, -1)
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val dy = y - lastY
                val moved = abs(x - downX) > dp(8f) || abs(y - downY) > dp(8f)
                if (moved) cancelPendingLongPress()
                if (abs(y - downY) > dp(8f) && pressIdx >= 0) { pressIdx = -1; rippleIdx = -1 }
                if (abs(y - downY) > dp(8f) && sidebarP < 0.01f) { scrollY -= dy; clampScroll(); invalidate() }
                lastY = y
                return true
            }
            MotionEvent.ACTION_UP -> {
                cancelPendingLongPress()
                if (longPressFired) {
                    longPressFired = false
                    if (pressIdx >= 0) { pressIdx = -1; invalidate() }
                    vt?.recycle(); vt = null
                    return true
                }
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
                cancelPendingLongPress()
                pressIdx = -1; rippleIdx = -1
                vt?.recycle(); vt = null
                invalidate(); return true
            }
        }
        return super.onTouchEvent(e)
    }

    private fun scheduleLongPress(x: Float, y: Float, idx: Int) {
        cancelPendingLongPress()
        val r = Runnable {
            longPressFired = true
            pressIdx = -1
            rippleIdx = -1
            invalidate()
            if (idx in fm.entries.indices) {
                if (selectionMode) toggleSelection(idx)
                else enterSelectionMode(idx)
            } else {
                if (selectionMode) exitSelectionMode()
                else showEmptyContextMenu()
            }
        }
        longPressRunnable = r
        longPressHandler.postDelayed(r, 500L)
    }

    private fun cancelPendingLongPress() {
        longPressRunnable?.let { longPressHandler.removeCallbacks(it) }
        longPressRunnable = null
    }

    private fun showEmptyContextMenu() {
        val options = mutableListOf<Pair<String, () -> Unit>>()
        if (fm.clipboardMode != ClipboardMode.None) {
            options.add("Paste" to { doPaste() })
        }
        options.add("New Folder" to { promptForName("New Folder") { fm.createDirectory(it); invalidate() } })
        options.add("New File" to { promptForName("New File") { fm.createFile(it); invalidate() } })
        val hiddenLabel = if (fm.showHidden) "Hide hidden files" else "Show hidden files"
        options.add(hiddenLabel to {
            fm.setShowHidden(!fm.showHidden)
            if (selectionMode) exitSelectionMode()
            invalidate()
        })
        options.add("Refresh" to { fm.refresh(); invalidate() })
        AlertDialog.Builder(context)
            .setItems(options.map { it.first }.toTypedArray()) { _, which ->
                options[which].second()
            }
            .show()
    }

    private fun doPaste() {
        showToast("Pasting...")
        fm.pasteAsync { success ->
            if (success) showToast("Pasted") else showToast("Paste failed")
            invalidate()
        }
    }

    private fun promptRename(idx: Int) {
        val e = fm.entries.getOrNull(idx) ?: return
        val input = EditText(context)
        input.setText(e.name)
        input.setSelection(e.name.length)
        AlertDialog.Builder(context)
            .setTitle("Rename")
            .setView(input)
            .setPositiveButton("OK") { _, _ ->
                val newName = input.text.toString()
                if (newName.isNotEmpty() && newName != e.name) {
                    exitSelectionMode()
                    if (fm.renameEntry(e.name, newName)) showToast("Renamed")
                    else showToast("Rename failed")
                    invalidate()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun confirmDelete(idx: Int) {
        val e = fm.entries[idx]
        AlertDialog.Builder(context)
            .setTitle("Delete")
            .setMessage("Permanently delete \"${e.name}\"?")
            .setPositiveButton("Delete") { _, _ ->
                if (fm.deleteEntry(e.name)) showToast("Deleted")
                else showToast("Delete failed")
                invalidate()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun enterSelectionMode(idx: Int) {
        selectionMode = true
        selectedIndices.clear()
        if (idx >= 0) selectedIndices.add(idx)
        selIdx = -1
        invalidate()
    }

    private fun exitSelectionMode() {
        if (!selectionMode) return
        selectionMode = false
        selectedIndices.clear()
        invalidate()
    }

    private fun toggleSelection(idx: Int) {
        if (selectedIndices.contains(idx)) selectedIndices.remove(idx)
        else selectedIndices.add(idx)
        if (selectedIndices.isEmpty()) exitSelectionMode()
        else invalidate()
    }

    private fun selectAll() {
        selectedIndices.clear()
        for (i in fm.entries.indices) selectedIndices.add(i)
        invalidate()
    }

    fun handleBack(): Boolean {
        if (selectionMode) {
            exitSelectionMode()
            return true
        }
        return false
    }

    private fun getSelectionBarButtonRects(): List<Pair<String, RectF>> {
        val rects = mutableListOf<Pair<String, RectF>>()
        val btnH = dp(34f)
        val cy = topBarH / 2
        val top = cy - btnH / 2
        val bot = cy + btnH / 2
        rects.add("close" to RectF(padX, top, padX + dp(36f), bot))
        val showMore = selectedIndices.size == 1
        val moreW = dp(58f)
        val allW = dp(46f)
        val delW = dp(46f)
        val cutW = dp(46f)
        val copyW = dp(54f)
        var ax = width - padX
        rects.add("All" to RectF(ax - allW, top, ax, bot)); ax -= allW + dp(4f)
        rects.add("Del" to RectF(ax - delW, top, ax, bot)); ax -= delW + dp(4f)
        rects.add("Cut" to RectF(ax - cutW, top, ax, bot)); ax -= cutW + dp(4f)
        rects.add("Copy" to RectF(ax - copyW, top, ax, bot)); ax -= copyW + dp(4f)
        if (showMore) {
            rects.add("More" to RectF(ax - moreW, top, ax, bot))
        }
        return rects
    }

    private fun handleSelectionBarTap(x: Float) {
        for ((name, rect) in getSelectionBarButtonRects()) {
            if (x >= rect.left && x <= rect.right) {
                when (name) {
                    "close" -> exitSelectionMode()
                    "Copy" -> {
                        val names = selectedIndices.mapNotNull { fm.entries.getOrNull(it)?.name }
                        if (fm.copyEntries(names)) showToast("Copied ${names.size} item${if (names.size != 1) "s" else ""}")
                        exitSelectionMode()
                    }
                    "Cut" -> {
                        val names = selectedIndices.mapNotNull { fm.entries.getOrNull(it)?.name }
                        if (fm.cutEntries(names)) showToast("Cut ${names.size} item${if (names.size != 1) "s" else ""}")
                        exitSelectionMode()
                    }
                    "Del" -> performBatchDelete()
                    "All" -> selectAll()
                    "More" -> showMoreMenu()
                }
                return
            }
        }
    }

    private fun performBatchDelete() {
        val count = selectedIndices.size
        if (count == 0) return
        AlertDialog.Builder(context)
            .setTitle("Delete")
            .setMessage("Permanently delete $count item${if (count != 1) "s" else ""}?")
            .setPositiveButton("Delete") { _, _ ->
                val names = selectedIndices.mapNotNull { fm.entries.getOrNull(it)?.name }
                var ok = true
                for (n in names) if (!fm.deleteEntry(n)) ok = false
                if (ok) showToast("Deleted") else showToast("Some items failed to delete")
                exitSelectionMode()
                invalidate()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showMoreMenu() {
        if (selectedIndices.size != 1) return
        val idx = selectedIndices.first()
        val e = fm.entries.getOrNull(idx) ?: return
        val options: List<Pair<String, () -> Unit>> = listOf(
            "Open" to { exitSelectionMode(); openEntry(idx) },
            "Rename" to { promptRename(idx) },
            "Copy" to { fm.copyEntries(listOf(e.name)); showToast("Copied"); exitSelectionMode() },
            "Cut" to { fm.cutEntries(listOf(e.name)); showToast("Cut"); exitSelectionMode() }
        )
        AlertDialog.Builder(context)
            .setTitle(e.name)
            .setItems(options.map { it.first }.toTypedArray()) { _, which ->
                options[which].second()
            }
            .show()
    }

    private fun drawSelectionBar(c: Canvas) {
        val rects = getSelectionBarButtonRects()
        val cy = topBarH / 2
        text.color = cText
        text.textSize = dp(15f)
        val title = "${selectedIndices.size} selected"
        val titleX = padX + dp(48f)
        val copyRect = rects.firstOrNull { it.first == "Copy" }
        val titleMaxW = (copyRect?.second?.left ?: width.toFloat()) - titleX - dp(8f)
        if (titleMaxW > 0) {
            c.save()
            c.clipRect(titleX, 0f, titleX + titleMaxW, topBarH)
            c.drawText(title, titleX, cy + dp(5f), text)
            c.restore()
        }
        text.textSize = dp(13f)
        for ((name, rect) in rects) {
            if (name == "close") {
                stroke.color = cText; stroke.strokeWidth = dp(2f)
                val ccx = rect.centerX()
                c.drawLine(ccx - dp(7f), cy - dp(7f), ccx + dp(7f), cy + dp(7f), stroke)
                c.drawLine(ccx - dp(7f), cy + dp(7f), ccx + dp(7f), cy - dp(7f), stroke)
            } else {
                fill.color = Color.rgb(55, 55, 55)
                c.drawRoundRect(rect, dp(4f), dp(4f), fill)
                text.color = cText
                val tw = text.measureText(name)
                c.drawText(name, rect.centerX() - tw / 2, cy + dp(5f), text)
            }
        }
        text.textSize = dp(14f)
    }

    /**
     * Opens the live filter dialog.
     *
     * The EditText is wired to [FileManager.setFilter] through a
     * TextWatcher, so the listing re-filters on every keystroke instead
     * of waiting for the dialog to close. OK keeps the current value;
     * Clear empties the filter; Cancel (button, back press, or tap
     * outside) restores the filter that was active when the dialog
     * opened. Exits selection mode on every change, since the entries
     * list is rebuilt and old row indices become stale.
     */
    private fun showFilterDialog() {
        val input = EditText(context)
        input.setText(fm.filter)
        input.setSelection(input.text.length)
        input.hint = "Filter files..."
        val original = fm.filter
        input.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                fm.setFilter(s?.toString() ?: "")
                if (selectionMode) exitSelectionMode()
                invalidate()
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })
        AlertDialog.Builder(context)
            .setTitle("Filter")
            .setView(input)
            .setPositiveButton("OK", null)
            .setNeutralButton("Clear") { _, _ ->
                fm.setFilter("")
                invalidate()
            }
            .setNegativeButton("Cancel") { _, _ ->
                fm.setFilter(original)
                invalidate()
            }
            .setOnCancelListener {
                fm.setFilter(original)
                invalidate()
            }
            .show()
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
            if (selectionMode) { handleSelectionBarTap(x); return }
            val filterBtnX = width - padX - btnSize
            if (x >= filterBtnX && x < filterBtnX + btnSize) { showFilterDialog(); return }
            if (x < padX + btnSize) { sidebarOpen = !sidebarOpen; postInvalidateOnAnimation(); return }
            if (x < padX + 2 * btnSize + dp(4f)) { goBack(); return }
            if (x < padX + 3 * btnSize + dp(8f)) { goUp(); return }
            return
        }
        if (y < topBarH + headerH) {
            if (selectionMode) return
            val col = headerColumnAt(x)
            if (col != null) {
                fm.toggleSort(col)
                invalidate()
            }
            return
        }

        if (!selectionMode) {
            val fabCx = width - dp(36f)
            val fabCy = height - statusBarH - dp(36f)
            val dx = x - fabCx
            val dy = y - fabCy
            if (dx * dx + dy * dy <= dp(30f) * dp(30f)) {
                showFabMenu()
                return
            }
        }

        val idx = rowAt(y)
        if (idx < 0) {
            if (selectionMode) exitSelectionMode()
            return
        }
        if (selectionMode) {
            toggleSelection(idx)
            return
        }
        val now = SystemClock.uptimeMillis()
        if (idx == lastTapIdx && now - lastTapT < 350) { openEntry(idx); lastTapIdx = -1 }
        else { selIdx = idx; lastTapIdx = idx; lastTapT = now }
        invalidate()
    }

    private fun openEntry(idx: Int) {
        val e = fm.entries[idx]
        if (e.isDirectory) {
            navigateTo(fm.currentPath.trimEnd('/') + "/" + e.name)
            return
        }
        if (e.isBrokenLink) {
            showToast("Broken link: ${e.name}")
            return
        }
        val fullPath = fm.currentPath.trimEnd('/') + "/" + e.name
        val uri = Uri.Builder()
            .scheme("content")
            .authority("com.mewfm.files")
            .path(fullPath)
            .build()
        val mime = try {
            context.contentResolver.getType(uri) ?: "*/*"
        } catch (ex: Exception) {
            "*/*"
        }
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, mime)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        try {
            context.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            showToast("No app can open ${e.name}")
        } catch (ex: Exception) {
            showToast("Failed to open ${e.name}")
        }
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
        if (!sidebarOpen && !selectionMode) drawFab(c)
        if (toastMsg.isNotEmpty()) drawToast(c, now)
    }

    private fun drawSortArrow(c: Canvas, cx: Float, cy: Float, ascending: Boolean) {
        fill.color = cText
        val s = dp(3.5f)
        val p = Path()
        if (ascending) {
            p.moveTo(cx - s, cy + s * 0.6f)
            p.lineTo(cx + s, cy + s * 0.6f)
            p.lineTo(cx, cy - s * 0.6f)
        } else {
            p.moveTo(cx - s, cy - s * 0.6f)
            p.lineTo(cx + s, cy - s * 0.6f)
            p.lineTo(cx, cy + s * 0.6f)
        }
        p.close()
        c.drawPath(p, fill)
    }

    /**
     * Maps an x-coordinate inside the header row to a sortable column.
     * NAME occupies the left side of the header, SIZE the right side, so
     * tapping anywhere in each region toggles the corresponding sort
     * field. Returns null when the tap falls outside both regions.
     */
    private fun headerColumnAt(x: Float): SortField? {
        val split = width - dp(90f)
        if (x >= padX && x < split) return SortField.Name
        if (x >= split) return SortField.Size
        return null
    }

    private fun drawTopBar(c: Canvas) {
        fill.color = cTop; c.drawRect(0f, 0f, width.toFloat(), topBarH, fill)
        fill.color = cSep; c.drawRect(0f, topBarH - 1f, width.toFloat(), topBarH, fill)
        if (selectionMode) { drawSelectionBar(c); return }
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
        val filterBtnX = width - padX - btnSize
        val pathX = padX + 3 * btnSize + dp(16f)
        val pathEndX = filterBtnX - dp(4f)
        c.save(); c.clipRect(pathX, 0f, pathEndX, topBarH)
        c.drawText(fm.currentPath, pathX, hy + dp(5f), text)
        c.restore()

        val fx = filterBtnX + btnSize / 2
        val filterActive = fm.filter.isNotEmpty()
        stroke.color = if (filterActive) cText else cTextDim
        stroke.strokeWidth = dp(1.5f)
        val glassR = dp(5f)
        val glassCx = fx - dp(2f)
        val glassCy = hy - dp(2f)
        c.drawCircle(glassCx, glassCy, glassR, stroke)
        val handleX1 = glassCx + glassR * 0.707f
        val handleY1 = glassCy + glassR * 0.707f
        c.drawLine(handleX1, handleY1, fx + dp(6f), hy + dp(6f), stroke)
    }

    private fun drawHeader(c: Canvas) {
        fill.color = cHeader; c.drawRect(0f, topBarH, width.toFloat(), topBarH + headerH, fill)
        small.color = cTextDim
        val headerCy = topBarH + headerH / 2 + dp(4f)
        val arrowCy = topBarH + headerH / 2 + dp(0.5f)

        val nameText = "NAME"
        c.drawText(nameText, padX, headerCy, small)
        if (fm.sortField == SortField.Name) {
            val nameW = small.measureText(nameText)
            drawSortArrow(c, padX + nameW + dp(6f), arrowCy, fm.sortAscending)
        }

        val sizeText = "SIZE"
        val sizeW = small.measureText(sizeText)
        val sizeX = width - padX - sizeW
        c.drawText(sizeText, sizeX, headerCy, small)
        if (fm.sortField == SortField.Size) {
            drawSortArrow(c, sizeX - dp(8f), arrowCy, fm.sortAscending)
        }
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
            val isSel = if (selectionMode) selectedIndices.contains(i) else (i == selIdx)
            if (isSel) { fill.color = cSel; c.drawRect(0f, y, width.toFloat(), y + rowH, fill) }
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
        val itemText = if (n == 1) "1 item" else "$n items"
        val label = if (fm.filter.isNotEmpty()) "Filter \"${fm.filter}\": $itemText" else itemText
        c.save()
        c.clipRect(padX, height - statusBarH, width - padX, height.toFloat())
        c.drawText(label, padX, height - statusBarH / 2 + dp(4f), small)
        c.restore()
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
