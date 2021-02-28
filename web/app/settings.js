function runSettingsUI() {
    httpAsync("/config.json", "GET", null, txt => {
        document.getElementById("configtxt").value = txt;
        document.getElementById("upload").removeAttribute("disabled");
        document.getElementById("reboot").removeAttribute("disabled");
        document.getElementById("loading-div").style = "display: none";
    });
}

function uploadConfig() {
    httpAsync("/newconfig", "POST", document.getElementById("configtxt").value, txt => {
        alert(txt);
    });
}

function rebootApp() {
    httpAsync("/reload", "GET", null, txt => {
        alert(txt);
    });
}