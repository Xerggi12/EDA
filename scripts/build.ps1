$ErrorActionPreference = 'Stop'
$PSNativeCommandArgumentPassing = 'Legacy'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'backend') -Filter '*.cpp' -File | Sort-Object Name | Select-Object -ExpandProperty FullName)
$executable = Join-Path $buildDirectory 'registro-personas.exe'
$portableCompiler = Join-Path $projectRoot 'tools\zig-windows-x86_64-0.13.0\zig.exe'
if (Test-Path -LiteralPath $portableCompiler) {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $buildDirectory 'zig-global-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $buildDirectory 'zig-local-cache'
    & $portableCompiler c++ -target x86_64-windows-gnu -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror @sourceFiles -o $executable
    if ($LASTEXITCODE -ne 0) { throw 'La compilacion C++ fallo.' }
    Write-Host "Compilado: $executable"
    exit 0
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($installation) {
        $environmentScript = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
        $quotedSources = ($sourceFiles | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $command = '""{0}" >nul && cl.exe /nologo /std:c++17 /O2 /EHsc /W4 /WX /utf-8 /DNDEBUG {1} /Fe"{2}""' -f $environmentScript, $quotedSources, $executable
        $processInfo = New-Object System.Diagnostics.ProcessStartInfo
        $processInfo.WorkingDirectory = $buildDirectory
        $processInfo.FileName = $env:ComSpec
        $processInfo.Arguments = '/d /s /c ' + $command
        $processInfo.UseShellExecute = $false
        $processInfo.CreateNoWindow = $true
        $processInfo.RedirectStandardOutput = $true
        $processInfo.RedirectStandardError = $true
        $compilerProcess = [System.Diagnostics.Process]::Start($processInfo)
        $compilerOutput = $compilerProcess.StandardOutput.ReadToEndAsync()
        $compilerErrors = $compilerProcess.StandardError.ReadToEndAsync()
        $compilerProcess.WaitForExit()
        Write-Host $compilerOutput.Result
        if ($compilerErrors.Result) { Write-Host $compilerErrors.Result }
        if ($compilerProcess.ExitCode -ne 0) { throw 'La compilacion C++ fallo.' }
        Write-Host "Compilado: $executable"
        exit 0
    }
}
$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if ($compiler) {
    & $compiler.Source -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror @sourceFiles -o $executable
    if ($LASTEXITCODE -ne 0) { throw 'La compilacion C++ fallo.' }
    exit 0
}
throw 'Se requiere Visual Studio Build Tools con C++ x64 o g++ en PATH.'
