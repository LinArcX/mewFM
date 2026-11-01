package com.mewfm

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings

class MainActivity : Activity() {
    private lateinit var view: FileListView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        view = FileListView(this)
        setContentView(view)
        
        if (checkAndRequestPermissions()) {
            view.navigateTo("/storage/emulated/0")
        } else {
            view.navigateTo(filesDir.absolutePath)
        }
    }

    private fun checkAndRequestPermissions(): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
                    intent.addCategory("android.intent.category.DEFAULT")
                    intent.data = Uri.parse(String.format("package:%s", applicationContext.packageName))
                    startActivity(intent)
                } catch (e: Exception) {
                    val intent = Intent()
                    intent.action = Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION
                    startActivity(intent)
                }
                return false
            }
        }
        return true
    }

    override fun onResume() {
        super.onResume()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager() && view.fm.currentPath == filesDir.absolutePath) {
                view.navigateTo("/storage/emulated/0")
            }
        }
    }

    override fun onBackPressed() {
        view.goUp()
    }
}
