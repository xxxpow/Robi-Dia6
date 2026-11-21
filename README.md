# 🤖 ROBI — Día 6: Construyendo Mi Robot (12 Pistas)

Código y recursos del **Día 6** del proyecto ROBI: un robot interactivo basado en **ESP32-WROOM-32D** que responde a comandos de voz enviados desde un teléfono Android.

---

## 📂 Estructura del Repositorio

```
Robi-Dia6/
├── arduino/
│   └── Robi_Dia6_12_pistas/
│       └── Robi_Dia6_12_pistas.ino   ← Sketch de Arduino para ESP32
├── app/
│   └── src/main/java/com/inventosyproyectos/robivozz/
│       └── MainActivity.kt           ← App Android (RobiVozz)
├── pistas/
│   ├── 01.mp3 … 12.mp3               ← 12 pistas de audio
└── README.md
```

## ⚙️ Hardware Necesario

| Componente | Conexión |
|---|---|
| **ESP32-WROOM-32D** | Microcontrolador principal |
| **TFT ST7789 (240×240)** | CS=-1, DC=GPIO 27, RST=GPIO 4, SCLK=GPIO 18, MOSI=GPIO 23 |
| **MP3-TF-18P** (DFPlayer Mini) | RX=GPIO 16, TX=GPIO 17, BUSY=GPIO 19 |
| **LED de boca** | GPIO 5 |
| **MicroSD** | Con archivos `0001.mp3` a `0012.mp3` |

## 📡 Endpoints del ESP32 (Wi-Fi AP)

El ESP32 crea una red Wi-Fi:
- **SSID:** `ROBI`
- **Contraseña:** `12345678`
- **IP:** `192.168.4.1`

| Endpoint | Descripción |
|---|---|
| `GET /` | Estado del robot |
| `GET /pista?p=1..12` | Reproduce una pista individual |
| `GET /dialogo?p=1` | Reproduce pistas 1 y 2 seguidas (monólogo) |
| `GET /alerta` | Ojos sorprendidos 2s → búsqueda visual 3s → pista 3 |
| `GET /ojos?estado=cerrar` | Cierra los ojos durante 5 segundos |

## 🎭 Guion de Frases → Acciones

| Frase del Creador | Acción de Robi |
|---|---|
| "Robi habla con el público" | Pista 1 → Pista 2 automática |
| "¿Con quién estás hablando?" | Ojos sorprendidos (2s) + búsqueda visual (3s) + Pista 3 |
| "Hola, Robi" | Pista 4 |
| "Hoy vamos a instalarte unos nuevos componentes" | Pista 5 |
| "Mira lo que te he traído" | Pista 6 |
| "Son los motores…" | Pista 7 |
| "Podrán moverse para ayudarte a expresar mejor tus emociones" | Pista 8 → Pista 9 automática |
| "Vas a verte mucho más genial" | Pista 10 |
| "Uno para cada motor" / "DRV8833" | Pista 11 |
| "Para que todos aprendan cómo conectar" | Pista 12 |
| "Cierra los ojos… voy a desconectarte para hacer el trabajo" | Cierra ojos 5s → abre automáticamente |

## 📱 App Android (RobiVozz)

La app usa el reconocimiento de voz del teléfono (no el micrófono del robot) y envía los comandos HTTP al ESP32 por Wi-Fi.

### Requisitos de la app:
- El teléfono debe estar conectado a la red Wi-Fi **ROBI** (contraseña `12345678`)
- `AndroidManifest.xml` debe incluir permisos `RECORD_AUDIO` e `INTERNET`
- Permitir HTTP cleartext para `192.168.4.1`
- El layout `activity_main.xml` debe tener los IDs: `btnHablar`, `txtEstado`, `txtReconocido`, `txtDialogo`

## 🎵 Pistas de Audio

Las 12 pistas MP3 van en la carpeta `pistas/` y deben copiarse a la microSD del módulo MP3-TF-18P como `0001.mp3` a `0012.mp3`.

## ⚠️ Notas Importantes

- Este sketch **NO** inicializa ni usa el INMP441, TTP223 o MPU-9250/IMU
- El reconocimiento de voz se hace en el **celular**, usando el micrófono del teléfono
- Las expresiones de los ojos (normal, pensativo, alegre, sorprendido, triste, enojado) cambian automáticamente según la pista que se reproduce

## 📜 Licencia

Proyecto educativo — **Inventos y Proyectos**
