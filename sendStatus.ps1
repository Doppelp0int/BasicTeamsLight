Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ------------------------
# Configuration
# ------------------------
$server = "http://ESP32.local" #<-- ESP32 Local IP
$checkInterval = 2000
$idleLimit = 120
# Enable/disable Basic Auth
$BasicAuth = $true
# Username / Password for Basic Auth
$user = "XXXX"
$pass = "XXXX"

# ------------------------
$global:lastState = ""
$securePass = ConvertTo-SecureString $pass -AsPlainText -Force
$cred = New-Object System.Management.Automation.PSCredential($user, $securePass)

Write-Host "[$(Get-Date -Format HH:mm:ss)] Status monitor started."

# ------------------------
# Idle Detection
# ------------------------
Add-Type @"
using System;
using System.Runtime.InteropServices;

public class IdleTime {
    [StructLayout(LayoutKind.Sequential)]
    struct LASTINPUTINFO {
        public uint cbSize;
        public uint dwTime;
    }

    [DllImport("user32.dll")]
    static extern bool GetLastInputInfo(ref LASTINPUTINFO plii);

    public static uint GetIdleTime() {
        LASTINPUTINFO lii = new LASTINPUTINFO();
        lii.cbSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf(lii);
        GetLastInputInfo(ref lii);
        return ((uint)Environment.TickCount - lii.dwTime);
    }
}
"@

function Get-IdleSeconds { return [IdleTime]::GetIdleTime() / 1000 }

# ------------------------
# Tray Icon Setup
# ------------------------
$notify = New-Object System.Windows.Forms.NotifyIcon
$notify.Visible = $true
$notify.Text = "Status Monitor"

$iconAvailable = [System.Drawing.SystemIcons]::Information
$iconCall = [System.Drawing.SystemIcons]::Error
$iconAway = [System.Drawing.SystemIcons]::Application
$iconOffline = [System.Drawing.SystemIcons]::Warning

# ------------------------
# Status Functions
# ------------------------
function Send-Status($state) {
    if ($state -ne $global:lastState) {
        Write-Host "[$(Get-Date -Format HH:mm:ss)] Sending status: $state"
        try {
            if ($BasicAuth) {
                # Send request with credentials if Basic Auth is enabled
                Invoke-WebRequest "$server/$state" -Credential $cred -UseBasicParsing -TimeoutSec 2
            } else {
                # Send request without credentials if Basic Auth is disabled
                Invoke-WebRequest "$server/$state" -UseBasicParsing -TimeoutSec 2
            }
        } catch {
            Write-Host ("[{0}] ERROR sending status {1}: {2}" -f (Get-Date -Format "HH:mm:ss"), $state, $_)
        }
        Update-Icon $state
        $global:lastState = $state
    }
}

function Update-Icon($status) {
    switch ($status) {
        "av"       { $notify.Icon = $iconAvailable; $notify.Text = "Status: Available" }
        "in_call"  { $notify.Icon = $iconCall;      $notify.Text = "Status: In Call" }
        "away"     { $notify.Icon = $iconAway;      $notify.Text = "Status: Away" }
        "offline"  { $notify.Icon = $iconOffline;   $notify.Text = "Status: Offline" }
    }
    Write-Host "[$(Get-Date -Format HH:mm:ss)] Tray icon updated: $status"
}

# ------------------------
# System Checks
# ------------------------
function Is-Locked { return (Get-Process -Name LogonUI -ErrorAction SilentlyContinue) -ne $null }

$micPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone"
function Is-MicrophoneActive {
    $active = $false
    Get-ChildItem $micPath | ForEach-Object {
        $props = Get-ItemProperty $_.PSPath
        if ($props.LastUsedTimeStop -eq 0) { $active = $true }
    }
    return $active
}

# ------------------------
# Status Logic
# ------------------------
function Check-Status {
    if (Is-Locked) { Send-Status "away"; return }
    if ((Get-IdleSeconds) -gt $idleLimit) { Send-Status "away"; return }
    if (Is-MicrophoneActive) { Send-Status "in_call"; return }
    Send-Status "av"
}

# ------------------------
# Tray Menu
# ------------------------
$menu = New-Object System.Windows.Forms.ContextMenu
$itemAvailable = New-Object System.Windows.Forms.MenuItem "Available"
$itemCall      = New-Object System.Windows.Forms.MenuItem "In Call"
$itemAway      = New-Object System.Windows.Forms.MenuItem "Away"
$itemOffline   = New-Object System.Windows.Forms.MenuItem "Offline"
$exit          = New-Object System.Windows.Forms.MenuItem "Exit"

$itemAvailable.add_Click({ Write-Host "[$(Get-Date -Format HH:mm:ss)] Menu: Available clicked"; Send-Status "av" })
$itemCall.add_Click({ Write-Host "[$(Get-Date -Format HH:mm:ss)] Menu: In Call clicked"; Send-Status "in_call" })
$itemAway.add_Click({ Write-Host "[$(Get-Date -Format HH:mm:ss)] Menu: Away clicked"; Send-Status "away" })
$itemOffline.add_Click({ Write-Host "[$(Get-Date -Format HH:mm:ss)] Menu: Offline clicked"; Send-Status "offline" })

$exit.add_Click({
    Write-Host "[$(Get-Date -Format HH:mm:ss)] Exit selected."
    Send-Status "offline"
    $notify.Visible = $false
    [System.Windows.Forms.Application]::Exit()
})

$menu.MenuItems.Add($itemAvailable)
$menu.MenuItems.Add($itemCall)
$menu.MenuItems.Add($itemAway)
$menu.MenuItems.Add($itemOffline)
$menu.MenuItems.Add("-")
$menu.MenuItems.Add($exit)
$notify.ContextMenu = $menu

# ------------------------
# Timer
# ------------------------
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = $checkInterval
$timer.Add_Tick({ Check-Status })
$timer.Start()

# ------------------------
# Invisible Form for Shutdown/Logout Detection
# ------------------------
$closingForm = New-Object System.Windows.Forms.Form
$closingForm.WindowState = 'Minimized'
$closingForm.ShowInTaskbar = $false
$closingForm.Add_FormClosing({
    param($sender, $e)
    Write-Host "[$(Get-Date -Format HH:mm:ss)] FormClosing Event (Shutdown/Logout detected)"
    Send-Status "offline"
    # Optional Icon Update
    $notify.Icon = [System.Drawing.SystemIcons]::Warning
    $notify.Text = "Status: Offline"
    Start-Sleep -Milliseconds 500
})
$closingForm.Show() # Form remains invisible, only for event

# ------------------------
# Initial Status
# ------------------------
Update-Icon "offline"

# ------------------------
# Minimize Window
# ------------------------
$hwnd = (Get-Process -Id $PID).MainWindowHandle
if ($hwnd -ne 0) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;

public class Win32 {
[DllImport("user32.dll")]
public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);
}
"@
    [Win32]::ShowWindowAsync($hwnd,2)
}

# ------------------------
# Start Tray App
# ------------------------
Write-Host "[$(Get-Date -Format HH:mm:ss)] Tray application running."
[System.Windows.Forms.Application]::Run()