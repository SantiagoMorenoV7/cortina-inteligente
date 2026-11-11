/*
 * Cortina inteligente con ESP32
 * ==============================
 * Sensor:     LDR GL5516 en divisor de tensión con 4,7 kΩ  -> GPIO 34
 * Actuador:   motor paso a paso 28BYJ-48 + driver ULN2003  -> GPIO 16, 17, 18, 19
 * Topes:      finales de carrera (abierta / cerrada)        -> GPIO 32 / 33
 * Filtro:     Kalman de 2 estados (nivel de luz y velocidad de cambio)
 * Control:    histéresis 320 / 620 (escala 0-1023), igual que el informe
 * App:        servida por el propio ESP32 (WiFi "Cortina-XXXX" -> 192.168.4.1)
 *
 * Módulos:
 *   config.h     pines y valores por defecto
 *   ajustes.*    ajustes guardados en la flash
 *   kalman.h     filtro de Kalman (álgebra matricial 2x2)
 *   luz.*        lectura del sensor, promedio y Kalman
 *   motor.*      tarea del motor paso a paso y finales de carrera
 *   control.*    modos, histéresis, horarios, eventos, botón y LED
 *   red.*        WiFi, servidor web y API de la app
 */

#include <Arduino.h>
#include "ajustes.h"
#include "config.h"
#include "control.h"
#include "luz.h"
#include "motor.h"
#include "red.h"

static uint32_t ultimoReporte = 0;
static String lineaSerie;

static void imprimirAyuda() {
  Serial.println();
  Serial.println("Comandos por el monitor serie:");
  Serial.println("  abrir | cerrar | parar | ir <0-100> | auto | manual");
  Serial.println("  buscar      busca el final de carrera de abierta");
  Serial.println("  red         muestra como entrar a la app");
  Serial.println("  ayuda");
  Serial.println();
}

static void procesarComando(String c) {
  c.trim();
  c.toLowerCase();
  if (c.isEmpty()) return;
  if (c == "abrir") accionAbrir("serie");
  else if (c == "cerrar") accionCerrar("serie");
  else if (c == "parar") accionParar("serie");
  else if (c == "auto") accionModo(MODO_AUTO, "serie");
  else if (c == "manual") accionModo(MODO_MANUAL, "serie");
  else if (c == "buscar") {
    accionModo(MODO_MANUAL, "serie");
    motorBuscarOrigen();
  } else if (c.startsWith("ir ")) accionPosicion(c.substring(3).toInt(), "serie");
  else if (c == "red" || c == "ip") redImprimirEstado();
  else imprimirAyuda();
}

static void leerSerie() {
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      procesarComando(lineaSerie);
      lineaSerie = "";
    } else if (lineaSerie.length() < 40) {
      lineaSerie += ch;
    }
  }
}

static void reportar() {
  const InfoLuz& l = luzInfo();
  const InfoMotor m = motorInfo();
  Serial.printf("N: %4.0f | prom: %6.1f | kalman: %6.1f | pos: %3d%% (%ld/%ld) | %s | %s\n",
                l.n, l.promedio, l.kalman.n, controlPorcentaje(), (long)m.pos, (long)m.recorrido,
                controlEstadoCortina(), controlModo() == MODO_AUTO ? "AUTO" : "MANUAL");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==== Cortina inteligente v" VERSION_FIRMWARE " ====");

  cargarAjustes();
  controlIniciar();
  luzIniciar();
  motorIniciar();
  motorConfigurar(ajustes.recorrido, ajustes.msPorPaso, ajustes.invertirMotor, ajustes.usarFinales);
  redIniciar();

  // Referencia inicial, como en el informe: abrir hasta el final de carrera.
  // Sin finales de carrera se asume que la cortina arranca abierta.
  motorBuscarOrigen();
  if (ajustes.usarFinales) {
    registrarEvento(EV_INFO, "Inicio: abriendo hasta el final de carrera para tomar referencia");
  } else {
    registrarEvento(EV_ALERTA, "Inicio sin finales de carrera: se asume que la cortina está abierta");
  }

  redImprimirEstado();
  imprimirAyuda();
}

void loop() {
  redActualizar();
  const bool nueva = luzActualizar();
  controlActualizar(nueva);
  leerSerie();

  if (millis() - ultimoReporte >= 1000) {
    ultimoReporte = millis();
    reportar();
  }
  delay(1);  // cede el núcleo a otras tareas
}
