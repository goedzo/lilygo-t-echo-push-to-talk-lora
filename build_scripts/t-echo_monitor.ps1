param(
    [string[]]$Ports = @(),
    [int]$DurationSeconds = 120,
    [int]$DiscoveryWaitSeconds = 10
)

# If passed as a single space-separated string, split it
if ($Ports.Count -eq 1 -and $Ports[0] -match '\s') {
    $Ports = ($Ports[0] -split '\s+') | where { $_.Trim().Length -gt 0 }
}

# Discover all available COM ports at runtime
function Get-AllCOMPorts {
    $comPorts = @()
    try {
        # Method 1: registry lookup
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

    # Method 2: wmic fallback
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

# Initial wait for USB re-enumeration
if ($DiscoveryWaitSeconds -gt 0) {
    Write-Host ("Waiting " + $DiscoveryWaitSeconds + "s for device enumeration...") -ForegroundColor DarkGray
    Start-Sleep -Seconds $DiscoveryWaitSeconds
}

$discoveredPorts = Get-AllCOMPorts

# Filter discovered ports if explicit Ports were passed
$activePorts = @()
if ($Ports.Count -gt 0) {
    foreach ($p in $discoveredPorts) {
        if ($Ports -contains $p) { $activePorts += $p }
    }
} else {
    $activePorts = $discoveredPorts
}

Write-Host "--- Serial output start ---" -ForegroundColor DarkGray
Write-Host ("Found " + $discoveredPorts.Count + " total COM port(s). Active monitoring list: " + ($activePorts -join ", ")) -ForegroundColor DarkGray

$openPorts = @()

# Open all target ports simultaneously
foreach ($p in $activePorts) {
    try {
        $s = New-Object System.IO.Ports.SerialPort($p, 115200)
        $s.DataBits = 8
        $s.Parity = [System.IO.Ports.Parity]::None
        $s.StopBits = [System.IO.Ports.StopBits]::One
        $s.ReadTimeout = 50
        $s.WriteTimeout = 50
        
        # CRITICAL: Assert DTR & RTS lines so nRF52840 virtual USB CDC outputs data
        $s.DtrEnable = $true
        $s.RtsEnable = $true
        
        $s.Open()
        Write-Host ("OPENED: " + $p) -ForegroundColor DarkGreen
        
        # Track port, metadata, and separate line buffers
        $openPorts += [PSCustomObject]@{
            Port = $s
            Name = $p
            LineBuffer = ""
            TotalRead = 0
        }
    } catch [System.UnauthorizedAccessException] {
        Write-Host ("BUSY: " + $p + " (in use by another application)") -ForegroundColor DarkYellow
    } catch {
        Write-Host ("FAILED to open " + $p + ": " + $_.Exception.Message) -ForegroundColor DarkRed
    }
}

if ($openPorts.Count -eq 0) {
    Write-Host "ERROR: No COM ports could be successfully opened for monitoring." -ForegroundColor Red
    Write-Host "--- Serial output end ---" -ForegroundColor DarkGray
    exit 1
}

# Main non-blocking read loop (multiplexing)
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$buf = New-Object System.Byte[] 4096

try {
    while ($sw.Elapsed.TotalSeconds -lt $DurationSeconds) {
        $hasActivity = $false
        
        foreach ($op in $openPorts) {
            $s = $op.Port
            if ($s.IsOpen) {
                try {
                    $avail = $s.BytesToRead
                    if ($avail -gt 0) {
                        $hasActivity = $true
                        $n = [math]::Min($avail, 4096)
                        $readBytes = $s.BaseStream.Read($buf, 0, $n)
                        if ($readBytes -gt 0) {
                            $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $readBytes)
                            foreach ($ch in $text.ToCharArray()) {
                                $code = [int]$ch
                                # Accept printable ASCII (>=32) and tabs (9)
                                if ($code -ge 32 -or $code -eq 9) {
                                    $op.LineBuffer += ([char]$ch)
                                } elseif ($code -eq 10 -or $code -eq 13) {
                                    if ($op.LineBuffer.Length -gt 0) {
                                        $ts = Get-Date -Format "HH:mm:ss"
                                        Write-Host ("[$ts] [" + $op.Name + "] " + $op.LineBuffer.TrimEnd())
                                        $op.LineBuffer = ""
                                    }
                                }
                            }
                            $op.TotalRead += $readBytes
                        }
                    }
                } catch {
                    # Handle unexpected device disconnection gracefully
                    Write-Host ("`n[" + (Get-Date -Format 'HH:mm:ss') + "] [" + $op.Name + "] Device disconnected.") -ForegroundColor DarkYellow
                }
            }
        }
        
        # If no bytes came in, sleep briefly to prevent high CPU utilization
        if (-not $hasActivity) {
            Start-Sleep -Milliseconds 50
        }
    }
} finally {
    # Clean up and close all opened ports safely
    foreach ($op in $openPorts) {
        try {
            if ($op.LineBuffer.Trim().Length -gt 0) {
                $ts = Get-Date -Format "HH:mm:ss"
                Write-Host ("[$ts] [" + $op.Name + "] " + $op.LineBuffer.Trim())
            }
            if ($op.Port.IsOpen) {
                $op.Port.Close()
                Write-Host ("CLOSED: " + $op.Name + " (Total bytes read: " + $op.TotalRead + ") [" + (Get-Date -Format 'HH:mm:ss') + "]") -ForegroundColor DarkGray
            }
        } catch {}
    }
    Write-Host "--- Serial output end ---" -ForegroundColor DarkGray
}