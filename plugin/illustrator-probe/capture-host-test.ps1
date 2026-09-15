# Pure runner tests: all external dependencies replaced in memory before execution.
$ErrorActionPreference='Stop'
$probeSource=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'capture-host.ps1'))
$probeTokens=$null;$probeParseErrors=$null
$probeAst=[Management.Automation.Language.Parser]::ParseInput($probeSource,[ref]$probeTokens,[ref]$probeParseErrors)
if($probeParseErrors.Count){throw 'Runner parse failed.'}
$probeReplacements=@{
    'Write-ProbeFile'=@'
function Write-ProbeFile([string]$Path,[string]$Text){
    if($script:probeMock.Fault.rawWrite -and $Path.EndsWith('-host.json')){throw 'injected raw write primary'}
    $script:probeMock.Files[$Path]=$Text
}
'@
    'Get-ProbeHost'=@'
function Get-ProbeHost {
    $script:probeMock.Attaches++
    $fake=[pscustomobject]@{}
    $fake|Add-Member ScriptMethod DoJavaScript {
        param($javascript)
        $script:probeMock.HostCalls++
        $script:probeMock.Wrapper=$javascript
        if($script:probeMock.Fault.call){throw 'injected host primary'}
        return '{"passed":true}'
    }
    return $fake
}
'@
    'Get-ProbeProcesses'=@'
function Get-ProbeProcesses {
    $script:probeMock.Queries++
    if($script:probeMock.Fault.zero){return}
    [pscustomobject]@{pid=42;startUtc='mock';windowHandle='0'}
}
'@
    'Add-ProbeJournal'=@'
function Add-ProbeJournal([string]$Text){
    $record=$Text|ConvertFrom-Json
    if($record.stage -eq 'runner.error' -and $script:probeMock.Fault.errorJournal){throw 'injected error journal secondary'}
    if($record.stage -eq 'runner.finally' -and $script:probeMock.Fault.finalSnapshot){throw 'injected final snapshot failure'}
    $script:probeMock.Records.Add($record)
}
'@
}
$probeFunctions=@($probeAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $probeReplacements.ContainsKey($node.Name)},$true))
if($probeFunctions.Count -ne 4){throw 'Unsafe mock setup: dependency count changed.'}
foreach($fn in ($probeFunctions|Sort-Object {$_.Extent.StartOffset} -Descending)){
    $probeSource=$probeSource.Substring(0,$fn.Extent.StartOffset)+$probeReplacements[$fn.Name]+$probeSource.Substring($fn.Extent.EndOffset)
}
# ScriptBlock invocation has no file root; retain read-only path resolution.
$probeSource=$probeSource.Replace('$PSScriptRoot',("'"+$PSScriptRoot.Replace("'","''")+"'"))
if($probeSource -match 'GetActiveObject|Get-Process|AppendAllText|File\]::Open'){throw 'Unsafe mock setup: external dependency remains.'}
$probeRunner=[scriptblock]::Create($probeSource)
function Assert-Probe($condition,[string]$message){if(-not $condition){throw $message}}
function Invoke-ProbeCase($fault){
    $script:probeMock=@{Fault=$fault;Files=@{};Records=(New-Object 'Collections.Generic.List[object]');Attaches=0;HostCalls=0;Queries=0;Wrapper=$null}
    $caught=$null;$output=@()
    try{$output=@(& $probeRunner -OutDir $PSScriptRoot -RunId ('mock-'+[Guid]::NewGuid().ToString('N')) -RunProof -Lifecycle)}catch{$caught=$_}
    return @{Mock=$script:probeMock;Error=$caught;Output=$output}
}
$r=Invoke-ProbeCase @{}
Assert-Probe ($null -eq $r.Error) ('success failed: '+$r.Error)
Assert-Probe ($r.Mock.HostCalls -eq 1 -and $r.Mock.Attaches -eq 1) 'success invokes only mocked host'
Assert-Probe (($r.Mock.Records.stage -join ',') -eq 'before.attach,after.attach,before.DoJavaScript,after.DoJavaScript,runner.finally') 'success stage order'
Assert-Probe ($r.Mock.Files.Count -eq 2) 'raw result preserved in memory'
Assert-Probe ($r.Mock.Wrapper.StartsWith('(function(){') -and $r.Mock.Wrapper -notmatch '__iseResult') 'local wrapper generated'
$r=Invoke-ProbeCase @{zero=$true}
Assert-Probe ([string]$r.Error -match 'Require exactly one existing Illustrator process') 'zero-process primary retained'
Assert-Probe ($r.Mock.Attaches -eq 0 -and $r.Mock.HostCalls -eq 0) 'zero-process gate stops before mock attachment'
foreach($fault in @(@{call=$true},@{call=$true;errorJournal=$true},@{call=$true;finalSnapshot=$true},@{call=$true;errorJournal=$true;finalSnapshot=$true})){
    $r=Invoke-ProbeCase $fault
    Assert-Probe ($r.Error.Exception.Message -match 'injected host primary') 'primary exception retained'
    $expected=[int][bool]$fault.errorJournal+[int][bool]$fault.finalSnapshot
    $secondary=@($r.Error.Exception.Data['ProbeSecondaryErrors']|Where-Object {$null -ne $_})
    Assert-Probe ($secondary.Count -eq $expected) 'all secondary failures retained'
    if($fault.errorJournal){Assert-Probe (($secondary -join ';') -match 'error journal secondary') 'journal failure recorded'}
    if($fault.finalSnapshot){Assert-Probe (($secondary -join ';') -match 'final snapshot failure') 'snapshot failure recorded'}
    Assert-Probe ($r.Mock.Queries -eq 4) 'final snapshot attempted despite primary/journal failure'
}
$r=Invoke-ProbeCase @{finalSnapshot=$true}
Assert-Probe ($r.Error.Exception.Message -match 'final snapshot failure') 'cleanup-only failure becomes primary'
Assert-Probe ($r.Mock.Files.Count -eq 2) 'raw evidence survives cleanup failure'
$r=Invoke-ProbeCase @{rawWrite=$true;errorJournal=$true;finalSnapshot=$true}
Assert-Probe ($r.Error.Exception.Message -match 'raw write primary') 'raw-write primary retained'
Assert-Probe ($r.Error.Exception.Data['ProbeSecondaryErrors'].Count -eq 2) 'both secondary failures accompany raw-write failure'
Write-Output 'PASS: 8 pure capture cases; generated wrapper, zero-process gate, raw evidence, primary and secondary failures. No Adobe/process/COM or filesystem writes.'
