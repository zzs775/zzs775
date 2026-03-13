import QtQuick 2.15

Rectangle {
    id: btn
    property string btnIcon: "minimize"
    property bool isCloseBtn: false
    signal btnClicked()

    width: 36
    height: 36
    radius: 8
    color: {
        if (!hov.containsMouse) return "transparent"
        if (isCloseBtn) return Qt.rgba(0.9, 0.2, 0.2, 0.25)
        return Qt.rgba(1, 1, 1, 0.15)
    }
    Behavior on color { ColorAnimation { duration: 120 } }

    Canvas {
        id: cv
        anchors.centerIn: parent
        width: 14
        height: 14

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = (isCloseBtn && hov.containsMouse)
                ? "#ff6b6b"
                : "rgba(255,255,255,0.85)"
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            if (btnIcon === "minimize") {
                ctx.moveTo(1, 9); ctx.lineTo(13, 9)
            } else if (btnIcon === "maximize") {
                ctx.rect(1.5, 1.5, 11, 11)
            } else if (btnIcon === "restore") {
                ctx.rect(3.5, 1.5, 9, 9)
                ctx.moveTo(1.5, 3.5)
                ctx.lineTo(1.5, 12.5)
                ctx.lineTo(10.5, 12.5)
            } else if (btnIcon === "close") {
                ctx.moveTo(2.5, 2.5); ctx.lineTo(11.5, 11.5)
                ctx.moveTo(11.5, 2.5); ctx.lineTo(2.5, 11.5)
            } else if (btnIcon === "collapse") {
                ctx.moveTo(9.5, 3); ctx.lineTo(6.5, 7); ctx.lineTo(9.5, 11)
                ctx.moveTo(4, 3); ctx.lineTo(4, 11)
            }
            ctx.stroke()
        }
    }

    HoverHandler {
        id: hov
        onHoveredChanged: cv.requestPaint()
    }

    TapHandler {
        onTapped: btn.btnClicked()
    }

    onBtnIconChanged: cv.requestPaint()
}
