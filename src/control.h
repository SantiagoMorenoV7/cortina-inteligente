#pragma once
/*
 * Lógica de la cortina: modos automático/manual, histéresis, horarios,
 * registro de eventos, historial para las gráficas, botón y LED.
 */
#include <Arduino.h>
#include <time.h>

enum Modo : uint8_t { MODO_AUTO = 0, MODO_MANUAL = 1 };
enum TipoEvento : uint8_t { EV_INFO = 0, EV_ACCION = 1, EV_ALERTA = 2 };

void controlIniciar();
void controlActualizar(bool lecturaNueva);

// Acciones. "origen" queda en el registro: "app", "boton", "serie", "horario"...
void accionAbrir(const char* origen);
void accionCerrar(const char* origen);
void accionParar(const char* origen);
void accionPosicion(int pct, const char* origen);   // 0 = abierta, 100 = cerrada
void accionAlternar(const char* origen);
void accionModo(Modo m, const char* origen);

Modo controlModo();
uint32_t controlReanudaEnSeg();      // 0 si no aplica
int8_t controlPendiente();           // +1 cerrará, -1 abrirá, 0 nada
uint32_t controlPendienteFaltaMs();  // tiempo que falta para confirmar
int controlPorcentaje();             // 0-100 (cerrada)
const char* controlEstadoCortina();  // abierta, cerrada, parcial, abriendo...

// Registro de eventos
void registrarEvento(uint8_t tipo, const char* formato, ...);
uint32_t eventosUltimoSeq();
void eventosAJson(String& salida, uint32_t desdeSeq);   // escribe un arreglo JSON

// Historial (1 punto por segundo, últimos 5 minutos)
void historialAJson(String& salida);

// Hora
bool horaValida();
void horaFijar(uint32_t epochUtc);
bool horaLocal(struct tm& t);
const char* horaFuente();
