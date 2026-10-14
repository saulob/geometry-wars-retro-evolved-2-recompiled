<#
.SYNOPSIS
  Prepares tools\rexglue\win-amd64 for this branch: builds the ReXGlue targets that the Windows patches in
  patches\rexglue-sdk\patches.txt change, from the pinned v0.10.0 source, and puts the stock release files back for
  the targets no patch changes.

.DESCRIPTION
  tools\build.ps1 runs this by itself whenever tools\rexglue does not match the branch, and the CMake build checks
  the same thing with -Check, so it is only needed by hand for the options below. The source is fetched from the
  official ReXGlue repository (network needed the first time) into -WorkDir, checked against the pinned commit in
  patches\rexglue-sdk\baseline.env, reset, patched and built. The installed files and the fingerprint of their inputs
  are recorded in tools\rexglue\win-amd64\rexruntime-patch.txt. See docs\rexglue-runtime-patch.md.

.EXAMPLE
  .\tools\build-patched-rexruntime.ps1                          # RelWithDebInfo and Release
  .\tools\build-patched-rexruntime.ps1 -Config Debug
  .\tools\build-patched-rexruntime.ps1 -Check -Config Release -Staged build-release   # verify only
  .\tools\build-patched-rexruntime.ps1 -StressTest -SkipBuild   # allocator stress test on the installed runtime
  .\tools\build-patched-rexruntime.ps1 -Restore                 # put every stock file back
#>
param(
    [string]$WorkDir,     # ReXGlue source and build folder; default: out\rexglue-sdk-src (a short path is safer)
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string[]]$Config = @('RelWithDebInfo', 'Release'),
    [switch]$Check,       # only verify tools\rexglue for -Config, and the DLLs in -Staged
    [string]$Staged,      # with -Check: a build folder that must hold the SDK's runtime and GPU plugin
    [switch]$Restore,     # reinstall every stock runtime and GPU plugin file from the SDK download and exit
    [switch]$StressTest,  # run tests\physical_heap_stress.cpp against the installed runtime
    [switch]$SkipBuild,   # with -StressTest: do not build the runtime first
    [int]$Jobs = 0
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'rexruntime-patch.ps1')
$Baseline = Read-RuntimeBaseline
$Sdk = Join-Path $Root 'tools\rexglue\win-amd64'
$SdkZip = Join-Path $Root "tools\rexglue\rexglue-sdk-$($Baseline.SDK_VERSION)-win-amd64.zip"
$WorkDir = if (-not $WorkDir) { Join-Path $Root 'out\rexglue-sdk-src' } elseif ([IO.Path]::IsPathRooted($WorkDir)) { $WorkDir } else { Join-Path $Root $WorkDir }
$StampPath = Join-Path $Sdk $RuntimeStampName

if (-not (Test-Path (Join-Path $Sdk 'lib\cmake\rexglue'))) { throw 'Missing tools\rexglue (run tools\codegen.ps1 first)' }

if ($Check) {
    $problems = @()
    foreach ($c in $Config) {
        $problems += @(Test-PatchedRuntime $Sdk $c)
        if ($Staged) { $problems += @(Test-StagedRuntime $Sdk $(if ([IO.Path]::IsPathRooted($Staged)) { $Staged } else { Join-Path $Root $Staged }) $c) }
    }
    if (-not $problems.Count) { exit 0 }
    Write-Host "ReXGlue: tools\rexglue or the build folder does not match this branch ($($Config -join ', ')):"
    foreach ($problem in $problems) { Write-Host "  $problem" }
    Write-Host 'Run .\tools\build.ps1, which prepares them (or .\tools\build-patched-rexruntime.ps1 -Config <config>).'
    exit 1
}

function Invoke-Native([string]$What, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE) { throw "$What failed ($LASTEXITCODE)" }
}

function Import-VsEnvironment {
    $vsRoot = Get-VisualStudioRoot
    $vsDevCmd = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
    $llvmBin = Join-Path $vsRoot 'VC\Tools\Llvm\x64\bin'
    $cmakeBin = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
    $ninjaBin = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
    foreach ($tool in @($vsDevCmd, (Join-Path $llvmBin 'clang++.exe'), (Join-Path $cmakeBin 'cmake.exe'), (Join-Path $ninjaBin 'ninja.exe'))) {
        if (-not (Test-Path $tool)) { throw "Required tool not found: $tool" }
    }
    $env:PATH = (($env:PATH -split ';') | Where-Object { $_ -and $_ -notmatch '\\msys64\\' }) -join ';'
    $env:PATH = "$env:PATH;$(Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer')"
    foreach ($line in (cmd.exe /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 -no_logo >nul && set")) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
    }
    if (-not $env:VCToolsVersion) { throw 'VsDevCmd.bat did not set up the x64 environment' }
    $env:PATH = "$llvmBin;$cmakeBin;$ninjaBin;$env:PATH"
}

# Puts the stock release files back from the SDK download; returns the ones it replaced.
function Restore-StockFiles([string[]]$Leaves) {
    $replaced = @()
    $pending = @($Leaves | Where-Object {
        $path = Join-Path $Sdk $_
        -not (Test-Path -LiteralPath $path) -or (Get-FileSha256 $path) -ne (Get-OfficialSha256 $Baseline $_)
    })
    if (-not $pending.Count) { return $replaced }
    if (-not (Test-Path $SdkZip)) { throw "Missing $SdkZip, needed to restore the stock $($pending -join ', ') (run tools\codegen.ps1)" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($SdkZip)
    try {
        foreach ($leaf in $pending) {
            $entry = $archive.GetEntry("win-amd64/$leaf")
            if (-not $entry) { throw "$leaf is not in the SDK download" }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $Sdk $leaf), $true)
            if ((Get-FileSha256 (Join-Path $Sdk $leaf)) -ne (Get-OfficialSha256 $Baseline $leaf)) { throw "$leaf does not match the stock ReXGlue release" }
            $replaced += $leaf
        }
    } finally { $archive.Dispose() }
    return $replaced
}

if ($Restore) {
    $all = @(foreach ($c in $RexConfigs) { foreach ($t in $RexTargets) { Get-TargetFiles $t $c } })
    Remove-Item -LiteralPath $StampPath -ErrorAction SilentlyContinue
    $replaced = @(Restore-StockFiles $all)
    Write-Host "Stock ReXGlue runtime and GPU plugin restored ($($replaced.Count) files replaced). The next build.ps1 prepares this branch's files again."
    return
}

function Install-PatchedRuntime([string[]]$Configs) {
    $patches = @(Get-SelectedPatches)
    $patched = @(Get-PatchedTargets)
    $installed = @()
    if ($patched.Count) {
        $src = $WorkDir
        $srcCmake = $src.Replace('\', '/')
        if (-not (Test-Path (Join-Path $src '.git'))) {
            Write-Host "Fetching ReXGlue $($Baseline.SDK_TAG) from $($Baseline.SDK_REPO) (network) into $src..."
            New-Item -ItemType Directory -Force (Split-Path $src) | Out-Null
            Invoke-Native 'git clone' { git clone --branch $Baseline.SDK_TAG --depth 1 --recurse-submodules --shallow-submodules --jobs 8 $Baseline.SDK_REPO $src }
        }
        $commit = "$(git -C $src rev-parse HEAD)".Trim()
        if ($commit -ne $Baseline.SDK_COMMIT) { throw "$src is at $commit; expected $($Baseline.SDK_TAG) $($Baseline.SDK_COMMIT) (delete the folder to fetch it again)" }

        # Windows checks out the libmspack symlinks as one-line text files; use copies of their targets.
        $mspack = Join-Path $src 'thirdparty\libmspack'
        foreach ($line in (git -C $mspack ls-files -s)) {
            if ($line -notmatch '^120000 \S+ \d\t(.+)$') { continue }
            $file = Join-Path $mspack $Matches[1]
            $item = Get-Item -LiteralPath $file
            if ($item.LinkType -or $item.Length -gt 260) { continue }
            $target = Join-Path (Split-Path $file) (Get-Content -LiteralPath $file -Raw).Trim()
            if (Test-Path -LiteralPath $target -PathType Leaf) { Copy-Item -LiteralPath $target $file -Force }
        }

        # Another branch may have left other patches in the tree, in any tracked file.
        Invoke-Native 'git checkout' { git -C $src checkout -- . }
        Invoke-Native 'git clean' { git -C $src clean -fdq -- src include tests cmake }
        foreach ($patch in $patches) {
            & git -C $src apply --check $patch.Path
            if ($LASTEXITCODE) { throw "patches\rexglue-sdk\$($patch.Name) no longer applies to ReXGlue $($Baseline.SDK_TAG)" }
            Invoke-Native "git apply $($patch.Name)" { git -C $src apply $patch.Path }
            Write-Host "  applied $($patch.Name)"
        }

        Import-VsEnvironment
        $flags = $RexBuildFlags.Replace('<source>', $srcCmake)
        Push-Location $src
        try {
            Invoke-Native 'ReXGlue configure' { cmake --preset win-amd64 "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags" }
            $parallel = @(if ($Jobs -gt 0) { '--parallel'; $Jobs } else { '--parallel' })
            foreach ($c in $Configs) {
                Invoke-Native "ReXGlue build ($c)" { cmake --build out/build/win-amd64 --config $c --target $patched @parallel }
            }
        } finally { Pop-Location }

        $built = @{}
        foreach ($c in $Configs) {
            foreach ($target in $patched) {
                foreach ($leaf in Get-TargetFiles $target $c) {
                    $path = Join-Path $src "out\win-amd64\$c\$(Split-Path $leaf -Leaf)"
                    if (-not (Test-Path $path)) { throw "ReXGlue build output is missing $path" }
                    if ((Get-FileSha256 $path) -eq (Get-OfficialSha256 $Baseline $leaf)) { throw "$leaf came out identical to the stock release; the patches were not built in" }
                    # Only Release is released; the other configurations keep their PDB paths.
                    if ($c -eq 'Release' -and $leaf -like '*.dll') {
                        $text = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($path))
                        foreach ($needle in @($src, $srcCmake)) {
                            if ($text.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0) { throw "$(Split-Path $leaf -Leaf) embeds the build folder" }
                        }
                    }
                    $built[$leaf] = $path
                }
            }
        }
        foreach ($leaf in $built.Keys) { Copy-Item -LiteralPath $built[$leaf] (Join-Path $Sdk $leaf) -Force }
        $installed = @($built.Keys)
    }

    $stock = @(foreach ($c in $Configs) { foreach ($t in $RexTargets) { if ($patched -notcontains $t) { Get-TargetFiles $t $c } } })
    foreach ($leaf in @(Restore-StockFiles $stock)) { Write-Host "  restored the stock $leaf" }

    $lines = Write-RuntimeStamp $Sdk $installed
    foreach ($c in $Configs) {
        $problems = @(Test-PatchedRuntime $Sdk $c)
        if ($problems.Count) { throw "tools\rexglue does not match the patches after the build ($c): $($problems -join '; ')" }
    }
    Write-Host "ReXGlue prepared for $($Configs -join ', '):"
    $lines | ForEach-Object { Write-Host "  $_" }
}

function Invoke-StressTest {
    $buildDir = Join-Path $Root 'build-release'
    if (-not (Test-Path (Join-Path $buildDir 'build.ninja'))) { throw 'Run tools\build.ps1 -Config Release -BuildDir build-release first' }
    Import-VsEnvironment
    Invoke-Native 'stress test build' { cmake --build $buildDir --target gw2_physical_heap_stress }
    # Run beside the installed runtime, not the copy the build left in $buildDir, which may be stale.
    $stage = Join-Path $buildDir 'physical_heap_stress'
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir 'gw2_physical_heap_stress.exe') $stage -Force
    Copy-Item -LiteralPath (Join-Path $Sdk 'bin\rexruntime.dll') $stage -Force
    $log = Join-Path $buildDir 'physical_heap_stress.log'
    Push-Location $stage
    try { & .\gw2_physical_heap_stress.exe *> $log; $code = $LASTEXITCODE } finally { Pop-Location }
    Get-Content $log | Where-Object { $_ -match '^(view|PASS|FAIL)' } | ForEach-Object { Write-Host $_ }
    if ($code) { throw "Allocator stress test failed ($code): the runtime in tools\rexglue is not the patched one, or the patch is not effective" }
}

if (-not $SkipBuild) { Install-PatchedRuntime $Config }
if ($StressTest) { Invoke-StressTest }
