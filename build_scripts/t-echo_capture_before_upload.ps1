param(
    [string]$Port = ""
)

if (-not $Port) {
    Write-Host "ERROR: No port specified for capture" -ForegroundColor Red
    exit 1
}

Write-Host ("[Pre-upload] Capturing serial output from " + $Port + " (releases after upload or 60s timeout)...") -ForegroundColor DarkGray

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lineBuffer = ""

while ($sw.Elapsed.TotalSeconds -lt 60) {
    try {
        $s = New-Object System.IO.Ports.SerialPort($Port, 115200)
        $s.DataBits = 8
        $s.Parity = [System.IO.Ports.Parity]::None
        $s.StopBits = [System.IO.Ports.StopBits]::One
        $s.ReadTimeout = 100
        $s.WriteTimeout = 50
        $s.DtrEnable = $true
        $s.RtsEnable = $true

        try {
            $s.Open()
            $buf = New-Object System.Byte[] 4096
            
            while ($sw.Elapsed.TotalSeconds -lt 60) {
                try {
                    $avail = $s.BytesToRead
                    if ($avail -gt 0) {
                        $n = [math]::Min($avail, 4096)
                        $readBytes = $s.BaseStream.Read($buf, 0, $n)
                        if ($readBytes -gt 0) {
                            $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $readBytes)
                            foreach ($ch in $text.ToCharArray()) {
                                $code = [int]$ch
                                if ($code -ge 32 -or $code -eq 9) {
                                    $lineBuffer += ([char]$ch)
                                } elseif ($code -eq 10 -or $code -eq 13) {
                                    if ($lineBuffer.Trim().Length -gt 0) {
                                        $ts = Get-Date -Format "HH:mm:ss"
                                        Write-Host ("[" + $ts + "] [PRE-UPLOAD] " + $lineBuffer.Trim())
                                        $lineBuffer = ""
                                    }
                                }
                            }
                        }
                    } else {
                        Start-Sleep -Milliseconds 50
                    }
                } catch [System.InvalidOperationException] {
                    # Port closed by another app (DFU tool) — this is expected during upload
                    Write-Host ("[" + (Get-Date -Format 'HH:mm:ss') + "] [PRE-UPLOAD] Port released for upload. Done." ) -ForegroundColor DarkGray
                    break
                } catch {}
            }
        } finally {
            if ($s.IsOpen) { $s.Close() }
        }
    } catch [System.UnauthorizedAccessException] {
        Start-Sleep -Milliseconds 200
    } catch {
        Start-Sleep -Milliseconds 200
    }

    # Check if device is still alive by trying to open the port
    $alive = $false
    try {
        $test = New-Object System.IO.Ports.SerialPort($Port, 115200)
        $test.Open()
        $test.Close()
        $alive = $true
    } catch {
        $alive = $false
    }

    if (-not $alive -and $sw.Elapsed.TotalSeconds -lt 55) {
        # Device not responding but still within timeout — might be reset during upload
        Start-Sleep -Milliseconds 1000
    } else {
        Start-Sleep -Milliseconds 100
    }
}

Write-Host ("[" + (Get-Date -Format 'HH:mm:ss') + "] [PRE-UPLOAD] Timeout reached. Moving to upload phase.") -ForegroundColor DarkGray
