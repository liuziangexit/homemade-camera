function httpGetAsync(theUrl, callback) {
    var xhr = new XMLHttpRequest();
    if ("withCredentials" in xhr) {
        xhr.open("GET", theUrl, true);
    } else if (typeof XDomainRequest != "undefined") {
        xhr = new XDomainRequest();
        xhr.open("GET", theUrl);
    } else {
        return false;
    }
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200)
            callback(xhr.responseText);
    }
    xhr.send();
    return true;
};

var activeTab;
var prevTab;
var prevTabDesturctor;

function tabClick(tabName, url, callback, dtor) {
    httpGetAsync(url, txt => {
        var mainDiv = document.getElementById("main");
        mainDiv.innerHTML = txt;
        feather.replace();
        prevTab = activeTab;
        activeTab = tabName;
        if (prevTabDesturctor)
            prevTabDesturctor();
        prevTabDesturctor = dtor;
        document.getElementById(tabName).classList.add("active");
        if (prevTab) {
            document.getElementById(prevTab).classList.remove("active");
        }
        callback();
    })
}

(function () {
    'use strict'
    document.getElementById("liveTab").click();
})();
