param([Parameter(Mandatory=$true)][object]$State)
$ErrorActionPreference='Stop'
$out=[ordered]@{schema=1;installed_components=@{};last_check_utc=$null;last_feed=$null;last_plan=$null;history=@()}
if($State){
    foreach($p in $State.PSObject.Properties){if(-not $out.Contains($p.Name)){$out[$p.Name]=$p.Value}}
}
[pscustomobject]$out
