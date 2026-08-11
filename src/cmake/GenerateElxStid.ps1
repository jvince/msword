param(
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][string]$OutputPath
)
$source = [IO.File]::ReadAllText($InputPath)
$startMatch = [regex]::Match($source, 'fprintf\s*\(\s*pfile\s*,\s*"[^"\r\n]*#ifdef STID\\n"\s*\)\s*;')
if (-not $startMatch.Success) { throw "STID start marker not found in $InputPath" }
$tail = $source.Substring($startMatch.Index + $startMatch.Length)
$endMatch = [regex]::Match($tail, 'fprintf\s*\(\s*pfile\s*,\s*"#endif\\n"\s*\)\s*;')
if (-not $endMatch.Success) { throw "STID end marker not found in $InputPath" }
$block = $tail.Substring(0, $endMatch.Index)
$builder = [Text.StringBuilder]::new()
[void]$builder.Append("/* Generated from MERGEELX's fixed STID payload. Do not edit. */`n#ifndef STID`ncsconst struct { HID hid; CABI cabi; unsigned celfd; ELFD rgelfd[1]; } rgeldi[] = { 0 };`ncsconst unsigned rgichName[] = { 0 };`ncsconst unsigned char rgchElkNames[] = { 0 };`ncsconst unsigned mpelkistName[elkAppMac] = { 0 };`n#else /* STID */`n")
$matches = [regex]::Matches($block, 'fprintf\(pfile,\s*"((?:[^"\\]|\\.)*)"\s*\);')
if ($matches.Count -eq 0) { throw "No STID payload records found in $InputPath" }
foreach ($match in $matches) {
    $decoded = [regex]::Unescape($match.Groups[1].Value).Replace('StringMap("SUPO", 0, 1)', '"SUPO"')
    [void]$builder.Append($decoded)
}
[void]$builder.Append("#endif /* STID */`n")
$outputDirectory = Split-Path -Parent $OutputPath
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[IO.File]::WriteAllText($OutputPath, $builder.ToString(), [Text.UTF8Encoding]::new($false))
