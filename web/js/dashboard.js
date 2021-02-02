var livestreamScale = {
    denominator: 16,
    numerator: 9
}
var state = "connect";
var canvas;
var socket;
var state_transfer_handler = null;
var frame = new Image();

function changeCanvasSize() {
    // Make it visually fill the positioned parent
    canvas.style.height = (canvas.offsetWidth * livestreamScale.numerator / livestreamScale.denominator) + "px";
    // ...then set the internal size to match
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
}

function draw() {
    changeCanvasSize(canvas);

    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);
    ctx.fillStyle = "rgba(180,180,180,1)";
    ctx.fillRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);

    /*ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(canvas.offsetWidth, canvas.offsetHeight);
    ctx.stroke();
    ctx.closePath();

    ctx.beginPath();
    ctx.moveTo(0, canvas.offsetHeight);
    ctx.lineTo(canvas.offsetWidth, 0);
    ctx.stroke();
    ctx.closePath();*/

    switch (state) {
        case "connect": {
            ctx.fillStyle = "black";
            ctx.font = "20px sans-serif";
            ctx.textBaseline = 'middle';
            ctx.textAlign = "center";
            ctx.fillText("连接中", canvas.offsetWidth / 2, canvas.offsetHeight / 2);
        }
            break;
        case "load": {
            ctx.fillStyle = "black";
            ctx.font = "20px sans-serif";
            ctx.textBaseline = 'middle';
            ctx.textAlign = "center";
            ctx.fillText("获取直播信息中", canvas.offsetWidth / 2, canvas.offsetHeight / 2);
        }
            break;
        case "play": {
            ctx.drawImage(frame, 0, 0);
        }
            break;
        case "close": {
            ctx.fillStyle = "black";
            ctx.font = "20px sans-serif";
            ctx.textBaseline = 'middle';
            ctx.textAlign = "center";
            ctx.fillText("无法播放", canvas.offsetWidth / 2, canvas.offsetHeight / 2);
        }
            break;
        default: {
            alert("invalid state");
        }
    }
}

function messageHandler(msg) {
    if (msg.data instanceof Blob) {
        // binary
        console.log("frame received");
        frame.src = URL.createObjectURL(msg.data);
        frame.onload = () => draw();

        /*var reader = new FileReader();
        reader.onloadend = () => {
            var uri = 'data:image/jpeg;base64,' + reader.result;
            frame = new Image();
            /!*frame.src = uri;*!/
            /!*frame.onload = () => draw();*!/
        }
        reader.readAsDataURL(msg.data);*/
    } else {
        // text
        console.log(msg.data);
    }

}

function startLivestream() {
    if (!canvas.getContext) {
        alert("浏览器不完整支持canvas的功能，无法进行直播");
        return;
    }
    draw(canvas);

    socket = new WebSocket("ws://" + window.location.hostname + ":" + window.location.port);
    socket.binaryType = "blob";
    socket.addEventListener('open', function (event) {
        state = "load";
        draw();
        socket.send('STREAM_ON');
        state_transfer_handler = (e) => {
            if (e.data === "ok") {
                console.log("stream on succeed");
                state = "play";
                return null;
            } else {
                console.log("stream on failed");
                socket.close();
                state = "close";
                draw();
                return null;
            }
        };
    });
    socket.addEventListener('close', function (event) {
        state = "close";
        draw();
    });
    socket.addEventListener('error', function (event) {
        state = "close";
        draw();
        alert('WebSocket错误:' + event);
    });
    socket.addEventListener('message', function (event) {
        if (state_transfer_handler) {
            state_transfer_handler = state_transfer_handler(event);
        } else {
            messageHandler(event);
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
        draw(canvas);
    };
})();
