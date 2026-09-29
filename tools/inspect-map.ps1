param(
    [Parameter(Mandatory=$true)][string]$Path,
    [string]$Output,
    [int]$ObjectId = -1
)
$ErrorActionPreference = 'Stop'
$map = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
if ($map.schema -ne 1) { throw 'Unsupported map schema.' }
$objects = @{}
foreach ($o in $map.objects) { $objects[[int]$o.id] = $o }
$rooms = @($map.rooms)
$modules = @($rooms | ForEach-Object { $_.modules })
$faces = 0; $vertices = 0; $missing = 0
foreach ($m in $modules) {
    if ($null -eq $m.mesh) { ++$missing; continue }
    $faces += @($m.mesh.faces).Count
    $vertices += @($m.mesh.vertices).Count
}
$lines = [Collections.Generic.List[string]]::new()
$lines.Add("Level: $($map.level). Species code: $($map.species).")
$lines.Add("Rooms: $($rooms.Count). Render modules: $($modules.Count). Objects: $(@($map.objects).Count).")
$lines.Add("Base module geometry: $vertices vertices, $faces faces; $missing modules without a mesh.")
$lines.Add('Coordinates are millimetres, Y down. Base geometry is not animated collision. Graph links are not a proof of player traversal.')
$lines.Add("Player: $($map.player -join ', ').")
$lines.Add('')
$lines.Add('Room connections:')
foreach ($r in $rooms) {
    $links = @($r.links | ForEach-Object {
        $restriction = if ($_.alien_only -eq 1) { ' Alien-only' } elseif ($_.alien_only -lt 0) { ' missing entry' } else { '' }
        "$($_.to)$restriction"
    })
    $names = @($r.modules | ForEach-Object { "$($_.name) (module $($_.id))" })
    $lines.Add("Room $($r.id) [$($names -join ', ')]: $($links -join '; '). AI passable now: $($r.ai_passable_now).")
}
$lines.Add(''); $lines.Add('Module vertical extents (Y down; overlapping ranges do not prove connectivity):')
foreach ($m in $modules) {
    $lo = [long]$m.position[1] + [long]$m.bounds_local[0][1]
    $hi = [long]$m.position[1] + [long]$m.bounds_local[1][1]
    $lines.Add("Module $($m.id), $($m.name): Y $lo to $hi millimetres.")
}
$lines.Add('')
$lines.Add('Switches and doors (snapshot IDs; do not reuse IDs between exports):')
foreach ($o in $map.objects) {
    if (!$o.kind) { continue }
    $targets = @($o.targets | Where-Object { $null -ne $_ } | ForEach-Object { "$($_.id) request=$($_.request)" })
    $lines.Add("Object $($o.id): $($o.kind), state=$($o.state), module=$($o.module), position=[$($o.position -join ', ')], targets=[$($targets -join '; ')], prerequisites=[$($o.prerequisites -join ', ')].")
}
if ($ObjectId -ge 0) {
    if (!$objects.ContainsKey($ObjectId)) { throw "Unknown object $ObjectId" }
    $lines.Add(''); $lines.Add("Incoming switch chain for object $ObjectId (requests shown without interpreting puzzle semantics):")
    $queue = [Collections.Generic.Queue[int]]::new(); $queue.Enqueue($ObjectId)
    $seen = [Collections.Generic.HashSet[int]]::new(); [void]$seen.Add($ObjectId)
    while ($queue.Count) {
        $target = $queue.Dequeue()
        foreach ($sender in $map.objects) {
            foreach ($edge in $sender.targets) {
                if ($null -ne $edge -and $edge.id -eq $target) {
                    $lines.Add("Object $($sender.id) ($($sender.kind), state=$($sender.state)) -> $target, request $($edge.request).")
                    if ($seen.Add([int]$sender.id)) { $queue.Enqueue([int]$sender.id) }
                }
            }
        }
    }
}
if ($Output) { $lines | Set-Content -LiteralPath $Output -Encoding utf8 }
else { $lines }
