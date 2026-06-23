param(
    [string[]]$Ports = @("COM3"),
    [int]$DurationSeconds = 120
)

# If passed as a single space-separated string, split it
if ($Ports.Count -eq 1 -and $Ports[0] -match '\s') {
    $Ports = ($Ports[0] -split '\s+') | where { $_.Trim().Length -gt 0 }
}

Add-Type -AssemblyName System
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lineBuf = ""

Write-Host "--- Serial output start ---" -ForegroundColor DarkGray
Write-Host ("Monitoring " + $Ports.Count + " port(s): " + ($Ports -join ", ")) -ForegroundColor DarkGray

try {
    foreach ($p in $Ports) {
        # Skip duplicate ports (avoid "access denied" from re-opening)
        $seenPort = ($Ports | where { $_ -eq $p }).Count
        if ($seenPort -gt 1) { continue }
        
        Start-Sleep -Milliseconds 500
        try {
            Write-Host ("Opening " + $p + " at 115200 baud...") -ForegroundColor DarkGray
            $s = New-Object System.IO.Ports.SerialPort($p, 115200)
            $s.DataBits = 8
            $s.Parity = [System.IO.Ports.Parity]::None
            $s.StopBits = [System.IO.Ports.StopBits]::One
            $s.ReadTimeout = 50
            $s.WriteTimeout = 50
            $s.Open()
            Write-Host ("OPENED: " + $p) -ForegroundColor DarkGreen

            $buf = New-Object System.Byte[] 4096
            Start-Sleep -Seconds 3

            while ($sw.Elapsed.TotalSeconds -lt $DurationSeconds) {
                $avail = $s.BytesToRead
                if ($avail -gt 0) {
                    try {
                        $n = [math]::Min($avail, 4096)
                        $readBytes = $s.BaseStream.Read($buf, 0, $n)
                        if ($readBytes -gt 0) {
                            $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $readBytes)
                            foreach ($ch in $text.ToCharArray()) {
                                $code = [int]$ch
                                if ($code -ge 32) {
                                    $lineBuf += ([char]$ch)
                                } elseif ($code -eq 10 -or $code -eq 13) {
                                    if ($lineBuf.Length -gt 0) {
                                        Write-Host ("[" + $p + "] " + $lineBuf.TrimEnd())
                                        $lineBuf = ""
                                    }
                                }
                            }
                        }
                    } catch { Start-Sleep -Milliseconds 100 }
                }
                Start-Sleep -Milliseconds 100
            }
        } catch { Write-Host ("FAILED: " + $p + ": " + $_.Exception.Message) -ForegroundColor DarkRed }
    }
} catch { Write-Host ("Error: " + $_.Exception.Message) }
finally {
    if ($lineBuf.Trim().Length -gt 0) { Write-Host ("[" + $p + "] " + $lineBuf.Trim()) }
    Write-Host "--- Serial output end ---" -ForegroundColor DarkGray
}
