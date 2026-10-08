[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent $repo
$reference = '/work/OpenBK7231T_App-32269ac9e9e249c337cee136d8514b2ac9b71eb3'
$toolchain = '/work/toolchains/andes-v3/nds32le-elf-mculib-v3.txz'
$output = '/work/SV6x66_loader/artifacts'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $repo "artifacts\build_$stamp.log"

$Host.UI.RawUI.WindowTitle = 'SV6x66_loader clean build and host tests'
Start-Transcript -Path $log
try {
	Write-Host 'SV6x66_loader CLEAN BUILD' -ForegroundColor Cyan
	& docker run --rm --entrypoint bash -v "${workspace}:/work" openbk_builder `
		/work/SV6x66_loader/build/build.sh $reference $toolchain $output
	if ($LASTEXITCODE -ne 0) {
		throw "loader build exited $LASTEXITCODE"
	}

	Push-Location (Join-Path $repo 'host')
	try {
		python -m unittest -v
		if ($LASTEXITCODE -ne 0) {
			throw "host tests exited $LASTEXITCODE"
		}
	}
	finally {
		Pop-Location
	}
	Write-Host 'CLEAN BUILD AND HOST TESTS PASSED' -ForegroundColor Green
}
finally {
	Stop-Transcript
}

