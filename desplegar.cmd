@echo off
rem Despliega la cortina inteligente. Ver desplegar.ps1 para las opciones.
rem Este archivo existe para poder ejecutarlo sin cambiar la politica de scripts de PowerShell.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0desplegar.ps1" %*
exit /b %ERRORLEVEL%
