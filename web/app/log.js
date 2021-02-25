function runLogUI() {
    fetchLog();
}

var mut = 0;

function fetchLog() {
    if (mut != 0) {
        return;
    }
    mut = 1;
    httpGetAsync("/log", txt => {
        document.getElementById("fetch").removeAttribute("disabled");
        mut = 0;
    });
}