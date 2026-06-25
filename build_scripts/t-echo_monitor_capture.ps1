param(
    [string]$Port = "",
    [int]$DurationSeconds = 300,
    [int]$PostUploadWaitMinutes = 3
)

# Discover COM ports at start and persist the list
function Get-AllCOMPorts {
    $comPorts = @()
    try {
        $regKeys = reg query "HKLM\HARDWARE\DEVICEMAP\SERIALCOMM" 2>nul
        if ($LASTEXITCODE -eq 0) {
            foreach ($line in $regKeys) {
                $parts = $line -split '\s+'
                foreach ($part in $parts) {
                    if ($part -match '^COM\d+$') {
                        $comPorts += $part.Trim()
                    }
                }
            }
        }
    } catch {}

    if ($comPorts.Count -eq 0) {
        try {
            $serialPorts = Get-WmiObject Win32_SerialPort -ErrorAction SilentlyContinue
            foreach ($sp in $serialPorts) {
                if ($sp.DeviceID -match '^COM\d+$') {
                    $comPorts += $sp.DeviceID.Trim()
                }
            }
        } catch {}
    }

    return $comPorts | where { $_.Trim().Length -gt 0 } | select -unique
}

Add-Type -AssemblyName System

if (-not $Port) {
    Write-Host "ERROR: No port specified for capture" -ForegroundColor Red
    exit 1
}

Write-Host ("[Capture] Listening on " + $Port + " for up to " + $DurationSeconds + " seconds...") -ForegroundColor DarkGray
Write-Host "[Capture] This will continue through upload, device reboot, and boot messages." -ForegroundColor DarkGray

$sw = [System.Diagnostics.Stopwatch]::StartNew()
# Phase tracking: we close the port during upload (DFU tool needs it), then reopen after
$uploading = $false

while ($sw.Elapsed.TotalSeconds -lt $DurationSeconds) {
    # During upload phase, release port and just poll until it's free
    if ($uploading) {
        Start-Sleep -Milliseconds 500
        continue
    }

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
            $lineBuffer = ""

            # Read loop - keep the port open even if device disconnects temporarily
            while ($sw.Elapsed.TotalSeconds -lt $DurationSeconds) {
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
                                        Write-Host ("[" + $ts + "] [BOOT] " + $lineBuffer.Trim())
                                        $lineBuffer = ""
                                    }
                                }
                            }
                        }
                    } else {
                        Start-Sleep -Milliseconds 50
                    }
                } catch [System.InvalidOperationException] {
                    # Port closed by another app or device disconnected
                    if ($sw.Elapsed.TotalSeconds -lt 60) {
                        # Still early - wait for upload to finish, then release and reopen
                        Write-Host ("[" + (Get-Date -Format 'HH:mm:ss') + "] [Capture] Device reset detected. Releasing port for upload...") -ForegroundColor DarkGray
                        try { $s.Close() } catch {}
                        $uploading = $true
                        Start-Sleep -Milliseconds 200
                        $uploading = $false
                    } else {
                        # Past initial phase - just close and let next loop reopen
                        Write-Host ("[" + (Get-Date -Format 'HH:mm:ss') + "] [Capture] Port closed, will retry...") -ForegroundColor DarkGray
                        break
                    }
                } catch {}
            }
        } finally {
            if ($s.IsOpen) { $s.Close() }
        }
    } catch [System.UnauthorizedAccessException] {
        # Port not yet available (device hasn't enumerated) - just wait and retry
        Start-Sleep -Milliseconds 200
    } catch {
        Start-Sleep -Milliseconds 200
    }
}

Write-Host ("[" + (Get-Date -Format 'HH:mm:ss') + "] [Capture] Done. Captured up to " + $DurationSeconds + " seconds from " + $Port) -ForegroundColor DarkGray
