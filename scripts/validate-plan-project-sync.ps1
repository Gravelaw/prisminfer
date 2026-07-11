[CmdletBinding()]
param(
    [string]$Owner = "Gravelaw",
    [int]$ProjectNumber = 2
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI (gh) is required for the read-only Project sync check."
}

$projectJson = & gh project view $ProjectNumber --owner $Owner --format json
if ($LASTEXITCODE -ne 0) {
    throw "Unable to read GitHub Project #$ProjectNumber."
}

$itemsJson = & gh project item-list $ProjectNumber --owner $Owner --format json --limit 200
if ($LASTEXITCODE -ne 0) {
    throw "Unable to read GitHub Project #$ProjectNumber items."
}

$project = $projectJson | ConvertFrom-Json
$items = @(($itemsJson | ConvertFrom-Json).items)
$failures = [System.Collections.Generic.List[string]]::new()
$byNumber = @{}

foreach ($item in $items) {
    if ($null -ne $item.content.number) {
        $byNumber[[int]$item.content.number] = $item
    }
}

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

function Require-Item {
    param([int]$Number)
    if (-not $script:byNumber.ContainsKey($Number)) {
        Add-Failure "Project item #$Number is missing."
        return $null
    }
    return $script:byNumber[$Number]
}

function Assert-Value {
    param(
        [int]$Number,
        [string]$Field,
        [AllowNull()][object]$Actual,
        [AllowNull()][object]$Expected
    )
    if ([string]$Actual -cne [string]$Expected) {
        Add-Failure "#$Number $Field is '$Actual'; expected '$Expected'."
    }
}

$phaseStatus = [ordered]@{
    73 = "Done"
    74 = "Ready"
    75 = "Blocked"
    76 = "Ready"
    77 = "Blocked"
    78 = "Blocked"
    79 = "In Progress"
    80 = "Ready"
    81 = "Ready"
    82 = "Ready"
    83 = "Ready"
    84 = "Blocked"
    85 = "Blocked"
    86 = "Blocked"
    87 = "Blocked"
    88 = "Blocked"
    89 = "Blocked"
    90 = "Backlog"
    91 = "Backlog"
    92 = "Backlog"
    93 = "Backlog"
    94 = "Backlog"
    95 = "Backlog"
    96 = "Backlog"
    97 = "Backlog"
    98 = "Backlog"
    99 = "Blocked"
    100 = "Blocked"
    101 = "Backlog"
    102 = "Backlog"
    103 = "Ready"
}

$expectedTitles = @{
    74 = "P6-07 Implement exact per-tensor GGML quant semantics for the pinned artifact"
    75 = "P6-08 Add a strict manifest-emitting benchmark/evidence runner"
    76 = "P6-09/P6-10 Add mandatory quality fixtures and optional offline KV evaluation"
    77 = "P6-11/P6-12/P6-13 Collect supervised same-cell foundation baseline evidence"
    79 = "P7-00 Freeze final Plan, council contracts, thresholds and novelty boundary"
    80 = "P7-01 Pin Llama 3.1 8B foundation and Ornith 9B stress artifacts"
    84 = "P7-05 Run early exact-artifact 8B/9B/30B/70B/90B admission"
    89 = "P7-10 Validate foundation replay, then Ornith stress, and audit Phase 7"
    103 = "P6-04A Implement fail-closed hardware supervisor and staged admission boundary"
}

foreach ($entry in $phaseStatus.GetEnumerator()) {
    $number = [int]$entry.Key
    $item = Require-Item $number
    if ($null -eq $item) {
        continue
    }

    $expectedCoarse = if ($number -eq 79) { "In Progress" } elseif ($number -eq 73) { "Done" } else { "Todo" }
    Assert-Value $number "Status" $item.status $expectedCoarse
    Assert-Value $number "Phase Status" $item.'phase Status' $entry.Value

    foreach ($field in @("priority", "risk", "roadmap Phase", "roadmap Slice", "roadmap Gate")) {
        if ([string]::IsNullOrWhiteSpace([string]$item.$field)) {
            Add-Failure "#$number is missing $field."
        }
    }
    if ($null -eq $item.milestone -or [string]::IsNullOrWhiteSpace([string]$item.milestone.title)) {
        Add-Failure "#$number is missing a milestone."
    }

    if (($number -ge 73 -and $number -le 78) -or $number -eq 103) {
        Assert-Value $number "Roadmap Phase" $item.'roadmap Phase' "Phase 6"
        Assert-Value $number "Roadmap Gate" $item.'roadmap Gate' "Phase 6 Evidence"
        Assert-Value $number "milestone" $item.milestone.title "Phase 6: Safety and Exact Evidence Foundation"
    } elseif ($number -ge 79 -and $number -le 89) {
        Assert-Value $number "Roadmap Phase" $item.'roadmap Phase' "Phase 7"
        Assert-Value $number "Roadmap Gate" $item.'roadmap Gate' "Phase 7 Calibrated Execution"
        Assert-Value $number "milestone" $item.milestone.title "Phase 7: Secure Static Calibrated Controller"
    } elseif ($number -ge 90 -and $number -le 96) {
        Assert-Value $number "Roadmap Phase" $item.'roadmap Phase' "Phase 8"
        Assert-Value $number "Roadmap Gate" $item.'roadmap Gate' "Phase 8 Optional Mechanisms"
        Assert-Value $number "milestone" $item.milestone.title "Phase 8: 30B Static Truth and Optional Mechanisms"
    } else {
        Assert-Value $number "Roadmap Phase" $item.'roadmap Phase' "Phase 9"
        Assert-Value $number "Roadmap Gate" $item.'roadmap Gate' "Phase 9 Scale Validation"
        Assert-Value $number "milestone" $item.milestone.title "Phase 9: Gated Scale, Portability, and Final Audit"
    }

    if ($expectedTitles.ContainsKey($number)) {
        Assert-Value $number "title" $item.title $expectedTitles[$number]
    }
}

foreach ($item in $items | Where-Object { $_.status -eq "Done" }) {
    if ($item.'phase Status' -ne "Done") {
        Add-Failure "Completed item #$($item.content.number) has Phase Status '$($item.'phase Status')'."
    }
}

$historicalNumbers = @(1..15) + @(60..72)
foreach ($number in $historicalNumbers) {
    $item = Require-Item $number
    if ($null -eq $item) {
        continue
    }
    foreach ($field in @("roadmap Phase", "roadmap Slice", "roadmap Gate")) {
        if ([string]::IsNullOrWhiteSpace([string]$item.$field)) {
            Add-Failure "Historical item #$number is missing $field."
        }
    }
}

$pr72 = Require-Item 72
if ($null -ne $pr72) {
    Assert-Value 72 "Status" $pr72.status "Done"
    Assert-Value 72 "Phase Status" $pr72.'phase Status' "Done"
    Assert-Value 72 "Roadmap Phase" $pr72.'roadmap Phase' "Phase 6"
    Assert-Value 72 "Roadmap Gate" $pr72.'roadmap Gate' "Phase 6 Evidence"
    Assert-Value 72 "milestone" $pr72.milestone.title "Phase 6: Safety and Exact Evidence Foundation"
}

foreach ($needle in @("Plan.md", "#103", "Llama 3.1 8B", "Phase 8 Optional Mechanisms")) {
    if (-not ([string]$project.readme).Contains($needle)) {
        Add-Failure "Project README is missing '$needle'."
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Plan/Project drift detected:`n- " + ($failures -join "`n- "))
    exit 1
}

Write-Output ("Plan/Project sync PASS: {0} items checked; #73-#103 status, roadmap, milestone, title and README contracts match." -f $items.Count)
