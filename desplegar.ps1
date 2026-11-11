<#
  Despliega la cortina inteligente en un ESP32 con un solo comando.

  Uso (desde la carpeta del proyecto):
      .\desplegar.cmd                 instala lo que falte, compila, sube al ESP32 y abre el monitor
      .\desplegar.cmd -Puerto COM5    usa ese puerto en vez de detectarlo
      .\desplegar.cmd -SoloCompilar   solo compila (no necesita la placa)
      .\desplegar.cmd -SinMonitor     sube el firmware y termina
      .\desplegar.cmd -Demo           abre la app en modo demostración (sin placa)

  Lo que hace:
    1. Busca Python 3 (si no está, lo instala con winget).
    2. Usa PlatformIO de VS Code si ya existe; si no, lo instala en
       %LOCALAPPDATA%\cortina-inteligente (no ensucia el proyecto).
    3. Compila el firmware (la app web se incrusta sola).
    4. Detecta el ESP32 conectado por USB y le sube el firmware.
    5. Abre el monitor serie para ver qué hace la cortina.
#>
param(
  [string]$Puerto = "",
  [switch]$SoloCompilar,
  [switch]$SinMonitor,
  [switch]$Demo
)

$ErrorActionPreference = 'Stop'
$Proyecto = $PSScriptRoot
Set-Location $Proyecto

function Paso([string]$t) { Write-Host ""; Write-Host "==> $t" -ForegroundColor Cyan }
function Bien([string]$t) { Write-Host "    $t" -ForegroundColor Green }
function Nota([string]$t) { Write-Host "    $t" -ForegroundColor Yellow }
function Fallar([string]$t) {
  Write-Host ""
  Write-Host "ERROR: $t" -ForegroundColor Red
  exit 1
}

Write-Host ""
Write-Host "  Cortina inteligente - despliegue" -ForegroundColor White
Write-Host "  ---------------------------------"

if ($Demo) {
  Paso "Abriendo la app en modo demostración"
  Start-Process (Join-Path $Proyecto 'app\index.html')
  Bien "Listo. La app usa un simulador con la misma lógica del ESP32."
  exit 0
}

# ---------------------------------------------------------------- 1. PlatformIO
function Probar-Python([string[]]$cmd) {
  try {
    $exe = $cmd[0]
    $resto = @()
    if ($cmd.Length -gt 1) { $resto = $cmd[1..($cmd.Length - 1)] }
    $salida = & $exe @resto -c "import sys; print(sys.version_info >= (3, 8))" 2>$null
    return ($LASTEXITCODE -eq 0 -and "$salida".Trim() -eq 'True')
  } catch {
    return $false
  }
}

function Buscar-Python {
  foreach ($c in @(@('py', '-3'), @('python'), @('python3'))) {
    if (Probar-Python $c) { return , $c }
  }
  return $null
}

function Recargar-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
              [Environment]::GetEnvironmentVariable('Path', 'User')
}

Paso "Preparando PlatformIO"
$pio = $null
$pioVsCode = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
$herramientas = Join-Path $env:LOCALAPPDATA 'cortina-inteligente'
$pioPropio = Join-Path $herramientas 'venv\Scripts\pio.exe'

if (Test-Path $pioVsCode) {
  $pio = $pioVsCode
  Bien "Usando el PlatformIO de VS Code."
} elseif (Test-Path $pioPropio) {
  $pio = $pioPropio
  Bien "PlatformIO ya estaba instalado."
} else {
  $py = Buscar-Python
  if (-not $py) {
    Nota "No se encontró Python 3. Instalándolo con winget..."
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
      Fallar "No hay Python ni winget. Instala Python 3 desde https://www.python.org/downloads/ (marca 'Add python.exe to PATH') y vuelve a ejecutar este comando."
    }
    winget install -e --id Python.Python.3.12 --scope user --accept-package-agreements --accept-source-agreements
    Recargar-Path
    $py = Buscar-Python
    if (-not $py) {
      Fallar "Python se instaló pero esta ventana aún no lo ve. Cierra la terminal, ábrela de nuevo y repite el comando."
    }
  }
  Bien ("Python encontrado: " + ($py -join ' '))
  Nota "Instalando PlatformIO (solo la primera vez, tarda un par de minutos)..."
  New-Item -ItemType Directory -Force $herramientas | Out-Null
  $pyExe = $py[0]
  $pyArgs = @()
  if ($py.Length -gt 1) { $pyArgs = $py[1..($py.Length - 1)] }
  & $pyExe @pyArgs -m venv (Join-Path $herramientas 'venv')
  if ($LASTEXITCODE -ne 0) { Fallar "No se pudo crear el entorno de Python." }
  $pip = Join-Path $herramientas 'venv\Scripts\python.exe'
  & $pip -m pip install --disable-pip-version-check -q "platformio>=6.1,<7"
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $pioPropio)) { Fallar "No se pudo instalar PlatformIO. Revisa la conexión a internet." }
  $pio = $pioPropio
  Bien "PlatformIO instalado."
}

# ---------------------------------------------------------------- 2. Compilar
Paso "Compilando el firmware (la primera vez descarga el compilador del ESP32, unos 5 min)"
& $pio run
if ($LASTEXITCODE -ne 0) { Fallar "La compilación falló. Revisa los mensajes de arriba." }
Bien "Firmware compilado: .pio\build\esp32dev\firmware.bin"

if ($SoloCompilar) {
  Write-Host ""
  Bien "Listo (solo compilar)."
  exit 0
}

# ---------------------------------------------------------------- 3. Buscar la placa
Paso "Buscando el ESP32 conectado por USB"
if (-not $Puerto) {
  $json = & $pio device list --serial --json-output 2>$null
  $puertos = @()
  # ForEach-Object desenvuelve el arreglo: PowerShell 5.1 lo entrega como un solo objeto.
  if ($LASTEXITCODE -eq 0 -and $json) { $puertos = @(($json -join "`n") | ConvertFrom-Json | ForEach-Object { $_ }) }
  # Chips USB-serie de las placas ESP32: CP210x, CH340, CH9102, FTDI y USB nativo
  $conocidos = '10C4:EA60|1A86:7523|1A86:55D4|1A86:55D3|0403:6001|0403:6010|0403:6015|303A:'
  $candidatos = @($puertos | Where-Object { $_.hwid -match $conocidos })
  if ($candidatos.Count -eq 0) { $candidatos = @($puertos | Where-Object { $_.hwid -match 'VID:PID' }) }

  if ($candidatos.Count -eq 0) {
    Write-Host ""
    Write-Host "No se encontró ningún ESP32 conectado." -ForegroundColor Red
    Write-Host "  - Conecta la placa con un cable USB de DATOS (algunos solo cargan)."
    Write-Host "  - Si tu placa tiene el chip CH340, instala su driver: https://www.wch-ic.com/downloads/CH341SER_EXE.html"
    Write-Host "  - Si tu placa tiene el chip CP2102, instala su driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    Write-Host "  - Luego repite el comando, o indícale el puerto: .\desplegar.cmd -Puerto COM5"
    Write-Host "  - Para ver la app sin la placa: .\desplegar.cmd -Demo"
    exit 2
  }
  if ($candidatos.Count -gt 1) {
    Nota "Hay varias placas conectadas; se usa la primera. Para elegir otra: -Puerto COMx"
    foreach ($c in $candidatos) { Nota ("  " + $c.port + "  " + $c.description) }
  }
  $Puerto = $candidatos[0].port
}
Bien "Placa en $Puerto"

# ---------------------------------------------------------------- 4. Subir
Paso "Subiendo el firmware al ESP32"
Nota "Si se queda en 'Connecting....', mantén presionado el botón BOOT de la placa hasta que empiece a escribir."
& $pio run -t upload --upload-port $Puerto
if ($LASTEXITCODE -ne 0) {
  Nota "Falló la subida. Se reintenta a velocidad baja: mantén presionado BOOT ahora..."
  & $pio run -t upload --upload-port $Puerto -O "upload_speed=115200"
  if ($LASTEXITCODE -ne 0) {
    Fallar "No se pudo subir el firmware. Cierra cualquier otro programa que use $Puerto (Monitor Serie, Arduino IDE), mantén BOOT presionado y repite."
  }
}

Write-Host ""
Write-Host "  ============================================================" -ForegroundColor Green
Write-Host "   Cortina desplegada." -ForegroundColor Green
Write-Host "   1. Conéctate al WiFi 'Cortina-XXXX' (clave: cortina123)." -ForegroundColor Green
Write-Host "   2. Abre http://192.168.4.1 en el celular o el computador." -ForegroundColor Green
Write-Host "   3. Ve a Ajustes > Calibración para probar el motor y los topes." -ForegroundColor Green
Write-Host "  ============================================================" -ForegroundColor Green

if ($SinMonitor) { exit 0 }

# ---------------------------------------------------------------- 5. Monitor
Paso "Monitor serie (Ctrl+C para salir). Puedes escribir: abrir, cerrar, parar, ir 50, auto, red, ayuda"
& $pio device monitor -p $Puerto -b 115200
exit 0
