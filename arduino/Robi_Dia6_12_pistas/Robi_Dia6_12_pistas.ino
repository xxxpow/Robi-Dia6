/*
  ROBI - DIA 6 (ESP32-WROOM-32D)
  Version reducida para el guion actual: 12 pistas.
  Incluye TFT ST7789, MP3-TF-18P, LED de boca y Wi-Fi para RobiVozz.
  OMITIDOS: microfono INMP441 del robot, TTP223 y MPU-9250/IMU.

  Endpoints:
    /pista?p=1..12             reproduce una pista
    /dialogo?p=1               reproduce pistas 1 y 2 seguidas
    /alerta                    sorpresa 2 s + mirada 4 direcciones + pista 3
    /ojos?estado=cerrar        cierra ojos durante 5 segundos y los abre
    /                         estado del robot
*/

#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>

const char* WIFI_SSID = "ROBI";
const char* WIFI_PASSWORD = "12345678";
WebServer server(80);

#define TFT_CS   -1
#define TFT_DC   27
#define TFT_RST  4
#define TFT_SCLK 18
#define TFT_MOSI 23
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

#define COLOR_NORMAL    0x067F
#define COLOR_ALEGRE    0xFFE0
#define COLOR_PENSATIVO 0x067F
#define COLOR_ENOJO     0xF800

HardwareSerial mp3Serial(2);
#define MP3_RX 16
#define MP3_TX 17
#define CMD_PLAY_INDEX 0x03
#define CMD_SET_VOLUME 0x06
#define CMD_SEL_DEV 0x09
#define DEV_TF 0x02

#define MP3_BUSY_PIN 19
#define LED_VOZ_PIN 5
#define MP3_REPRODUCIENDO LOW
#define INTERVALO_LED_VOZ 180UL

const unsigned long DURACION_SORPRESA_ALERTA = 2000UL;
const unsigned long DURACION_BUSQUEDA_ALERTA = 3000UL;
const unsigned long INTERVALO_MIRADA_ALERTA = 350UL;
const unsigned long DURACION_OJOS_CERRADOS = 5000UL;
const unsigned long INTERVALO_PARPADEO_OJOS = 4000UL;
const unsigned long DURACION_FASE_PARPADEO = 100UL;

// Numeros fisicos de archivo en la microSD. Por defecto 1 -> 0001.mp3, etc.
const uint16_t PISTAS_ROBI[12] = {1,2,3,4,5,6,7,8,9,10,11,12};

enum Expresion { EXPRESION_NORMAL, EXPRESION_PENSATIVA, EXPRESION_ALEGRE, EXPRESION_SORPRENDIDO, EXPRESION_TRISTE, EXPRESION_ENOJADO };
Expresion expresionActual = EXPRESION_NORMAL;

enum EstadoAlerta { ALERTA_INACTIVA, ALERTA_SORPRESA, ALERTA_BUSCANDO };
EstadoAlerta estadoAlerta = ALERTA_INACTIVA;
unsigned long inicioEstadoAlerta = 0;
unsigned long ultimoCambioMirada = 0;
uint8_t pasoMirada = 0;
bool alertaDebeResponderPista3 = false;

bool mp3Reproduciendo = false;
bool respuestaEnCurso = false;
int estadoBusyAnterior = HIGH;
uint16_t pistaActual = 0;
unsigned long ultimoParpadeoLED = 0;
bool estadoLEDVoz = false;

bool ojosCerrados = false;
unsigned long inicioOjosCerrados = 0;
unsigned long ultimoParpadeoOjos = 0;
unsigned long inicioFaseParpadeoOjos = 0;
enum EstadoParpadeo { PARPADEO_NINGUNO, PARPADEO_DESAPARECIDO, PARPADEO_LINEAS, PARPADEO_FINAL };
EstadoParpadeo estadoParpadeo = PARPADEO_NINGUNO;

bool dialogo12EnCurso = false;
bool secuenciaPista89 = false;
uint8_t siguientePistaDialogo = 0;
unsigned long ultimoParpadeoVoz = 0;
bool ojosCerradosPorVoz = false;
unsigned long inicioCierreOjosVoz = 0;

// ---------------- DIBUJO DE OJOS ----------------
void dibujarOjosNormales(int dx = 0, int dy = 0) {
  tft.fillScreen(ST77XX_BLACK);
  int x1 = 20 + dx, x2 = 137 + dx, y = 80 + dy;
  tft.fillRoundRect(x1, y, 100, 120, 17, COLOR_NORMAL);
  tft.fillRoundRect(x2, y, 100, 120, 17, COLOR_NORMAL);
  tft.fillCircle(x1 + 50, y + 60, 20, ST77XX_BLACK);
  tft.fillCircle(x2 + 50, y + 60, 20, ST77XX_BLACK);
}

void dibujarOjosPensativos() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(20, 80, 100, 80, 17, COLOR_PENSATIVO);
  tft.fillCircle(80, 115, 30, ST77XX_BLACK);
  tft.fillRoundRect(137, 80, 100, 80, 17, COLOR_PENSATIVO);
  tft.fillCircle(197, 115, 30, ST77XX_BLACK);
}

void dibujarUnArco(int cx, int cy) {
  for (int a = 200; a <= 340; a += 5) {
    float rad = a * PI / 180.0f;
    int x = cx + (int)(cos(rad) * 44);
    int y = cy + (int)(sin(rad) * 28);
    tft.fillCircle(x, y, 5, COLOR_ALEGRE);
  }
}
void dibujarOjosAlegres() {
  tft.fillScreen(ST77XX_BLACK);
  dibujarUnArco(82, 140); dibujarUnArco(178, 140);
}
void dibujarOjosSorprendidos() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(60, 140, 70, ST77XX_WHITE);
  tft.fillCircle(60, 140, 10, ST77XX_BLACK);
  tft.fillCircle(178, 140, 70, ST77XX_WHITE);
  tft.fillCircle(178, 140, 10, ST77XX_BLACK);
}
void dibujarOjosTristes() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(60, 140, 70, COLOR_NORMAL); tft.fillCircle(60, 126, 50, ST77XX_BLACK);
  tft.fillCircle(178, 140, 70, COLOR_NORMAL); tft.fillCircle(178, 126, 50, ST77XX_BLACK);
}
void dibujarOjosEnojados() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(20, 100, 100, 70, 12, COLOR_ENOJO);
  tft.fillRoundRect(137, 100, 100, 70, 12, COLOR_ENOJO);
  tft.fillTriangle(20, 100, 120, 120, 120, 100, ST77XX_BLACK);
  tft.fillTriangle(137, 120, 237, 100, 137, 100, ST77XX_BLACK);
}
void mostrarExpresion(Expresion e, bool forzar = false) {
  if (ojosCerrados && !forzar) return;
  expresionActual = e;
  switch (e) {
    case EXPRESION_NORMAL: dibujarOjosNormales(); break;
    case EXPRESION_PENSATIVA: dibujarOjosPensativos(); break;
    case EXPRESION_ALEGRE: dibujarOjosAlegres(); break;
    case EXPRESION_SORPRENDIDO: dibujarOjosSorprendidos(); break;
    case EXPRESION_TRISTE: dibujarOjosTristes(); break;
    case EXPRESION_ENOJADO: dibujarOjosEnojados(); break;
  }
}
Expresion expresionDePista(uint16_t p) {
  switch (p) {
    case 1: case 11: return EXPRESION_NORMAL;
    case 2: case 5: case 7: return EXPRESION_PENSATIVA;
    case 3: case 6: case 8: return EXPRESION_SORPRENDIDO;
    case 4: case 9: case 10: case 12: return EXPRESION_ALEGRE;
    default: return EXPRESION_NORMAL;
  }
}

void cerrarOjos() {
  ojosCerrados = true; inicioOjosCerrados = millis(); estadoParpadeo = PARPADEO_NINGUNO;
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(20, 137, 100, 6, 3, COLOR_NORMAL);
  tft.fillRoundRect(137, 137, 100, 6, 3, COLOR_NORMAL);
}
void abrirOjos() {
  ojosCerrados = false; estadoAlerta = ALERTA_INACTIVA;
  expresionActual = EXPRESION_NORMAL; dibujarOjosNormales();
  ultimoParpadeoOjos = millis(); estadoParpadeo = PARPADEO_NINGUNO;
}

// ---------------- MP3 ----------------
void enviarComandoMP3(byte comando, uint16_t parametro) {
  byte b[10];
  b[0]=0x7E; b[1]=0xFF; b[2]=0x06; b[3]=comando; b[4]=0x00;
  b[5]=parametro >> 8; b[6]=parametro & 0xFF;
  uint16_t checksum = 0 - (0xFF + 0x06 + comando + 0x00 + (parametro >> 8) + (parametro & 0xFF));
  b[7]=checksum >> 8; b[8]=checksum & 0xFF; b[9]=0xEF;
  mp3Serial.write(b, 10);
}
void configurarMP3() {
  mp3Serial.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  delay(500); enviarComandoMP3(CMD_SEL_DEV, DEV_TF); delay(500);
  enviarComandoMP3(CMD_SET_VOLUME, 25); delay(300);
}
void iniciarPista(uint16_t p) {
  if (p < 1 || p > 12) return;
  pistaActual = p;
  mostrarExpresion(expresionDePista(p), true);
  respuestaEnCurso = true;
  ojosCerradosPorVoz = false;
  enviarComandoMP3(CMD_PLAY_INDEX, PISTAS_ROBI[p - 1]);
  Serial.printf("ROBI Dia 6: pista logica %u -> archivo %u\n", p, PISTAS_ROBI[p - 1]);
}

void manejarPista() {
  if (!server.hasArg("p")) { server.send(400, "text/plain", "Falta parametro p"); return; }
  int p = server.arg("p").toInt();
  if (p < 1 || p > 12) { server.send(400, "text/plain", "Pista valida: 1 a 12"); return; }
  if (mp3Reproduciendo || respuestaEnCurso || estadoAlerta != ALERTA_INACTIVA || dialogo12EnCurso) {
    server.send(409, "text/plain", "ROBI ocupado; espera a que termine"); return;
  }
  dialogo12EnCurso = false; siguientePistaDialogo = 0;
  secuenciaPista89 = (p == 8);
  iniciarPista((uint16_t)p);
  server.send(200, "text/plain", "OK pista " + String(p));
}

void manejarDialogo() {
  if (mp3Reproduciendo || respuestaEnCurso || estadoAlerta != ALERTA_INACTIVA) {
    server.send(409, "text/plain", "ROBI ocupado"); return;
  }
  int id = server.hasArg("p") ? server.arg("p").toInt() : 1;
  if (id != 1) { server.send(400, "text/plain", "Dialogo valido: p=1"); return; }
  dialogo12EnCurso = true; siguientePistaDialogo = 2;
  iniciarPista(1);
  server.send(200, "text/plain", "Inicio de monologo: pistas 1 y 2");
}

// ---------------- SECUENCIA DE SORPRESA Y BUSQUEDA ----------------
void iniciarAlerta() {
  ojosCerrados = false;
  alertaDebeResponderPista3 = true;
  expresionActual = EXPRESION_NORMAL;
  mostrarExpresion(EXPRESION_SORPRENDIDO, true);
  estadoAlerta = ALERTA_SORPRESA;
  inicioEstadoAlerta = millis(); ultimoCambioMirada = millis(); pasoMirada = 0;
  estadoParpadeo = PARPADEO_NINGUNO;
}
void manejarAlerta() {
  if (mp3Reproduciendo || respuestaEnCurso || estadoAlerta != ALERTA_INACTIVA) {
    server.send(409, "text/plain", "ROBI ocupado; espera a que termine"); return;
  }
  iniciarAlerta();
  server.send(200, "text/plain", "Sorpresa 2 s, busqueda visual y despues pista 3");
}
void actualizarAlerta() {
  if (estadoAlerta == ALERTA_INACTIVA || ojosCerrados) return;
  unsigned long ahora = millis();
  if (estadoAlerta == ALERTA_SORPRESA) {
    if (ahora - inicioEstadoAlerta >= DURACION_SORPRESA_ALERTA) {
      estadoAlerta = ALERTA_BUSCANDO;
      inicioEstadoAlerta = ahora; ultimoCambioMirada = ahora; pasoMirada = 0;
      dibujarOjosNormales(-12, -8);
    }
    return;
  }
  if (estadoAlerta == ALERTA_BUSCANDO) {
    if (ahora - inicioEstadoAlerta >= DURACION_BUSQUEDA_ALERTA) {
      estadoAlerta = ALERTA_INACTIVA;
      if (alertaDebeResponderPista3) {
        alertaDebeResponderPista3 = false;
        iniciarPista(3); // Se reproduce solo despues de terminar la busqueda.
      } else mostrarExpresion(EXPRESION_NORMAL, true);
      return;
    }
    if (ahora - ultimoCambioMirada >= INTERVALO_MIRADA_ALERTA) {
      ultimoCambioMirada = ahora;
      pasoMirada = (pasoMirada + 1) % 4;
      int dx = (pasoMirada % 2 == 0) ? -12 : 12;
      int dy = (pasoMirada < 2) ? -8 : 8;
      dibujarOjosNormales(dx, dy);
    }
  }
}

void manejarOjos() {
  String estado = server.hasArg("estado") ? server.arg("estado") : "cerrar";
  estado.toLowerCase();
  if (estado != "cerrar") { server.send(400, "text/plain", "Usa estado=cerrar"); return; }
  if (mp3Reproduciendo || respuestaEnCurso || estadoAlerta != ALERTA_INACTIVA) {
    server.send(409, "text/plain", "Espera a que Robi termine"); return;
  }
  cerrarOjos();
  server.send(200, "text/plain", "Ojos cerrados durante 5 segundos");
}

void actualizarParpadeoOjos() {
  if (ojosCerrados || estadoAlerta != ALERTA_INACTIVA || expresionActual != EXPRESION_NORMAL || mp3Reproduciendo) {
    estadoParpadeo = PARPADEO_NINGUNO; return;
  }
  unsigned long ahora = millis();
  if (estadoParpadeo == PARPADEO_NINGUNO) {
    if (ahora - ultimoParpadeoOjos >= INTERVALO_PARPADEO_OJOS) {
      ultimoParpadeoOjos = ahora; inicioFaseParpadeoOjos = ahora;
      tft.fillScreen(ST77XX_BLACK); estadoParpadeo = PARPADEO_DESAPARECIDO;
    }
  } else if (ahora - inicioFaseParpadeoOjos >= DURACION_FASE_PARPADEO) {
    if (estadoParpadeo == PARPADEO_DESAPARECIDO) {
      tft.fillRoundRect(20,137,100,6,3,COLOR_NORMAL); tft.fillRoundRect(137,137,100,6,3,COLOR_NORMAL);
      estadoParpadeo = PARPADEO_LINEAS; inicioFaseParpadeoOjos = ahora;
    } else if (estadoParpadeo == PARPADEO_LINEAS) {
      tft.fillScreen(ST77XX_BLACK); estadoParpadeo = PARPADEO_FINAL; inicioFaseParpadeoOjos = ahora;
    } else {
      dibujarOjosNormales(); estadoParpadeo = PARPADEO_NINGUNO;
    }
  }
}

void actualizarBusyMP3() {
  int estado = digitalRead(MP3_BUSY_PIN);
  bool reproduciendo = (estado == MP3_REPRODUCIENDO);
  mp3Reproduciendo = reproduciendo;
  if (reproduciendo) {
    if (millis() - ultimoParpadeoLED >= INTERVALO_LED_VOZ) {
      ultimoParpadeoLED = millis(); estadoLEDVoz = !estadoLEDVoz;
      digitalWrite(LED_VOZ_PIN, estadoLEDVoz ? HIGH : LOW);
    }
  } else { estadoLEDVoz = false; digitalWrite(LED_VOZ_PIN, LOW); }

  if (respuestaEnCurso && estadoBusyAnterior == MP3_REPRODUCIENDO && !reproduciendo) {
    respuestaEnCurso = false;
    Serial.printf("ROBI: termino pista %u\n", pistaActual);
    if (dialogo12EnCurso && pistaActual == 1 && siguientePistaDialogo == 2) {
      delay(250);
      iniciarPista(2);
    } else if (secuenciaPista89 && pistaActual == 8) {
      secuenciaPista89 = false;
      delay(250);
      iniciarPista(9);
    } else {
      dialogo12EnCurso = false; siguientePistaDialogo = 0;
      secuenciaPista89 = false;
      if (!ojosCerrados && estadoAlerta == ALERTA_INACTIVA) {
        if (pistaActual != 3) mostrarExpresion(EXPRESION_NORMAL, true);
        else mostrarExpresion(EXPRESION_NORMAL, true);
      }
      pistaActual = 0;
    }
  }
  estadoBusyAnterior = estado;
}

void paginaPrincipal() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'></head><body>"
    "<h1>ROBI - DIA 6</h1><p>ESP32-WROOM-32D listo. Solo 12 pistas.</p>"
    "<p>Individual: /pista?p=1..12</p><p>Monologo: /dialogo?p=1</p>"
    "<p>Secuencia de sorpresa y pista 3: /alerta</p><p>Cerrar ojos 5 s: /ojos?estado=cerrar</p></body></html>";
  server.send(200, "text/html", html);
}
void configurarWiFi() {
  WiFi.mode(WIFI_AP); WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(); Serial.println("==============================");
  Serial.println(" WIFI ROBI - DIA 6"); Serial.print("SSID: "); Serial.println(WIFI_SSID);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP()); Serial.println("==============================");
  server.on("/", paginaPrincipal);
  server.on("/pista", manejarPista);
  server.on("/dialogo", manejarDialogo);
  server.on("/alerta", manejarAlerta);
  server.on("/ojos", manejarOjos);
  server.begin(); Serial.println("Servidor HTTP iniciado");
}

void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 240, SPI_MODE3); tft.setRotation(1); tft.fillScreen(ST77XX_BLACK);
  pinMode(MP3_BUSY_PIN, INPUT); pinMode(LED_VOZ_PIN, OUTPUT); digitalWrite(LED_VOZ_PIN, LOW);
  estadoBusyAnterior = digitalRead(MP3_BUSY_PIN);
  configurarMP3();
  mostrarExpresion(EXPRESION_NORMAL, true);
  ultimoParpadeoOjos = millis();
  configurarWiFi();
  Serial.println("ROBI Dia 6 listo. Sin INMP441, TTP223 ni IMU.");
}
void loop() {
  server.handleClient();
  actualizarBusyMP3();
  actualizarAlerta();
  actualizarParpadeoOjos();
  if (ojosCerrados && millis() - inicioOjosCerrados >= DURACION_OJOS_CERRADOS) abrirOjos();
  delay(5);
}
