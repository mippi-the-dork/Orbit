param(
    [Parameter(Mandatory = $true)] [string] $Project,
    [Parameter(Mandatory = $true)] [string] $EngineRoot,
    [Parameter(Mandatory = $true)] [string] $ArchiveDirectory,
    [ValidateSet('Development', 'Shipping')] [string] $Configuration = 'Shipping'
)
$ErrorActionPreference = 'Stop'

# This helper never touches source control or deletes files. Both configurations
# use the same real Win64 game packaging pipeline, not BuildPlugin's editor host.
$ProjectPath = (Resolve-Path -LiteralPath $Project -ErrorAction Stop).Path
$RootPath = (Resolve-Path -LiteralPath $EngineRoot -ErrorAction Stop).Path
$UAT = Join-Path $RootPath 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $UAT -PathType Leaf)) {
    throw "Could not find RunUAT.bat: $UAT"
}
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf) -or
    -not $ProjectPath.EndsWith('.uproject', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Cannot find a valid .uproject: $ProjectPath"
}
if (-not (Test-Path -LiteralPath $ArchiveDirectory)) {
    New-Item -ItemType Directory -Force -Path $ArchiveDirectory | Out-Null
}
$ArchivePath = (Resolve-Path -LiteralPath $ArchiveDirectory).Path
$ProjectDirectory = [System.IO.Path]::GetDirectoryName($ProjectPath)
if ($ArchivePath.TrimEnd([char]92) -eq $ProjectDirectory.TrimEnd([char]92)) {
    throw 'ArchiveDirectory cannot be the project root. Choose a separate folder for packaged output.'
}
Write-Host 'Orbit game packaging validation: no files are deleted and Perforce is disabled for UAT.'
Write-Host "Target: Win64 $Configuration"
Write-Host "Project: $ProjectPath"
Write-Host "UAT: $UAT"
Write-Host "Archive: $ArchivePath"
Write-Host 'IMPORTANT: run Development and Shipping into DIFFERENT archive directories.'

& $UAT BuildCookRun -nop4 -utf8output -unattended "-project=$ProjectPath" `
    -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -archive `
    "-archivedirectory=$ArchivePath"
if ($LASTEXITCODE -ne 0) {
    throw "Win64 $Configuration packaging failed, UAT exit code $LASTEXITCODE. Find the FIRST UHT/compiler/linker/cook error in the complete AutomationTool log."
}
Write-Host "Win64 $Configuration packaging step finished: $ArchivePath"
Write-Host 'Validation is incomplete until the packaged game is LAUNCHED and its Orbit gravity behavior is checked.'
