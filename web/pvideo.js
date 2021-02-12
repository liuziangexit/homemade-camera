function secondsToString(sec) {
    if (sec < 60) {
        return sec + "秒";
    }
    if (sec < 60 * 60) {
        return Math.floor(sec / 60) + "分钟" + sec % 60 + "秒";
    }
    return Math.floor(sec / 60 / 60) + "小时" + Math.floor(sec / 60) + "分钟";
}

function addVideo(videos, json) {
    var div = document.createElement("div");
    div.className = "col-sm p-2";

    var img = document.createElement("img");
    img.src = "video/" + json.preview;
    img.style = "width:16em;cursor: pointer";
    div.appendChild(img);

    var filename = document.createElement("p");
    filename.textContent = json.filename;
    filename.style = "font-size:1.2em;margin: auto;cursor: pointer";
    div.appendChild(filename);

    var desc = document.createElement("p");
    desc.textContent = secondsToString(json.length);
    desc.style = "font-size:1em;margin: auto;color: #888888";
    div.appendChild(desc);
    videos.appendChild(div);

    var download = () => {
        window.open("video/" + json.filename, '_blank');
    };
    img.onclick = download;
    div.onclick = download;
}

function runVideoUI() {
    httpGetAsync("video/file_log.json", txt => {
        var files = JSON.parse(txt);
        var videos = document.getElementById("videos");
        for (var i in files) {
            if (files[i].finished) {
                addVideo(videos, files[i]);
            }
        }
        //不知道为啥，file.length%2==0的时候，最后一个元素就没有对齐了
        //这里通过瞎搞的方式解决了，我也不想研究这些东西
        for (var i = 0; i < files.length % 3; i++) {
            var div = document.createElement("div");
            div.className = "col-sm-4 p-2";
            videos.appendChild(div);
        }
    })
}