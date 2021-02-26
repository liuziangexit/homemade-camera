function runLogUI() {
    fetchLog();
}

var mut = 0;

function displayLog(log) {
    //[warn] CAPTURE at 2021/02/26 18:44:32: frame drop detected, fps: 25
    var tableBody = document.getElementById("table-body");
    tableBody.innerHTML = "";
    var lines = log.split("\r\n");
    for (var i = lines.length - 1; i >= 0; i--) {
        var level, component, time, message;
        var line = lines[i];
        if (line == "")
            continue;
        /*console.log(line);*/
        {
            //level
            var begin = line.search(/\[/);
            if (begin != -1) {
                var end = line.search(/\]/);
                if (end != -1) {
                    level = line.substr(begin + 1, end - begin - 1);
                }
            }
            if (level == null) {
                level = "NONE";
            }
            /*console.log("level:" + level);*/
        }
        {
            //component
            var begin = line.search(/\] /);
            if (begin != -1) {
                begin++;
                var end = line.search(/ at /);
                if (end != -1) {
                    component = line.substr(begin + 1, end - begin - 1);
                }
            }
            if (component == null) {
                component = "NONE";
            }
            /*console.log("component:" + component);*/
        }
        {
            //time
            var begin = line.search(/ at /);
            if (begin != -1) {
                begin += 3;
                var end = line.search(/: /);
                if (end != -1) {
                    time = line.substr(begin + 1, end - begin - 1);
                }
            }
            if (time == null) {
                time = "NONE";
            }
            /*console.log("time:" + time);*/
        }
        {
            //message
            var begin = line.search(/: /);
            if (begin != -1) {
                begin += 1;
                var end = line.length;
                if (end != -1) {
                    message = line.substr(begin + 1, end - begin - 1);
                }
            }
            if (message == null) {
                message = "NONE";
            }
            /*console.log("message:" + message);*/
        }
        var tr = document.createElement("tr");
        var td;

        td = document.createElement("td");
        td.innerText = level;
        tr.appendChild(td);

        td = document.createElement("td");
        td.innerText = component;
        tr.appendChild(td);

        td = document.createElement("td");
        td.innerText = time;
        tr.appendChild(td);

        td = document.createElement("td");
        td.innerText = message;
        tr.appendChild(td);

        tableBody.appendChild(tr);
    }
}

function fetchLog() {
    if (mut != 0) {
        return;
    }
    mut = 1;
    httpGetAsync("/log", txt => {
        document.getElementById("fetch").removeAttribute("disabled");
        displayLog(txt);
        mut = 0;
    });
}