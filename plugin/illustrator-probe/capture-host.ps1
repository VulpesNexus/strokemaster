# Attach-only evidence runner. Requires Windows PowerShell 5.1, never launches Adobe.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId,
    [switch]$RunProof,
    [switch]$Lifecycle,
    [string]$RequireBuild='native-prep-4',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -ne 'Desktop'){throw 'Use Windows PowerShell 5.1.'}
if($Lifecycle -and -not $RunProof){throw 'Lifecycle requires RunProof.'}
$probeDirectory=(Resolve-Path -LiteralPath $OutDir).ProviderPath
if(-not (Test-Path -LiteralPath $probeDirectory -PathType Container)){throw 'OutDir must be a directory.'}
$probeJournal=Join-Path $probeDirectory ($RunId+'-process.jsonl')
$probeResult=Join-Path $probeDirectory ($RunId+'-host.json')
if((Test-Path -LiteralPath $probeJournal) -or (Test-Path -LiteralPath $probeResult)){throw 'Evidence paths already exist.'}
$probeScript=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\tools\jsx\ai-probe-native-stroke.jsx'))
if(-not (Test-Path -LiteralPath $probeScript -PathType Leaf)){throw 'Host proof script missing.'}
if($ValidateOnly){Write-Output 'VALIDATED: no process query, COM call, host action, or evidence write.';return}
$probeUtf8=New-Object Text.UTF8Encoding($false)
function Write-ProbeFile([string]$Path,[string]$Text){
    $probeStream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$probeBytes=$probeUtf8.GetBytes($Text);$probeStream.Write($probeBytes,0,$probeBytes.Length);$probeStream.Flush()}
    finally{$probeStream.Dispose()}
}
Write-ProbeFile $probeJournal ''
function Get-ProbeHost { [Runtime.InteropServices.Marshal]::GetActiveObject('Illustrator.Application') }
function Add-ProbeJournal([string]$Text){ [IO.File]::AppendAllText($probeJournal,$Text+[Environment]::NewLine,$probeUtf8) }
function Get-ProbeProcesses {
    Get-Process | Where-Object {$_.ProcessName -eq 'Illustrator'} | ForEach-Object {
        $probeStart=$null;try{$probeStart=$_.StartTime.ToUniversalTime().ToString('o')}catch{}
        [ordered]@{pid=$_.Id;startUtc=$probeStart;windowHandle=[string]$_.MainWindowHandle}
    }
}
function Snapshot-Probe([string]$Stage){
    $probeProcesses=@(Get-ProbeProcesses)
    $probeRecord=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');stage=$Stage;runnerPid=$PID;
        illustratorProcesses=$probeProcesses;pidAttribution='process enumeration; COM does not expose target PID';
        hostClosesCleanly='NOT_MEASURED'}
    Add-ProbeJournal ($probeRecord|ConvertTo-Json -Depth 6 -Compress)
    return ,$probeProcesses
}
$probeFailure=$null
$probeSecondaryFailures=New-Object 'Collections.Generic.List[string]'
try{
    $probeBefore=Snapshot-Probe 'before.attach'
    if($probeBefore.Count -ne 1){throw 'Require exactly one existing Illustrator process; no launch attempted.'}
    $probeHost=Get-ProbeHost
    $probeAttached=Snapshot-Probe 'after.attach'
    if($probeAttached.Count -ne 1 -or $probeAttached[0].pid -ne $probeBefore[0].pid -or $probeAttached[0].startUtc -ne $probeBefore[0].startUtc){throw 'Process identity changed during attach.'}
    if($RunProof){
        $probeArgs=@{outDir=$probeDirectory;runId=$RunId;lifecycle=[bool]$Lifecycle;diagnostics=$true;requireBuild=$RequireBuild;expectedHostPid=$probeBefore[0].pid}|ConvertTo-Json -Compress
        $probeLiteral=ConvertTo-Json -InputObject ($probeScript -replace '\\','/') -Compress
        $probeJs='(function(){var had=Object.prototype.hasOwnProperty.call($.global,"ISE_AI_ARGS"),previous=$.global.ISE_AI_ARGS;try{$.global.ISE_AI_ARGS='+$probeArgs+';return $.evalFile(new File('+$probeLiteral+'));}finally{if(had){$.global.ISE_AI_ARGS=previous;}else{delete $.global.ISE_AI_ARGS;}}})();'
    }else{$probeJs='app.sendScriptMessage("StrokeMasterProposedProbe","diagnostics","")'}
    $null=Snapshot-Probe 'before.DoJavaScript'
    $probeRaw=[string]$probeHost.DoJavaScript($probeJs)
    # Preserve returned evidence before parsing or any additional host call.
    Write-ProbeFile $probeResult $probeRaw
    $probeReturned=Snapshot-Probe 'after.DoJavaScript'
    if($probeReturned.Count -ne 1 -or $probeReturned[0].pid -ne $probeBefore[0].pid -or $probeReturned[0].startUtc -ne $probeBefore[0].startUtc){throw 'Process identity changed during host call; inspect raw evidence.'}
    $probeParsed=$probeRaw|ConvertFrom-Json
    if($RunProof){if($probeParsed.passed -ne $true){throw 'Host proof failed; raw result preserved.'}}
    elseif($probeParsed.schema -ne 2 -or $probeParsed.plugin -ne 'StrokeMasterProposedProbe' -or $probeParsed.build -ne $RequireBuild -or $probeParsed.processId -ne $probeBefore[0].pid){throw 'Unexpected diagnostic identity/schema/process ID.'}
    Write-Output $probeRaw
}catch{
    $probeFailure=$_
    try{Add-ProbeJournal (@{utc=[DateTime]::UtcNow.ToString('o');stage='runner.error';error=[string]$probeFailure}|ConvertTo-Json -Compress)}
    catch{$probeSecondaryFailures.Add('runner.error journal: '+[string]$_)}
}finally{
    try{$null=Snapshot-Probe 'runner.finally'}
    catch{
        if($probeFailure){$probeSecondaryFailures.Add('runner.finally snapshot: '+[string]$_)}
        else{$probeFailure=$_}
    }
    # Do not Quit or force-release COM references while investigating host exits.
}
if($probeFailure){
    if($probeSecondaryFailures.Count){
        $probeFailure.Exception.Data['ProbeSecondaryErrors']=$probeSecondaryFailures.ToArray()
        $probeFailure.ErrorDetails=New-Object Management.Automation.ErrorDetails(([string]$probeFailure)+[Environment]::NewLine+'Secondary errors: '+($probeSecondaryFailures -join '; '))
    }
    throw $probeFailure
}
