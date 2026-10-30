[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$SkipRegeneration,
    [ValidateRange(1, 16)][int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRepo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
$taskImage = 'devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282'
$taskExe = Join-Path $taskRepo 'game\sh2pc.exe'

foreach ($taskCommand in @('docker', 'git', 'python')) { Get-Command $taskCommand -ErrorAction Stop | Out-Null }
if (-not (Test-Path -LiteralPath $taskExe)) { throw 'Copy your own supported executable to game/sh2pc.exe.' }
if ((Get-Item -LiteralPath $taskExe).Length -ne 5685248 -or
    (Get-FileHash -LiteralPath $taskExe -Algorithm SHA1).Hash.ToLowerInvariant() -ne '3201a5e1029f3ae3b77770255dcbe10360b3cd09') {
    throw 'Unsupported PC executable: re-check guest addresses before adapting a different executable.'
}
$taskMarkers = @{
    'src\win32\kernel32.c' = 'lang/%s'
    'src\game\ee.c' = 'ee_font_init();'
    'src\game\ee_lang.c' = 'uint32_t ee_lang_string('
    'src\game\ee_font.c' = 'void ee_font_overlay('
    'src\d3d8\d3d8_res.c' = 'ee_font_overlay('
}
foreach ($taskFile in $taskMarkers.Keys) {
    $taskContent = Get-Content -LiteralPath (Join-Path $taskRepo $taskFile) -Raw
    if (-not $taskContent.Contains($taskMarkers[$taskFile])) { throw "Missing adaptation hook in $taskFile" }
}
foreach ($taskFont in @('font000.tga', 'fontwdata.bin')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskRepo ('game\lang\font\' + $taskFont)))) {
        throw "Missing private test input game/lang/font/$taskFont"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $taskRepo 'game\lang\etc\resource\r_menu_j.res'))) {
    throw 'Missing private menu test input game/lang/etc/resource/r_menu_j.res.'
}
docker info --format '{{.ServerVersion}}'
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop first.' }
docker image inspect $taskImage --format '{{.Id}}'
if ($LASTEXITCODE -ne 0) { throw "Missing compiler image. Run: docker pull $taskImage" }
Write-Output "Repository: $taskRepo"
Write-Output 'PC executable and Chinese adaptation hooks checked.'
if ($CheckOnly) { Write-Output 'Check only: no dependencies installed and no build outputs generated.'; return }

$taskBuild = @'
set -eu
cd /src
mkdir -p build
if [ ! -x .venv/bin/python3 ]; then
  if ! python3 -m venv .venv; then
    apt-get update -qq
    apt-get install -y python3-venv
    python3 -m venv .venv
  fi
fi
if [ ! -f .venv/.sh2-requirements.sha256 ] || ! sha256sum -c .venv/.sh2-requirements.sha256 >/dev/null 2>&1; then
  .venv/bin/python3 -m pip install -r requirements.txt
  sha256sum requirements.txt > .venv/.sh2-requirements.sha256
fi
.venv/bin/python3 -m pip freeze > build/zh-python-requirements.txt
# A generated LF copy keeps tracked Windows CRLF files unchanged. build/ and tools/ share the same parent.
python3 -c 'from pathlib import Path; Path("build/regen-zh.sh").write_text(Path("tools/regen.sh").read_text(encoding="utf-8-sig"), encoding="utf-8", newline="\n")'
if [ "$SH2_SKIP_REGEN" = 1 ]; then
  test -f src/recomp/gen/recomp_types.h || { echo 'Generated sources missing; run without -SkipRegeneration.'; exit 1; }
else
  sh build/regen-zh.sh
fi
python3 tests/local/prepare_path_fixtures.py
cc -O2 -ffunction-sections -fdata-sections -Isrc/recomp/gen tests/local/path_driver.c src/win32/kernel32.c -Wl,--gc-sections -pthread -lm -o build/path-test
./build/path-test
cc -O2 -Wall -Wextra -Werror -Isrc/recomp/gen tests/local/font_driver.c -o build/font-test
./build/font-test
.venv/bin/python3 tests/local/check_font_output.py
python3 tests/local/prepare_lang_test.py
cc -O2 -ffunction-sections -fdata-sections -Ibuild -Isrc/recomp/gen tests/local/lang_driver.c src/game/ee_lang.c src/win32/kernel32.c -Wl,--gc-sections -pthread -lm -o build/lang-test
for case in valid lf missing short embedded_nul long oversize; do ./build/lang-test "$case"; done
cmake -S . -B build/switch -G Ninja -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/switch -j "$SH2_BUILD_JOBS"
'@
$taskSkip = if ($SkipRegeneration) { '1' } else { '0' }
$taskArguments = @('run', '--rm', '--pull=never', '--mount', "type=bind,source=$taskRepo,target=/src",
    '-w', '/src', '-e', "SH2_BUILD_JOBS=$Jobs", '-e', "SH2_SKIP_REGEN=$taskSkip",
    $taskImage, 'sh', '-lc', $taskBuild)
& docker @taskArguments
if ($LASTEXITCODE -ne 0) { throw 'Build or validation failed; the current release package was not modified.' }
$taskNro = Join-Path $taskRepo 'build\switch\sh2-nx.nro'
& python (Join-Path $PSScriptRoot 'package_zh.py') inspect $taskNro
if ($LASTEXITCODE -ne 0) { throw 'Built NRO failed metadata inspection.' }
Write-Output "Built and checked: $taskNro"
