#pragma once
/*
 * Configuración fija del hardware (pines) y valores por defecto.
 * Casi todo lo demás se ajusta desde la app y se guarda en la memoria
 * del ESP32, así que no hace falta recompilar para calibrar.
 */
#include <Arduino.h>

#define VERSION_FIRMWARE "2.0.0"

// ---------------- Pines ----------------
// Sensor de luz: debe ir en un pin del ADC1 (32-39); el ADC2 no funciona
// mientras el WiFi está encendido.
const int PIN_LDR = 34;

// Driver ULN2003 del motor 28BYJ-48 (IN1..IN4).
const int PIN_IN1 = 16;
const int PIN_IN2 = 17;
const int PIN_IN3 = 18;
const int PIN_IN4 = 19;

// Finales de carrera (microswitch entre el pin y GND, pull-up interno).
// Los pines 34-39 no tienen pull-up, por eso se usan 32 y 33.
const int PIN_FC_ABIERTA = 32;
const int PIN_FC_CERRADA = 33;

const int PIN_LED   = 2;   // LED integrado de la placa
const int PIN_BOTON = 0;   // botón BOOT: pulsación corta = abrir/cerrar

// ---------------- Motor ----------------
// 28BYJ-48 en paso completo: 2048 pasos por vuelta del eje de salida.
const int32_t PASOS_POR_VUELTA = 2048;

// ---------------- Sensor ----------------
const uint32_t PERIODO_MUESTRA_MS = 100;   // 10 lecturas por segundo
const int      MUESTRAS_PROMEDIO  = 10;    // promedio del informe (1 s)
const float    VREF_MV            = 3300;  // el divisor se alimenta a 3,3 V

// ---------------- Valores por defecto (modificables desde la app) ----------------
const int      DEF_UMBRAL_ABRIR   = 320;    // escala 0-1023, igual que el informe
const int      DEF_UMBRAL_CERRAR  = 620;
const uint32_t DEF_CONFIRMAR_MS   = 1000;   // tiempo fuera de la banda antes de mover
const float    DEF_KALMAN_R       = 25.0;   // varianza del ruido de medición (σ = 5)
const float    DEF_KALMAN_Q       = 100.0;  // varianza de la aceleración de la luz
const int32_t  DEF_RECORRIDO      = 16297;  // 60 cm con polea de 1,2 cm de radio
const int      DEF_MS_POR_PASO    = 3;      // 3 ms/paso ≈ 9,8 RPM
const uint16_t DEF_REANUDAR_MIN   = 30;     // vuelve a automático tras 30 min en manual
const int16_t  DEF_ZONA_MIN       = -300;   // Colombia, UTC-5

// Margen extra de pasos para buscar el final de carrera más allá del
// recorrido nominal antes de declarar un error (equivale a PASOS_MAX del
// informe: 16297 + 503 = 16800).
const int32_t MARGEN_FINAL_CARRERA = 503;

// ---------------- Red ----------------
const char AP_PREFIJO[]    = "Cortina-";
const char DEF_AP_CLAVE[]  = "cortina123";
const char NOMBRE_MDNS[]   = "cortina";      // http://cortina.local
