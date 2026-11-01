package com.mewfm

import android.app.Activity
import android.os.Bundle

class MainActivity : Activity() {
    private lateinit var view: FileListView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        view = FileListView(this)
        setContentView(view)
        view.navigateTo(filesDir.absolutePath)
    }

    override fun onBackPressed() {
        view.goUp()
    }
}
