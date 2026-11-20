param(
    [string]$VisualStudioRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community',
    [string]$OpenGLRoot = "$env:USERPROFILE/source/libs/OpenGL"
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$outputRoot = Join-Path $repoRoot 'build/school-performance'
$vcvars = Join-Path $VisualStudioRoot 'VC/Auxiliary/Build/vcvars64.bat'
New-Item -ItemType Directory -Force $outputRoot | Out-Null
$sources = @('tests/SchoolPerformance.cpp', 'SchoolRoom.cpp', 'SchoolRenderer.cpp', 'SchoolCorridor.cpp') |
    ForEach-Object { '"' + (Join-Path $repoRoot "rooms/school/$_") + '"' }
$command = @"
@echo off
call "$vcvars" >nul
cl /nologo /std:c++20 /EHsc /W4 /MD /O2 /utf-8 /DGLFW_INCLUDE_NONE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I "$OpenGLRoot/include" /I "$OpenGLRoot/include/GL" $($sources -join ' ') /Fe:SchoolPerformance.exe /link /LIBPATH:"$OpenGLRoot/lib/x64" glfw3.lib glew32.lib opengl32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib
if errorlevel 1 exit /b 1
SchoolPerformance.exe
"@
$batch = Join-Path $outputRoot 'compile-run.cmd'
[IO.File]::WriteAllText($batch, $command, [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath "$OpenGLRoot/bin/x64/glew32.dll" -Destination $outputRoot
New-Item -ItemType Directory -Force "$outputRoot/assets" | Out-Null
Copy-Item -LiteralPath "$repoRoot/rooms/school/assets/art-class-still-life-v2.png" -Destination "$outputRoot/assets"
Push-Location $outputRoot
try {
    & cmd.exe /d /c $batch *> performance.log
    $result = $LASTEXITCODE
    Get-Content performance.log
    if ($result -ne 0) { throw "School performance run failed: $outputRoot/performance.log" }
} finally {
    Pop-Location
}
