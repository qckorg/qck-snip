param([string]$Program = 'build\coverage-build\RelWithDebInfo\qck-snip.exe')
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw '[Test:AdminTask] This integration check is restricted to a disposable GitHub-hosted runner.'
}
if ($env:JIETUBA_TEST_SANDBOX) { throw '[Test:AdminTask] Refusing to bypass the test sandbox.' }
$Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$Principal = [Security.Principal.WindowsPrincipal]::new($Identity)
if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw '[Test:AdminTask] Elevated runner required.' }
$Program = (Resolve-Path -LiteralPath $Program).Path
$TaskName = 'QckSnip RunAsAdmin (' + $Identity.Name.Split('\')[-1] + ')'
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) { throw '[Test:AdminTask] An existing task must be left untouched.' }
$Owned = $false
function InvokeTaskHelper([string]$Argument) {
    $Process = Start-Process -FilePath $Program -ArgumentList $Argument -WindowStyle Hidden -Wait -PassThru
    if ($Process.ExitCode -ne 0) { throw "[Test:AdminTask] Helper failed: $Argument, exit $($Process.ExitCode)." }
}
function GetSid([string]$Account) {
    if ($Account -like 'S-1-*') { return $Account }
    return [Security.Principal.NTAccount]::new($Account).Translate([Security.Principal.SecurityIdentifier]).Value
}
try {
    $Owned = $true
    InvokeTaskHelper '--qck-install-admin-task'
    [xml]$Definition = Export-ScheduledTask -TaskName $TaskName
    if ((GetSid ([string]$Definition.Task.Principals.Principal.UserId)) -ne $Identity.User.Value -or
        (GetSid ([string]$Definition.Task.Triggers.LogonTrigger.UserId)) -ne $Identity.User.Value) { throw '[Test:AdminTask] Wrong account.' }
    if ([string]$Definition.Task.Actions.Exec.Command -ine $Program) { throw '[Test:AdminTask] Wrong command.' }
    if ([string]$Definition.Task.Actions.Exec.Arguments) { throw '[Test:AdminTask] Unexpected arguments.' }
    if ([string]$Definition.Task.Actions.Exec.WorkingDirectory -ine (Split-Path $Program -Parent)) { throw '[Test:AdminTask] Wrong working directory.' }
    if ([string]$Definition.Task.Principals.Principal.RunLevel -ne 'HighestAvailable') { throw '[Test:AdminTask] Wrong run level.' }
    if ([string]$Definition.Task.Principals.Principal.LogonType -ne 'InteractiveToken') { throw '[Test:AdminTask] Wrong logon type.' }
    if ([string]$Definition.Task.Triggers.LogonTrigger.Delay -ne 'PT3S') { throw '[Test:AdminTask] Wrong logon delay.' }
    if ([string]$Definition.Task.Settings.MultipleInstancesPolicy -ne 'Parallel' -or [string]$Definition.Task.Settings.ExecutionTimeLimit -ne 'PT0S') { throw '[Test:AdminTask] Wrong lifetime settings.' }
    if ([string]$Definition.Task.Settings.DisallowStartIfOnBatteries -ne 'false' -or [string]$Definition.Task.Settings.StopIfGoingOnBatteries -ne 'false') { throw '[Test:AdminTask] Wrong battery settings.' }
    InvokeTaskHelper '--qck-remove-admin-task'
    if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) { throw '[Test:AdminTask] Task was not removed.' }
    Write-Output '[Test:AdminTask] Fixed helper registered, verified and removed its task.'
} finally {
    if ($Owned) {
        $Task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
        if ($Task) {
            if ($Task.Actions.Count -ne 1 -or $Task.Actions[0].Execute -ine $Program) { throw '[Test:AdminTask] Unexpected task identity; left untouched.' }
            Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        }
    }
}
