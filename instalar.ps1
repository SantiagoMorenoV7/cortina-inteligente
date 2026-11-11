# Instala la cortina inteligente en un PC nuevo con una sola linea de PowerShell:
#
#   irm https://raw.githubusercontent.com/SantiagoMorenoV7/cortina-inteligente/main/instalar.ps1 | iex
#
# Descarga (o actualiza) el proyecto en Documentos\cortina-inteligente y ejecuta
# desplegar.ps1: instala lo necesario, compila y sube el firmware al ESP32.
# (Este archivo usa solo caracteres ASCII a proposito: se ejecuta con "iex".)

$ErrorActionPreference = 'Stop'
$repo = 'SantiagoMorenoV7/cortina-inteligente'
$destino = $env:CORTINA_DESTINO
if (-not $destino) { $destino = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'cortina-inteligente' }

Write-Host ''
Write-Host '==> Descargando el proyecto' -ForegroundColor Cyan

if (Test-Path (Join-Path $destino '.git')) {
  Write-Host "    Ya existe en $destino; actualizando..."
  git -C $destino pull --ff-only
  if ($LASTEXITCODE -ne 0) { Write-Host '    No se pudo actualizar (hay cambios locales); se usa la version que ya tienes.' -ForegroundColor Yellow }
} elseif (Get-Command git -ErrorAction SilentlyContinue) {
  git clone "https://github.com/$repo.git" $destino
  if ($LASTEXITCODE -ne 0) { throw 'No se pudo clonar el repositorio.' }
} else {
  Write-Host '    Git no esta instalado; se descarga como ZIP.'
  $zip = Join-Path $env:TEMP 'cortina-inteligente.zip'
  $tmp = Join-Path $env:TEMP ('cortina-inteligente-' + [guid]::NewGuid())
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  Invoke-WebRequest "https://github.com/$repo/archive/refs/heads/main.zip" -OutFile $zip -UseBasicParsing
  Expand-Archive $zip -DestinationPath $tmp -Force
  New-Item -ItemType Directory -Force $destino | Out-Null
  Copy-Item -Path (Join-Path $tmp 'cortina-inteligente-main\*') -Destination $destino -Recurse -Force
  Remove-Item $zip, $tmp -Recurse -Force
}

Write-Host "    Proyecto en: $destino" -ForegroundColor Green
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $destino 'desplegar.ps1')
