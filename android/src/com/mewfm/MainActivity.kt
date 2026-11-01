package com.mewfm

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.os.Bundle
import android.view.ViewGroup
import android.widget.ScrollView
import android.widget.TextView
import java.io.File

/**
 * Temporary smoke-test host for Task 2.
 *
 * Lists the app-private directory and the external-storage root, then
 * exercises sort and filter. Replaced by the Canvas view in Task 3.
 */
class MainActivity : Activity() {
    private lateinit var output: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val scroll = ScrollView(this)
        scroll.setBackgroundColor(Color.rgb(30, 30, 30))
        output = TextView(this)
        output.layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )
        output.setTextColor(Color.WHITE)
        output.textSize = 11f
        output.typeface = Typeface.MONOSPACE
        output.setPadding(16, 16, 16, 16)
        scroll.addView(output)
        setContentView(scroll)
        runSmokeTest()
    }

    private fun runSmokeTest() {
        val sb = StringBuilder()
        try {
            // Seed the app dir so the listing is non-empty even without
            // external-storage permission.
            File(filesDir, "hello.txt").writeText("Hello from mewFM on Android\n")
            File(filesDir, "data.csv").writeText("name,size,type\na.txt,12,file\nb.txt,34,file\n")

            val fm = FileManager()
            dump(sb, fm, "app private dir", filesDir.absolutePath)

            val externalCandidates = listOf("/storage/emulated/0", "/sdcard")
            for (candidate in externalCandidates) {
                if (File(candidate).isDirectory) {
                    dump(sb, fm, "external storage", candidate)
                    break
                }
            }

            // Sort test on the app dir.
            fm.setPath(filesDir.absolutePath)
            fm.setSort(SortField.Size, false)
            sb.append("[sort by size desc]\n")
            fm.entries.take(5).forEach { e ->
                sb.append(pad(e.sizeText, 12)).append(e.name).append('\n')
            }

            // Filter test.
            fm.setFilter("txt")
            sb.append("\n[filter \"txt\"] -> ").append(fm.entries.size).append(" entries\n")
            fm.entries.forEach { e -> sb.append("  ").append(e.name).append('\n') }

            sb.append("\nOK\n")
        } catch (e: Throwable) {
            sb.append("ERROR: ").append(e.message).append('\n')
            sb.append(e.stackTraceToString())
        }
        output.text = sb.toString()
    }

    private fun dump(sb: StringBuilder, fm: FileManager, label: String, path: String) {
        sb.append("== ").append(label).append(" ==\n")
        sb.append("path: ").append(path).append('\n')
        val ok = fm.setPath(path)
        sb.append("setPath -> ").append(ok).append('\n')
        sb.append("current: ").append(fm.currentPath).append('\n')
        sb.append("entries: ").append(fm.entries.size).append('\n')
        fm.entries.take(15).forEach { e ->
            sb.append(pad(e.permText, 11)).append(' ')
                .append(pad(e.ownerText, 8)).append(' ')
                .append(pad(e.sizeText, 10)).append(' ')
                .append(pad(e.typeText, 16)).append(' ')
                .append(pad(e.modifiedText, 17)).append(' ')
                .append(e.name).append('\n')
        }
        sb.append('\n')
    }

    private fun pad(s: String, w: Int): String =
        if (s.length >= w) s else s + " ".repeat(w - s.length)
}
