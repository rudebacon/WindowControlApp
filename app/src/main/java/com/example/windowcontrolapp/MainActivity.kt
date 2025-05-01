package com.example.windowcontrolapp

import android.graphics.Color
import android.os.Bundle
import android.widget.Button
import android.widget.NumberPicker
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.delay

class MainActivity : AppCompatActivity() {
    private val esp32Url = "http://192.168.1.195"

    private lateinit var btnAuto: Button
    private lateinit var btnOpen: Button
    private lateinit var btnClose: Button
    private lateinit var btnStop: Button
    private lateinit var textTemp: TextView
    private lateinit var textRoomTemp: TextView

    // Track the current ongoing command
    private var currentCommandJob: Job? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val numberPicker = findViewById<NumberPicker>(R.id.numberPicker)
        numberPicker.minValue = 5
        numberPicker.maxValue = 30

        textTemp = findViewById(R.id.textTemp)
        btnAuto = findViewById(R.id.btnAuto)
        btnOpen = findViewById(R.id.btnOpen)
        btnClose = findViewById(R.id.btnClose)
        btnStop = findViewById(R.id.btnStop)
        textRoomTemp = findViewById(R.id.textRoomTemp)

        updateRoomTempDisplay()
        updateTempDisplay()
        updateModeHighlight()

        findViewById<Button>(R.id.btnSetTemp).setOnClickListener {
            val selectedTemp = numberPicker.value
            sendCommand("/set-temp?value=$selectedTemp") {
                updateTempDisplay()
            }
        }

        btnAuto.setOnClickListener {
            sendCommand("/auto") {
                highlightButton("auto")
            }
        }

        btnOpen.setOnClickListener {
            sendCommand("/open") {
                highlightButton("open")
            }
        }

        btnClose.setOnClickListener {
            sendCommand("/close") {
                highlightButton("close")
            }
        }

        btnStop.setOnClickListener {
            sendCommand("/stop") {
                highlightButton("stop")
            }
        }

        // Update room temperature every 10 seconds
        lifecycleScope.launch {
            while (true) {
                updateRoomTempDisplay()  // Update the room temp
                updateTempDisplay() // Update the set temp
                delay(10000)  // 10 second delay before repeating
            }
        }
    }

    private fun sendCommand(path: String, onComplete: (() -> Unit)? = null) {
        // Cancel the previous job if it's still running
        currentCommandJob?.cancel()

        // Launch a new job to send the command
        currentCommandJob = lifecycleScope.launch {
            try {
                val url = URL("$esp32Url$path")
                withContext(Dispatchers.IO) {
                    with(url.openConnection() as HttpURLConnection) {
                        requestMethod = "GET"
                        inputStream.bufferedReader().readText()
                    }
                }
                onComplete?.invoke()  // Invoke the callback after completion
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun updateTempDisplay() {
        lifecycleScope.launch {
            try {
                val url = URL("$esp32Url/get-temp")
                val response = withContext(Dispatchers.IO) {
                    with(url.openConnection() as HttpURLConnection) {
                        requestMethod = "GET"
                        inputStream.bufferedReader().readText()
                    }
                }
                textTemp.text = "Set Temp: $response°C"
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun updateModeHighlight() {
        lifecycleScope.launch {
            try {
                val url = URL("$esp32Url/get-mode")
                val mode = withContext(Dispatchers.IO) {
                    with(url.openConnection() as HttpURLConnection) {
                        requestMethod = "GET"
                        inputStream.bufferedReader().readText()
                    }
                }.trim()
                highlightButton(mode)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun highlightButton(mode: String) {
        val yellow = Color.parseColor("#FFFF00")
        val setTempButton = findViewById<Button>(R.id.btnSetTemp)
        val defaultBackground = setTempButton.background

        val buttons = mapOf(
            "auto" to btnAuto,
            "open" to btnOpen,
            "close" to btnClose,
            "stop" to btnStop
        )

        buttons.forEach { (key, button) ->
            if (key == mode) {
                button.setBackgroundColor(yellow)
            } else {
                button.background = defaultBackground
            }
        }
    }

    private fun updateRoomTempDisplay() {
        lifecycleScope.launch {
            try {
                val url = URL("$esp32Url/get-room-temp")
                val response = withContext(Dispatchers.IO) {
                    with(url.openConnection() as HttpURLConnection) {
                        requestMethod = "GET"
                        inputStream.bufferedReader().readText()
                    }
                }
                textRoomTemp.text = "Room Temp: $response°C"
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

