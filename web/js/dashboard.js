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
    var height = 20;
    var leftPadding = 5;

    var ctx = canvas.getContext('2d');
    ctx.fillStyle = "white";
    ctx.font = height + "px sans-serif";
    ctx.textBaseline = 'top';
    ctx.textAlign = "start";
    ctx.fillText(text, leftPadding * 2 + height / 2, 0);

    ctx.beginPath();
    ctx.arc(leftPadding + height / 4, height / 2, height / 4, 0, Math.PI * 2, true);
    ctx.fillStyle = color;
    ctx.fill();
}


var prevColor;
var prevText;
var prevFrame;

function draw(color, text, frame, nochange) {
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
    if (!canvas.getContext) {
        alert("浏览器不完整支持canvas的功能，无法进行直播");
        return;
    }
    draw("yellow", "连接中...", null);

    socket = new WebSocket("ws://" + window.location.hostname + ":" + window.location.port);
    socket.binaryType = "blob";
    socket.addEventListener('open', function (event) {
        draw("yellow", "等待直播...", null);
        socket.send('STREAM_ON');
        messageHandler = (e) => {
            if (e.data === "ok") {
                draw("yellow", "已请求直播...", null);
                console.log("stream on succeed");
                var callback = (self, msg) => {
                    if (msg.data instanceof Blob) {
                        // binary
                        console.log("frame received");
                        var frame = new Image();
                        frame.src = URL.createObjectURL(msg.data);
                        frame.onload = () => {
                            canvasScale.numerator = frame.height;
                            canvasScale.denominator = frame.width;
                            draw("green", "LIVE", frame);
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
    });
    socket.addEventListener('error', function (event) {
        draw("red", "连接错误", null);
    });
    socket.addEventListener('message', function (event) {
        if (messageHandler) {
            messageHandler = messageHandler(event);
        } else {
            console.warn("ws message ignored");
        }
    });

}

(function () {
    'use strict'
    feather.replace();

    // LiveStream
    canvas = document.getElementById("hcam-live");
    startLivestream();

    window.onresize = () => {
        draw("", "", null, true);
    };
})();
