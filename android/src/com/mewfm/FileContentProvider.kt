package com.mewfm

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.webkit.MimeTypeMap
import java.io.File
import java.io.FileNotFoundException

/**
 * Minimal read/write content provider that exposes files from the
 * filesystem to other applications.
 *
 * Used by [FileListView.openEntry] to hand a `content://` URI to the
 * external application chosen by the `ACTION_VIEW` intent. Android
 * forbids passing `file://` URIs between applications since API 24,
 * so a content provider is required.
 *
 * Authority: `com.mewfm.files`. The URI path is the absolute file
 * path, for example `content://com.mewfm.files/storage/emulated/0/notes.txt`.
 *
 * The provider is declared `exported="false"` in the manifest and
 * relies on `Intent.FLAG_GRANT_READ_URI_PERMISSION` for per-launch,
 * per-recipient access.
 */
class FileContentProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
        val path = uri.path ?: throw FileNotFoundException("No path in URI: $uri")
        val file = File(path)
        if (!file.exists()) {
            throw FileNotFoundException(path)
        }
        val flags = when (mode) {
            "r" -> ParcelFileDescriptor.MODE_READ_ONLY
            "w" -> ParcelFileDescriptor.MODE_WRITE_ONLY or ParcelFileDescriptor.MODE_CREATE
            "wt" -> ParcelFileDescriptor.MODE_WRITE_ONLY or ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_TRUNCATE
            "wa" -> ParcelFileDescriptor.MODE_WRITE_ONLY or ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_APPEND
            "rw" -> ParcelFileDescriptor.MODE_READ_WRITE or ParcelFileDescriptor.MODE_CREATE
            "rwt" -> ParcelFileDescriptor.MODE_READ_WRITE or ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_TRUNCATE
            else -> throw IllegalArgumentException("Unsupported mode: $mode")
        }
        return ParcelFileDescriptor.open(file, flags)
    }

    override fun getType(uri: Uri): String {
        val path = uri.path ?: return "application/octet-stream"
        val ext = path.substringAfterLast('.', "").lowercase()
        if (ext.isEmpty()) {
            return "application/octet-stream"
        }
        return MimeTypeMap.getSingleton()
            .getMimeTypeFromExtension(ext) ?: "application/octet-stream"
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(
        uri: Uri,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int = 0
}
