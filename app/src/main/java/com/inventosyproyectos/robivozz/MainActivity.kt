package com.inventosyproyectos.robivozz

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.widget.Button
import android.widget.TextView
import java.net.HttpURLConnection
import java.net.URL
import java.text.Normalizer
import java.util.Locale

class MainActivity : Activity() {
    private lateinit var btnHablar: Button
    private lateinit var txtEstado: TextView
    private lateinit var txtReconocido: TextView
    private lateinit var txtDialogo: TextView
    private var reconocedor: SpeechRecognizer? = null

    companion object {
        private const val PERMISO_MICROFONO = 100
        private const val IP_ROBI = "192.168.4.1"
    }

    private data class Comando(val ruta: String, val descripcion: String)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        btnHablar = findViewById(R.id.btnHablar)
        txtEstado = findViewById(R.id.txtEstado)
        txtReconocido = findViewById(R.id.txtReconocido)
        txtDialogo = findViewById(R.id.txtDialogo)

        if (SpeechRecognizer.isRecognitionAvailable(this)) {
            reconocedor = SpeechRecognizer.createSpeechRecognizer(this).apply {
                setRecognitionListener(object : RecognitionListener {
                    override fun onReadyForSpeech(params: Bundle?) {
                        txtEstado.text = "ESCUCHANDO AL CREADOR..."
                    }
                    override fun onBeginningOfSpeech() {}
                    override fun onRmsChanged(rmsdB: Float) {}
                    override fun onBufferReceived(buffer: ByteArray?) {}
                    override fun onEndOfSpeech() {
                        txtEstado.text = "PROCESANDO VOZ..."
                    }
                    override fun onError(error: Int) {
                        txtEstado.text = when (error) {
                            SpeechRecognizer.ERROR_NETWORK -> "ERROR: SERVICIO DE VOZ SIN RED"
                            SpeechRecognizer.ERROR_NETWORK_TIMEOUT -> "TIEMPO DE ESPERA AGOTADO"
                            SpeechRecognizer.ERROR_NO_MATCH -> "NO SE RECONOCIÓ LA FRASE DEL CREADOR"
                            SpeechRecognizer.ERROR_SPEECH_TIMEOUT -> "NO SE DETECTÓ VOZ"
                            SpeechRecognizer.ERROR_AUDIO -> "ERROR DE MICRÓFONO DEL CELULAR"
                            SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS -> "FALTA PERMISO DE MICRÓFONO"
                            SpeechRecognizer.ERROR_RECOGNIZER_BUSY -> "RECONOCEDOR OCUPADO; INTENTA OTRA VEZ"
                            SpeechRecognizer.ERROR_CLIENT -> "ERROR DEL SERVICIO DE RECONOCIMIENTO"
                            else -> "ERROR DE RECONOCIMIENTO: $error"
                        }
                    }
                    override fun onResults(results: Bundle?) {
                        val frases = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                        if (frases.isNullOrEmpty()) {
                            txtEstado.text = "NO SE RECIBIÓ NINGUNA FRASE"
                            return
                        }
                        // Se revisan las alternativas porque a veces reconoce Robi como Robbie.
                        val comando = frases.firstNotNullOfOrNull { identificarComando(normalizar(it)) }
                        txtReconocido.text = "Reconocido: ${frases[0]}"
                        if (comando == null) {
                            txtEstado.text = "FRASE NO CONFIGURADA: usa una indicación del guion"
                            txtDialogo.text = ""
                        } else {
                            txtDialogo.text = comando.descripcion
                            enviarComando(comando)
                        }
                    }
                    override fun onPartialResults(partialResults: Bundle?) {}
                    override fun onEvent(eventType: Int, params: Bundle?) {}
                })
            }
        } else {
            txtEstado.text = "NO HAY SERVICIO DE RECONOCIMIENTO DE VOZ"
        }

        btnHablar.setOnClickListener {
            if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), PERMISO_MICROFONO)
            } else escuchar()
        }
    }

    private fun escuchar() {
        val servicio = reconocedor
        if (servicio == null) {
            txtEstado.text = "NO HAY SERVICIO DE RECONOCIMIENTO DE VOZ"
            return
        }
        txtEstado.text = "PREPARANDO RECONOCIMIENTO..."
        txtReconocido.text = ""
        txtDialogo.text = ""
        val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_LANGUAGE, "es-US")
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "es-US")
            putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 5)
            putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
        }
        try {
            servicio.startListening(intent)
        } catch (_: Exception) {
            txtEstado.text = "NO SE PUDO INICIAR EL RECONOCIMIENTO"
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISO_MICROFONO) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) escuchar()
            else txtEstado.text = "PERMISO DE MICRÓFONO DENEGADO"
        }
    }

    override fun onDestroy() {
        reconocedor?.destroy()
        reconocedor = null
        super.onDestroy()
    }

    private fun normalizar(texto: String): String =
        Normalizer.normalize(texto.lowercase(Locale.ROOT), Normalizer.Form.NFD)
            .replace(Regex("\\p{Mn}+"), "")
            .replace(Regex("[^a-z0-9\\s]"), " ")
            .replace(Regex("\\s+"), " ").trim()

    private fun identificarComando(t: String): Comando? {
        // Escena 1: pista 1 y luego pista 2 se reproducen automaticamente en el ESP32.
        if (t.contains("robi habla con el publico") || t.contains("robbie habla con el publico") ||
            t.contains("robi hablale al publico")) {
            return Comando("/dialogo?p=1", "Monólogo inicial: pistas 1 y 2")
        }

        // Esta orden NO manda la pista 3 directamente: primero ejecuta sorpresa y búsqueda visual.
        if (t.contains("con quien estas hablando")) {
            return Comando("/alerta", "Ojos sorprendidos 2 s, búsqueda visual y después pista 3")
        }

        if (t.contains("hola robi") || t.contains("hola robbie")) {
            return Comando("/pista?p=4", "Pista 4 · saludo alegre")
        }
        if (t.contains("hoy vamos a instalarte unos nuevos componentes") ||
            t.contains("vamos a instalarte unos nuevos componentes")) {
            return Comando("/pista?p=5", "Pista 5 · nuevos componentes")
        }
        if (t.contains("mira lo que te he traido") || t.contains("mira lo que traje")) {
            return Comando("/pista?p=6", "Pista 6 · Robi ve las piezas")
        }
        if (t.contains("son los motores") || t.contains("motores y los modulos")) {
            return Comando("/pista?p=7", "Pista 7 · Robi pregunta por las orejas")
        }
        if (t.contains("podran moverse para ayudarte a expresar mejor tus emociones") ||
            t.contains("moverse para expresar tus emociones")) {
            return Comando("/pista?p=8", "Pistas 8 y 9 · sorpresa y alegría")
        }
        if (t.contains("vas a verte mucho mas genial") || t.contains("verte mucho mas genial")) {
            return Comando("/pista?p=10", "Pista 10 · estilo robótico")
        }
        if (t.contains("uno para cada motor") || t.contains("dvr8833")) {
            return Comando("/pista?p=11", "Pista 11 · conexión de motores")
        }
        if (t.contains("para que todos aprendan como conectar") ||
            t.contains("para que todos aprendan a conectar")) {
            return Comando("/pista?p=12", "Pista 12 · emoción por la transformación")
        }
        if ((t.contains("cierra los ojos") || t.contains("cierra tus ojos")) &&
            (t.contains("desconectarte") || t.contains("hacer el trabajo") || t.contains("trabajar"))) {
            return Comando("/ojos?estado=cerrar", "Robi cierra los ojos durante 5 segundos")
        }
        return null
    }

    private fun enviarComando(comando: Comando) {
        txtEstado.text = "ENVIANDO INDICACIÓN A ROBI..."
        Thread {
            var conexion: HttpURLConnection? = null
            try {
                val direccion = "http://$IP_ROBI${comando.ruta}"
                conexion = URL(direccion).openConnection() as HttpURLConnection
                conexion.requestMethod = "GET"
                conexion.connectTimeout = 4000
                conexion.readTimeout = 4000
                val codigo = conexion.responseCode
                runOnUiThread {
                    txtEstado.text = when {
                        codigo in 200..299 -> "COMANDO ENVIADO A ROBI"
                        codigo == 409 -> "ROBI ESTÁ OCUPADO; ESPERA A QUE TERMINE"
                        else -> "ROBI RESPONDIÓ CON ERROR $codigo"
                    }
                }
            } catch (_: Exception) {
                runOnUiThread { txtEstado.text = "ERROR DE CONEXIÓN: CONECTA EL CELULAR A LA RED ROBI" }
            } finally {
                conexion?.disconnect()
            }
        }.start()
    }
}
