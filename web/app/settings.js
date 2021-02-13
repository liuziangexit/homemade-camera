function runSettingsUI() {
    httpGetAsync("/config.json", txt => {
        configtxt.textContent = txt;
        configtxt.style = "width: 100%;-webkit-box-sizing: border-box;-moz-box-sizing: border-box;box-sizing: border-box;";
        document.getElementById("apply").removeAttribute("disabled");
    });
}

function applyConfig() {
}