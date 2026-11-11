#include "control.h"
#include <stdarg.h>
#include <sys/time.h>
#include "ajustes.h"
#include "config.h"
#include "luz.h"
#include "motor.h"

// ---------------- Modo y automatización ----------------
static Modo modo = MODO_AUTO;
static uint32_t manualDesde = 0;
static int8_t pendiente = 0;
static uint32_t pendienteDesde = 0;

// ---------------- Eventos ----------------
struct Evento {
  uint32_t seq;
  uint32_t ms;
  uint32_t epoch;  // 0 si la hora no era válida
  uint8_t tipo;
  char texto[80];
};
static const int MAX_EVENTOS = 40;
static Evento eventos[MAX_EVENTOS];
static uint32_t ultimoSeq = 0;

// ---------------- Historial ----------------
struct Punto {
  uint16_t n, prom, filt;
  uint8_t pct;
};
static const int LARGO_HISTORIAL = 300;
static Punto historial[LARGO_HISTORIAL];
static int histInicio = 0, histCuenta = 0;
static uint32_t ultimoPuntoMs = 0;

// ---------------- Hora ----------------
static bool horaDesdeApp = false;
static int32_t ultimoMinutoHorario = -1;

// ---------------- Botón y LED ----------------
static bool botonPresionado = false;
static uint32_t botonDesde = 0;
static bool botonLargoHecho = false;
static uint32_t botonCambio = 0;

// =====================================================================
// Eventos
// =====================================================================
void registrarEvento(uint8_t tipo, const char* formato, ...) {
  Evento& e = eventos[(ultimoSeq) % MAX_EVENTOS];
  e.seq = ++ultimoSeq;
  e.ms = millis();
  e.epoch = horaValida() ? (uint32_t)time(nullptr) : 0;
  e.tipo = tipo;
  va_list args;
  va_start(args, formato);
  vsnprintf(e.texto, sizeof(e.texto), formato, args);
  va_end(args);
  Serial.printf(">>> %s\n", e.texto);
}

uint32_t eventosUltimoSeq() { return ultimoSeq; }

static void agregarTextoJson(String& s, const char* t) {
  s += '"';
  for (; *t; t++) {
    const char c = *t;
    if (c == '"' || c == '\\') {
      s += '\\';
      s += c;
    } else if ((uint8_t)c < 0x20) {
      s += ' ';
    } else {
      s += c;
    }
  }
  s += '"';
}

void eventosAJson(String& s, uint32_t desdeSeq) {
  s += '[';
  bool primero = true;
  const uint32_t primeroDisponible = ultimoSeq > MAX_EVENTOS ? ultimoSeq - MAX_EVENTOS + 1 : 1;
  for (uint32_t seq = max(desdeSeq + 1, primeroDisponible); seq <= ultimoSeq; seq++) {
    const Evento& e = eventos[(seq - 1) % MAX_EVENTOS];
    if (!primero) s += ',';
    primero = false;
    s += "{\"seq\":";
    s += e.seq;
    s += ",\"ms\":";
    s += e.ms;
    s += ",\"epoch\":";
    s += e.epoch;
    s += ",\"tipo\":";
    s += e.tipo;
    s += ",\"texto\":";
    agregarTextoJson(s, e.texto);
    s += '}';
  }
  s += ']';
}

// =====================================================================
// Historial
// =====================================================================
static void guardarPunto() {
  const InfoLuz& l = luzInfo();
  Punto p;
  p.n = (uint16_t)lroundf(l.n);
  p.prom = (uint16_t)lroundf(l.promedio);
  p.filt = (uint16_t)lroundf(constrain(l.kalman.n, 0.0f, 1023.0f));
  p.pct = (uint8_t)controlPorcentaje();
  const int idx = (histInicio + histCuenta) % LARGO_HISTORIAL;
  historial[idx] = p;
  if (histCuenta < LARGO_HISTORIAL) {
    histCuenta++;
  } else {
    histInicio = (histInicio + 1) % LARGO_HISTORIAL;
  }
}

void historialAJson(String& s) {
  const char* nombres[4] = {"n", "prom", "kalman", "pct"};
  s += "{\"intervaloMs\":1000,";
  for (int serie = 0; serie < 4; serie++) {
    s += '"';
    s += nombres[serie];
    s += "\":[";
    for (int i = 0; i < histCuenta; i++) {
      const Punto& p = historial[(histInicio + i) % LARGO_HISTORIAL];
      if (i) s += ',';
      switch (serie) {
        case 0: s += p.n; break;
        case 1: s += p.prom; break;
        case 2: s += p.filt; break;
        default: s += p.pct; break;
      }
    }
    s += ']';
    if (serie < 3) s += ',';
  }
  s += '}';
}

// =====================================================================
// Hora
// =====================================================================
bool horaValida() { return time(nullptr) > 1700000000; }

void horaFijar(uint32_t epochUtc) {
  timeval tv;
  tv.tv_sec = (time_t)epochUtc;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  horaDesdeApp = true;
}

bool horaLocal(struct tm& t) {
  if (!horaValida()) return false;
  const time_t local = time(nullptr) + (time_t)ajustes.zonaMin * 60;
  gmtime_r(&local, &t);
  return true;
}

const char* horaFuente() {
  if (!horaValida()) return "ninguna";
  return horaDesdeApp ? "app" : "internet";
}

// =====================================================================
// Estado de la cortina
// =====================================================================
int controlPorcentaje() {
  const InfoMotor m = motorInfo();
  if (m.recorrido <= 0) return 0;
  const long pct = lroundf(m.pos * 100.0f / m.recorrido);
  return constrain((int)pct, 0, 100);
}

const char* controlEstadoCortina() {
  const InfoMotor m = motorInfo();
  if (m.estado == M_ERROR) return "error";
  if (m.estado == M_BUSCANDO) return "referenciando";
  if (m.estado == M_MOVIENDO) return m.dir > 0 ? "cerrando" : "abriendo";
  const int pct = controlPorcentaje();
  if (pct <= 1) return "abierta";
  if (pct >= 99) return "cerrada";
  return "parcial";
}

Modo controlModo() { return modo; }

uint32_t controlReanudaEnSeg() {
  if (modo != MODO_MANUAL || ajustes.reanudarMin == 0) return 0;
  const uint32_t total = (uint32_t)ajustes.reanudarMin * 60000UL;
  const uint32_t pasado = millis() - manualDesde;
  return pasado >= total ? 0 : (total - pasado) / 1000;
}

int8_t controlPendiente() { return pendiente; }

uint32_t controlPendienteFaltaMs() {
  if (pendiente == 0) return 0;
  const uint32_t pasado = millis() - pendienteDesde;
  return pasado >= ajustes.confirmarMs ? 0 : ajustes.confirmarMs - pasado;
}

// =====================================================================
// Acciones
// =====================================================================
static void pasarAManual() {
  if (modo != MODO_MANUAL) {
    modo = MODO_MANUAL;
  }
  manualDesde = millis();  // cada acción manual reinicia la cuenta para volver a automático
  pendiente = 0;
}

void accionAbrir(const char* origen) {
  pasarAManual();
  motorAbrir();
  registrarEvento(EV_ACCION, "Abrir (%s)", origen);
}

void accionCerrar(const char* origen) {
  pasarAManual();
  motorCerrar();
  registrarEvento(EV_ACCION, "Cerrar (%s)", origen);
}

void accionParar(const char* origen) {
  pasarAManual();
  motorParar();
  registrarEvento(EV_ACCION, "Parar (%s)", origen);
}

void accionPosicion(int pct, const char* origen) {
  pct = constrain(pct, 0, 100);
  pasarAManual();
  const int32_t rec = motorInfo().recorrido;
  motorIrA((int32_t)lroundf(rec * pct / 100.0f));
  registrarEvento(EV_ACCION, "Ir a %d%% cerrada (%s)", pct, origen);
}

void accionAlternar(const char* origen) {
  const InfoMotor m = motorInfo();
  if (m.estado == M_MOVIENDO || m.estado == M_BUSCANDO) {
    accionParar(origen);
  } else if (m.objetivo * 2 >= m.recorrido) {
    accionAbrir(origen);
  } else {
    accionCerrar(origen);
  }
}

void accionModo(Modo m, const char* origen) {
  if (m == MODO_MANUAL) {
    pasarAManual();
    registrarEvento(EV_ACCION, "Modo manual (%s)", origen);
  } else {
    modo = MODO_AUTO;
    pendiente = 0;
    registrarEvento(EV_ACCION, "Modo automático (%s)", origen);
  }
}

// =====================================================================
// Automatización con histéresis
// =====================================================================
static void automatizar() {
  const InfoMotor m = motorInfo();
  if (m.estado == M_ERROR || m.estado == M_BUSCANDO) {
    pendiente = 0;
    return;
  }
  const float f = luzInfo().filtrada;

  int8_t deseado = 0;
  if (f > ajustes.umbralCerrar && m.objetivo != m.recorrido) {
    deseado = 1;
  } else if (f < ajustes.umbralAbrir && m.objetivo != 0) {
    deseado = -1;
  }
  // Dentro de la banda (o ya en la posición correcta): no se mueve.

  if (deseado != pendiente) {
    pendiente = deseado;
    pendienteDesde = millis();
  }
  if (pendiente != 0 && millis() - pendienteDesde >= ajustes.confirmarMs) {
    if (pendiente > 0) {
      motorCerrar();
      registrarEvento(EV_ACCION, "Automático: cierra (luz %d > %d)", (int)f, ajustes.umbralCerrar);
    } else {
      motorAbrir();
      registrarEvento(EV_ACCION, "Automático: abre (luz %d < %d)", (int)f, ajustes.umbralAbrir);
    }
    pendiente = 0;
  }
}

// =====================================================================
// Horarios
// =====================================================================
static void revisarHorarios() {
  struct tm t;
  if (!horaLocal(t)) return;
  const int32_t minuto = t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min;
  if (minuto == ultimoMinutoHorario) return;
  const bool primeraVez = ultimoMinutoHorario < 0;
  ultimoMinutoHorario = minuto;
  if (primeraVez) return;  // no dispara al encender ni al sincronizar la hora

  for (int i = 0; i < ajustes.numHorarios; i++) {
    const Horario& h = ajustes.horarios[i];
    if (!h.activo || !(h.dias & (1 << t.tm_wday))) continue;
    if (h.hora != t.tm_hour || h.minuto != t.tm_min) continue;
    switch (h.accion) {
      case H_ABRIR: accionAbrir("horario"); break;
      case H_CERRAR: accionCerrar("horario"); break;
      case H_POSICION: accionPosicion(h.pct, "horario"); break;
      case H_AUTOMATICO: accionModo(MODO_AUTO, "horario"); break;
    }
  }
}

// =====================================================================
// Botón BOOT y LED
// =====================================================================
static void revisarBoton() {
  const bool leido = digitalRead(PIN_BOTON) == LOW;
  const uint32_t ahora = millis();
  if (leido != botonPresionado) {
    if (ahora - botonCambio < 40) return;  // antirrebote
    botonCambio = ahora;
    botonPresionado = leido;
    if (leido) {
      botonDesde = ahora;
      botonLargoHecho = false;
    } else if (!botonLargoHecho) {
      accionAlternar("boton");
    }
    return;
  }
  if (botonPresionado && !botonLargoHecho && ahora - botonDesde >= 3000) {
    botonLargoHecho = true;
    accionModo(MODO_AUTO, "boton");
  }
}

static void actualizarLed() {
  const InfoMotor m = motorInfo();
  const uint32_t ahora = millis();
  bool encendido;
  if (m.estado == M_ERROR) {
    encendido = (ahora / 100) % 2;  // parpadeo rápido
  } else if (m.estado == M_MOVIENDO || m.estado == M_BUSCANDO) {
    encendido = (ahora / 400) % 2;  // parpadeo lento
  } else {
    encendido = controlPorcentaje() >= 50;  // encendido = cerrada
  }
  digitalWrite(PIN_LED, encendido ? HIGH : LOW);
}

// =====================================================================
void controlIniciar() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  memset(eventos, 0, sizeof(eventos));
}

void controlActualizar(bool lecturaNueva) {
  const uint32_t ahora = millis();

  if (modo == MODO_MANUAL && ajustes.reanudarMin > 0 &&
      ahora - manualDesde >= (uint32_t)ajustes.reanudarMin * 60000UL) {
    modo = MODO_AUTO;
    pendiente = 0;
    registrarEvento(EV_INFO, "Vuelve a modo automático tras %u min", ajustes.reanudarMin);
  }

  if (lecturaNueva && modo == MODO_AUTO) automatizar();

  if (ahora - ultimoPuntoMs >= 1000) {
    ultimoPuntoMs = ahora;
    guardarPunto();
    revisarHorarios();
  }

  revisarBoton();
  actualizarLed();
}
