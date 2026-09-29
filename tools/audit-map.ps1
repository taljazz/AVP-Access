param([Parameter(Mandatory=$true)][string]$Path,[string]$Output)
$ErrorActionPreference='Stop'
$map=Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
if($map.schema -ne 1){throw 'Unsupported map schema'}
$lines=[Collections.Generic.List[string]]::new()
$lines.Add("Level-wide static navigation audit: $($map.level)")
$lines.Add('This inventories authored coverage and restrictions. It does not certify physical passage or completion.')
$ids=[Collections.Generic.HashSet[int]]::new()
foreach($r in $map.rooms){[void]$ids.Add([int]$r.id)}
$edges=0;$missing=0;$alienOnly=0;$invalid=0;$volumes=0;$roomsWithVolumes=0
foreach($r in $map.rooms){
    foreach($link in $r.links){
        $edges++
        if(!$ids.Contains([int]$link.to)){$invalid++}
        if($null -eq $link.entry){$missing++}
        if($link.alien_only -eq 1){$alienOnly++}
    }
    $w=@($r.waypoints)
    if(!$w.Count){continue}
    $roomsWithVolumes++;$volumes+=$w.Count
    $walkingMissing=0;$alienMissing=0
    foreach($allowAlien in @($false,$true)){
        foreach($start in $w){
            $seen=[Collections.Generic.HashSet[int]]::new()
            $q=[Collections.Generic.Queue[int]]::new();$q.Enqueue([int]$start.id);[void]$seen.Add([int]$start.id)
            while($q.Count){
                $at=$q.Dequeue()
                foreach($edge in $w[$at].links){
                    if($edge.to -lt 0 -or $edge.to -ge $w.Count){$invalid++;continue}
                    if(($edge.flags -band 2) -or (!$allowAlien -and ($edge.flags -band 4))){continue}
                    if($seen.Add([int]$edge.to)){$q.Enqueue([int]$edge.to)}
                }
            }
            if($allowAlien){$alienMissing+=$w.Count-$seen.Count}else{$walkingMissing+=$w.Count-$seen.Count}
        }
    }
    $lines.Add("Room $($r.id) [$($r.modules.name -join ', ')]: $($w.Count) volumes; unreachable ordered pairs: walking=$walkingMissing, Alien=$alienMissing. Restricted crawling volumes can account for walking gaps.")
}
$lines.Insert(2,"Rooms=$(@($map.rooms).Count); directed room links=$edges; missing entry points=$missing; Alien-only room links=$alienOnly; invalid references=$invalid.")
$lines.Insert(3,"Authored intra-room coverage: $roomsWithVolumes rooms, $volumes volumes. Other rooms retain portal/local guidance.")
$lines.Add('')
$lines.Add('Platform lifts (automatic boarding/terminal state):')
foreach($o in $map.objects){
    if($o.kind -ne 'platform_lift'){continue}
    $lines.Add("Object $($o.id), position [$($o.position -join ', ')], upper Y=$($o.up_y), lower Y=$($o.down_y), state=$($o.state), enabled=$($o.enabled), one-use=$($o.one_use).")
}
$lines.Add('')
$lines.Add('Physical switches and their outgoing requests:')
foreach($o in $map.objects){
    if($o.kind -notin @('binary_switch','link_switch') -or $null -eq $o.position){continue}
    $lines.Add("Object $($o.id), position [$($o.position -join ', ')], state=$($o.state), security=$($o.security), target count=$(@($o.targets).Count).")
}
if($Output){$lines | Set-Content -LiteralPath $Output -Encoding utf8}else{$lines}
if($invalid){throw 'Invalid graph references found; see report'}
