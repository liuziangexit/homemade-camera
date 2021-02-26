function httpAsync(theUrl, method, data, callback) {
    var xhr = new XMLHttpRequest();
    if ("withCredentials" in xhr) {
        xhr.open(method, theUrl, true);
    } else if (typeof XDomainRequest != "undefined") {
        xhr = new XDomainRequest();
        xhr.open(method, theUrl);
    } else {
        return false;
    }
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200)
            callback(xhr.responseText);
    }
    if (data) {
        xhr.send(data);
    } else {
        xhr.send();
    }
    return true;
}

var activeTab;
var prevTab;
var prevTabDesturctor;
var loadingPanel = false;

//TODO 这里现在类似于加锁，但实际上想做成当新的tab被click时，老的loading（主要是httpget）就被取消了
function tabClick(tabName, label, url, callback, dtor) {
    if (loadingPanel)
        return;
    loadingPanel = true;
    document.getElementById(tabName).classList.add("active");
    if (activeTab) {
        document.getElementById(activeTab).classList.remove("active");
    }
    if (document.getElementById("menu-button").getAttribute("aria-expanded") === "true") {
        document.getElementById("menu-button").click();
    }
    if (prevTabDesturctor)
        prevTabDesturctor();
    document.getElementById("loading-div").style = "height:100%";
    document.getElementById("loading-label").innerText = label;

    httpAsync(url, "GET", null, txt => {
        var eCopy;
        try {
            prevTab = activeTab;
            activeTab = tabName;
            prevTabDesturctor = dtor;

            var mainDiv = document.getElementById("main");
            mainDiv.innerHTML = txt;
            feather.replace();
            callback();
        } catch (e) {
            eCopy = e;
        }
        document.getElementById("loading-div").style = "display: none";
        loadingPanel = false;
        if (eCopy)
            throw eCopy;
    })
}

(function () {
    'use strict'
    document.getElementById("liveTab").click();
})();
