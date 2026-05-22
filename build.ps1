# build.ps1 — NightClub Mode build script
# Usage: pwsh ./build.ps1 [--release]
# Requires: qpm, Android NDK r25+, CMake 3.22+

param([switch]$release)

$ErrorActionPreference = "Stop"

# ── 1. Restore qpm dependencies ──────────────────────────────────────────────
Write-Host "📦 Restoring dependencies via qpm..." -ForegroundColor Cyan
qpm restore
if ($LASTEXITCODE -ne 0) { throw "qpm restore failed" }

# ── 2. Configure CMake ────────────────────────────────────────────────────────
$buildType = if ($release) { "Release" } else { "Debug" }
$buildDir  = "build"

Write-Host "🔧 Configuring CMake ($buildType)..." -ForegroundColor Cyan

# Resolve NDK path — edit this if your NDK is elsewhere
$ndkPath = $env:ANDROID_NDK_HOME
if (-not $ndkPath) { $ndkPath = "$env:LOCALAPPDATA\Android\Sdk\ndk\25.2.9519653" }
if (-not (Test-Path $ndkPath)) {
    throw "Android NDK not found at '$ndkPath'. Set `$ANDROID_NDK_HOME or update build.ps1."
}

cmake -B $buildDir `
      -DCMAKE_BUILD_TYPE=$buildType `
      -DCMAKE_TOOLCHAIN_FILE="$ndkPath\build\cmake\android.toolchain.cmake" `
      -DANDROID_ABI=arm64-v8a `
      -DANDROID_PLATFORM=android-29 `
      -G Ninja
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }

# ── 3. Build ──────────────────────────────────────────────────────────────────
Write-Host "🏗️  Building..." -ForegroundColor Cyan
cmake --build $buildDir --config $buildType
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host ""
Write-Host "✅ Build succeeded!" -ForegroundColor Green
Write-Host "   Output: $buildDir/libNightClubMode.so" -ForegroundColor Green
Write-Host ""
Write-Host "Run  pwsh ./package.ps1  to create the installable .qmod file." -ForegroundColor Yellow
