#pragma once
/*
 * WiFi, servidor web y API de la app.
 *
 * El ESP32 siempre crea su propia red WiFi ("Cortina-XXXX", clave por
 * defecto cortina123) y la app queda en http://192.168.4.1. Si además se
 * configura el WiFi de la casa desde la app, también queda disponible en
 * http://cortina.local (o en la IP que le asigne el router).
 */

void redIniciar();
void redActualizar();
void redImprimirEstado();  // por el monitor serie
