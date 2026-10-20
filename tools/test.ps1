<#
.SYNOPSIS
  Builds and runs tests\live_tests.cpp (the local stats store and the guest-buffer helpers behind the offline Live
  services), tests\achievements_overlay_rules_tests.cpp (the achievements overlay's controller and window-size
  rules), tests\launcher_tests.cpp (game-file validation, package reading and safe install, launcher settings,
  the window fit and the achievement summary), tests\launcher_settings_tests.cpp (the settings model, the PLAY gate,
  the Display, Graphics and Gameplay options), tests\graphics_adapter_tests.cpp (the Graphics card values and
  the adapter order), tests\aspect_ratio_tests.cpp (the Aspect Ratio table, canvas, window sizes and the retired
  Letterbox setting), tests\window_host_tests.cpp (the window title, the frame-rate
  format and the sampler), tests\cheats_tests.cpp (the cheat settings, the in-game menu model and the host
  hotkeys) and tests\user_data_tests.cpp (the portable user_data root and the move of the system
  folder into it), with Visual Studio's clang++, then checks that the release folder check refuses user data and
  saves (scratch folders only). No game files and no ReXGlue runtime are needed. The
  launcher tests need TRG Launcher: third_party\trg-launcher, or the folder named by GW2_TRG_LAUNCHER_DIR.

.EXAMPLE
  .\tools\test.ps1
#>
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$OutDir = Join-Path $Root 'build\tests'
$Exe = Join-Path $OutDir 'live_tests.exe'
$RulesExe = Join-Path $OutDir 'achievements_overlay_rules_tests.exe'
$LauncherExe = Join-Path $OutDir 'launcher_tests.exe'
$SettingsExe = Join-Path $OutDir 'launcher_settings_tests.exe'
$WindowHostExe = Join-Path $OutDir 'window_host_tests.exe'
$FrameCounterExe = Join-Path $OutDir 'frame_counter_tests.exe'
$FrameLimiterExe = Join-Path $OutDir 'frame_limiter_tests.exe'
$PlacementExe = Join-Path $OutDir 'launcher_placement_tests.exe'
$UserDataExe = Join-Path $OutDir 'user_data_tests.exe'
$CheatsExe = Join-Path $OutDir 'cheats_tests.exe'
$OptionsMenuExe = Join-Path $OutDir 'options_menu_tests.exe'
$AdapterExe = Join-Path $OutDir 'graphics_adapter_tests.exe'
$AspectExe = Join-Path $OutDir 'aspect_ratio_tests.exe'

$TrgDir = Join-Path $Root 'third_party\trg-launcher'
if (-not (Test-Path (Join-Path $TrgDir 'src\settings.cpp'))) {
    throw 'TRG Launcher not found: run `git submodule update --init third_party/trg-launcher`'
}

$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $Vswhere)) { throw 'Visual Studio Installer (vswhere.exe) not found' }
$VsRoot = & $Vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
if (-not $VsRoot) { throw 'Visual Studio with the C++ x64 tools and C++ Clang tools is required' }
$VsDevCmd = Join-Path $VsRoot 'Common7\Tools\VsDevCmd.bat'
$ClangXX = Join-Path $VsRoot 'VC\Tools\Llvm\x64\bin\clang++.exe'
foreach ($tool in @($VsDevCmd, $ClangXX)) {
    if (-not (Test-Path $tool)) { throw "Required tool not found: $tool" }
}

$SavedEnv = [Environment]::GetEnvironmentVariables()
try {
    # Keep MSYS2's gcc out of the build.
    $env:PATH = (($env:PATH -split ';') | Where-Object { $_ -and $_ -notmatch '\\msys64\\' -and $_ -notmatch '^C:\\msys64' }) -join ';'
    $vsEnv = cmd.exe /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 -no_logo >nul && set"
    foreach ($line in $vsEnv) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
    }

    New-Item -ItemType Directory -Force $OutDir | Out-Null
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\live_tests.cpp') (Join-Path $Root 'src\local_stats.cpp') (Join-Path $Root 'src\live_stats.cpp') `
        -o $Exe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\achievements_overlay_rules_tests.cpp') -o $RulesExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') -I (Join-Path $Root 'tools\rexglue\win-amd64\include') -I (Join-Path $TrgDir 'include') `
        (Join-Path $Root 'tests\launcher_tests.cpp') (Join-Path $Root 'src\game_files.cpp') `
        (Join-Path $Root 'src\stfs.cpp') (Join-Path $Root 'src\launcher_achievements.cpp') `
        -lbcrypt -o $LauncherExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\window_host_tests.cpp') -o $WindowHostExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\frame_counter_tests.cpp') -o $FrameCounterExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\frame_limiter_tests.cpp') (Join-Path $Root 'src\frame_wait.cpp') -o $FrameLimiterExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -D_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS `
        -I (Join-Path $Root 'src') -isystem (Join-Path $Root 'tools\rexglue\win-amd64\include') -I (Join-Path $TrgDir 'include') `
        (Join-Path $Root 'tests\launcher_placement_tests.cpp') -o $PlacementExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -D_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS `
        -I (Join-Path $Root 'src') -isystem (Join-Path $Root 'tools\rexglue\win-amd64\include') -I (Join-Path $TrgDir 'include') `
        (Join-Path $Root 'tests\launcher_settings_tests.cpp') (Join-Path $Root 'src\launcher_settings.cpp') `
        (Join-Path $Root 'src\launcher_options.cpp') `
        (Join-Path $TrgDir 'src\settings.cpp') (Join-Path $TrgDir 'src\platform.cpp') `
        -lshell32 -lole32 -lcomdlg32 -lwinmm -luser32 -o $SettingsExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -D_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS `
        -I (Join-Path $Root 'src') -isystem (Join-Path $Root 'tools\rexglue\win-amd64\include') -I (Join-Path $TrgDir 'include') `
        (Join-Path $Root 'tests\user_data_tests.cpp') (Join-Path $Root 'src\user_data.cpp') -o $UserDataExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\cheats_tests.cpp') -o $CheatsExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\options_menu_tests.cpp') -o $OptionsMenuExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\graphics_adapter_tests.cpp') (Join-Path $Root 'src\graphics_adapters.cpp') `
        -ldxgi -ld3d12 -o $AdapterExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
    & $ClangXX -std=c++23 -O1 -Wall -Wextra -I (Join-Path $Root 'src') `
        (Join-Path $Root 'tests\aspect_ratio_tests.cpp') -o $AspectExe
    if ($LASTEXITCODE) { throw "Test build failed ($LASTEXITCODE)" }
} finally {
    foreach ($name in @([Environment]::GetEnvironmentVariables().Keys)) {
        if (-not $SavedEnv.Contains($name)) { Remove-Item "Env:$name" }
    }
    foreach ($name in $SavedEnv.Keys) { Set-Item "Env:$name" $SavedEnv[$name] }
}

& $Exe
if ($LASTEXITCODE) { throw "Tests failed ($LASTEXITCODE)" }
& $RulesExe
if ($LASTEXITCODE) { throw "Achievements overlay rule tests failed ($LASTEXITCODE)" }
& $LauncherExe
if ($LASTEXITCODE) { throw "Launcher tests failed ($LASTEXITCODE)" }
& $WindowHostExe
if ($LASTEXITCODE) { throw "Window title tests failed ($LASTEXITCODE)" }
& $FrameCounterExe
if ($LASTEXITCODE) { throw "Frame counter tests failed ($LASTEXITCODE)" }
& $FrameLimiterExe
if ($LASTEXITCODE) { throw "Frame limiter tests failed ($LASTEXITCODE)" }
& $PlacementExe
if ($LASTEXITCODE) { throw "Launcher placement tests failed ($LASTEXITCODE)" }
& $SettingsExe
if ($LASTEXITCODE) { throw "Launcher settings tests failed ($LASTEXITCODE)" }
& $OptionsMenuExe
if ($LASTEXITCODE) { throw "Options menu tests failed ($LASTEXITCODE)" }
& $UserDataExe
if ($LASTEXITCODE) { throw "User data tests failed ($LASTEXITCODE)" }
& $CheatsExe
if ($LASTEXITCODE) { throw "Cheats tests failed ($LASTEXITCODE)" }
& $AdapterExe
if ($LASTEXITCODE) { throw "Graphics card tests failed ($LASTEXITCODE)" }
& $AspectExe
if ($LASTEXITCODE) { throw "Aspect ratio tests failed ($LASTEXITCODE)" }

# The release check accepts a fresh extraction and refuses user data, migration leftovers, saves, logs and game files.
$ReleaseCheck = Join-Path $PSScriptRoot 'package-release.ps1'
$StageRoot = Join-Path $OutDir 'release_check'
$ReleaseCases = [ordered]@{
    'fresh extraction'    = $null
    'empty user_data'     = 'user_data\'
    'profile'             = 'user_data\584108FF\profile\User\63E83FFF'
    'scores'              = 'user_data\584108FF\live\stats.toml'
    'achievements'        = 'user_data\achievements\584108FF.toml'
    'launcher settings'   = 'user_data\launcher\settings.toml'
    'cache'               = 'cache\shaders\a.bin'
    'migration leftover'  = 'user_data.migrating\'
    'log'                 = 'logs\geometry_wars_retro_evolved_2_001.log'
    'game file'           = 'game\default.xex'
}
Write-Host 'release check'
$releaseFailures = 0
foreach ($case in $ReleaseCases.Keys) {
    $stage = Join-Path $StageRoot ($case -replace '\W', '_')
    New-Item -ItemType Directory -Force (Join-Path $stage 'licenses') | Out-Null
    foreach ($leaf in 'geometry_wars_retro_evolved_2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'geometry_wars_retro_evolved_2.toml', 'README.txt', 'LICENSE',
        'licenses\ReXGlue-LICENSE.txt', 'licenses\THIRD-PARTY-LICENSES.txt') {
        Set-Content -LiteralPath (Join-Path $stage $leaf) -Value 'placeholder'
    }
    $extra = $ReleaseCases[$case]
    if ($extra) {
        $target = Join-Path $stage $extra
        if ($extra.EndsWith('\')) {
            New-Item -ItemType Directory -Force $target | Out-Null
        } else {
            New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
            Set-Content -LiteralPath $target -Value 'placeholder'
        }
    }
    $global:LASTEXITCODE = 0
    & $ReleaseCheck -VerifyOnly $stage *> $null
    if (($LASTEXITCODE -eq 0) -ne ($null -eq $extra)) {
        Write-Host "  FAIL $case"
        $releaseFailures++
    }
}
Remove-Item -LiteralPath $StageRoot -Recurse -Force
Write-Host "$($ReleaseCases.Count) release check cases, $releaseFailures failures"
if ($releaseFailures) { throw 'Release check tests failed' }
$global:LASTEXITCODE = 0
