$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot 'build\registro-personas.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if (-not (Test-Path -LiteralPath $executable)) { throw 'No se pudo compilar el programa C++.' }
}
$portableNode = Join-Path $projectRoot 'tools\node\node.exe'
$nodeExecutable = $null
if (Test-Path -LiteralPath $portableNode) {
    $nodeExecutable = $portableNode
} else {
    $nodeCommand = Get-Command node -ErrorAction SilentlyContinue
    if ($nodeCommand) { $nodeExecutable = $nodeCommand.Source }
}
if (-not $nodeExecutable) { throw 'No se encontro tools\node\node.exe ni Node.js en PATH. Se requiere Node.js 20 o superior.' }
$nodeMajorVersion = & $nodeExecutable -p "process.versions.node.split('.')[0]"
if ($LASTEXITCODE -ne 0 -or [int]$nodeMajorVersion -lt 20) { throw 'El Node.js encontrado debe ser version 20 o superior.' }
& $nodeExecutable (Join-Path $projectRoot 'server\server.mjs')
if ($LASTEXITCODE -ne 0) { throw 'El servidor termino con un error.' }
