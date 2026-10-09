param(
    [Parameter(Mandatory = $true)]
    [string]$CertificatePath,

    [Parameter(Mandatory = $true)]
    [string]$CatalogPath,

    [Parameter(Mandatory = $true)]
    [string]$InfPath,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [switch]$PreserveExistingBinding
)

$ErrorActionPreference = 'Stop'
$expectedThumbprint = '05EDA7260EFC3557700B02F8E4F70823E99A6DDD'
$expectedCertificateHash = '39CE00A770AB28C9DE39DFD92EEDECB1BA342A7BA21AE9A78E7D242934AF150E'
$expectedCatalogHash = '5FF62B27A436DF0AB2D649458CC93839E24D57E3B944DCB35E0FB068A59DF85E'
$expectedInfHash = '57FC8C322A7EA96BDAB5DB98E770ACA4A2BD6828FCDE79A857971BD1B38BA14E'
$hardwareId = 'USB\VID_0582&PID_00DA'
$rootAdded = $false
$publisherAdded = $false
$failureExitCode = 50
$driverActionStarted = $false

function Write-InstallLog {
    param([string]$Message)
    Add-Content -LiteralPath $LogPath -Value ("{0:u} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

function Get-ExactCertificate {
    param([string]$StoreName)
    Get-ChildItem -LiteralPath ("Cert:\LocalMachine\{0}" -f $StoreName) |
        Where-Object { $_.Thumbprint -eq $expectedThumbprint } |
        Select-Object -First 1
}

function Remove-NewTrust {
    foreach ($entry in @(
        @{ Store = 'TrustedPublisher'; Added = $publisherAdded },
        @{ Store = 'Root'; Added = $rootAdded }
    )) {
        if ($entry.Added) {
            $path = "Cert:\LocalMachine\$($entry.Store)\$expectedThumbprint"
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Force
                Write-InstallLog ("ROLLBACK: newly added certificate removed from LocalMachine\{0}." -f $entry.Store)
            }
        }
    }
}

function Invoke-LoggedNative {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-InstallLog ("COMMAND: {0} {1}" -f $FilePath, ($Arguments -join ' '))
    # Windows PowerShell 5.1 turns redirected native stderr into ErrorRecords.
    # Capture those records and the exit code before applying failure policy.
    $savedErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $FilePath @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    foreach ($line in $output) {
        Write-InstallLog ([string]$line)
    }
    $hexCode = '0x' + ([BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exitCode), 0)).ToString('X8')
    Write-InstallLog ("EXIT CODE: {0}; HEX={1}" -f $exitCode, $hexCode)
    if ($hexCode -eq '0x800B0109' -or ($output -join ' ') -match '800B0109|CERT_E_UNTRUSTEDROOT') {
        Write-Result 'CERT_E_UNTRUSTEDROOT'
        throw 'CERT_E_UNTRUSTEDROOT (0x800B0109): certificate chain is not trusted; no security bypass or retry.'
    }
    return $exitCode
}

function Get-Gt10Devices {
    $devices = @(Get-CimInstance Win32_PnPEntity -ErrorAction Stop | Where-Object {
        $_.Present -and @($_.HardwareID) -contains $hardwareId
    })
    $cimError = ''
    try { $drivers = @(Get-CimInstance Win32_PnPSignedDriver -ErrorAction Stop) }
    catch { $drivers = @(); $cimError = $_.Exception.Message }
    foreach ($device in $devices) {
        $driver = @($drivers | Where-Object { $_.DeviceID -eq $device.PNPDeviceID })
        $cimInf = if ($driver.Count -eq 1) { [string]$driver[0].InfName } else { '' }
        $pnp = @{}
        $pnpDevice = $null
        $pnpError = ''
        try {
            $pnpDevice = Get-PnpDevice -InstanceId $device.PNPDeviceID -ErrorAction Stop
            if ($pnpDevice.InstanceId -ne $device.PNPDeviceID) { throw 'PnP device identity mismatch.' }
            $keys = @('DriverInfPath', 'DriverProvider', 'DriverVersion', 'DriverDate', 'Service', 'ProblemCode') |
                ForEach-Object { 'DEVPKEY_Device_' + $_ }
            $properties = @(Get-PnpDeviceProperty -InstanceId $device.PNPDeviceID -KeyName $keys -ErrorAction Stop)
            foreach ($property in $properties) {
                if ($property.InstanceId -ne $device.PNPDeviceID) { throw 'PnP property identity mismatch.' }
                $pnp[$property.KeyName] = $property.Data
            }
        } catch {
            $pnp = @{}; $pnpError = $_.Exception.Message
        }
        # Current device properties are authoritative; never fall back to cached CIM metadata.
        $inf = [string]$pnp['DEVPKEY_Device_DriverInfPath']
        $infHash = ''
        if ($inf -match '^oem\d+\.inf$') {
            $publishedInf = Join-Path $env:SystemRoot ('INF\' + $inf)
            if (Test-Path -LiteralPath $publishedInf) {
                $infHash = (Get-FileHash -LiteralPath $publishedInf -Algorithm SHA256).Hash
            }
        }
        [pscustomobject]@{
            InstanceId = $device.PNPDeviceID; HardwareIDs = @($device.HardwareID)
            CompatibleIDs = @($device.CompatibleID); Service = $pnp['DEVPKEY_Device_Service']
            Provider = $pnp['DEVPKEY_Device_DriverProvider']
            DriverVersion = $pnp['DEVPKEY_Device_DriverVersion']
            DriverDate = $pnp['DEVPKEY_Device_DriverDate']
            INF = $inf; InfHash = $infHash; Status = $pnpDevice.Status
            ProblemCode = $pnp['DEVPKEY_Device_ProblemCode']; Class = $device.PNPClass
            CurrentPnpInf = $inf; CimReportedInf = $cimInf
            DriverMetadataDivergence = [bool]($inf -and $cimInf -and $inf -ine $cimInf)
            ExpectedInfHash = $expectedInfHash; PnpQueryError = $pnpError; CimQueryError = $cimError
        }
    }
}

function Get-BindingState {
    param([object[]]$Devices, [bool]$PackagePresent = $false)
    if ($Devices.Count -eq 0) { return 'GT10_NOT_PRESENT' }
    if ($Devices.Count -ne 1) { return 'GT10_AMBIGUOUS' }
    $device = $Devices[0]
    if (-not $device.INF) { return 'CURRENT_DRIVER_INF_UNRESOLVED' }
    if ($null -eq $device.ProblemCode -or $device.ProblemCode -ne 0 -or $device.Status -ne 'OK') {
        return 'GT10_BROKEN_BINDING'
    }
    if ($device.Service -ieq 'WINUSB' -and $device.Provider -eq 'GT LAB' -and
        $device.INF -match '^oem\d+\.inf$' -and $device.InfHash -eq $expectedInfHash -and
        $device.DriverVersion -eq '1.0.0.0') { return 'WINUSB_BOUND_OK' }
    if ($device.Provider -match '^Roland(?: Corporation)?$' -and $device.Service) { return 'ROLAND_DRIVER_OK' }
    if ($PackagePresent -or $device.Service -ieq 'WINUSB') { return 'WINUSB_PRESENT_BUT_NOT_BOUND_TO_GT10' }
    return 'GT10_UNKNOWN_DRIVER'
}

function Write-DeviceState {
    param([string]$Stage)
    $devices = @(Get-Gt10Devices)
    Write-InstallLog ("{0}: GT10_COUNT={1}" -f $Stage, $devices.Count)
    foreach ($device in $devices) {
        Write-InstallLog ("{0}: {1}" -f $Stage, ($device | ConvertTo-Json -Compress -Depth 4))
    }
    Write-InstallLog ("{0}: FinalClassification={1}" -f $Stage, (Get-BindingState $devices))
    return $devices
}

function Write-Result {
    param([string]$State)
    Write-InstallLog ("FINAL_STATUS={0}" -f $State)
    Set-Content -LiteralPath ($LogPath + '.ini') -Encoding Unicode -Value @('[WinUSB]', ('State=' + $State))
}

$logDirectory = Split-Path -Parent $LogPath
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Set-Content -LiteralPath $LogPath -Value 'GT Lab Editor Universal WinUSB installation log' -Encoding UTF8

Write-Result 'DRIVER_INSTALL_FAILED'
$os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
$osVersion = [version]$os.Version
Write-InstallLog ("WINDOWS_EDITION={0}; ARCHITECTURE={1}" -f $os.Caption, $os.OSArchitecture)
Write-InstallLog ("HELPER_PATH={0}" -f $PSCommandPath)
Write-InstallLog ("POWERSHELL_VERSION={0}; EDITION={1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-InstallLog ("WINDOWS_VERSION={0}; BUILD={1}; IS_64_BIT={2}" -f $osVersion, $osVersion.Build, [Environment]::Is64BitOperatingSystem)
if ($osVersion.Build -lt 22000) {
    Write-InstallLog 'ERROR: helper refused to run because this is not Windows 11 (build < 22000).'
    exit 2
}

try {
    $files = @(
        @{ Path = $CertificatePath; Expected = $expectedCertificateHash; Label = 'CER' },
        @{ Path = $CatalogPath; Expected = $expectedCatalogHash; Label = 'CAT' },
        @{ Path = $InfPath; Expected = $expectedInfHash; Label = 'INF' }
    )
    foreach ($file in $files) {
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.Path).Hash
        Write-InstallLog ("{0} SHA256={1}" -f $file.Label, $actual)
        if ($actual -ne $file.Expected) {
            throw "The $($file.Label) hash does not match the audited R4-B package."
        }
    }

    $certificate = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($CertificatePath)
    if ($certificate.Thumbprint -ne $expectedThumbprint) {
        throw "The certificate thumbprint does not match the expected value: $($certificate.Thumbprint)"
    }
    if ($certificate.HasPrivateKey) {
        throw 'The packaged certificate unexpectedly contains a private key.'
    }
    Write-InstallLog ("CERTIFICATE_VALIDATED: Subject={0}; Thumbprint={1}; HasPrivateKey=False" -f
        $certificate.Subject, $certificate.Thumbprint)

    $rootPreexisting = $null -ne (Get-ExactCertificate 'Root')
    $publisherPreexisting = $null -ne (Get-ExactCertificate 'TrustedPublisher')
    Write-InstallLog ("CERTIFICATE_PREEXISTING: Root={0}; TrustedPublisher={1}" -f $rootPreexisting, $publisherPreexisting)

    $devicesBefore = @(Write-DeviceState 'BEFORE')
    $stateBefore = Get-BindingState $devicesBefore
    if ($stateBefore -notin @('WINUSB_BOUND_OK', 'GT10_NOT_PRESENT', 'GT10_AMBIGUOUS')) {
        $existingPackages = @(Get-WindowsDriver -Online -All | Where-Object {
            (Split-Path -Leaf $_.OriginalFileName) -ieq 'gt10-winusb.inf' -and $_.ProviderName -eq 'GT LAB'
        })
        $stateBefore = Get-BindingState $devicesBefore ($existingPackages.Count -gt 0)
        Write-InstallLog ('PACKAGE_COUNT=' + $existingPackages.Count)
    }
    Write-InstallLog ('BEFORE_STATE=' + $stateBefore)
    if ($PreserveExistingBinding -and $stateBefore -ne 'WINUSB_BOUND_OK') {
        Write-Result $stateBefore
        Write-InstallLog 'PRESERVE_EXISTING_BINDING=YES; unexpected state; no driver/certificate action permitted.'
        exit 40
    }
    if ($stateBefore -in @('GT10_NOT_PRESENT', 'GT10_AMBIGUOUS')) {
        Write-InstallLog 'DRIVER_ACTION_REQUIRED=NOT_ATTEMPTED; connect exactly one GT-10 before configuring WinUSB.'
        Write-Result $stateBefore
        exit 11
    }
    if ($stateBefore -eq 'WINUSB_BOUND_OK') {
        Write-InstallLog 'DRIVER_ACTION_REQUIRED=NO; PNPUTIL=SKIPPED; CERTIFICATE_IMPORT=SKIPPED; preserving existing binding and stores.'
        $signature = Get-AuthenticodeSignature -FilePath $CatalogPath
        Write-InstallLog ("CAT_SIGNATURE: Status={0}; StatusMessage={1}" -f $signature.Status, $signature.StatusMessage)
        if (-not $rootPreexisting -or -not $publisherPreexisting -or $signature.Status -ne 'Valid' -or
            $signature.SignerCertificate.Thumbprint -ne $expectedThumbprint) {
            Write-Result 'CERTIFICATE_NOT_TRUSTED'
            exit 20
        }
        $devicesAfter = @(Write-DeviceState 'AFTER')
        if ((Get-BindingState $devicesAfter) -ne 'WINUSB_BOUND_OK' -or
            $devicesBefore[0].InstanceId -ne $devicesAfter[0].InstanceId -or
            $devicesBefore[0].INF -ne $devicesAfter[0].INF) {
            Write-Result 'GT10_BROKEN_BINDING'
            exit 40
        }
        Write-InstallLog 'BINDING_RESULT=PASS; DRIVER_PRESERVED=YES; CERTIFICATES_PRESERVED=YES'
        Write-Result 'WINUSB_PRESERVED'
        exit 0
    }
    Write-InstallLog 'DRIVER_ACTION_REQUIRED=YES; explicit installer consent required; no forced driver removal.'

    $failureExitCode = 10
    $certutil = Join-Path $env:SystemRoot 'System32\certutil.exe'
    if (-not $rootPreexisting) {
        if ((Invoke-LoggedNative $certutil @('-f', '-addstore', 'Root', $CertificatePath)) -ne 0) {
            throw 'Could not add the audited certificate to LocalMachine\Root.'
        }
        $rootAdded = $true
    }
    if (-not $publisherPreexisting) {
        if ((Invoke-LoggedNative $certutil @('-f', '-addstore', 'TrustedPublisher', $CertificatePath)) -ne 0) {
            throw 'Could not add the audited certificate to LocalMachine\TrustedPublisher.'
        }
        $publisherAdded = $true
    }
    Write-InstallLog ("CERT_RESULT=PASS; ADDED_ROOT={0}; ADDED_TRUSTED_PUBLISHER={1}" -f $rootAdded, $publisherAdded)

    $failureExitCode = 20
    $catalogSignature = Get-AuthenticodeSignature -FilePath $CatalogPath
    $catalogSignerThumbprint = if ($catalogSignature.SignerCertificate) { $catalogSignature.SignerCertificate.Thumbprint } else { '' }
    Write-InstallLog ("CAT_SIGNATURE: Status={0}; SignerThumbprint={1}; StatusMessage={2}" -f $catalogSignature.Status, $catalogSignerThumbprint, $catalogSignature.StatusMessage)
    if (($catalogSignature.Status -ne 'Valid') -or ($catalogSignerThumbprint -ne $expectedThumbprint)) {
        Write-Result 'CERTIFICATE_NOT_TRUSTED'
        throw 'The WinUSB catalog was not validated as trusted with the audited signer. See CAT_SIGNATURE StatusMessage; CERT_E_UNTRUSTEDROOT is 0x800B0109.'
    }
    Write-InstallLog 'CAT_RESULT=PASS'

    $failureExitCode = 30
    # Recheck identity immediately before the one permitted driver action.
    $currentDevices = @(Write-DeviceState 'PRE_ACTION')
    if ($currentDevices.Count -ne 1 -or $currentDevices[0].InstanceId -ne $devicesBefore[0].InstanceId) {
        Write-Result 'GT10_NOT_PRESENT'
        throw 'GT-10 identity changed before driver action; stopped without pnputil.'
    }
    $pnputil = Join-Path $env:SystemRoot 'System32\pnputil.exe'
    $driverActionStarted = $true
    $driverResult = Invoke-LoggedNative $pnputil @('/add-driver', $InfPath, '/install')
    if (($driverResult -ne 0) -and ($driverResult -ne 3010)) {
        throw "pnputil failed with exit code $driverResult."
    }
    Write-InstallLog ("INF_COMMAND_ACCEPTED; PNPUTIL_EXIT_CODE={0}; RETRY_COUNT=0" -f $driverResult)

    $failureExitCode = 40
    Start-Sleep -Seconds 2
    $devicesAfter = @(Write-DeviceState 'AFTER_REENUMERATION')
    $stateAfter = Get-BindingState $devicesAfter $true
    Write-InstallLog ('POST_BIND_STATE=' + $stateAfter)
    if ($stateAfter -ne 'WINUSB_BOUND_OK' -or $devicesAfter[0].InstanceId -ne $devicesBefore[0].InstanceId) {
        Write-Result $stateAfter
        Write-InstallLog 'BINDING_RESULT=FAIL; reboot/reconnect may be needed; no automatic retry and no forced rebind.'
        # Keep newly established trust once the driver action was accepted.
        # Removing it here could leave an installed package without its trust.
        exit 40
    }
    Write-InstallLog 'BINDING_RESULT=PASS; exact GT-10 provider/service/INF/status/problem code verified.'
    if ($driverResult -eq 3010) {
        Write-Result 'WINUSB_BOUND_REBOOT_REQUIRED'
        exit 3010
    }
    Write-Result 'WINUSB_BOUND_OK'

    Write-InstallLog 'WinUSB setup completed without changing HVCI, Secure Boot, or boot policies.'
    exit 0
} catch {
    Write-InstallLog ("ERROR_TYPE={0}; ERROR_MESSAGE={1}" -f $_.Exception.GetType().FullName, $_.Exception.Message)
    try { if (-not $driverActionStarted) { Remove-NewTrust } else { Write-InstallLog 'TRUST_PRESERVED_AFTER_DRIVER_ACTION: inspect native output before any cleanup.' } } catch { Write-InstallLog ("ROLLBACK_ERROR_TYPE={0}; ROLLBACK_ERROR_MESSAGE={1}" -f $_.Exception.GetType().FullName, $_.Exception.Message) }
    Write-InstallLog ("HELPER_EXIT_CODE={0}" -f $failureExitCode)
    exit $failureExitCode
}

