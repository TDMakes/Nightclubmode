# package.ps1 — Packages the compiled .so into a valid .qmod file
# A .qmod is just a renamed .zip with a specific layout

param([string]$soPath = "build/libNightClubMode.so")

$ErrorActionPreference = "Stop"

if (-not (Test-Path $soPath)) {
    throw "Compiled .so not found at '$soPath'. Run build.ps1 first."
}

$outDir    = "package"
$qmodName  = "NightClubMode-1.0.0.qmod"

# ── Create package staging area ───────────────────────────────────────────────
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory $outDir | Out-Null

# ── Copy files into staging ───────────────────────────────────────────────────
Copy-Item "mod.json"  "$outDir/mod.json"
Copy-Item $soPath     "$outDir/libNightClubMode.so"

# ── Zip and rename to .qmod ───────────────────────────────────────────────────
$zipPath  = "$outDir.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath }

Compress-Archive -Path "$outDir/*" -DestinationPath $zipPath
Rename-Item $zipPath $qmodName

Write-Host ""
Write-Host "🎉 Package ready: $qmodName" -ForegroundColor Green
Write-Host ""
Write-Host "Install via BMBF:" -ForegroundColor Cyan
Write-Host "  1. Open BMBF on your Quest" -ForegroundColor White
Write-Host "  2. Go to Browser tab → Upload" -ForegroundColor White
Write-Host "  3. Select $qmodName" -ForegroundColor White
Write-Host "  4. Restart Beat Saber and enjoy the club! 🕺" -ForegroundColor White
