package com.mewfm

import android.os.Handler
import android.os.Looper
import java.io.File
import java.nio.file.Files
import java.nio.file.LinkOption
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
 * exposes back/forward/up navigation, and (in later tasks) will drive
 * long-running copy/move operations.
 *
 * NOT thread-safe: all calls must happen on the same thread (the UI thread).
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
        executor.execute {
            val success = loadPath(path)
            handler.post {
                if (success) {
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
        executor.execute {
            val success = loadPath(target)
            handler.post {
                if (success) {
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

    private fun loadPath(path: String): Boolean {
        val dir = File(path)
        if (!dir.isDirectory) return false
        val children = runCatching { dir.listFiles() }.getOrNull() ?: return false

        // Build into a temporary list first so a partial failure leaves
        // the manager on its previous directory.
        val newEntries = ArrayList<Entry>(children.size)
        for (child in children) {
            if (!showHidden && child.name.startsWith(".")) continue
            buildEntry(child)?.let { newEntries.add(it) }
        }

        currentPath = path
        unfiltered.clear()
        unfiltered.addAll(newEntries)
        applyFilter()
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
