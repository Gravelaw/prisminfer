$ErrorActionPreference = "Stop"

$validator = Join-Path $PSScriptRoot "validate-model-cell-catalog.ps1"
$catalogPath = Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Remove-TestJunction {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $item = Get-Item -LiteralPath $Path -Force
    $rootPrefix = $repoRoot.TrimEnd('\') + '\'
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0 -or
        -not $item.FullName.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove an unexpected test path: $Path"
    }
    [IO.Directory]::Delete($Path, $false)
}
& powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $catalogPath
if ($LASTEXITCODE -ne 0) {
    throw "Valid model-cell catalog was rejected."
}

$tempRoot = [IO.Path]::GetFullPath((Join-Path $env:TEMP ("prisminfer-model-cell-catalog-" + $PID)))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    $catalogText = Get-Content -LiteralPath $catalogPath -Raw

    $badHashPath = Join-Path $tempRoot "bad-hash.json"
    $badHashText = $catalogText.Replace(
        "7002dc2bd5b77fb496c00236843472589eb63a8121dfb3a5db474470faddfc34",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badHashPath -Value $badHashText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badHashOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badHashPath 2>&1
        $badHashExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badHashExit -eq 0) {
        throw "Catalog validator accepted a mismatched smoke hash."
    }

    $badPath = Join-Path $tempRoot "bad-path.json"
    $badPathText = $catalogText.Replace(
        "tests/fixtures/model-cells/tiny-smoke-artifact.txt",
        "../outside-smoke-artifact.txt")
    Set-Content -LiteralPath $badPath -Value $badPathText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badPathOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badPath 2>&1
        $badPathExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badPathExit -eq 0) {
        throw "Catalog validator accepted an artifact path outside the repository."
    }

    $reparseLink = Join-Path $repoRoot "tests\fixtures\model-cells\.catalog-reparse-test"
    New-Item -ItemType Junction -Path $reparseLink -Target $env:TEMP -Force | Out-Null
    try {
        $reparsePath = Join-Path $tempRoot "bad-reparse.json"
        $reparseText = $catalogText.Replace(
            "tests/fixtures/model-cells/tiny-smoke-artifact.txt",
            "tests/fixtures/model-cells/.catalog-reparse-test/outside.txt")
        Set-Content -LiteralPath $reparsePath -Value $reparseText -NoNewline
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $reparseOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $reparsePath 2>&1
            $reparseExit = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedPreference
        }
        if ($reparseExit -eq 0) {
            throw "Catalog validator accepted an artifact path through a reparse point."
        }
    }
    finally {
        Remove-TestJunction -Path $reparseLink
    }
}
finally {
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-hash.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-path.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-reparse.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tempRoot -Force -ErrorAction SilentlyContinue
}

Write-Output "Model-cell catalog negative tests PASS."
exit 0
