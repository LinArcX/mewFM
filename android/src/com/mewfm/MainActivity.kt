package com.mewfm

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val text = TextView(this)
        text.layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        text.setBackgroundColor(Color.rgb(30, 30, 30))
        text.text = "mewFM"
        text.setTextColor(Color.WHITE)
        text.textSize = 24f
        text.gravity = Gravity.CENTER
        setContentView(text)
    }
}
