# Shared by build.ps1, build-patched-rexruntime.ps1 and package-release.ps1: the pinned ReXGlue baseline, the patches
# Windows applies (patches\rexglue-sdk\patches.txt), and the checks that an SDK folder holds the runtime and GPU plugin
# built from them and that a build folder holds the SDK's copies.

$RuntimePatchDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'patches\rexglue-sdk'
$RuntimeStampName = 'rexruntime-patch.txt'
$GameExeName = 'geometry_wars_retro_evolved_2.exe'
$RexPlatform = 'windows'
$RexTargets = @('rexruntime', 'rexgpu-xenos')
$RexConfigs = @('Debug', 'RelWithDebInfo', 'Release')
$RexBuildFlags = '-march=x86-64-v2 -ffile-prefix-map=<source>=.'

function Read-RuntimeBaseline {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $RuntimePatchDir 'baseline.env')) {
        if ($line -match '^\s*([A-Z0-9_]+)=(.*)$') { $values[$Matches[1]] = $Matches[2].Trim() }
    }
    return $values
}

function Get-FileSha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# Every patch in patches.txt, in order. Stops on a patch file that is not listed, a missing one, a reused number or a
# list out of numeric order.
function Read-PatchManifest {
    $files = @(Get-ChildItem -LiteralPath $RuntimePatchDir -Filter '*.patch' -File | ForEach-Object { $_.Name })
    $duplicates = @($files | Group-Object { $_.Substring(0, [Math]::Min(4, $_.Length)) } | Where-Object { $_.Count -gt 1 })
    if ($duplicates.Count) { throw "patches\rexglue-sdk has more than one patch numbered $(($duplicates | ForEach-Object { $_.Name }) -join ', ')" }
    $patches = @()
    $previous = ''
    foreach ($line in Get-Content -LiteralPath (Join-Path $RuntimePatchDir 'patches.txt')) {
        if ($line -match '^\s*(#|$)') { continue }
        $fields = @($line.Trim() -split '\s+')
        if ($fields.Count -ne 3 -or $fields[0] -notmatch '^\d{4}-\S+\.patch$') { throw "patches.txt: cannot read '$line'" }
        $platforms = @($fields[1] -split ',')
        $targets = @($fields[2] -split ',')
        foreach ($p in $platforms) { if (@('windows', 'linux') -notcontains $p) { throw "patches.txt: unknown platform '$p' for $($fields[0])" } }
        foreach ($t in $targets) { if ($RexTargets -notcontains $t) { throw "patches.txt: unknown target '$t' for $($fields[0])" } }
        if ([string]::CompareOrdinal($fields[0], $previous) -le 0) { throw "patches.txt must list the patches in numeric order ($($fields[0]))" }
        $previous = $fields[0]
        $path = Join-Path $RuntimePatchDir $fields[0]
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "patches.txt lists $($fields[0]), which is missing" }
        $patches += [pscustomobject]@{ Name = $fields[0]; Path = $path; Platforms = $platforms; Targets = $targets }
    }
    $unlisted =@($files | Where-Object { @($patches | ForEach-Object { $_.Name }) -notcontains $_ })
    if ($unlisted.Count) { throw "patches\rexglue-sdk\patches.txt does not list $($unlisted -join ', ')" }
    return $patches
}

function Get-SelectedPatches {
    return @(Read-PatchManifest | Where-Object { $_.Platforms -contains $RexPlatform })
}

function Get-PatchedTargets {
    $targets = @(Get-SelectedPatches | ForEach-Object { $_.Targets })
    return @($RexTargets | Where-Object { $targets -contains $_ })
}

function Get-ConfigSuffix([string]$Config) {
    return @{ Debug = 'd'; RelWithDebInfo = 'rd'; Release = '' }[$Config]
}

# SDK-relative files of a target in one configuration.
function Get-TargetFiles([string]$Target, [string]$Config) {
    $s = Get-ConfigSuffix $Config
    if ($Target -eq 'rexruntime') { return @("bin/rexruntime$s.dll", "lib/rexruntime$s.lib") }
    return @("bin/rexgpu-xenos$s.dll")
}

function Get-OfficialSha256($Baseline, [string]$Leaf) {
    $kind = if ($Leaf -match 'rexgpu-xenos') { 'GPU' } else { 'RUNTIME' }
    $variant = if ($Leaf -match 'rd\.(dll|lib)$') { '_RD' } elseif ($Leaf -match 'd\.(dll|lib)$') { '_D' } else { '' }
    $lib = if ($Leaf -like '*.lib') { '_LIB' } else { '' }
    return $Baseline["OFFICIAL_WIN_$kind$variant$($lib)_SHA256"]
}

function Get-VisualStudioRoot {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'Visual Studio was not found (vswhere)' }
    $root = "$(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath)".Trim()
    if (-not $root) { throw 'Visual Studio was not found (vswhere)' }
    return $root
}

function Get-CompilerIdentity {
    $vsRoot = Get-VisualStudioRoot
    $clang = "$(& (Join-Path $vsRoot 'VC\Tools\Llvm\x64\bin\clang++.exe') --version | Select-Object -First 1)".Trim()
    $msvc = "$(Get-Content -LiteralPath (Join-Path $vsRoot 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -TotalCount 1)".Trim()
    return "$clang; MSVC $msvc"
}

# Everything the patched files are built from besides the source commit's own files. A stamp with other inputs is stale.
function Get-RuntimeInputs {
    $baseline = Read-RuntimeBaseline
    $lines = @("sdk_version=$($baseline.SDK_VERSION)", "sdk_tag=$($baseline.SDK_TAG)", "sdk_commit=$($baseline.SDK_COMMIT)",
               "platform=$RexPlatform")
    foreach ($patch in Get-SelectedPatches) { $lines += "patch=$($patch.Name) $(Get-FileSha256 $patch.Path) $($patch.Targets -join ',')" }
    $lines += @("targets=$((Get-PatchedTargets) -join ' ')", "compiler=$(Get-CompilerIdentity)", "flags=$RexBuildFlags")
    return $lines
}

function Read-RuntimeStamp([string]$SdkDir) {
    $path = Join-Path $SdkDir $RuntimeStampName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    $stamp = @{ Inputs = @(); Hashes = @{} }
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -match '^([0-9a-f]{64})  (.+)$') { $stamp.Hashes[$Matches[2]] = $Matches[1] }
        elseif ($line) { $stamp.Inputs += $line }
    }
    return $stamp
}

# Records the files just installed. Files of other configurations stay recorded only if they were built from the same inputs.
function Write-RuntimeStamp([string]$SdkDir, [string[]]$Installed) {
    $inputs = @(Get-RuntimeInputs)
    $hashes = @{}
    $stamp = Read-RuntimeStamp $SdkDir
    if ($stamp -and (($stamp.Inputs -join "`n") -eq ($inputs -join "`n"))) { $hashes = $stamp.Hashes }
    foreach ($leaf in $Installed) { $hashes[$leaf] = Get-FileSha256 (Join-Path $SdkDir $leaf) }
    $lines = $inputs + @($hashes.Keys | Sort-Object | ForEach-Object { "$($hashes[$_])  $_" })
    [IO.File]::WriteAllText((Join-Path $SdkDir $RuntimeStampName), (($lines -join "`n") + "`n"), (New-Object Text.UTF8Encoding $false))
    return $lines
}

# Returns the problems found; an empty list means $SdkDir holds, for $Config, the files built from the current
# baseline and patches, and the stock release files of every target no patch changes.
function Test-PatchedRuntime([string]$SdkDir, [string]$Config) {
    $problems = New-Object System.Collections.Generic.List[string]
    $baseline = Read-RuntimeBaseline
    $patched = @(Get-PatchedTargets)
    $stamp = Read-RuntimeStamp $SdkDir
    if ($patched.Count) {
        if (-not $stamp) {
            $problems.Add("$RuntimeStampName is missing")
        } elseif (($stamp.Inputs -join "`n") -ne ((Get-RuntimeInputs) -join "`n")) {
            $old = @($stamp.Inputs)
            $changed = @(Get-RuntimeInputs | Where-Object { $old -notcontains $_ })
            $problems.Add("built from other inputs than this branch's; now: $($changed -join '; ')")
        }
    }
    foreach ($target in $RexTargets) {
        foreach ($leaf in Get-TargetFiles $target $Config) {
            $path = Join-Path $SdkDir $leaf
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $problems.Add("missing: $leaf"); continue }
            $hash = Get-FileSha256 $path
            $official = Get-OfficialSha256 $baseline $leaf
            if ($patched -notcontains $target) {
                if ($hash -ne $official) { $problems.Add("$leaf is not the stock ReXGlue release file, and no patch for this platform changes $target") }
            } elseif ($hash -eq $official) {
                $problems.Add("$leaf is the stock ReXGlue release file")
            } elseif (-not $stamp -or $stamp.Hashes[$leaf] -ne $hash) {
                $problems.Add("$leaf does not match the patched build")
            }
        }
    }
    return $problems
}

# The runtime and GPU plugin DLLs a configuration loads from beside the executable.
function Get-RuntimeDllNames([string]$Config) {
    $s = Get-ConfigSuffix $Config
    return @("rexruntime$s.dll", "rexgpu-xenos$s.dll")
}

# Returns the problems found; an empty list means $BuildDir holds the executable and exactly the SDK's DLLs.
function Test-StagedRuntime([string]$SdkDir, [string]$BuildDir, [string]$Config) {
    $problems = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDir $GameExeName) -PathType Leaf)) { $problems.Add("missing: $GameExeName in $BuildDir") }
    foreach ($name in Get-RuntimeDllNames $Config) {
        $expected = Join-Path $SdkDir "bin\$name"
        $actual = Join-Path $BuildDir $name
        if (-not (Test-Path -LiteralPath $actual -PathType Leaf)) { $problems.Add("$name is not next to the executable"); continue }
        if ((Test-Path -LiteralPath $expected -PathType Leaf) -and (Get-FileSha256 $expected) -ne (Get-FileSha256 $actual)) {
            $problems.Add("$name next to the executable is not the one in tools\rexglue")
        }
    }
    return $problems
}

# Rebuilds the patched files for $Config when the installed ones are not current, and stops if they still are not.
function Update-PatchedRuntime([string]$SdkDir, [string]$Config, [int]$Jobs = 0) {
    $problems = @(Test-PatchedRuntime $SdkDir $Config)
    if ($problems.Count) {
        Write-Host "ReXGlue ($Config): the runtime and GPU plugin in tools\rexglue do not match this branch, preparing them:"
        foreach ($problem in $problems) { Write-Host "  $problem" }
        $builderArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'build-patched-rexruntime.ps1'), '-Config', $Config)
        if ($Jobs -gt 0) { $builderArgs += @('-Jobs', $Jobs) }
        & (Get-Process -Id $PID).Path @builderArgs
        if ($LASTEXITCODE) { throw "Preparing the ReXGlue runtime and GPU plugin failed ($LASTEXITCODE)" }
        $problems = @(Test-PatchedRuntime $SdkDir $Config)
        if ($problems.Count) { throw "tools\rexglue still does not match this branch: $($problems -join '; ')" }
    }
    $names = @(Get-SelectedPatches | ForEach-Object { $_.Name })
    Write-Host "ReXGlue ($Config): current; patches: $(if ($names.Count) { $names -join ', ' } else { 'none' })"
}
