param(
    [string]$Configuration = "Debug",
    [string]$DebugFile = "",
    [string]$WorkbenchPath = "D:\Program Files\IAR"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$iarBuild = Join-Path $WorkbenchPath "common\bin\iarbuild.exe"
$cspyBat = Join-Path $WorkbenchPath "common\bin\cspybat.exe"
$projectFile = Join-Path $repoRoot "basic.ewp"
$generalXcl = Join-Path $repoRoot "settings\basic.$Configuration.general.xcl"
$driverXcl = Join-Path $repoRoot "settings\basic.$Configuration.driver.xcl"

if (!(Test-Path -LiteralPath $iarBuild)) {
    throw "iarbuild.exe not found: $iarBuild"
}
if (!(Test-Path -LiteralPath $cspyBat)) {
    throw "cspybat.exe not found: $cspyBat"
}
if (!(Test-Path -LiteralPath $projectFile)) {
    throw "IAR project not found: $projectFile"
}
if (!(Test-Path -LiteralPath $generalXcl)) {
    throw "C-SPY general options not found: $generalXcl"
}
if (!(Test-Path -LiteralPath $driverXcl)) {
    throw "C-SPY driver options not found: $driverXcl"
}

if ($DebugFile -eq "") {
    $generalLines = Get-Content -LiteralPath $generalXcl
    foreach ($line in $generalLines) {
        $trimmed = $line.Trim().Trim('"')
        if ($trimmed.EndsWith(".out")) {
            $DebugFile = $trimmed
            break
        }
    }
}

if ($DebugFile -eq "") {
    throw "Unable to resolve debug output from: $generalXcl"
}

Write-Host "Building $Configuration..."
& $iarBuild $projectFile -build $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (!(Test-Path -LiteralPath $DebugFile)) {
    throw "Debug output not found after build: $DebugFile"
}

Write-Host "Downloading and starting target..."
& $cspyBat `
    -f $generalXcl `
    --debug_file=$DebugFile `
    --download_only `
    --leave_target_running `
    --backend `
    -f $driverXcl

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Download complete. C-SPY download-only session ended."
