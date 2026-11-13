param(
    [string]$BuildDirectory = 'build\coverage-build',
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateRange(1, 8)][int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
$ProjectDirectory = Split-Path $PSScriptRoot -Parent
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$VsDirectory = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$Cmake = Join-Path $VsDirectory 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$Ctest = Join-Path (Split-Path $Cmake -Parent) 'ctest.exe'
$BuildDirectory = [IO.Path]::GetFullPath((Join-Path $ProjectDirectory $BuildDirectory))
if (-not (Test-Path -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt'))) { throw '[Build:Capture] Configure the integration build first.' }
$Options = @('-DQCK_TEST_PRIVILEGED_HOST=OFF')
foreach ($Feature in 'OCR', 'TRANSLATION', 'CLIPBOARD_HISTORY', 'ADMIN_STARTUP', 'UPDATES') { $Options += "-DQCK_ENABLE_${Feature}=OFF" }
& $Cmake -S $ProjectDirectory -B $BuildDirectory @Options
if ($LASTEXITCODE) { throw '[Build:Capture] Configuration failed.' }
$env:CARGO_BUILD_JOBS = "$Jobs"
& $Cmake --build $BuildDirectory --config $Configuration --target jietuba_cpp qck_policy_check qck_feature_services_tests jietuba_core_tests jietuba_update_tests --parallel $Jobs
if ($LASTEXITCODE) { throw '[Build:Capture] Compilation failed.' }
& $Ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure -j 1 -R '^qck_feature_policy$|^qck_features_(disabled|malformed|unavailable)$|^python_core_parity$|^updater$'
if ($LASTEXITCODE) {
    & (Join-Path $PSScriptRoot 'show_failed_tests.ps1') -BuildDir $BuildDirectory -Config $Configuration -NoRerun
    throw '[Build:Capture] Service-boundary checks failed.'
}
