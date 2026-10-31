# Owned fresh-process text parity test. Launch on the isolated hidden desktop.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [Parameter(Mandatory=$true)][string]$Font,
    [ValidateSet('point','area','path','rotated','scaled','threaded')][string]$Kind='threaded',
    [ValidateSet('region-prep-18','region-prep-19','region-prep-20','region-prep-21')][string]$ExpectedBuild='region-prep-20',
    [switch]$Control,
    [switch]$Flow,
    [switch]$RouteOnly,
    [switch]$FixOnly,
    [switch]$ValidateOnly,
    [switch]$StoryGroup,
    [switch]$StoryDriver,
    [switch]$DeferredReopen
)
$ErrorActionPreference='Stop'
$json=@{font=$Font;kinds=@($Kind);expectedBuild=$ExpectedBuild;control=[bool]$Control;flow=[bool]$Flow;routeOnly=[bool]$RouteOnly;fixOnly=[bool]$FixOnly;storyGroup=[bool]$StoryGroup;storyDriver=[bool]$StoryDriver;deferredReopen=[bool]$DeferredReopen} | ConvertTo-Json -Compress
if(-not $ValidateOnly){
    $later=@()
    if($DeferredReopen){$later=@('ai-text-parity-open.jsx','ai-text-parity-edit.jsx','ai-text-parity-final.jsx')}
    & (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-text-parity-sweep.jsx -Script2 $later -IdleSeconds 4 -ArgJson $json
    if($LASTEXITCODE){exit $LASTEXITCODE}
}
$result=Get-Content -LiteralPath (Join-Path $OutDir ($RunId+'-host.txt')) -Raw
if($result -notmatch '^host=30\.8\.2\|DONE\|' -or $result -match '\|FAIL\|'){throw 'Host parity script failed.'}
$record=Get-Content -LiteralPath (Join-Path $OutDir ($RunId+'-results.json')) -Raw | ConvertFrom-Json
if($record.font -ne $Font -or $record.build -ne $ExpectedBuild -or [bool]$record.flow -ne [bool]$Flow -or [bool]$record.storyGroup -ne [bool]$StoryGroup -or [bool]$record.storyDriver -ne [bool]$StoryDriver -or [bool]$record.deferredReopen -ne [bool]$DeferredReopen){throw 'Evidence identity mismatch.'}
if($ExpectedBuild -ne 'region-prep-18' -and -not $Control){
    foreach($row in $record.rows){
        foreach($response in @($row.route,$row.fix)){
            if($response -and -not ($response | ConvertFrom-Json).ok){throw 'Bridge command failed.'}
        }
        if(-not $row.stylesPreserved -or -not $row.editable){throw 'Correction changed formatting or editability.'}
        if($Kind -eq 'threaded'){
            foreach($snapshot in @($row.beforeRoute,$row.after,$row.reopened)){
                if(@($snapshot.frames).Count -ne 2 -or @($snapshot.frames | Where-Object {$_.storyFrames -ne 2}).Count){throw 'Threaded story changed.'}
                if($Flow -and @($snapshot.frames | Where-Object {$_.frameCharacters -le 0}).Count){throw 'Flow fixture needs visible text in both frames.'}
            }
        }
    }
}
