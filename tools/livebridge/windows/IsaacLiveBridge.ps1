# Isaac LiveBridge - local receiver for Nintendo Switch (Windows PowerShell 5.1+)
# Listen on the configured PC IPv4 and accept telemetry ONLY from the Switch.
# No uploads, credentials, or internet connections. Stop with Ctrl+C.
param(
    [string] $ListenIP = '10.10.0.165',
    [string] $SwitchIP = '10.10.0.162',
    [ValidateRange(1024,65535)] [int] $Port = 47821
)

$ErrorActionPreference = 'Stop'
$udp = $null
$logPath = Join-Path $PSScriptRoot 'livebridge-events.log'
$encoding = New-Object System.Text.UTF8Encoding($false)
$received = 0

try {
    $ipAddress = [System.Net.IPAddress]::Parse($ListenIP)
    $allowedIP = [System.Net.IPAddress]::Parse($SwitchIP)
    $endpoint = New-Object System.Net.IPEndPoint($ipAddress, $Port)
    $udp = New-Object System.Net.Sockets.UdpClient($endpoint)
    $udp.Client.ReceiveTimeout = 1000
    Write-Host 'Isaac LiveBridge - UDP receiver' -ForegroundColor Cyan
    Write-Host ("Listening on {0}:{1} from Switch {2}" -f $ListenIP, $Port, $SwitchIP)
    Write-Host ("Output: {0}" -f $logPath)
    Write-Host 'Waiting for packets. Ctrl+C stops the listener.'
    while ($true) {
        try {
            $remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
            [byte[]] $data = $udp.Receive([ref]$remote)
            if (-not $remote.Address.Equals($allowedIP)) {
                # Deliberately ignore other devices on this local network.
                continue
            }
            if ($data.Length -eq 0 -or $data.Length -gt 2048) { continue }
            $message = $encoding.GetString($data)
            if (-not $message.StartsWith('ILB1|')) { continue }
            $message = $message.Substring(5)
            # Keep log lines bounded, printable, and single-line.
            $message = [System.Text.RegularExpressions.Regex]::Replace($message, '[^\x20-\x7E]', '?')
            $message = $message.Substring(0, [Math]::Min($message.Length, 800))
            $row = ('{0:yyyy-MM-dd HH:mm:ss.fff} | {1}' -f (Get-Date), $message)
            Write-Host $row
            [System.IO.File]::AppendAllText($logPath, $row + [Environment]::NewLine, $encoding)
            $received++
        }
        catch {
            $errorObject = $_.Exception
            if ($errorObject.InnerException) { $errorObject = $errorObject.InnerException }
            if ($errorObject -is [System.Net.Sockets.SocketException] -and
                $errorObject.SocketErrorCode -eq [System.Net.Sockets.SocketError]::TimedOut) {
                continue
            }
            throw
        }
    }
}
finally {
    if ($null -ne $udp) { $udp.Close() }
    Write-Host ("Stopped. Received {0} packets." -f $received)
}
