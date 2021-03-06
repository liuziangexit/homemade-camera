var canvasScale = {
    denominator: 16,
    numerator: 9
}
var canvas;
var socket;
var closeReason;
var messageHandler = null;

function changeCanvasSize() {
    // Make it visually fill the positioned parent
    canvas.style.height = (canvas.offsetWidth * canvasScale.numerator / canvasScale.denominator) + "px";
    // ...then set the internal size to match
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
}

function drawStatus(color, text) {
    var height = canvas.offsetHeight / 12;
    var topPadding = 5;
    var leftPadding = 5;

    var ctx = canvas.getContext('2d');
    ctx.fillStyle = "white";
    ctx.font = height + "px sans-serif";
    ctx.textBaseline = 'middle';
    ctx.textAlign = "start";
    ctx.fillText(text, leftPadding * 2 + height / 2, height / 2 + topPadding + 1);

    ctx.beginPath();
    ctx.arc(leftPadding + height / 4, height / 2 + topPadding, height / 4, 0, Math.PI * 2, true);
    ctx.fillStyle = color;
    ctx.fill();
}


var prevColor;
var prevText;
var prevFrame;
var prevRawFrame;

var lastFrameTime = 0;
var twinkleTime = Date.now();
var twinkleId = 0;
var twinkle = ["forestgreen", "rgba(0,0,0,0)"];
var debug = false;

var roundBegin = 0;
var frameCnt = 0;
var fps = 0;

function draw(color, text, frame, nochange) {
    if (activeTab != "liveTab")
        return;
    changeCanvasSize(canvas);

    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);
    ctx.fillStyle = "rgba(0,0,0,1)";
    ctx.fillRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);

    if (nochange) {
        color = prevColor;
        text = prevText;
        frame = prevFrame;
    }
    if (frame) {
        ctx.drawImage(frame, 0, 0, canvas.offsetWidth, canvas.offsetHeight);
    }
    drawStatus(color, text);
    prevColor = color;
    prevText = text;
    prevFrame = frame;
}

function startLivestream() {
    document.getElementById("loading-div").style = "display: none";
    if (!canvas.getContext) {
        alert("浏览器不完整支持canvas的功能，无法进行直播");
        return;
    }
    draw("yellow", "连接中...", null);

    socket = new WebSocket((window.location.protocol === "http:" ? "ws://" : "wss://") + window.location.hostname + ":" + window.location.port);
    socket.binaryType = "blob";
    socket.addEventListener('open', function (event) {
        draw("yellow", "等待直播...", null);
        socket.send('STREAM_ON');
        messageHandler = (e) => {
            if (e.data === "ok") {
                draw("yellow", "已请求直播...", null);
                document.getElementById("saveButton").removeAttribute("disabled");
                console.log("stream on succeed");
                lastFrameTime = Date.now();

                var timerFunc = (self) => {
                    if (Date.now() - lastFrameTime >= 10000) {
                        console.log("socket closed by timer callback");
                        socket.close();
                    } else {
                        setTimeout(() => self(self), 10000);
                    }
                };
                setTimeout(() =>
                        timerFunc(timerFunc)
                    , 10000);

                var callback = (self, msg) => {
                    if (msg.data instanceof Blob) {
                        // binary
                        console.log("frame received");
                        lastFrameTime = Date.now();

                        if (lastFrameTime - roundBegin >= 1000) {
                            fps = frameCnt;
                            frameCnt = 0;
                            roundBegin = lastFrameTime;
                        } else {
                            frameCnt++;
                        }

                        prevRawFrame = msg.data;
                        var frame = new Image();
                        frame.src = URL.createObjectURL(msg.data);
                        frame.onload = () => {
                            canvasScale.numerator = frame.height;
                            canvasScale.denominator = frame.width;
                            var now = Date.now();
                            if (now - twinkleTime >= 1000) {
                                twinkleId++;
                                twinkleTime = now;
                            }
                            var text = "LIVE";
                            if (debug) {
                                text = "" + fps + " fps";
                            }
                            draw(twinkle[(twinkleId) % 2], text, frame);
                        }
                    } else {
                        // text
                        console.log(msg.data);
                    }
                    return (e) => {
                        return self(self, e);
                    };
                };
                return (e) => {
                    return callback(callback, e);
                };
            } else {
                console.log("stream on failed");
                socket.close();
                draw("red", "获取直播信息失败", null);
                return null;
            }
        };
    });
    socket.addEventListener('close', function (event) {
        draw("red", "连接已关闭", null);
        var btn = document.getElementById("saveButton")
        if (btn) btn.setAttribute("disabled", "disabled");
    });
    socket.addEventListener('error', function (event) {
        draw("red", "连接错误", null);
        var btn = document.getElementById("saveButton")
        if (btn) btn.setAttribute("disabled", "disabled");
    });
    socket.addEventListener('message', function (event) {
        if (messageHandler) {
            messageHandler = messageHandler(event);
        } else {
            console.warn("ws message ignored");
        }
    });
}

function getBase64Image(img) {
    var canvas = document.createElement("canvas");
    canvas.width = img.width;
    canvas.height = img.height;
    var ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0);
    return canvas.toDataURL("image/jpeg");
}

function saveFrame() {
    if (prevFrame) {
        /*var imgData = getBase64Image(prevFrame);
        var imgControl = '<img src="' + imgData + '">';
        var w = window.open("", '_blank');
        w.document.write(imgControl);
        w.document.close();*/

        var elementA = document.createElement('a');

        //文件的名称为时间戳加文件名后缀
        elementA.download = +new Date() + ".jpg";
        elementA.style.display = 'none';

        //生成一个blob二进制数据，内容为json数据
        var blob = new Blob([prevRawFrame]);

        //生成一个指向blob的URL地址，并赋值给a标签的href属性
        elementA.href = URL.createObjectURL(blob);
        document.body.appendChild(elementA);
        elementA.click();
        document.body.removeChild(elementA);
    }
}