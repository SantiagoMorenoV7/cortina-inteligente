#include "red.h"
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include "ajustes.h"
#include "app_web.h"
#include "config.h"
#include "control.h"
#include "luz.h"
#include "motor.h"

static WebServer servidor(80);
static DNSServer dns;
static char nombreAp[24];
static uint32_t reiniciarEn = 0;       // millis() en que se reinicia (0 = no)
static bool reconectarWifi = false;
static bool staEstabaConectado = false;
static bool mdnsIniciado = false;
static bool otaEnCurso = false;

// =====================================================================
// Utilidades
// =====================================================================
static void enviarJson(int codigo, const String& cuerpo) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(codigo, "application/json", cuerpo);
}

static void enviarError(int codigo, const char* mensaje) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = mensaje;
  String s;
  serializeJson(d, s);
  enviarJson(codigo, s);
}

static void enviarOk() { enviarJson(200, "{\"ok\":true}"); }

// Lee el cuerpo JSON de la petición. Devuelve false (y responde 400) si no es válido.
static bool leerCuerpo(JsonDocument& d) {
  if (!servidor.hasArg("plain")) {
    enviarError(400, "Falta el cuerpo de la petición");
    return false;
  }
  const DeserializationError err = deserializeJson(d, servidor.arg("plain"));
  if (err) {
    enviarError(400, "JSON inválido");
    return false;
  }
  return true;
}

// Número con decimales fijos para el JSON; si no es finito se envía 0
// (un "nan" o "inf" dejaría el JSON inválido).
// (serialized(String) guarda su propia copia del texto en el documento.)
static auto numJson(float x, unsigned int decimales) -> decltype(serialized(String())) {
  return serialized(isfinite(x) ? String(x, decimales) : String("0"));
}

static bool esNumero(JsonVariantConst v) { return v.is<float>() || v.is<long>(); }

static const char* textoEstadoMotor(EstadoMotor e) {
  switch (e) {
    case M_MOVIENDO: return "moviendo";
    case M_BUSCANDO: return "buscando";
    case M_ERROR: return "error";
    default: return "reposo";
  }
}

// =====================================================================
// GET /api/estado?ev=<seq>
// =====================================================================
static void apiEstado() {
  const uint32_t desde = servidor.hasArg("ev") ? (uint32_t)servidor.arg("ev").toInt() : 0;

  JsonDocument d;
  d["nombre"] = ajustes.nombre;
  d["version"] = VERSION_FIRMWARE;
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["modo"] = controlModo() == MODO_AUTO ? "auto" : "manual";
  d["reanudaEn"] = controlReanudaEnSeg();
  d["cortina"] = controlEstadoCortina();
  d["pct"] = controlPorcentaje();

  const InfoMotor m = motorInfo();
  JsonObject jm = d["motor"].to<JsonObject>();
  jm["pos"] = m.pos;
  jm["obj"] = m.objetivo;
  jm["rec"] = m.recorrido;
  jm["estado"] = textoEstadoMotor(m.estado);
  jm["error"] = motorTextoError(m.error);
  jm["dir"] = m.dir;
  jm["fcA"] = m.fcAbierta;
  jm["fcC"] = m.fcCerrada;
  jm["usarFc"] = ajustes.usarFinales;
  jm["ref"] = m.referenciado;
  jm["pasos"] = m.pasosTotales;

  const InfoLuz& l = luzInfo();
  JsonObject jl = d["luz"].to<JsonObject>();
  jl["n"] = numJson(l.n, 1);
  jl["mv"] = l.mv;
  jl["prom"] = numJson(l.promedio, 1);
  jl["filt"] = numJson(l.filtrada, 1);
  jl["fuente"] = ajustes.filtro == F_KALMAN ? "kalman" : "promedio";
  jl["cal"] = l.calibrando;
  jl["calPct"] = l.progresoCalibracion;
  JsonObject jk = jl["k"].to<JsonObject>();
  jk["n"] = numJson(l.kalman.n, 2);
  jk["v"] = numJson(l.kalman.v, 2);
  jk["k0"] = numJson(l.kalman.k0, 4);
  jk["k1"] = numJson(l.kalman.k1, 4);
  jk["y"] = numJson(l.kalman.y, 2);
  jk["p00"] = numJson(l.kalman.p00, 3);
  jk["p01"] = numJson(l.kalman.p01, 3);
  jk["p11"] = numJson(l.kalman.p11, 3);
  jk["q"] = ajustes.kalmanQ;
  jk["r"] = ajustes.kalmanR;

  JsonObject ja = d["auto"].to<JsonObject>();
  ja["abrir"] = ajustes.umbralAbrir;
  ja["cerrar"] = ajustes.umbralCerrar;
  ja["pendiente"] = controlPendiente();
  ja["falta"] = controlPendienteFaltaMs();
  ja["confirmar"] = ajustes.confirmarMs;

  JsonObject jh = d["hora"].to<JsonObject>();
  jh["valida"] = horaValida();
  jh["epoch"] = horaValida() ? (uint32_t)time(nullptr) : 0;
  jh["zona"] = ajustes.zonaMin;
  jh["fuente"] = horaFuente();

  JsonObject jr = d["red"].to<JsonObject>();
  jr["ap"] = nombreAp;
  jr["apIp"] = WiFi.softAPIP().toString();
  jr["apClientes"] = WiFi.softAPgetStationNum();
  jr["sta"] = ajustes.staSsid;
  jr["conectado"] = WiFi.status() == WL_CONNECTED;
  jr["staIp"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
  jr["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  jr["mdns"] = String(NOMBRE_MDNS) + ".local";

  String eventos;
  eventos.reserve(512);
  eventosAJson(eventos, desde);
  d["ev"] = serialized(eventos);
  d["evSeq"] = eventosUltimoSeq();

  String s;
  serializeJson(d, s);
  enviarJson(200, s);
}

// =====================================================================
// GET /api/historial
// =====================================================================
static void apiHistorial() {
  String s;
  s.reserve(7000);
  historialAJson(s);
  enviarJson(200, s);
}

// =====================================================================
// POST /api/comando   {"accion": "...", "pct": n}
// =====================================================================
static void apiComando() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;
  const char* accion = d["accion"] | "";

  if (!strcmp(accion, "abrir")) accionAbrir("app");
  else if (!strcmp(accion, "cerrar")) accionCerrar("app");
  else if (!strcmp(accion, "parar")) accionParar("app");
  else if (!strcmp(accion, "alternar")) accionAlternar("app");
  else if (!strcmp(accion, "auto")) accionModo(MODO_AUTO, "app");
  else if (!strcmp(accion, "manual")) accionModo(MODO_MANUAL, "app");
  else if (!strcmp(accion, "posicion")) {
    if (!esNumero(d["pct"])) return enviarError(400, "Falta pct (0-100)");
    accionPosicion(d["pct"].as<int>(), "app");
  } else {
    return enviarError(400, "Acción desconocida");
  }
  enviarOk();
}

// =====================================================================
// POST /api/calibrar   {"accion": "mover|buscar|fijarAbierta|fijarCerrada|ruido", "pasos": n}
// =====================================================================
static void apiCalibrar() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;
  const char* accion = d["accion"] | "";
  const InfoMotor m = motorInfo();

  if (!strcmp(accion, "mover")) {
    if (!esNumero(d["pasos"])) return enviarError(400, "Falta pasos");
    const long pasos = d["pasos"].as<long>();
    if (pasos == 0 || labs(pasos) > 50000) return enviarError(400, "Pasos fuera de rango");
    accionModo(MODO_MANUAL, "calibracion");
    motorMoverRelativo((int32_t)pasos);
  } else if (!strcmp(accion, "buscar")) {
    accionModo(MODO_MANUAL, "calibracion");
    motorBuscarOrigen();
    registrarEvento(EV_ACCION, "Buscando referencia (final de carrera de abierta)");
  } else if (!strcmp(accion, "fijarAbierta")) {
    accionModo(MODO_MANUAL, "calibracion");
    motorFijarAbierta();
    registrarEvento(EV_ACCION, "Posición actual fijada como ABIERTA");
  } else if (!strcmp(accion, "fijarCerrada")) {
    if (m.estado != M_REPOSO) return enviarError(409, "Espera a que el motor se detenga");
    if (m.pos < 100) return enviarError(400, "La cortina está demasiado cerca de la posición abierta");
    ajustes.recorrido = m.pos;
    guardarAjustes();
    motorConfigurar(ajustes.recorrido, ajustes.msPorPaso, ajustes.invertirMotor, ajustes.usarFinales);
    registrarEvento(EV_ACCION, "Recorrido fijado en %ld pasos", (long)ajustes.recorrido);
  } else if (!strcmp(accion, "ruido")) {
    luzIniciarCalibracionRuido();
    registrarEvento(EV_INFO, "Midiendo el ruido del sensor (5 s, no muevas la luz)");
  } else {
    return enviarError(400, "Acción desconocida");
  }
  enviarOk();
}

// =====================================================================
// Ajustes
// =====================================================================
static void ajustesAJson(JsonDocument& d) {
  d["nombre"] = ajustes.nombre;
  d["umbralAbrir"] = ajustes.umbralAbrir;
  d["umbralCerrar"] = ajustes.umbralCerrar;
  d["confirmarMs"] = ajustes.confirmarMs;
  d["reanudarMin"] = ajustes.reanudarMin;
  d["filtro"] = ajustes.filtro == F_KALMAN ? "kalman" : "promedio";
  d["kalmanR"] = ajustes.kalmanR;
  d["kalmanQ"] = ajustes.kalmanQ;
  d["invertirSensor"] = ajustes.invertirSensor;
  d["recorrido"] = ajustes.recorrido;
  d["msPorPaso"] = ajustes.msPorPaso;
  d["invertirMotor"] = ajustes.invertirMotor;
  d["usarFinales"] = ajustes.usarFinales;
  d["zonaMin"] = ajustes.zonaMin;
  d["staSsid"] = ajustes.staSsid;
  d["apNombre"] = nombreAp;
}

static void apiAjustesGet() {
  JsonDocument d;
  ajustesAJson(d);
  String s;
  serializeJson(d, s);
  enviarJson(200, s);
}

// Aplica un campo numérico si viene en el JSON. Devuelve false si está fuera de rango.
template <typename T>
static bool campoNumero(JsonDocument& d, const char* clave, T& destino, double minimo, double maximo,
                        String& error) {
  JsonVariantConst v = d[clave];
  if (v.isNull()) return true;
  if (!esNumero(v)) {
    error = String(clave) + " debe ser un número";
    return false;
  }
  const double x = v.as<double>();
  if (!(x >= minimo && x <= maximo)) {
    error = String(clave) + " fuera de rango (" + String(minimo, 0) + " a " + String(maximo, 0) + ")";
    return false;
  }
  destino = (T)x;
  return true;
}

static bool campoBool(JsonDocument& d, const char* clave, bool& destino, String& error) {
  JsonVariantConst v = d[clave];
  if (v.isNull()) return true;
  if (!v.is<bool>()) {
    error = String(clave) + " debe ser verdadero o falso";
    return false;
  }
  destino = v.as<bool>();
  return true;
}

static void apiAjustesPost() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;

  Ajustes nuevo = ajustes;
  String error;
  bool ok = true;

  if (!d["nombre"].isNull()) {
    const char* nombre = d["nombre"] | "";
    const size_t largo = strlen(nombre);
    if (largo < 1 || largo >= sizeof(nuevo.nombre)) {
      ok = false;
      error = "El nombre debe tener entre 1 y 31 caracteres";
    } else {
      strlcpy(nuevo.nombre, nombre, sizeof(nuevo.nombre));
    }
  }
  if (!d["filtro"].isNull()) {
    const char* f = d["filtro"] | "";
    if (!strcmp(f, "kalman")) nuevo.filtro = F_KALMAN;
    else if (!strcmp(f, "promedio")) nuevo.filtro = F_PROMEDIO;
    else {
      ok = false;
      error = "filtro debe ser kalman o promedio";
    }
  }
  if (!d["apClave"].isNull()) {
    const char* c = d["apClave"] | "";
    const size_t largo = strlen(c);
    if (largo < 8 || largo >= sizeof(nuevo.apClave)) {
      ok = false;
      error = "La clave del WiFi propio debe tener entre 8 y 63 caracteres";
    } else {
      strlcpy(nuevo.apClave, c, sizeof(nuevo.apClave));
    }
  }

  ok = ok && campoNumero(d, "umbralAbrir", nuevo.umbralAbrir, 0, 1023, error);
  ok = ok && campoNumero(d, "umbralCerrar", nuevo.umbralCerrar, 0, 1023, error);
  ok = ok && campoNumero(d, "confirmarMs", nuevo.confirmarMs, 0, 60000, error);
  ok = ok && campoNumero(d, "reanudarMin", nuevo.reanudarMin, 0, 1440, error);
  ok = ok && campoNumero(d, "kalmanR", nuevo.kalmanR, 0.1, 100000, error);
  ok = ok && campoNumero(d, "kalmanQ", nuevo.kalmanQ, 0.001, 100000, error);
  ok = ok && campoNumero(d, "recorrido", nuevo.recorrido, 100, 200000, error);
  ok = ok && campoNumero(d, "msPorPaso", nuevo.msPorPaso, 2, 20, error);
  ok = ok && campoNumero(d, "zonaMin", nuevo.zonaMin, -720, 840, error);
  ok = ok && campoBool(d, "invertirSensor", nuevo.invertirSensor, error);
  ok = ok && campoBool(d, "invertirMotor", nuevo.invertirMotor, error);
  ok = ok && campoBool(d, "usarFinales", nuevo.usarFinales, error);

  if (ok && nuevo.umbralAbrir + 10 > nuevo.umbralCerrar) {
    ok = false;
    error = "El umbral de abrir debe ser al menos 10 menor que el de cerrar";
  }
  if (!ok) return enviarError(400, error.c_str());

  const bool cambioClaveAp = strcmp(nuevo.apClave, ajustes.apClave) != 0;
  ajustes = nuevo;
  guardarAjustes();
  motorConfigurar(ajustes.recorrido, ajustes.msPorPaso, ajustes.invertirMotor, ajustes.usarFinales);
  luzAplicarAjustes();
  registrarEvento(EV_INFO, "Ajustes guardados");
  if (cambioClaveAp) {
    registrarEvento(EV_ALERTA, "Clave del WiFi propio cambiada: reiniciando");
    reiniciarEn = millis() + 1500;
  }

  JsonDocument r;
  ajustesAJson(r);
  r["ok"] = true;
  String s;
  serializeJson(r, s);
  enviarJson(200, s);
}

// =====================================================================
// Horarios
// =====================================================================
static void apiHorariosGet() {
  JsonDocument d;
  JsonArray a = d["horarios"].to<JsonArray>();
  for (int i = 0; i < ajustes.numHorarios; i++) {
    const Horario& h = ajustes.horarios[i];
    JsonObject o = a.add<JsonObject>();
    o["activo"] = h.activo;
    o["hora"] = h.hora;
    o["minuto"] = h.minuto;
    o["dias"] = h.dias;
    o["accion"] = h.accion;
    o["pct"] = h.pct;
  }
  String s;
  serializeJson(d, s);
  enviarJson(200, s);
}

static void apiHorariosPost() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;
  JsonArray a = d["horarios"].as<JsonArray>();
  if (a.isNull()) return enviarError(400, "Falta la lista de horarios");
  if (a.size() > MAX_HORARIOS) return enviarError(400, "Máximo 8 horarios");

  Horario lista[MAX_HORARIOS];
  int n = 0;
  for (JsonObject o : a) {
    const int hora = o["hora"] | -1;
    const int minuto = o["minuto"] | -1;
    const int dias = o["dias"] | -1;
    const int accion = o["accion"] | -1;
    const int pct = o["pct"] | 0;
    if (hora < 0 || hora > 23 || minuto < 0 || minuto > 59 || dias < 0 || dias > 127 ||
        accion < H_ABRIR || accion > H_AUTOMATICO || pct < 0 || pct > 100) {
      return enviarError(400, "Horario inválido");
    }
    Horario& h = lista[n++];
    h.activo = o["activo"] | true;
    h.hora = hora;
    h.minuto = minuto;
    h.dias = dias;
    h.accion = accion;
    h.pct = pct;
  }
  memcpy(ajustes.horarios, lista, sizeof(Horario) * n);
  ajustes.numHorarios = n;
  guardarAjustes();
  registrarEvento(EV_INFO, "Horarios guardados (%d)", n);
  apiHorariosGet();
}

// =====================================================================
// Hora, WiFi y sistema
// =====================================================================
static void apiHora() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;
  if (!esNumero(d["epoch"])) return enviarError(400, "Falta epoch");
  const double epoch = d["epoch"].as<double>();
  if (epoch < 1700000000.0 || epoch > 4000000000.0) return enviarError(400, "Hora inválida");
  const bool antes = horaValida();
  horaFijar((uint32_t)epoch);
  if (!antes) registrarEvento(EV_INFO, "Hora sincronizada desde la app");
  enviarOk();
}

static void apiRedes() {
  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return enviarJson(200, "{\"escaneando\":true,\"redes\":[]}");
  if (n < 0) {
    WiFi.scanNetworks(true);
    return enviarJson(200, "{\"escaneando\":true,\"redes\":[]}");
  }
  JsonDocument d;
  d["escaneando"] = false;
  JsonArray a = d["redes"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["segura"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String s;
  serializeJson(d, s);
  enviarJson(200, s);
}

static void apiWifi() {
  JsonDocument d;
  if (!leerCuerpo(d)) return;
  if (d["olvidar"] | false) {
    ajustes.staSsid[0] = '\0';
    ajustes.staClave[0] = '\0';
  } else {
    const char* ssid = d["ssid"] | "";
    const char* clave = d["clave"] | "";
    if (strlen(ssid) < 1 || strlen(ssid) > 32) return enviarError(400, "Nombre de red inválido");
    if (strlen(clave) > 63) return enviarError(400, "Clave demasiado larga");
    strlcpy(ajustes.staSsid, ssid, sizeof(ajustes.staSsid));
    strlcpy(ajustes.staClave, clave, sizeof(ajustes.staClave));
  }
  guardarAjustes();
  reconectarWifi = true;  // se aplica después de responder
  enviarOk();
}

static void apiReiniciar() {
  registrarEvento(EV_ALERTA, "Reiniciando por petición de la app");
  reiniciarEn = millis() + 800;
  enviarOk();
}

static void apiRestablecer() {
  ajustesPorDefecto(ajustes);
  guardarAjustes();
  registrarEvento(EV_ALERTA, "Ajustes de fábrica restablecidos: reiniciando");
  reiniciarEn = millis() + 800;
  enviarOk();
}

// Actualización de firmware por la app (archivo .bin de PlatformIO)
static void apiOtaFin() {
  otaEnCurso = false;
  if (Update.hasError()) {
    enviarError(500, Update.errorString());
  } else {
    enviarOk();
    reiniciarEn = millis() + 1000;
  }
}

static void apiOtaDatos() {
  HTTPUpload& up = servidor.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaEnCurso = true;
    motorParar();
    Serial.printf(">>> Actualizando firmware: %s\n", up.filename.c_str());
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!Update.hasError()) Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaEnCurso = false;
  }
}

// =====================================================================
// App web y portal cautivo
// =====================================================================
static void servirApp() {
  servidor.sendHeader("Content-Encoding", "gzip");
  servidor.sendHeader("Cache-Control", "no-cache");
  servidor.send_P(200, "text/html; charset=utf-8", (const char*)APP_HTML_GZ, APP_HTML_GZ_LEN);
}

static const char MANIFIESTO[] PROGMEM = R"({
  "name": "Cortina inteligente",
  "short_name": "Cortina",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#0f1115",
  "theme_color": "#0f1115",
  "icons": [{"src": "/icono.svg", "sizes": "any", "type": "image/svg+xml"}]
})";

static const char ICONO[] PROGMEM =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
    "<rect width='64' height='64' rx='14' fill='#e8a33d'/>"
    "<rect x='12' y='12' width='40' height='5' rx='2.5' fill='#1b1d22'/>"
    "<path d='M15 18h14v30c-4 0-8-2-14-4z' fill='#1b1d22'/>"
    "<path d='M49 18H35v30c4 0 8-2 14-4z' fill='#1b1d22'/>"
    "</svg>";

static bool esIp(const String& s) {
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c != '.' && c != ':' && (c < '0' || c > '9')) return false;
  }
  return s.length() > 0;
}

static void noEncontrado() {
  if (servidor.uri().startsWith("/api/")) return enviarError(404, "Ruta no encontrada");
  // Portal cautivo: los teléfonos que se conectan al WiFi de la cortina
  // abren la app automáticamente al detectar esta redirección.
  const String host = servidor.hostHeader();
  if (!esIp(host) && !host.endsWith(".local")) {
    servidor.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    servidor.send(302, "text/plain", "");
    return;
  }
  servidor.sendHeader("Location", "/", true);
  servidor.send(302, "text/plain", "");
}

// =====================================================================
static void conectarSta() {
  WiFi.disconnect(false, false);
  if (ajustes.staSsid[0]) {
    Serial.printf(">>> Conectando a la red \"%s\"...\n", ajustes.staSsid);
    WiFi.begin(ajustes.staSsid, ajustes.staClave);
  }
}

void redIniciar() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(nombreAp, sizeof(nombreAp), "%s%02X%02X", AP_PREFIJO, mac[4], mac[5]);

  WiFi.persistent(false);  // las credenciales las guarda la app, no la librería
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);    // respuesta más rápida de la app
  WiFi.setAutoReconnect(true);
  WiFi.softAP(nombreAp, ajustes.apClave);
  conectarSta();

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  configTime(0, 0, "pool.ntp.org", "time.google.com");  // hora UTC; la zona la aplica control.cpp

  servidor.enableCORS(true);  // permite usar la app desde el PC en modo desarrollo
  servidor.on("/", HTTP_GET, servirApp);
  servidor.on("/index.html", HTTP_GET, servirApp);
  servidor.on("/manifest.webmanifest", HTTP_GET, []() {
    servidor.send_P(200, "application/manifest+json", MANIFIESTO);
  });
  servidor.on("/icono.svg", HTTP_GET, []() {
    servidor.sendHeader("Cache-Control", "max-age=86400");
    servidor.send_P(200, "image/svg+xml", ICONO);
  });
  servidor.on("/api/estado", HTTP_GET, apiEstado);
  servidor.on("/api/historial", HTTP_GET, apiHistorial);
  servidor.on("/api/comando", HTTP_POST, apiComando);
  servidor.on("/api/calibrar", HTTP_POST, apiCalibrar);
  servidor.on("/api/ajustes", HTTP_GET, apiAjustesGet);
  servidor.on("/api/ajustes", HTTP_POST, apiAjustesPost);
  servidor.on("/api/horarios", HTTP_GET, apiHorariosGet);
  servidor.on("/api/horarios", HTTP_POST, apiHorariosPost);
  servidor.on("/api/hora", HTTP_POST, apiHora);
  servidor.on("/api/redes", HTTP_GET, apiRedes);
  servidor.on("/api/wifi", HTTP_POST, apiWifi);
  servidor.on("/api/reiniciar", HTTP_POST, apiReiniciar);
  servidor.on("/api/restablecer", HTTP_POST, apiRestablecer);
  servidor.on("/api/ota", HTTP_POST, apiOtaFin, apiOtaDatos);
  servidor.onNotFound(noEncontrado);
  servidor.begin();

  registrarEvento(EV_INFO, "WiFi propio: %s (http://%s)", nombreAp, WiFi.softAPIP().toString().c_str());
}

void redActualizar() {
  dns.processNextRequest();
  servidor.handleClient();

  if (reconectarWifi) {
    reconectarWifi = false;
    conectarSta();
  }

  const bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado != staEstabaConectado) {
    staEstabaConectado = conectado;
    if (conectado) {
      registrarEvento(EV_INFO, "Conectada a \"%s\" (IP %s)", ajustes.staSsid,
                      WiFi.localIP().toString().c_str());
      if (!mdnsIniciado && MDNS.begin(NOMBRE_MDNS)) {
        MDNS.addService("http", "tcp", 80);
        mdnsIniciado = true;
      }
    } else if (ajustes.staSsid[0]) {
      registrarEvento(EV_ALERTA, "Sin conexión con \"%s\"", ajustes.staSsid);
    }
  }

  if (reiniciarEn && (int32_t)(millis() - reiniciarEn) >= 0 && !otaEnCurso) {
    motorParar();
    delay(100);
    ESP.restart();
  }
}

void redImprimirEstado() {
  Serial.printf("WiFi propio: %s  clave: %s  ->  http://%s\n", nombreAp, ajustes.apClave,
                WiFi.softAPIP().toString().c_str());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Red de la casa: %s  ->  http://%s  (http://%s.local)\n", ajustes.staSsid,
                  WiFi.localIP().toString().c_str(), NOMBRE_MDNS);
  } else if (ajustes.staSsid[0]) {
    Serial.printf("Red de la casa: %s (sin conexion)\n", ajustes.staSsid);
  } else {
    Serial.println("Red de la casa: no configurada (configurala desde la app)");
  }
}
