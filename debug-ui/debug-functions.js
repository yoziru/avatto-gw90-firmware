/**
 * Debug functions — handles page switching and interaction
 */

// Page content mapping
const pageContents = {
    root: '../src/websrc/html/PAGE_ROOT.html',
    general: '../src/websrc/html/PAGE_GENERAL.html',
    ethernet: '../src/websrc/html/PAGE_ETHERNET.html',
    wifi: '../src/websrc/html/PAGE_WIFI.html',
    serial: '../src/websrc/html/PAGE_SERIAL.html',
    security: '../src/websrc/html/PAGE_SECURITY.html',
    mqtt: '../src/websrc/html/PAGE_MQTT.html',
    wg: '../src/websrc/html/PAGE_WG.html',
    systools: '../src/websrc/html/PAGE_SYSTOOLS.html',
    about: '../src/websrc/html/PAGE_ABOUT.html'
};

/**
 * Load page content
 */
function loadPage(pageName) {
    const pageUrl = pageContents[pageName];
    
    if (!pageUrl) {
        $('#content-area').html('<div class="alert alert-danger">Page not found</div>');
        return;
    }
    
    // Load HTML content
    $.get(pageUrl, function(data) {
        $('#content-area').html(data);
        
        // Fill mock data
        populateData(pageName);
        
        // Initialize Masonry (if page uses it)
        // Note: for Masonry inside tabs, initialized separately on tab switch
        if (pageName !== 'systools' && $('.masonry').length > 0) {
            setTimeout(function() {
                $('.masonry').masonry({
                    percentPosition: true,
                    itemSelector: '.col-sm-12, .col-md-6'
                });
            }, 100);
        }
        
        // Intercept form submission (prevent actual submit)
        $('form').on('submit', function(e) {
            e.preventDefault();
            const formData = $(this).serializeArray();
            console.log('Form submitted (mock):', formData);
            alert('Form data logged to console\n\nIn production, this data would be saved to the device.');
        });
        
        // Intercept button clicks
        $('button[data-cmd]').on('click', function() {
            const cmd = $(this).attr('data-cmd');
            console.log('Command triggered:', cmd);
            alert('Command ' + cmd + ' triggered\n\nIn production, this would control the device\'s LED or other functions.');
        });
        
        // Initialize config generator (Serial page)
        if (pageName === 'serial') {
            setTimeout(() => {
                if (typeof generateConfig === 'function') {
                    generateConfig('z2m'); // default to Z2M config
                }
            }, 200);
        }
        
        // Initialize System Tools tab functionality
        if (pageName === 'systools') {
            setTimeout(() => {
                // Ensure Bootstrap tabs work correctly
                initSystemToolsTabs();
            }, 200);
        }
        
        console.log('Loaded page:', pageName);
    }).fail(function() {
        $('#content-area').html('<div class="alert alert-danger">Failed to load page content</div>');
    });
}

/**
 * Update navigation active state
 */
function updateNavigation(pageName) {
    $('.nav-link').removeClass('active');
    $('.nav-link[data-page="' + pageName + '"]').addClass('active');
    $('#pageSelect').val(pageName);
    
    // Update page title
    const pageTitles = {
        root: 'Dashboard',
        general: 'General Settings',
        ethernet: 'Ethernet Configuration',
        wifi: 'WiFi Configuration',
        serial: 'Serial / Z2M and ZHA',
        security: 'Security Settings',
        mqtt: 'MQTT Configuration',
        wg: 'WireGuard VPN',
        systools: 'System and Tools',
        about: 'About'
    };
    
    $('#pagenamePC').text(pageTitles[pageName] || 'AVATTO-GW90-Ti');
}

/**
 * Initialize
 */
$(document).ready(function() {
    console.log('Debug UI initialized');
    
    // Load Dashboard by default
    loadPage('root');
    
    // Dropdown selector switches page
    $('#pageSelect').on('change', function() {
        const pageName = $(this).val();
        loadPage(pageName);
        updateNavigation(pageName);
    });
    
    // Sidebar navigation click
    $('.nav-link').on('click', function(e) {
        e.preventDefault();
        const pageName = $(this).attr('data-page');
        loadPage(pageName);
        updateNavigation(pageName);
    });
    
    // Debug keyboard shortcuts
    $(document).on('keydown', function(e) {
        // Ctrl + R: reload current page
        if (e.ctrlKey && e.key === 'r') {
            e.preventDefault();
            const currentPage = $('#pageSelect').val();
            console.log('Reloading page:', currentPage);
            loadPage(currentPage);
        }
        
        // Ctrl + D: open browser devtools hint
        if (e.ctrlKey && e.key === 'd') {
            e.preventDefault();
            console.log('=== Debug Info ===');
            console.log('Current page:', $('#pageSelect').val());
            console.log('Mock data:', mockData);
            console.log('==================');
        }
    });
    
    console.log('Tips:');
    console.log('- Use the top dropdown or sidebar to switch pages');
    console.log('- Press Ctrl+R to reload current page');
    console.log('- Press Ctrl+D to view debug info');
    console.log('- Edit mock-data.js to change displayed data');
    console.log('- Test new styles in index.html\'s <style> tag');
});

/**
 * Initialize System Tools page tab functionality
 */
function initSystemToolsTabs() {
    console.log('Initializing System Tools tabs...');
    
    // Ensure tab buttons are clickable
    const tabButtons = document.querySelectorAll('[data-bs-toggle="tab"]');
    tabButtons.forEach(button => {
        button.addEventListener('click', function(e) {
            e.preventDefault();
            
            // Remove all active states
            tabButtons.forEach(btn => {
                btn.classList.remove('active');
                btn.setAttribute('aria-selected', 'false');
            });
            
            document.querySelectorAll('.tab-pane').forEach(pane => {
                pane.classList.remove('show', 'active');
            });
            
            // Activate current tab
            this.classList.add('active');
            this.setAttribute('aria-selected', 'true');
            
            // Show corresponding content
            const target = this.getAttribute('data-bs-target');
            const targetPane = document.querySelector(target);
            if (targetPane) {
                targetPane.classList.add('show', 'active');
                
                // Re-initialize Masonry layout (delay to ensure content is shown)
                setTimeout(() => {
                    const $masonry = $(targetPane).find('.masonry');
                    if ($masonry.length > 0) {
                        console.log('Re-initializing Masonry for:', target);
                        
                        // Destroy existing Masonry instance
                        if ($masonry.data('masonry')) {
                            $masonry.masonry('destroy');
                        }
                        
                        // Re-create Masonry instance
                        $masonry.masonry({
                            percentPosition: true,
                            itemSelector: '.col-sm-12, .col-md-6'
                        });
                        
                        // Force re-layout
                        $masonry.masonry('layout');
                    }
                }, 150);
            }
            
            console.log('Switched to tab:', target);
        });
    });
    
    // Initialize first tab's Masonry
    setTimeout(() => {
        const activePane = document.querySelector('.tab-pane.active');
        if (activePane) {
            const $masonry = $(activePane).find('.masonry');
            if ($masonry.length > 0) {
                $masonry.masonry({
                    percentPosition: true,
                    itemSelector: '.col-sm-12, .col-md-6'
                });
            }
        }
    }, 200);
    
    // Initialize file browser data
    initFileBrowser();
    
    // Initialize console
    initDebugConsole();
}
