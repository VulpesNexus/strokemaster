# Region host proof runner. Launches a fresh Illustrator when none is running,
# runs the region proof through COM, and preserves raw evidence. Never Quits
# the host: the launched Illustrator stays open for inspection.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId,
    [string]$RequireBuild='region-prep-2',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -ne 'Desktop'){throw 'Use Windows PowerShell 5.1.'}
$regionDirectory=(Resolve-Path -LiteralPath $OutDir).ProviderPath
if(-not (Test-Path -LiteralPath $regionDirectory -PathType Container)){throw 'OutDir must be a directory.'}
$regionJournal=Join-Path $regionDirectory ($RunId+'-process.jsonl')
$regionResult=Join-Path $regionDirectory ($RunId+'-host.json')
if((Test-Path -LiteralPath $regionJournal) -or (Test-Path -LiteralPath $regionResult)){throw 'Evidence paths already exist.'}
$proofScript=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\tools\jsx\ai-region-host-proof.jsx'))
if(-not (Test-Path -LiteralPath $proofScript -PathType Leaf)){throw 'Region proof script missing.'}
$hostExe='C:\Program Files\Adobe\Adobe Illustrator 2026\Support Files\Contents\Windows\Illustrator.exe'
if(-not (Test-Path -LiteralPath $hostExe)){throw 'Illustrator 2026 executable not found.'}
if($ValidateOnly){Write-Output 'VALIDATED: no process query, COM call, host action, or evidence write.';return}
$regionUtf8=New-Object Text.UTF8Encoding($false)
function Write-RegionFile([string]$Path,[string]$Text){
    $regionStream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{$regionBytes=$regionUtf8.GetBytes($Text);$regionStream.Write($regionBytes,0,$regionBytes.Length);$regionStream.Flush()}
    finally{$regionStream.Dispose()}
}
Write-RegionFile $regionJournal ''
function Get-RegionHost { [Runtime.InteropServices.Marshal]::GetActiveObject('Illustrator.Application') }
function Add-RegionJournal([string]$Text){ [IO.File]::AppendAllText($regionJournal,$Text+[Environment]::NewLine,$regionUtf8) }
function Get-RegionProcesses {
    Get-Process | Where-Object {$_.ProcessName -eq 'Illustrator'} | ForEach-Object {
        $regionStart=$null;try{$regionStart=$_.StartTime.ToUniversalTime().ToString('o')}catch{}
        [ordered]@{pid=$_.Id;startUtc=$regionStart;windowHandle=[string]$_.MainWindowHandle}
    }
}
function Snapshot-Region([string]$Stage){
    $regionProcesses=@(Get-RegionProcesses)
    $regionRecord=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');stage=$Stage;runnerPid=$PID;
        illustratorProcesses=$regionProcesses;pidAttribution='process enumeration; COM does not expose target PID';
        hostClosesCleanly='NOT_MEASURED'}
    Add-RegionJournal ($regionRecord|ConvertTo-Json -Depth 6 -Compress)
    return ,$regionProcesses
}
$regionFailure=$null
try{
    $regionBefore=Snapshot-Region 'before.launch'
    if($regionBefore.Count -gt 1){throw 'More than one Illustrator process exists; refusing to attach ambiguously.'}
    $regionAfter=$regionBefore
    if($regionBefore.Count -eq 0){
        Start-Process -FilePath $hostExe | Out-Null
        $deadline=(Get-Date).AddMinutes(3)
        do{
            Start-Sleep -Seconds 5
            $regionAfter=Snapshot-Region 'launch.wait'
            if($regionAfter.Count -gt 1){throw 'More than one Illustrator process after launch.'}
        }while($regionAfter.Count -eq 0 -and (Get-Date) -lt $deadline)
        if($regionAfter.Count -ne 1){throw 'Illustrator did not reach a single running process.'}
    }
    $regionDeadline=(Get-Date).AddMinutes(2)
    $regionHost=$null
    while(-not $regionHost -and (Get-Date) -lt $regionDeadline){
        try{$regionHost=Get-RegionHost}catch{Start-Sleep -Seconds 5}
    }
    if(-not $regionHost){throw 'COM automation object never became available.'}
    $regionAttached=Snapshot-Region 'after.attach'
    if($regionAttached.Count -ne 1 -or $regionAttached[0].pid -ne $regionAfter[0].pid){throw 'Process identity changed during attach.'}
    $regionArgs=@{outDir=$regionDirectory;runId=$RunId;requireBuild=$RequireBuild;
        effectPdf=(Join-Path $regionDirectory ($RunId+'-effect.pdf'));
        controlPdf=(Join-Path $regionDirectory ($RunId+'-control.pdf'));
        log=(Join-Path $regionDirectory ($RunId+'-checkpoint.log'))}|ConvertTo-Json -Compress
    $regionLiteral=ConvertTo-Json -InputObject ($proofScript -replace '\\','/') -Compress
    $regionJs='(function(){var had=Object.prototype.hasOwnProperty.call($.global,"ISE_REGION_ARGS"),previous=$.global.ISE_REGION_ARGS;try{$.global.ISE_REGION_ARGS='+$regionArgs+';return $.evalFile(new File('+$regionLiteral+'));}finally{if(had){$.global.ISE_REGION_ARGS=previous;}else{delete $.global.ISE_REGION_ARGS;}}})();'
    $null=Snapshot-Region 'before.DoJavaScript'
    $regionRaw=[string]$regionHost.DoJavaScript($regionJs)
    Write-RegionFile $regionResult $regionRaw
    $regionReturned=Snapshot-Region 'after.DoJavaScript'
    if($regionReturned.Count -ne 1 -or $regionReturned[0].pid -ne $regionAfter[0].pid){throw 'Process identity changed during host call; inspect raw evidence.'}
    if(-not $regionRaw.StartsWith('PASS|')){throw 'Region host proof failed; raw result preserved.'}
    Write-Output $regionRaw
}catch{
    $regionFailure=$_
    try{Add-RegionJournal (@{utc=[DateTime]::UtcNow.ToString('o');stage='runner.error';error=[string]$regionFailure}|ConvertTo-Json -Compress)}
    catch{Write-Warning ('journal failure: '+[string]$_)}
}finally{
    try{$null=Snapshot-Region 'runner.finally'}
    catch{if(-not $regionFailure){$regionFailure=$_}}
    # Do not Quit the host or force-release COM references; leave it running.
}
if($regionFailure){
    Write-Error ('Region runner failed: '+[string]$regionFailure)
    exit 1
}
