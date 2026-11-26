<#
.SYNOPSIS
    Actualiza la lista de shaders del Shader Dock sin reinstalar el plugin.
    Updates the Shader Dock's list of shaders without reinstalling the plugin.

.DESCRIPTION
    Baja el shaders.json más reciente del repo y lo guarda en la carpeta de configuración del
    plugin (no pide permisos de administrador). El dock lo lee al abrir OBS y lo mezcla con el que
    trae instalado.

    Downloads the latest shaders.json and saves it in the plugin's config folder (no admin rights
    needed). The dock reads it when OBS starts and merges it with the bundled one.

.EXAMPLE
    irm https://raw.githubusercontent.com/hjmeneses/shader-dock/main/tools/update-manifest.ps1 | iex
#>
$ErrorActionPreference = 'Stop'
$url = 'https://raw.githubusercontent.com/hjmeneses/shader-dock/main/data/shaders.json'
$dir = Join-Path $env:APPDATA 'obs-studio\plugin_config\shader-dock'
$dst = Join-Path $dir 'shaders.json'

$json = (Invoke-WebRequest $url -UseBasicParsing).Content
if ($json -is [byte[]]) { $json = [Text.Encoding]::UTF8.GetString($json) }
$list = $json | ConvertFrom-Json
if (-not ($list -is [array]) -or $list.Count -eq 0 -or -not $list[0].repo) {
    throw 'La lista descargada no es válida / The downloaded list is not valid'
}

New-Item -ItemType Directory -Force $dir | Out-Null
$old = if (Test-Path $dst) { @(Get-Content $dst -Raw | ConvertFrom-Json).Count } else { 0 }
[IO.File]::WriteAllText($dst, $json, (New-Object Text.UTF8Encoding $false))

Write-Host "Shader Dock: $($list.Count) shaders" -ForegroundColor Green -NoNewline
if ($old) { Write-Host " (antes / before: $old)" } else { Write-Host '' }
$labels = $list | ForEach-Object { $_.label }
Write-Host ($labels -join ', ') -ForegroundColor DarkGray
if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    Write-Host 'Reinicia OBS para ver los cambios / Restart OBS to see the changes' -ForegroundColor Yellow
}
