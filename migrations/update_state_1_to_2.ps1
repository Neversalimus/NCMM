param([Parameter(Mandatory=$true)][object]$State)
$ErrorActionPreference='Stop'
$out=[ordered]@{}
foreach($p in $State.PSObject.Properties){$out[$p.Name]=$p.Value}
$out['schema']=2
if(-not $out.Contains('channel')){$out['channel']='experimental'}
if(-not $out.Contains('auto_apply')){$out['auto_apply']=$false}
if(-not $out.Contains('feed_etag')){$out['feed_etag']=$null}
if(-not $out.Contains('last_successful_update')){$out['last_successful_update']=$null}
[pscustomobject]$out
