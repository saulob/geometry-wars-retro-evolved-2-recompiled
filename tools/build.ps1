<#
.SYNOPSIS
  Builds build\geometry_wars_retro_evolved_2.exe with Visual Studio's clang++,
  lld-link, CMake and Ninja. Run tools\codegen.ps1 first.
  -Run launches the game with the extracted game files in game\extracted.

.DESCRIPTION
  First makes sure tools\rexglue holds the ReXGlue runtime and GPU plugin this branch needs for -Config (see
  patches\rexglue-sdk\patches.txt), rebuilding them with build-patched-rexruntime.ps1 when it does not. The build
  then stages them next to the executable, and this script stops unless the staged copies match.

.EXAMPLE
  .\tools\build.ps1              # incremental build only
  .\tools\build.ps1 -Clean       # clean configure and build
  .\tools\build.ps1 -Run         # build, then launch the game
  .\tools\build.ps1 -Clean -Run
  .\tools\build.ps1 -Config Release -BuildDir build-release   # separate tree, used by package-release.ps1
#>
param(
    [switch]$Clean,
    [switch]$Run,
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string]$Config = 'RelWithDebInfo',
    [int]$Jobs = 0,
    [string]$BuildDir  # relative to the repository root; default: build
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'rexruntime-patch.ps1')
$SdkRoot = Join-Path $Root 'tools\rexglue\win-amd64'
$BuildDir =if (-not $BuildDir) { Join-Path $Root 'build' } elseif ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
$GameData = Join-Path $Root 'game\extracted'
$UserData = Join-Path $BuildDir 'user_data'
$Exe = Join-Path $BuildDir 'geometry_wars_retro_evolved_2.exe'
$XexSha256 = '6D5D9980A1D17DC74B9CCF540CEC16C2E144E7CEE3A74938E78C237B8ACEEB88'

$Xex = Join-Path $GameData 'default.xex'
if (-not (Test-Path $Xex)) { throw "Missing $Xex (extract the package into game\extracted first: tools\extract_stfs.ps1)" }
if ((Get-FileHash $Xex -Algorithm SHA256).Hash -ne $XexSha256) { throw "Unsupported $Xex (expected SHA-256 $XexSha256)" }
if (-not (Test-Path (Join-Path $Root 'tools\rexglue\win-amd64\lib\cmake\rexglue'))) { throw 'Missing tools\rexglue (run tools\codegen.ps1 first)' }
if (-not (Test-Path (Join-Path $Root 'generated\default\sources.cmake'))) { throw 'Missing generated\default (run tools\codegen.ps1 first)' }

$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $Vswhere)) { throw 'Visual Studio Installer (vswhere.exe) not found' }
$VsRoot = & $Vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
if (-not $VsRoot) { throw 'Visual Studio with the C++ x64 tools and C++ Clang tools is required' }
$VsDevCmd = Join-Path $VsRoot 'Common7\Tools\VsDevCmd.bat'
$LlvmBin = Join-Path $VsRoot 'VC\Tools\Llvm\x64\bin'
$Clang = Join-Path $LlvmBin 'clang.exe'
$ClangXX = Join-Path $LlvmBin 'clang++.exe'
$LldLink = Join-Path $LlvmBin 'lld-link.exe'
$CMake = Join-Path $VsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$Ninja = Join-Path $VsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
foreach ($tool in @($VsDevCmd, $Clang, $ClangXX, $LldLink, $CMake, $Ninja)) {
    if (-not (Test-Path $tool)) { throw "Required tool not found: $tool" }
}

Update-PatchedRuntime $SdkRoot $Config $Jobs

$SavedEnv =[Environment]::GetEnvironmentVariables()
try {
    # Keep MSYS2's gcc, cmake and ninja out of the build.
    $env:PATH = (($env:PATH -split ';') | Where-Object { $_ -and $_ -notmatch '\\msys64\\' -and $_ -notmatch '^C:\\msys64' }) -join ';'

    Write-Host ''
    Write-Host '===================================================='
    Write-Host " Geometry Wars: Retro Evolved 2 - $Config build"
    Write-Host '===================================================='
    Write-Host ''
    Write-Host 'Loading Visual Studio x64 environment...'
    $vsEnv = cmd.exe /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 -no_logo >nul && set"
    foreach ($line in $vsEnv) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
    }
    if (-not $env:VCToolsVersion) { throw 'VsDevCmd.bat did not set up the x64 environment' }

    $env:PATH = "$LlvmBin;$(Split-Path $CMake);$(Split-Path $Ninja);$env:PATH"

    Write-Host ''
    Write-Host "Clang++:`n  $ClangXX`n  $(& $ClangXX --version | Select-Object -First 1)"
    Write-Host "CMake:`n  $CMake"
    Write-Host "MSVC tools:`n  $env:VCToolsVersion"
    Write-Host ''

    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host 'Cleaning build (keeping build\user_data)...'
        Get-ChildItem $BuildDir -Force | Where-Object { $_.Name -ne 'user_data' -and $_.Name -ne 'dump_image' } | Remove-Item -Recurse -Force
    }

    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    $configured = (Test-Path (Join-Path $BuildDir 'build.ninja')) -and (Test-Path $cache) -and
        (Select-String -Path $cache -SimpleMatch "CMAKE_BUILD_TYPE:STRING=$Config" -Quiet) -and
        (Select-String -Path $cache -SimpleMatch "CMAKE_CXX_COMPILER:FILEPATH=$($ClangXX -replace '\\', '/')" -Quiet)
    if (-not $configured) {
        Write-Host 'Configuring...'
        & $CMake -S $Root -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config" `
            "-DCMAKE_CXX_COMPILER:FILEPATH=$ClangXX" "-DCMAKE_LINKER:FILEPATH=$LldLink" "-DCMAKE_MAKE_PROGRAM:FILEPATH=$Ninja"
        if ($LASTEXITCODE) { throw "CMake configure failed ($LASTEXITCODE)" }
    }

    Write-Host 'Building...'
    $parallel = if ($Jobs -gt 0) { @('--parallel', $Jobs) } else { @() }
    & $CMake --build $BuildDir @parallel
    if ($LASTEXITCODE) { throw "Build failed ($LASTEXITCODE)" }
} finally {
    foreach ($name in @([Environment]::GetEnvironmentVariables().Keys)) {
        if (-not $SavedEnv.Contains($name)) { Remove-Item "Env:$name" }
    }
    foreach ($name in $SavedEnv.Keys) { Set-Item "Env:$name" $SavedEnv[$name] }
}

$suffix = @{ Debug = 'd'; RelWithDebInfo = 'rd'; Release = '' }[$Config]
$required = @('geometry_wars_retro_evolved_2.exe', "rexruntime$suffix.dll", "rexgpu-xenos$suffix.dll", 'geometry_wars_retro_evolved_2.toml')
if ($Config -ne 'Release') { $required += "TracyClient$suffix.dll" }
foreach ($file in $required) {
    $path = Join-Path $BuildDir $file
    if (-not (Test-Path $path)) { throw "Build output is missing $path" }
}
$problems = @(Test-PatchedRuntime $SdkRoot $Config) + @(Test-StagedRuntime $SdkRoot $BuildDir $Config)
if ($problems.Count) {
    foreach ($problem in $problems) { Write-Host "  $problem" -ForegroundColor Red }
    throw 'The ReXGlue runtime and GPU plugin next to the executable are not the ones this branch needs'
}

Write-Host ''
Write-Host '===================================================='
Write-Host ' BUILD SUCCESSFUL'
Write-Host '===================================================='
Write-Host "  $Exe"
Write-Host "  $((Get-RuntimeDllNames $Config) -join ', ') match tools\rexglue"
Write-Host ''

if (-not $Run) { return }

$running = Get-Process -Name 'geometry_wars_retro_evolved_2' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe }
if ($running) {
    Write-Warning "geometry_wars_retro_evolved_2.exe is already running (PID $($running.Id -join ', ')); not launching another copy."
    return
}

$gameArgs = @("--game_data_root=`"$GameData`"", "--user_data_root=`"$UserData`"")
Write-Host 'Launching:'
Write-Host "  $Exe $($gameArgs -join ' ')"
Write-Host "  working directory: $BuildDir"
Write-Host "  logs: $(Join-Path $BuildDir 'logs')"
Start-Process -FilePath $Exe -ArgumentList $gameArgs -WorkingDirectory $BuildDir
