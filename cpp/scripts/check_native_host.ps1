$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw '[Test:NativeHost] Disposable hosted runner required.' }
$Identity = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $Identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw '[Test:NativeHost] Elevated runner required.' }
$ProjectDirectory = Split-Path $PSScriptRoot -Parent
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$VsDirectory = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module (Join-Path $VsDirectory 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $VsDirectory -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$BuildDirectory = Join-Path $env:RUNNER_TEMP ('qck-native-host-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($BuildDirectory) | Out-Null
$PreviousKeepToken = $env:QCK_TEST_KEEP_TOKEN
function CheckExit([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "[Test:NativeHost] $Step failed, exit $LASTEXITCODE." }
}
Push-Location $BuildDirectory
try {
    & cl.exe /nologo /EHsc /W4 /MT /Z7 /Od /std:c++20 /I "$ProjectDirectory\src" "$ProjectDirectory\tests\native_host_probe.cpp" "$ProjectDirectory\src\core\ocr_security.cpp" /Fe:NativeHost.exe /link /DEBUG /INCREMENTAL:NO advapi32.lib
    CheckExit 'Guard compilation'
    & cl.exe /nologo /EHsc /W4 /MT "$ProjectDirectory\tests\test_launcher.cpp" /Fe:TestLauncher.exe /link advapi32.lib
    CheckExit 'Launcher compilation'
    $Probe = Join-Path $BuildDirectory 'NativeHost.exe'
    $Launcher = Join-Path $BuildDirectory 'TestLauncher.exe'
    & $Probe --deny
    CheckExit 'Elevated denial'
    $env:QCK_TEST_KEEP_TOKEN = '0'
    & $Launcher $Probe --allow
    CheckExit 'Ordinary-host control'
    $Collector = Get-ChildItem (Join-Path $VsDirectory 'Common7\IDE\Extensions\Microsoft\CodeCoverage.Console') -Filter 'Microsoft.CodeCoverage.Console.exe' -Recurse | Select-Object -First 1 -ExpandProperty FullName
    if (-not $Collector) { throw '[Test:NativeHost] Native coverage collector required.' }
    # Start the collector inside the ordinary token so its instrumentation IPC
    # shares the same authority as the native process it measures.
    & $Launcher $Collector collect $Probe --allow -s (Join-Path $PSScriptRoot 'coverage.config.xml') -o NativeHostCoverage.xml -f cobertura
    CheckExit 'Ordinary-host coverage'
    $Coverage = [xml](Get-Content -LiteralPath NativeHostCoverage.xml -Raw)
    $Covered = @($Coverage.SelectNodes("//class[contains(@filename, 'ocr_security.cpp')]/lines/line[@hits != '0']"))
    if (-not $Covered.Count) { throw '[Test:NativeHost] Ordinary OCR guard execution was not recorded by coverage.' }
    Write-Output "[Test:NativeHost] Ordinary guard coverage: $($Covered.Count) lines."
    $env:QCK_TEST_KEEP_TOKEN = '1'
    & $Launcher $Probe --deny
    CheckExit 'Preserved-host denial'
} finally {
    $env:QCK_TEST_KEEP_TOKEN = $PreviousKeepToken
    Pop-Location
}
