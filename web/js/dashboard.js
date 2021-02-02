function changeCanvasSize(canvas, denominator, numerator) {
    // Make it visually fill the positioned parent
    canvas.style.height = (canvas.offsetWidth * numerator / denominator) + "px";
    // ...then set the internal size to match
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
}

function startLivestream(canvas) {
    if (!canvas.getContext) {
        alert("浏览器不完整支持canvas的功能，无法进行直播");
        return;
    }

    changeCanvasSize(canvas, 16, 9);

    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);
    ctx.fillStyle = "rgba(20,50,150,0.3)";
    ctx.fillRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);
}

(function () {
    'use strict'
    feather.replace();

    // LiveStream
    startLivestream(document.getElementById("hcam-live"));
})();
