package com.mewfm

import android.os.Handler
import android.os.Looper
import java.io.File
import java.nio.file.Files
import java.nio.file.LinkOption
import java.nio.file.StandardCopyOption
import java.nio.file.attribute.PosixFilePermission
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors

/**
 * Column used to sort the entries of the current directory.
 */
enum class SortField {
    Name, Size, Type, Owner, Permissions, DateCreated, DateModified
}

/**
 * Identifies the current mode of the internal clipboard.
 */
enum class ClipboardMode {
    None, Copy, Cut
}

/**
 * One row in the directory listing. Both the raw metadata and the
 * pre-formatted strings are stored so the view can render each row
 * without issuing any per-frame syscalls.
 */
data class Entry(
    val name: String,
    val sizeText: String,
    val typeText: String,
    val ownerText: String,
    val permText: String,
    val createdText: String,
    val modifiedText: String,
    val sizeBytes: Long,
    val createdTime: Long,
    val modifiedTime: Long,
    val isDirectory: Boolean,
    val isExecutable: Boolean,
    val isSymlink: Boolean,
    val isBrokenLink: Boolean,
)

/**
 * Browsing state for a single directory view.
 *
 * Loads the current directory into memory, applies the filter and sort,
 * exposes back/forward/up navigation, and drives long-running copy/move
 * operations.
 *
 * Must be used from the main (UI) thread. The `Async` variants
 * ([setPathAsync], [goBackAsync], [goUpAsync], [pasteAsync]) perform
 * their filesystem work on a background executor but commit the
 * resulting state back on the main thread, so [entries] and
 * [currentPath] are never observed in a partially updated state.
 */
class FileManager {
    private val executor = Executors.newSingleThreadExecutor()
    private val handler = Handler(Looper.getMainLooper())

    var currentPath: String = "/"
        private set

    var sortField: SortField = SortField.Name
        private set
    var sortAscending: Boolean = true
        private set

    var showHidden: Boolean = false
        private set
    var followSymlinks: Boolean = true
        private set
    var filter: String = ""
        private set

    var clipboardMode: ClipboardMode = ClipboardMode.None
        private set
    private val clipboardNames = mutableListOf<String>()
    private var clipboardSource: String = ""
    private var pasteInProgress: Boolean = false

    private val entryList = mutableListOf<Entry>()
    private val unfiltered = mutableListOf<Entry>()
    private val backStack = ArrayDeque<String>()
    private val forwardStack = ArrayDeque<String>()

    val entries: List<Entry> get() = entryList

    init {
        loadPath("/")
    }

    fun setPath(path: String): Boolean {
        if (path == currentPath) return true
        val old = currentPath
        if (!loadPath(path)) return false
        backStack.addLast(old)
        forwardStack.clear()
        return true
    }

    fun setPathAsync(path: String, onComplete: (Boolean) -> Unit) {
        if (path == currentPath) {
            onComplete(true)
            return
        }
        val old = currentPath
        val hidden = showHidden
        executor.execute {
            val newEntries = buildEntries(path, hidden)
            handler.post {
                if (newEntries != null) {
                    commitPath(path, newEntries)
                    backStack.addLast(old)
                    forwardStack.clear()
                    onComplete(true)
                } else {
                    onComplete(false)
                }
            }
        }
    }

    fun goBackAsync(onComplete: (Boolean) -> Unit) {
        if (backStack.isEmpty()) { onComplete(false); return }
        val target = backStack.removeLast()
        val old = currentPath
        val hidden = showHidden
        executor.execute {
            val newEntries = buildEntries(target, hidden)
            handler.post {
                if (newEntries != null) {
                    commitPath(target, newEntries)
                    forwardStack.addLast(old)
                    onComplete(true)
                } else {
                    backStack.addLast(target)
                    onComplete(false)
                }
            }
        }
    }

    fun goUpAsync(onComplete: (Boolean) -> Unit) {
        val parent = parentOf(currentPath) ?: run { onComplete(false); return }
        if (parent == currentPath) { onComplete(false); return }
        setPathAsync(parent, onComplete)
    }

    fun resetTo(path: String) {
        backStack.clear()
        forwardStack.clear()
        loadPath(path)
    }

    fun goBack(): Boolean {
        if (backStack.isEmpty()) return false
        val target = backStack.removeLast()
        val old = currentPath
        if (!loadPath(target)) {
            backStack.addLast(target)
            return false
        }
        forwardStack.addLast(old)
        return true
    }

    fun goForward(): Boolean {
        if (forwardStack.isEmpty()) return false
        val target = forwardStack.removeLast()
        val old = currentPath
        if (!loadPath(target)) {
            forwardStack.addLast(target)
            return false
        }
        backStack.addLast(old)
        return true
    }

    fun goUp(): Boolean {
        val parent = parentOf(currentPath) ?: return false
        if (parent == currentPath) return false
        return setPath(parent)
    }

    fun canGoBack(): Boolean = backStack.isNotEmpty()
    fun canGoForward(): Boolean = forwardStack.isNotEmpty()

    fun setShowHidden(value: Boolean) {
        if (showHidden == value) return
        showHidden = value
        loadPath(currentPath)
    }

    fun setFollowSymlinks(value: Boolean) {
        followSymlinks = value
    }

    fun setFilter(value: String) {
        if (filter == value) return
        filter = value
        applyFilter()
    }

    fun setSort(field: SortField, ascending: Boolean) {
        if (sortField == field && sortAscending == ascending) return
        sortField = field
        sortAscending = ascending
        sortEntries()
    }

    fun toggleSort(field: SortField) {
        if (sortField == field) {
            sortAscending = !sortAscending
        } else {
            sortField = field
            sortAscending = true
        }
        sortEntries()
    }

    fun refresh(): Boolean = loadPath(currentPath)

    fun createDirectory(name: String): Boolean {
        if (name.isEmpty()) return false
        val dir = File(currentPath, name)
        if (dir.mkdirs()) {
            return loadPath(currentPath)
        }
        return false
    }

    fun createFile(name: String): Boolean {
        if (name.isEmpty()) return false
        val file = File(currentPath, name)
        return try {
            if (file.createNewFile()) {
                loadPath(currentPath)
            } else {
                false
            }
        } catch (e: Exception) {
            false
        }
    }

    fun renameEntry(oldName: String, newName: String): Boolean {
        if (oldName.isEmpty() || newName.isEmpty()) return false
        if (oldName == newName) return true
        if (oldName.contains('/') || newName.contains('/')) return false
        val src = File(currentPath, oldName)
        val dst = File(currentPath, newName)
        if (!src.exists() || dst.exists()) return false
        if (!src.renameTo(dst)) return false
        loadPath(currentPath)
        return true
    }

    fun deleteEntry(name: String): Boolean {
        if (name.isEmpty() || name == "." || name == "..") return false
        if (name.contains('/')) return false
        val f = File(currentPath, name)
        if (!f.exists()) return false
        if (!f.deleteRecursively()) return false
        loadPath(currentPath)
        return true
    }

    fun copyEntries(names: List<String>): Boolean {
        if (names.isEmpty()) return false
        for (n in names) {
            if (n.isEmpty() || n == "." || n == ".." || n.contains('/')) return false
            if (!File(currentPath, n).exists()) return false
        }
        clipboardNames.clear()
        clipboardNames.addAll(names)
        clipboardSource = currentPath
        clipboardMode = ClipboardMode.Copy
        return true
    }

    fun cutEntries(names: List<String>): Boolean {
        if (names.isEmpty()) return false
        for (n in names) {
            if (n.isEmpty() || n == "." || n == ".." || n.contains('/')) return false
            if (!File(currentPath, n).exists()) return false
        }
        clipboardNames.clear()
        clipboardNames.addAll(names)
        clipboardSource = currentPath
        clipboardMode = ClipboardMode.Cut
        return true
    }

    fun pasteAsync(onComplete: (Boolean) -> Unit) {
        if (pasteInProgress) { onComplete(false); return }
        if (clipboardMode == ClipboardMode.None || clipboardNames.isEmpty()) { onComplete(false); return }
        if (clipboardMode == ClipboardMode.Cut && clipboardSource == currentPath) { onComplete(false); return }
        pasteInProgress = true
        val mode = clipboardMode
        val source = clipboardSource
        val names = clipboardNames.toList()
        val dest = currentPath
        executor.execute {
            var success = true
            for (name in names) {
                val src = File(source, name)
                if (!src.exists()) { success = false; break }
                val dstName = uniqueNameIn(dest, name)
                if (dstName.isEmpty()) { success = false; break }
                val dst = File(dest, dstName)
                if (!copyRecursive(src, dst)) { success = false; break }
                if (mode == ClipboardMode.Cut) {
                    if (!src.deleteRecursively()) { success = false; break }
                }
            }
            handler.post {
                pasteInProgress = false
                if (success) {
                    clipboardNames.clear()
                    clipboardSource = ""
                    clipboardMode = ClipboardMode.None
                    loadPath(currentPath)
                }
                onComplete(success)
            }
        }
    }

    private fun uniqueNameIn(dir: String, name: String): String {
        if (!File(dir, name).exists()) return name
        val dot = name.lastIndexOf('.')
        val stem = if (dot > 0) name.substring(0, dot) else name
        val ext = if (dot > 0) name.substring(dot) else ""
        var i = 2
        while (i < 1000) {
            val candidate = "$stem ($i)$ext"
            if (!File(dir, candidate).exists()) return candidate
            i++
        }
        return ""
    }

    private fun copyRecursive(src: File, dst: File): Boolean {
        return try {
            if (Files.isSymbolicLink(src.toPath())) {
                val target = Files.readSymbolicLink(src.toPath())
                Files.createSymbolicLink(dst.toPath(), target)
            } else if (src.isDirectory) {
                if (!dst.exists() && !dst.mkdirs()) return false
                val children = src.listFiles() ?: return false
                for (child in children) {
                    if (!copyRecursive(child, File(dst, child.name))) return false
                }
            } else {
                Files.copy(src.toPath(), dst.toPath(), StandardCopyOption.REPLACE_EXISTING)
            }
            true
        } catch (e: Exception) {
            false
        }
    }

    private fun buildEntries(path: String, showHiddenFiles: Boolean): List<Entry>? {
        val dir = File(path)
        if (!dir.isDirectory) return null
        val children = runCatching { dir.listFiles() }.getOrNull() ?: return null

        // Build into a temporary list first so a partial failure leaves
        // the manager on its previous directory.
        val newEntries = ArrayList<Entry>(children.size)
        for (child in children) {
            if (!showHiddenFiles && child.name.startsWith(".")) continue
            buildEntry(child)?.let { newEntries.add(it) }
        }
        return newEntries
    }

    /**
     * Applies a previously built entry list as the new current directory.
     *
     * Must be called on the main (UI) thread. The filesystem walk is done
     * by [buildEntries] on the executor; this method is the only place that
     * mutates [currentPath], [unfiltered], and [entryList], so the UI never
     * observes a partially committed directory.
     */
    private fun commitPath(path: String, newEntries: List<Entry>) {
        currentPath = path
        unfiltered.clear()
        unfiltered.addAll(newEntries)
        applyFilter()
    }

    private fun loadPath(path: String): Boolean {
        val newEntries = buildEntries(path, showHidden) ?: return false
        commitPath(path, newEntries)
        return true
    }

    private fun applyFilter() {
        entryList.clear()
        if (filter.isEmpty()) {
            entryList.addAll(unfiltered)
        } else {
            for (e in unfiltered) {
                if (e.name.contains(filter)) entryList.add(e)
            }
        }
        sortEntries()
    }

    private fun sortEntries() {
        val cmp = Comparator<Entry> { a, b ->
            if (a.isDirectory != b.isDirectory) {
                return@Comparator if (a.isDirectory) -1 else 1
            }
            val c = when (sortField) {
                SortField.Name -> a.name.compareTo(b.name)
                SortField.Size -> a.sizeBytes.compareTo(b.sizeBytes)
                SortField.Type -> a.typeText.compareTo(b.typeText)
                SortField.Owner -> a.ownerText.compareTo(b.ownerText)
                SortField.Permissions -> a.permText.compareTo(b.permText)
                SortField.DateCreated -> a.createdTime.compareTo(b.createdTime)
                SortField.DateModified -> a.modifiedTime.compareTo(b.modifiedTime)
            }
            if (c == 0 && sortField != SortField.Name) a.name.compareTo(b.name) else c
        }
        entryList.sortWith(cmp)
        if (!sortAscending) entryList.reverse()
    }

    private fun buildEntry(f: File): Entry? {
        return runCatching {
            val path = f.toPath()
            val isLink = Files.isSymbolicLink(path)
            val isBroken = isLink && !f.exists()
            if (isBroken) {
                return@runCatching Entry(
                    name = f.name,
                    sizeText = "-",
                    typeText = "Broken link",
                    ownerText = "-",
                    permText = "-",
                    createdText = "-",
                    modifiedText = "-",
                    sizeBytes = 0L,
                    createdTime = 0L,
                    modifiedTime = 0L,
                    isDirectory = false,
                    isExecutable = false,
                    isSymlink = true,
                    isBrokenLink = true,
                )
            }

            val isDir = f.isDirectory
            val isExec = !isDir && f.canExecute()

            val uid = runCatching {
                Files.getAttribute(path, "unix:uid", LinkOption.NOFOLLOW_LINKS) as Int
            }.getOrNull()
            val ownerText = uid?.let { ownerNameForUid(it) } ?: "-"

            val perms = runCatching {
                Files.getPosixFilePermissions(path, LinkOption.NOFOLLOW_LINKS)
            }.getOrNull()
            val permText = perms?.let { permToString(it, isDir) } ?: "-"

            // Android does not cheaply expose ctime; use mtime for both
            // until a later task refines this via statx if desired.
            val mtime = f.lastModified()

            val sizeBytes: Long
            val sizeText: String
            val rawType: String
            if (isDir) {
                sizeBytes = 0L
                sizeText = countChildren(f)
                rawType = "Folder"
            } else {
                sizeBytes = f.length()
                sizeText = formatSize(sizeBytes)
                rawType = typeForName(f.name)
            }
            val typeText = if (isLink) "$rawType ->" else rawType

            Entry(
                name = f.name,
                sizeText = sizeText,
                typeText = typeText,
                ownerText = ownerText,
                permText = permText,
                createdText = formatTime(mtime),
                modifiedText = formatTime(mtime),
                sizeBytes = sizeBytes,
                createdTime = mtime,
                modifiedTime = mtime,
                isDirectory = isDir,
                isExecutable = isExec,
                isSymlink = isLink,
                isBrokenLink = false,
            )
        }.getOrNull()
    }

    private fun permToString(perms: Set<PosixFilePermission>, isDir: Boolean): String {
        val sb = StringBuilder(10)
        sb.append(if (isDir) 'd' else '-')
        sb.append(if (PosixFilePermission.OWNER_READ in perms) 'r' else '-')
        sb.append(if (PosixFilePermission.OWNER_WRITE in perms) 'w' else '-')
        sb.append(if (PosixFilePermission.OWNER_EXECUTE in perms) 'x' else '-')
        sb.append(if (PosixFilePermission.GROUP_READ in perms) 'r' else '-')
        sb.append(if (PosixFilePermission.GROUP_WRITE in perms) 'w' else '-')
        sb.append(if (PosixFilePermission.GROUP_EXECUTE in perms) 'x' else '-')
        sb.append(if (PosixFilePermission.OTHERS_READ in perms) 'r' else '-')
        sb.append(if (PosixFilePermission.OTHERS_WRITE in perms) 'w' else '-')
        sb.append(if (PosixFilePermission.OTHERS_EXECUTE in perms) 'x' else '-')
        return sb.toString()
    }

    private fun ownerNameForUid(uid: Int): String = when (uid) {
        0 -> "root"
        1000 -> "system"
        else -> uid.toString()
    }

    private fun countChildren(f: File): String {
        val children = runCatching { f.listFiles() }.getOrNull() ?: return "-"
        return if (children.size == 1) "1 item" else "${children.size} items"
    }

    private fun formatSize(bytes: Long): String {
        val units = arrayOf("B", "KB", "MB", "GB", "TB")
        var value = bytes.toDouble()
        var unit = 0
        while (value >= 1024.0 && unit < 4) {
            value /= 1024.0
            unit++
        }
        return if (unit == 0) {
            String.format(Locale.US, "%d %s", bytes, units[unit])
        } else {
            String.format(Locale.US, "%.1f %s", value, units[unit])
        }
    }

    private fun formatTime(t: Long): String {
        if (t == 0L) return "-"
        return SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date(t))
    }

    private fun typeForName(name: String): String {
        val dot = name.lastIndexOf('.')
        if (dot < 0 || dot == 0 || dot + 1 >= name.length) return "File"
        return "${name.substring(dot + 1)} file"
    }

    companion object {
        fun parentOf(path: String): String? {
            if (path.isEmpty() || path == "/") return null
            var p = path
            while (p.length > 1 && p.endsWith("/")) p = p.dropLast(1)
            val slash = p.lastIndexOf('/')
            if (slash < 0) return null
            return if (slash == 0) "/" else p.substring(0, slash)
        }
    }
}
