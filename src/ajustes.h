#pragma once
/*
 * Ajustes persistentes (se guardan en la memoria flash del ESP32 con
 * Preferences y sobreviven a reinicios y cortes de luz).
 */
#include <Arduino.h>

const int MAX_HORARIOS = 8;

enum AccionHorario : uint8_t {
  H_ABRIR = 0,
  H_CERRAR = 1,
  H_POSICION = 2,
  H_AUTOMATICO = 3,
};

struct Horario {
  bool activo;
  uint8_t hora;     // 0-23
  uint8_t minuto;   // 0-59
  uint8_t dias;     // bit 0 = domingo ... bit 6 = sábado
  uint8_t accion;   // AccionHorario
  uint8_t pct;      // solo para H_POSICION (0 = abierta, 100 = cerrada)
};

enum FuenteFiltro : uint8_t { F_PROMEDIO = 0, F_KALMAN = 1 };

struct Ajustes {
  char nombre[32];

  // Automatización (histéresis sobre la escala 0-1023)
  int16_t umbralAbrir;
  int16_t umbralCerrar;
  uint32_t confirmarMs;
  uint16_t reanudarMin;      // 0 = no volver solo a automático

  // Sensor
  uint8_t filtro;            // FuenteFiltro
  float kalmanR;
  float kalmanQ;
  bool invertirSensor;       // true si la lectura BAJA con más luz

  // Motor
  int32_t recorrido;         // pasos entre abierta y cerrada
  uint8_t msPorPaso;
  bool invertirMotor;
  bool usarFinales;

  // Hora
  int16_t zonaMin;           // desfase respecto a UTC en minutos

  // Red
  char apClave[64];
  char staSsid[33];
  char staClave[64];

  // Horarios
  uint8_t numHorarios;
  Horario horarios[MAX_HORARIOS];
};

extern Ajustes ajustes;

void ajustesPorDefecto(Ajustes& a);
void cargarAjustes();
void guardarAjustes();
