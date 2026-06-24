/**
 * Mock data — used for debugging the UI
 * Modify these values to test different display states
 */

const mockData = {
    root: {
        // Device status
        operationalMode: 'LAN Mode',
        connectedEther: 'Yes',
        connectedSocketStatus: 'Connected (3 clients)',
        wifiEnabled: 'No',
        wifiConnected: 'Disconnected',
        wifiModeAP: 'No',
        wifiModeAPStatus: 'Disabled',
        uptime: '5 days 12:34:56',
        connectedSocket: '2 days 08:15:23',
        
        // Device information
        hwRev: 'AVATTO-01',
        VERSION: 'v0.2.6',
        espModel: 'ESP32-SOLO1',
        deviceTemp: '42.5',
        espCores: '1',
        espFreq: '160',
        espFlashSize: '4',
        espFlashType: 'Internal',
        espHeapFree: '128',
        espHeapSize: '320',
        zigbeeHwRev: 'CC2652P',
        zigbeeFwRev: '20221226',
        
        // Ethernet
        ethConnection: 'Connected',
        ethDhcp: 'Enabled',
        ethIp: '192.168.1.100',
        etchMask: '255.255.255.0',
        ethGate: '192.168.1.1',
        ethSpd: '100 Mbps',
        ethMac: '24:6F:28:12:34:56',
        
        // WiFi
        wifiMode: 'Disabled',
        wifiSsid: 'N/A',
        wifiIp: 'N/A',
        wifiMask: 'N/A',
        wifiGate: 'N/A',
        wifiRssi: 'N/A',
        wifiMac: '24:6F:28:12:34:57',
        
        // VPN
        wgInit: 'Yes',
        wgDeviceAddr: '10.0.0.2',
        wgRemoteAddr: '12.34.56.78:51820'
    },
    
    general: {
        checkedLanMode: 'checked',
        checkedWifiMode: '',
        checkedUsbMode: '',
        keepWeb: 'checked',
        checkedDisableLedPwr: '',
        checkedDisableLedUSB: ''
    },
    
    ethernet: {
        ethDhcp: 'checked',
        ethIp: '192.168.1.100',
        ethMask: '255.255.255.0',
        ethGate: '192.168.1.1',
        ethDns: '8.8.8.8'
    },
    
    wifi: {
        wifiEnabled: 'checked',
        wifiSsid: 'MyNetwork',
        wifiPassword: '********',
        wifiDhcp: 'checked',
        wifiIp: '192.168.1.101',
        wifiMask: '255.255.255.0',
        wifiGate: '192.168.1.1'
    },
    
    serial: {
        '115200': 'selected',  // Baud rate selection
        socketPort: '9999',
        generatedFile: `# Zigbee2MQTT configuration example
serial:
  port: tcp://192.168.1.100:9999
  adapter: auto
  baudrate: 115200

# Optional settings
advanced:
  log_level: info
  network_key: GENERATE
  pan_id: 6754`
    },
    
    security: {
        webAuth: 'checked',
        webUser: 'admin',
        webPass: '********',
        fwEnabled: '',
        fwIp: '192.168.1.50'
    },
    
    mqtt: {
        mqttEnabled: 'checked',
        mqttServer: 'mqtt.example.com',
        mqttPort: '1883',
        mqttUser: 'AVATTO',
        mqttPass: '********',
        mqttTopic: 'zigbee2mqtt',
        mqttInterval: '60',
        mqttDiscovery: 'checked'
    },
    
    wg: {
        wgEnabled: 'checked',
        wgLocalAddr: '10.0.0.2/24',
        wgLocalPrivKey: 'aBcDeFgHiJkLmNoPqRsTuVwXyZ0123456789abcd=',
        wgEndAddr: '12.34.56.78:51820',
        wgEndPubKey: 'ZyXwVuTsRqPoNmLkJiHgFeDcBa9876543210zyxwv='
    },
    
    systools: {
        VERSION: 'v0.2.6',
        latestVersion: 'v0.2.6',
        updateAvailable: false,
        hostname: 'AVATTO-01',
        refreshLogs: '3000',
        timeZones: [
            { value: 'UTC', text: 'UTC +0:00' },
            { value: 'CET-1CEST,M3.5.0,M10.5.0/3', text: 'Europe/Berlin (CET-1CEST) +1:00' },
            { value: 'GMT0BST,M3.5.0/1,M10.5.0', text: 'Europe/London (GMT0BST) +0:00' },
            { value: 'EST5EDT,M3.2.0,M11.1.0', text: 'America/New_York (EST5EDT) -5:00' },
            { value: 'PST8PDT,M3.2.0,M11.1.0', text: 'America/Los_Angeles (PST8PDT) -8:00' },
            { value: 'CST-8', text: 'Asia/Shanghai (CST-8) +8:00' }
        ]
    },
    
    about: {
        VERSION: 'v0.2.6',
        hwRev: 'AVATTO-01',
        espModel: 'ESP32-SOLO1',
        zigbeeHwRev: 'CC2652P'
    }
};

/**
 * Replace data placeholders in the page
 */
function populateData(page) {
    const data = mockData[page] || {};
    
    // Replace elements with data-replace attribute
    $('[data-replace]').each(function() {
        const key = $(this).attr('data-replace');
        
        if (data[key] !== undefined) {
            if ($(this).is('input[type="checkbox"]')) {
                $(this).prop('checked', data[key] === 'checked');
            } else if ($(this).is('input[type="radio"]')) {
                $(this).prop('checked', data[key] === 'checked');
            } else if ($(this).is('input')) {
                $(this).val(data[key]);
            } else if ($(this).is('select')) {
                // Handle dropdown selectors
                if (key === 'timeZones' && Array.isArray(data[key])) {
                    // Special handling for timezone selector
                    $(this).empty();
                    data[key].forEach(tz => {
                        $(this).append($('<option></option>')
                            .attr('value', tz.value)
                            .text(tz.text));
                    });
                } else {
                    $(this).val(data[key]);
                }
            } else if ($(this).is('option')) {
                // Handle option selection state
                if (data[key] === 'selected') {
                    $(this).prop('selected', true);
                }
            } else if ($(this).is('textarea')) {
                $(this).val(data[key]);
            } else {
                $(this).text(data[key]);
            }
        }
    });
}

/**
 * Mock functions (to avoid errors)
 */
function checkLatestESPrelease() {
    console.log('Mock: Checking latest ESP release...');
}

function modalConstructor(topic, data) {
    console.log('Mock: Opening modal for:', topic, data);
    
    const modalTitles = {
        'keepWeb': 'Keep Web Server',
        'flashZB': 'Flash Zigbee Module',
        'flashWarning': 'Flash Warning',
        'espFlashGitInfo': 'ESP32 Firmware Information'
    };
    
    const modalContents = {
        'keepWeb': 'When enabled, the web interface and network connection will remain active even in USB mode. This allows you to manage the device remotely.',
        'flashZB': 'This will put the Zigbee module into flash mode. You can then use tools like ZigStar Multi Tool to update the firmware.',
        'flashWarning': '⚠️ Warning: Flashing firmware may take several minutes. Do not power off the device during this process!',
        'espFlashGitInfo': data ? `${data.text}\n\n${data.chglog || ''}` : 'Firmware information loading...'
    };
    
    const title = modalTitles[topic] || 'Information';
    const content = modalContents[topic] || `Information about "${topic}".\n\nIn production, detailed help would be displayed here.`;
    
    alert(`📋 ${title}\n\n${content}`);
}

function KeepWebDsbl(disabled) {
    console.log('Mock: Keep Web toggle:', disabled);
    $('#keepWeb').prop('disabled', disabled);
}

/**
 * Mock config generator
 */
function generateConfig(type) {
    console.log('Mock: Generating config for:', type);
    
    const configs = {
        z2m: `# Zigbee2MQTT configuration
serial:
  port: tcp://192.168.1.100:9999
  adapter: auto
  baudrate: 115200

advanced:
  log_level: info
  network_key: GENERATE
  pan_id: 6754`,
        
        zha: `# ZHA configuration for Home Assistant
# Add this to your configuration.yaml:

zha:
  zigpy_config:
    ota:
      ikea_provider: true
  device_config:
    !input device_ieee
  database_path: /config/zigbee.db
  serial:
    port: socket://192.168.1.100:9999
    baudrate: 115200`,
        
        usb: `# USB Mode Configuration
# Device will appear as: /dev/ttyUSB0 or COM port
# 
# For Zigbee2MQTT:
serial:
  port: /dev/ttyUSB0
  baudrate: 115200
  
# For ZHA:
# Use the auto-discovery feature or manually specify /dev/ttyUSB0`
    };
    
    $('#generatedFile').val(configs[type] || configs.z2m);
}

/**
 * Copy code to clipboard (mock)
 */
function copyCode() {
    const code = $('#generatedFile').val();
    
    // Try Clipboard API
    if (navigator.clipboard) {
        navigator.clipboard.writeText(code).then(() => {
            alert('✅ Config copied to clipboard!');
        }).catch(() => {
            alert('⚠️ Copy failed — please select and copy manually');
        });
    } else {
        // Fallback
        $('#generatedFile').select();
        try {
            document.execCommand('copy');
            alert('✅ Config copied to clipboard!');
        } catch (err) {
            alert('⚠️ Copy failed — please select and copy manually');
        }
    }
    
    console.log('Mock: Copied config to clipboard');
}

/**
 * Initialize file browser (mock)
 */
function initFileBrowser() {
    console.log('Mock: Initializing file browser...');
    
    const mockFiles = [
        { name: '/config/system.json', size: '1.2 KB' },
        { name: '/config/configWifi.json', size: '856 B' },
        { name: '/config/configEther.json', size: '512 B' },
        { name: '/config/configGeneral.json', size: '1.5 KB' },
        { name: '/config/configSecurity.json', size: '432 B' },
        { name: '/config/configSerial.json', size: '256 B' },
        { name: '/config/configMqtt.json', size: '1.1 KB' },
        { name: '/config/configWg.json', size: '892 B' }
    ];
    
    const tbody = $('<tbody></tbody>');
    mockFiles.forEach(file => {
        const row = $('<tr></tr>')
            .css('cursor', 'pointer')
            .hover(
                function() { $(this).css('background-color', '#f8f9fa'); },
                function() { $(this).css('background-color', ''); }
            )
            .click(function() {
                loadMockFile(file.name);
            });
        
        row.append($('<td></td>').text(file.name));
        row.append($('<td></td>').text(file.size));
        tbody.append(row);
    });
    
    $('#filelist tbody').remove();
    $('#filelist').append(tbody);
}

/**
 * Load mock file content
 */
function loadMockFile(filename) {
    console.log('Mock: Loading file:', filename);
    
    const mockFileContents = {
        '/config/system.json': JSON.stringify({
            hostname: 'AVATTO-01',
            refreshLogs: 3000,
            timezone: 'UTC'
        }, null, 2),
        '/config/configWifi.json': JSON.stringify({
            ssid: 'MyNetwork',
            dhcp: true,
            ip: '192.168.1.101'
        }, null, 2),
        '/config/configEther.json': JSON.stringify({
            dhcp: true,
            ip: '192.168.1.100'
        }, null, 2)
    };
    
    const content = mockFileContents[filename] || `{\n  "mock": "data",\n  "file": "${filename}"\n}`;
    
    $('#filename').val(filename);
    $('#title').text(filename);
    $('#config_file').val(content);
    
    alert(`📄 Loaded file: ${filename}\n\nIn production, you can edit and save this file.`);
}

/**
 * Initialize debug console (mock)
 */
function initDebugConsole() {
    console.log('Mock: Initializing debug console...');
    
    const mockConsoleData = `[2026-01-19 10:30:15] System started
[2026-01-19 10:30:16] Ethernet connected: 192.168.1.100
[2026-01-19 10:30:17] Zigbee module initialized
[2026-01-19 10:30:18] MQTT connected: mqtt.example.com:1883
[2026-01-19 10:30:19] WebServer started on port 80
[2026-01-19 10:30:20] System ready
`;
    
    $('#console').val(mockConsoleData);
    
    // Simulate real-time log updates
    let logCounter = 0;
    setInterval(() => {
        if ($('#console').length) {
            const now = new Date();
            const timestamp = now.toISOString().replace('T', ' ').substring(0, 19);
            const newLog = `[${timestamp}] Heartbeat #${++logCounter}\n`;
            const currentVal = $('#console').val();
            const lines = currentVal.split('\n');
            
            // Keep at most 20 lines
            if (lines.length > 20) {
                lines.shift();
            }
            
            $('#console').val(lines.join('\n') + newLog);
            
            // Auto-scroll to bottom
            const textarea = document.getElementById('console');
            if (textarea) {
                textarea.scrollTop = textarea.scrollHeight;
            }
        }
    }, 5000);
}

/**
 * Mock ESP32 firmware update events
 */
function ESPfwStartEvents() {
    console.log('Mock: ESP32 firmware update started');
    $('#prg').html('Upload starting...');
    
    let progress = 0;
    const interval = setInterval(() => {
        progress += 10;
        $('#prg').html('Upload progress: ' + progress + '%');
        $('#bar').css('width', progress + '%');
        
        if (progress >= 100) {
            clearInterval(interval);
            $('#prg').html('✅ Update completed! Rebooting...');
            setTimeout(() => {
                alert('ESP32 firmware update complete!\n\nIn production, the device would reboot.');
            }, 1000);
        }
    }, 500);
}

/**
 * Mock Zigbee firmware update events
 */
function ZBfwStartEvents() {
    console.log('Mock: Zigbee firmware update started');
    $('#prg_zb').html('Starting Zigbee flash...');
}

/**
 * Mock waiting for firmware flash
 */
function espFlashGitWait() {
    console.log('Mock: Downloading firmware from GitHub...');
    $('#prg').html('Downloading from GitHub...');
    
    let progress = 0;
    const interval = setInterval(() => {
        progress += 5;
        $('#prg').html('Downloading: ' + progress + '%');
        $('#bar').css('width', progress + '%');
        
        if (progress >= 100) {
            clearInterval(interval);
            $('#prg').html('Download complete! Flashing...');
            setTimeout(() => {
                ESPfwStartEvents();
            }, 1000);
        }
    }, 300);
}

/**
 * Process responses (mock)
 */
async function processResponses() {
    return {
        textData: '0.2.6',
        jsonData: {
            tag_name: 'v0.2.6',
            body: '## What\'s New\n\n- Bug fixes\n- Performance improvements\n- New features',
            assets: [
                { download_count: 1234 },
                { download_count: 5678 }
            ]
        }
    };
}

/**
 * Mock ESP32 file selection
 */
function sub(obj) {
    let fileName = obj.value.split('\\');
    if (fileName[fileName.length - 1] !== "") {
        $("#updButton").removeAttr("disabled");
        document.getElementById('file-input').innerHTML = '   📄 ' + fileName[fileName.length - 1];
        console.log('Mock: Selected ESP32 file:', fileName[fileName.length - 1]);
    } else {
        $("#updButton").attr("disabled", "disabled");
    }
}

/**
 * Mock Zigbee file selection
 */
function sub_zb(obj) {
    let fileName = obj.value.split('\\');
    if (fileName[fileName.length - 1] !== "") {
        $("#updButton_zb").removeAttr("disabled");
        document.getElementById('file-input_zb').innerHTML = '   📄 ' + fileName[fileName.length - 1];
        console.log('Mock: Selected Zigbee file:', fileName[fileName.length - 1]);
    } else {
        $("#updButton_zb").attr("disabled", "disabled");
    }
}

/**
 * Mock reboot wait
 */
function rebootWait() {
    console.log('Mock: Device rebooting...');
    alert('🔄 Device rebooting...\n\nIn production, the page would auto-refresh after reboot.');
}
