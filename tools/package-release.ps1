<#
.SYNOPSIS
  Builds the Release configuration and packages the Windows release ZIP into dist\.
  Local only: nothing is published, tagged or uploaded.

.DESCRIPTION
  Builds into build-release\ (the normal build\ folder is not touched), stages the files
  the program needs, refuses to continue if the staging folder holds game or user files,
  then writes dist\GeometryWarsRetroEvolved2-v<version>-windows-x64.zip and its .sha256 file.
  The version is the project VERSION in CMakeLists.txt.

.EXAMPLE
  .\tools\package-release.ps1              # incremental Release build, then package
  .\tools\package-release.ps1 -Clean       # clean Release build first
  .\tools\package-release.ps1 -SkipBuild   # package the existing build-release\ output
  .\tools\package-release.ps1 -VerifyOnly dist\GeometryWarsRetroEvolved2-v0.2.3-windows-x64
#>
param(
    [switch]$Clean,
    [switch]$SkipBuild,
    [string]$VerifyOnly  # only check a staged folder for game or user files, then exit
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$ReleaseBuild = Join-Path $Root 'build-release'
$Packaging = Join-Path $Root 'packaging'
$SdkRoot = Join-Path $Root 'tools\rexglue\win-amd64'
$Sdk = Join-Path $SdkRoot 'bin'
. (Join-Path $PSScriptRoot 'rexruntime-patch.ps1')

$Utf8NoBom = New-Object System.Text.UTF8Encoding $false
$RootFiles = @('geometry_wars_retro_evolved_2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'geometry_wars_retro_evolved_2.toml', 'README.txt', 'LICENSE')
$LicenseFiles = @('ReXGlue-LICENSE.txt', 'THIRD-PARTY-LICENSES.txt')
# Names that mean game data, a package, saves, settings, caches or logs.
$ForbiddenNames = @('default.xex', 'Data.bin', 'game', 'game-install-temp', 'game-install-backup', 'user_data',
    'user_data.migrating', 'content', 'cache', 'logs', 'settings.toml', 'settings.toml.bak', 'ArcadeInfo.xml', 'nxeart', '584108FF')
$ForbiddenPatterns = @('*.xex', '*.log', '*.dmp', '*.pdb', '*.xsh', '*.xpso', '*.bak', '*.tmp', '*.sav')

# The repository folder, as text or UTF-16, in either slash style.
$Latin1 = [Text.Encoding]::GetEncoding(28591)
$LocalPaths = foreach ($path in @($Root, $Root.Replace('\', '/'))) {
    $path
    $Latin1.GetString([Text.Encoding]::Unicode.GetBytes($path))
}

function Test-ReleaseFolder([string]$Dir) {
    $problems = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { throw "Not a folder: $Dir" }
    $base = (Resolve-Path -LiteralPath $Dir).Path.TrimEnd('\')
    foreach ($item in Get-ChildItem -LiteralPath $base -Recurse -Force) {
        $relative = $item.FullName.Substring($base.Length + 1)
        if (-not $item.PSIsContainer) {
            $text = $Latin1.GetString([IO.File]::ReadAllBytes($item.FullName))
            foreach ($path in $LocalPaths) {
                if ($text.IndexOf($path, [StringComparison]::OrdinalIgnoreCase) -ge 0) { $problems.Add("local path in file: $relative"); break }
            }
        }
        if ($ForbiddenNames -contains $item.Name) { $problems.Add("forbidden name: $relative") }
        foreach ($pattern in $ForbiddenPatterns) {
            if ($item.Name -like $pattern) { $problems.Add("forbidden file type: $relative") }
        }
        if (-not $item.PSIsContainer -and $item.Length -ge 4) {
            $stream = [IO.File]::OpenRead($item.FullName)
            try {
                $magic = New-Object byte[] 4
                [void]$stream.Read($magic, 0, 4)
            } finally { $stream.Dispose() }
            $text = [Text.Encoding]::ASCII.GetString($magic)
            if ($text -in @('LIVE', 'PIRS', 'CON ')) { $problems.Add("Xbox 360 package: $relative") }
        }
        $parts = $relative -split '\\'
        if ($parts.Count -eq 1) {
            if ($item.PSIsContainer) { if ($item.Name -ne 'licenses') { $problems.Add("unexpected folder: $relative") } }
            elseif ($RootFiles -notcontains $item.Name) { $problems.Add("unexpected file: $relative") }
        } elseif ($parts[0] -ne 'licenses' -or $parts.Count -gt 2 -or $item.PSIsContainer -or $LicenseFiles -notcontains $item.Name) {
            $problems.Add("unexpected item: $relative")
        }
    }
    foreach ($leaf in $RootFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $base $leaf) -PathType Leaf)) { $problems.Add("missing: $leaf") }
    }
    foreach ($leaf in $LicenseFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $base "licenses\$leaf") -PathType Leaf)) { $problems.Add("missing: licenses\$leaf") }
    }
    return $problems
}

function Write-Problems($problems) {
    Write-Host ''
    Write-Host 'RELEASE CHECK FAILED' -ForegroundColor Red
    foreach ($problem in $problems) { Write-Host "  $problem" -ForegroundColor Red }
}

if ($VerifyOnly) {
    $problems = Test-ReleaseFolder $VerifyOnly
    if ($problems.Count) { Write-Problems $problems; exit 1 }
    Write-Host "Release folder is clean: $VerifyOnly"
    return
}

$cmake = Get-Content (Join-Path $Root 'CMakeLists.txt') -Raw
if ($cmake -notmatch '(?m)^\s*project\(\s*geometry_wars_retro_evolved_2\s+VERSION\s+(\d+\.\d+\.\d+)') { throw 'Project VERSION not found in CMakeLists.txt' }
$Version = $Matches[1]
$Name = "GeometryWarsRetroEvolved2-v$Version-windows-x64"
$Dist = Join-Path $Root 'dist'
$Stage = Join-Path $Dist $Name
$Zip = Join-Path $Dist "$Name.zip"
$ShaFile = Join-Path $Dist "$Name.sha256"

Write-Host ''
Write-Host "Geometry Wars: Retro Evolved 2 $Version - Windows release package (local only)"

if (-not $SkipBuild) {
    $build = @{ Config = 'Release'; BuildDir = $ReleaseBuild }
    if ($Clean) { $build.Clean = $true }
    & (Join-Path $PSScriptRoot 'build.ps1') @build
}

# The same check the normal build makes: tools\rexglue matches this branch's patches and the release build uses it.
$runtimeProblems = @(Test-PatchedRuntime $SdkRoot 'Release') + @(Test-StagedRuntime $SdkRoot $ReleaseBuild 'Release')
if ($runtimeProblems.Count) { Write-Problems $runtimeProblems; throw 'tools\rexglue or the release build does not hold the runtime and GPU plugin this branch needs (build without -SkipBuild)' }

foreach ($leaf in @('geometry_wars_retro_evolved_2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'geometry_wars_retro_evolved_2.toml')) {
    if (-not (Test-Path (Join-Path $ReleaseBuild $leaf))) { throw "Release build output is missing $leaf (run without -SkipBuild)" }
}

New-Item -ItemType Directory -Force $Dist | Out-Null
if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
New-Item -ItemType Directory $Stage, (Join-Path $Stage 'licenses') | Out-Null

foreach ($leaf in @('geometry_wars_retro_evolved_2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'geometry_wars_retro_evolved_2.toml')) {
    Copy-Item (Join-Path $ReleaseBuild $leaf) $Stage
}
foreach ($leaf in @('rexruntime.dll', 'rexgpu-xenos.dll')) {
    $built = (Get-FileHash (Join-Path $Stage $leaf) -Algorithm SHA256).Hash
    $installed = (Get-FileHash (Join-Path $Sdk $leaf) -Algorithm SHA256).Hash
    if ($built -ne $installed) { throw "$leaf is not the file installed in tools\rexglue" }
}
$RootLicense = Join-Path $Root 'LICENSE'
if (-not (Test-Path -LiteralPath $RootLicense -PathType Leaf)) { throw "Missing project license: $RootLicense" }
Copy-Item -LiteralPath $RootLicense $Stage
foreach ($leaf in $LicenseFiles) {
    $source = Join-Path $Packaging "licenses\$leaf"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing license file: $source" }
    Copy-Item -LiteralPath $source (Join-Path $Stage 'licenses')
}
$readme = (Get-Content (Join-Path $Packaging 'README.txt') -Raw).Replace('@VERSION@', $Version) -replace "\r?\n", "`r`n"
[IO.File]::WriteAllText((Join-Path $Stage 'README.txt'), $readme, $Utf8NoBom)

$problems = Test-ReleaseFolder $Stage
if ($problems.Count) { Write-Problems $problems; throw 'The staging folder holds files that must not be released' }

Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $Zip) { Remove-Item -LiteralPath $Zip }
$files = Get-ChildItem -LiteralPath $Stage -Recurse -File | Sort-Object { $_.FullName.Substring($Stage.Length + 1).Replace('\', '/') }
$stream = [IO.File]::Open($Zip, [IO.FileMode]::CreateNew)
try {
    $archive = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $files) {
            $entry = $file.FullName.Substring($Stage.Length + 1).Replace('\', '/')
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal)
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }

$archive = [IO.Compression.ZipFile]::OpenRead($Zip)
try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName })
} finally { $archive.Dispose() }
$expected = @($files | ForEach-Object { $_.FullName.Substring($Stage.Length + 1).Replace('\', '/') })
if (Compare-Object $expected $entries) { throw 'The ZIP does not hold exactly the staged files' }
if ($entries | Where-Object { $_ -match '^[\\/]|\.\.|:|\\' }) { throw 'The ZIP holds an unsafe entry name' }

$hash = (Get-FileHash $Zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($ShaFile, "$hash  $Name.zip`n", $Utf8NoBom)

Write-Host ''
Write-Host '===================================================='
Write-Host ' RELEASE PACKAGE READY (nothing was published)'
Write-Host '===================================================='
Write-Host "  ZIP:     $Zip ($([math]::Round((Get-Item $Zip).Length / 1MB, 1)) MB)"
Write-Host "  SHA-256: $hash"
Write-Host "  Hash file: $ShaFile"
Write-Host '  Contents:'
foreach ($entry in $entries) { Write-Host "    $entry" }
Write-Host ''
