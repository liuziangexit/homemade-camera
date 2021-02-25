function runSettingsUI() {
    httpGetAsync("/config.json", txt => {
        configtxt.textContent = txt;
    });
}

function applyConfig() {
}