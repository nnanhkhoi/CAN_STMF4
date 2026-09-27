param([string]$Compiler = "gcc")
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force -Path build | Out-Null
    # Native compiler only: these tests simulate CAN and do not flash the board.
    & $Compiler -std=c11 -Wall -Wextra -Werror -pedantic -ICore/BSW/CanTp `
        Core/BSW/CanTp/CanTp.c tests/cantp_test.c -o build/cantp_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp protocol test compilation failed" }
    & ./build/cantp_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp protocol tests failed" }
    & $Compiler -std=c11 -Wall -Wextra -Werror -pedantic -Itests/stubs -ICore/BSW/CanTp `
        Core/BSW/CanTp/CanTp_Port.c tests/cantp_port_test.c -o build/cantp_port_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp port test compilation failed" }
    & ./build/cantp_port_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp port tests failed" }
    # Link the real UDS modules as well; only HAL, time and UART are mocked.
    $diagnosticSources = Get-ChildItem Core/Diagnostic -Filter '*.c' | ForEach-Object { $_.FullName }
    & $Compiler -std=c11 -Wall -Werror -Itests/stubs -ICore/BSW/CanTp -ICore/Diagnostic `
        -ICore/Inc -ICore/BSW/Os Core/BSW/CanTp/CanTp.c @diagnosticSources `
        tests/cantp_integration_test.c -o build/cantp_integration_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp integration test compilation failed" }
    & ./build/cantp_integration_test.exe
    if ($LASTEXITCODE -ne 0) { throw "CanTp integration tests failed" }
} finally {
    Pop-Location
}
