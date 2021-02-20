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
    div.style = "text-align:center";

    var img = document.createElement("img");
    img.src = "/video/" + json.preview;
    img.style = "; width:16em;cursor: pointer";
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
        window.open("/video/" + json.filename, '_blank');
    };
    img.onclick = download;
    div.onclick = download;
}

const videoPerPage = 6;
var currentPage = 0;
var files;
var nextBtn;
var prevBtn;

function runVideoUI() {
    nextBtn = document.getElementById("nextBtn");
    prevBtn = document.getElementById("prevBtn");
    httpGetAsync("/video/file_log.json", txt => {
        files = JSON.parse(txt);
        displayPage();
        //不知道为啥，file.length%2==0的时候，最后一个元素就没有对齐了
        //这里通过瞎搞的方式解决了，我也不想研究这些东西
        /*  for (var i = 0; i < files.length % 3; i++) {
              var div = document.createElement("div");
              div.className = "col-sm p-2";
              videos.appendChild(div);
          }*/
    })
}

function displayPage() {
    var videos = document.getElementById("videos");
    videos.innerHTML = "";
    for (var i = currentPage * videoPerPage; i < files.length && i < (currentPage + 1) * videoPerPage; i++) {
        var actualIndex = files.length - (i + 1);
        if (!files[actualIndex].finished) {
            console.error("!files.finished ???");
        }
        addVideo(videos, files[actualIndex]);
    }
    if (currentPage == 0) {
        prevBtn.setAttribute("disabled", "disabled");
    } else {
        prevBtn.removeAttribute("disabled");
    }

    if ((currentPage + 1) * videoPerPage >= files.length) {
        nextBtn.setAttribute("disabled", "disabled");
    } else {
        nextBtn.removeAttribute("disabled");
    }
}

function nextPage() {
    if ((currentPage + 1) * videoPerPage >= files.length) {
        console.warn("不应该走到这里");
        return;
    }
    currentPage += 1;
    displayPage();
}

function prevPage() {
    if (currentPage == 0) {
        console.warn("不应该走到这里");
        return;
    }
    currentPage -= 1;
    displayPage();
}