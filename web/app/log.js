function runLogUI() {
    httpGetAsync("/nohup.out", txt => {
        logtxt.textContent = txt;
        logtxt.style = "width: 100%;-webkit-box-sizing: border-box;-moz-box-sizing: border-box;box-sizing: border-box;";
        document.getElementById("fetchBtn").removeAttribute("disabled");
    });
}

function fetchLog() {
}