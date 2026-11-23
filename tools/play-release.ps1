param(
    [switch]$BuildOnly,
    [string]$VisualStudioRoot,
    [string]$OpenGLRoot = "$env:USERPROFILE/source/libs/OpenGL"
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path

try {
    if (!$VisualStudioRoot) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (!(Test-Path -LiteralPath $vswhere)) {
            throw 'Visual Studio locator missing. Pass -VisualStudioRoot with a C++ Visual Studio installation.'
        }
        $VisualStudioRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or !$VisualStudioRoot) {
            throw 'Visual Studio C++ tools were not found.'
        }
    }
    $msbuild = Join-Path $VisualStudioRoot 'MSBuild/Current/Bin/MSBuild.exe'
    if (!(Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }
    if (!(Test-Path -LiteralPath "$OpenGLRoot/include/GL/glew.h")) {
        throw "OpenGL libraries not found: $OpenGLRoot. Pass -OpenGLRoot with the installed library folder."
    }

    $project = Join-Path $repoRoot 'project_code/Out_of_Scale/Out_of_Scale.vcxproj'
    # Match the established team Release verification path. Keep /O2; avoid the
    # toolchain's large-geometry LTCG issue without changing either project file.
    $properties = @('/p:Configuration=Release', '/p:Platform=x64', '/p:PreferredToolArchitecture=x64',
                    '/p:WholeProgramOptimization=false', "/p:OpenGLRoot=$OpenGLRoot", '/nologo')
    Write-Host 'Building the current branch for play (Release x64)...'
    & $msbuild $project /t:Build @properties /v:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed; the previous executable will not be started.' }
    $target = (& $msbuild $project @properties -getProperty:TargetPath | Select-Object -Last 1).Trim()
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $target)) {
        throw 'The current Release executable could not be located.'
    }
    Write-Host "Release ready: $target"
    if (!$BuildOnly) {
        Push-Location (Split-Path -Parent $target)
        try {
            & $target
            if ($LASTEXITCODE -ne 0) { throw "Game exited with code $LASTEXITCODE." }
        } finally {
            Pop-Location
        }
    }
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
