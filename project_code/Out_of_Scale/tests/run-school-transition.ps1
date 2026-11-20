param(
    [string]$VisualStudioRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community',
    [string]$OpenGLRoot = "$env:USERPROFILE/source/libs/OpenGL"
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$outputRoot = Join-Path $repoRoot 'build/school-transition-tests'
$msbuild = Join-Path $VisualStudioRoot 'MSBuild/Current/Bin/MSBuild.exe'
$vcvars = Join-Path $VisualStudioRoot 'VC/Auxiliary/Build/vcvars64.bat'
New-Item -ItemType Directory -Force $outputRoot | Out-Null
Push-Location $repoRoot
try {
    $project = 'project_code/Out_of_Scale/Out_of_Scale.vcxproj'
    $properties = @('/p:Configuration=Release', '/p:Platform=x64', '/p:PreferredToolArchitecture=x64',
                    "/p:OpenGLRoot=$OpenGLRoot", '/p:WholeProgramOptimization=false', '/nologo')
    & $msbuild $project /t:Build @properties /v:minimal *> "$outputRoot/team-build.log"
    if ($LASTEXITCODE -ne 0) { throw "Team build failed: $outputRoot/team-build.log" }
    $objectsRoot = (& $msbuild $project @properties -getProperty:IntDir | Select-Object -Last 1).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not locate current team object files' }
    $objects = @('MilitaryRoom', 'HomeRoom', 'TestRoom', 'BoxRenderer', 'PauseMenu',
                 'SchoolRoom', 'SchoolCorridor', 'SchoolRenderer') | ForEach-Object {
        $object = Join-Path $objectsRoot "$_.obj"
        if (!(Test-Path -LiteralPath $object)) { throw "Missing object: $object" }
        '"' + $object + '"'
    }
    $source = Join-Path $PSScriptRoot 'SchoolTransitionTests.cpp'
    $executable = Join-Path $outputRoot 'SchoolTransitionTests.exe'
    $batch = Join-Path $outputRoot 'compile-run.cmd'
    $command = @"
@echo off
call "$vcvars" >nul
cl /nologo /std:c++20 /EHsc /W4 /MD /O2 /utf-8 /DGLFW_INCLUDE_NONE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I "$OpenGLRoot/include" /I "$OpenGLRoot/include/GL" "$source" /Fo:"$outputRoot/SchoolTransitionTests.obj" /Fe:"$executable" $($objects -join ' ') /link /LIBPATH:"$OpenGLRoot/lib/x64" glfw3.lib glew32.lib opengl32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib
if errorlevel 1 exit /b 1
"$executable" "$outputRoot"
"@
    [IO.File]::WriteAllText($batch, $command, [Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath "$OpenGLRoot/bin/x64/glew32.dll" -Destination $outputRoot
    New-Item -ItemType Directory -Force "$outputRoot/assets" | Out-Null
    Copy-Item -LiteralPath 'rooms/school/assets/art-class-still-life-v2.png' -Destination "$outputRoot/assets"
    & cmd.exe /d /c $batch *> "$outputRoot/tests.log"
    $testExit = $LASTEXITCODE
    Get-Content "$outputRoot/tests.log"
    if ($testExit -ne 0) { throw "Transition tests failed: $outputRoot/tests.log" }
} finally {
    Pop-Location
}
