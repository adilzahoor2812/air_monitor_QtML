import QtQuick
import AirMonitor

Canvas {
    id: canvas

    property var points: []

    onPointsChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        if (!ctx)
            return
        ctx.clearRect(0, 0, width, height)
        var pts = canvas.points
        if (!pts || pts.length < 2 || width < 2 || height < 2)
            return
        ctx.strokeStyle = Theme.bar
        ctx.lineWidth = 1
        ctx.beginPath()
        var last = pts.length - 1
        for (var i = 0; i < pts.length; ++i) {
            var sample = Number(pts[i])
            if (!isFinite(sample))
                sample = 0
            sample = Math.max(0, Math.min(400, sample))
            var x = i * (width - 1) / last
            var y = (height - 1) - (sample / 400) * (height - 1)
            if (i === 0)
                ctx.moveTo(x, y)
            else
                ctx.lineTo(x, y)
        }
        ctx.stroke()
    }

    Component.onCompleted: requestPaint()
}
