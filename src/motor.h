#pragma once
/*
 * Control del motor paso a paso 28BYJ-48 con el driver ULN2003.
 *
 * El motor corre en su propia tarea de FreeRTOS, así que se mueve con un
 * ritmo constante aunque el WiFi o la app estén ocupados. El resto del
 * programa le da órdenes (abrir, cerrar, ir a un %, parar...) y consulta su
 * estado con motorInfo().
 *
 * Posición: 0 = abierta, recorrido = cerrada (en pasos).
 * Con finales de carrera activos, abrir y cerrar avanzan hasta tocar el
 * interruptor (con un margen máximo de seguridad), así los pasos perdidos
 * no se acumulan.
 */
#include <Arduino.h>

enum EstadoMotor : uint8_t {
  M_REPOSO = 0,
  M_MOVIENDO = 1,
  M_BUSCANDO = 2,  // buscando el final de carrera de "abierta" (referencia)
  M_ERROR = 3,
};

enum ErrorMotor : uint8_t {
  E_NINGUNO = 0,
  E_SIN_FC_ABIERTA = 1,  // no se encontró el final de carrera de "abierta"
  E_SIN_FC_CERRADA = 2,  // no se encontró el final de carrera de "cerrada"
};

struct InfoMotor {
  int32_t pos;
  int32_t objetivo;
  int32_t recorrido;
  EstadoMotor estado;
  ErrorMotor error;
  int8_t dir;          // -1 abriendo, 0 quieto, +1 cerrando
  bool fcAbierta;      // final de carrera presionado
  bool fcCerrada;
  bool referenciado;   // la posición se conoce con certeza
  uint32_t pasosTotales;
};

void motorIniciar();
void motorConfigurar(int32_t recorrido, int msPorPaso, bool invertir, bool usarFinales);

void motorAbrir();
void motorCerrar();
void motorIrA(int32_t pasos);            // se limita a [0, recorrido]
void motorParar();
void motorMoverRelativo(int32_t pasos);  // calibración: ignora el recorrido
void motorBuscarOrigen();                // abre hasta el final de carrera
void motorFijarAbierta();                // detiene y toma la posición actual como 0

InfoMotor motorInfo();
const char* motorTextoError(ErrorMotor e);
