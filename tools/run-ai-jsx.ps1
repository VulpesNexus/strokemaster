# run-ai-jsx.ps1 - run one script from tools\jsx in the running Adobe Illustrator.
#
# Usage:
#   powershell -NoProfile -File tools\run-ai-jsx.ps1 ai-dump-paths.jsx -ArgJson '{"out":"C:/tmp/p.json"}'
#
# The companion to run-jsx.ps1, which drives InDesign. Illustrator is a separate application with
# a separate automation interface, so it needs its own runner: the ProgID is Illustrator's and
# the entry point is DoJavaScript rather than DoScript.
#
# The bootstrap sent to Illustrator is built as a SINGLE LINE that evaluates the target file.
# That is deliberate. Scripts sent to these hosts as multi-line strings have returned an empty
# result in this project often enough to be worth avoiding, and keeping the payload to one line
# with the real work in a file removes the question entirely: the file is read by the host, so
# its formatting and comments are its own business.
#
# ISE_AI_ARGS carries the arguments and is deleted afterward, so a later run cannot inherit them.
# Pure ASCII: this file is read in the system codepage.
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$ArgJson = ''
)
$ErrorActionPreference = 'Stop'

$jsxDir = Join-Path $PSScriptRoot 'jsx'
$target = Join-Path $jsxDir $Script
if (-not (Test-Path -LiteralPath $target)) { throw "script not found: $target" }

# Forward slashes throughout: ExtendScript's File() takes them on Windows and they need no
# escaping inside the single-quoted string below.
$targetJs = ($target -replace '\\', '/') -replace "'", "\'"

$argsLiteral = '{}'
if ($ArgJson) {
    # Validate before sending: a malformed literal would fail inside the host, where the error is
    # far less legible than it is here. The overwhelmingly common cause is a Windows path pasted
    # into the JSON by hand, because a backslash starts an escape sequence in JSON and "C:\Users"
    # is therefore invalid, so say so rather than leaving the caller with "Unrecognized escape
    # sequence" and no clue which of their arguments caused it.
    try {
        $null = $ArgJson | ConvertFrom-Json
    } catch {
        throw ("-ArgJson is not valid JSON: " + $_.Exception.Message + [Environment]::NewLine +
               "If it contains a Windows path, use forward slashes, or build the argument with " +
               "ConvertTo-Json rather than writing it by hand, for example:" + [Environment]::NewLine +
               '  $json = @{ out = ''C:/tmp/p.json'' } | ConvertTo-Json -Compress')
    }
    $argsLiteral = $ArgJson
}

$js = "`$.global.ISE_AI_ARGS = $argsLiteral; var __r = 'no result'; try { __r = `$.evalFile(new File('$targetJs')); } catch (e) { __r = 'WRAPPER ERROR ' + e.message + ' line ' + e.line; } finally { try { delete `$.global.ISE_AI_ARGS; } catch (e2) {} } __r;"

$ai = New-Object -ComObject Illustrator.Application
$result = $ai.DoJavaScript($js)
if ([string]::IsNullOrEmpty($result)) {
    # An empty return and a script that never ran look identical, so say which is unknown rather
    # than reporting success.
    Write-Output "(no result returned; check for a side effect before assuming the script ran)"
} else {
    Write-Output $result
}
