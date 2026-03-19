import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    height: 64
    color: Qt.rgba(0.02, 0.06, 0.12, 0.82)
    border.color: Qt.rgba(0.278, 0.333, 0.412, 0.5)
    border.width: 1

    // ==============================================
    // 1. Status Bar (20px)
    // ==============================================
    Item {
        id: statusBar
        width: parent.width
        height: 20
        anchors.top: parent.top

        Rectangle { anchors.fill: parent; color: Qt.rgba(0, 0, 0, 0.65) }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(0.278, 0.333, 0.412, 0.35) }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            // Scale Indicator
            Row {
                spacing: 5
                anchors.verticalCenter: parent.verticalCenter
                Item {
                    width: 30; height: 8; anchors.verticalCenter: parent.verticalCenter
                    Rectangle { width: 30; height: 1; color: "#0efcff"; anchors.centerIn: parent }
                    Rectangle { width: 1; height: 6; color: "#0efcff"; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle { width: 1; height: 6; color: "#0efcff"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter }
                }
                Text {
                    text: (typeof ControlBridge !== "undefined") ? ControlBridge.scaleText : "--- km"
                    color: "#0efcff"; font.pixelSize: 11; font.family: "Arial"; font.bold: true
                }
            }

            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.25); font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: {
                    var v = (typeof ControlBridge !== "undefined") ? ControlBridge.longitude.toFixed(8) : "0.00000000";
                    return "经度: " + v;
                }
                color: "#f8fafc"; font.pixelSize: 11; font.family: "Arial"
            }
            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.3); font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: {
                    var v = (typeof ControlBridge !== "undefined") ? ControlBridge.latitude.toFixed(8) : "0.00000000";
                    return "纬度: " + v;
                }
                color: "#f8fafc"; font.pixelSize: 11; font.family: "Arial"
            }
            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.3); font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: {
                    var raw = (typeof ControlBridge !== "undefined") ? ControlBridge.altitude : 0.0;
                    var v = Math.max(0, raw).toFixed(2);
                    return "海拔: " + v + " 米";
                }
                color: "#f8fafc"; font.pixelSize: 11; font.family: "Arial"
            }
            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.3); font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: {
                    var p = (typeof ControlBridge !== "undefined") ? ControlBridge.pitch.toFixed(2) : "0.00";
                    return "视角: " + p;
                }
                color: "#f8fafc"; font.pixelSize: 11; font.family: "Arial"
            }
            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.3); font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: {
                    var v = (typeof ControlBridge !== "undefined") ? ControlBridge.viewHeight.toFixed(0) : "0";
                    return "视高: " + v + " 米";
                }
                color: "#f8fafc"; font.pixelSize: 11; font.family: "Arial"
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            // ACMI 数据接收状态
            Text {
                visible: (typeof ControlBridge !== "undefined")
                text: {
                    if (typeof ControlBridge === "undefined") return "";
                    var cnt = ControlBridge.acmiPacketCount;
                    if (cnt > 0)
                        return "📡 " + cnt + " 帧";
                    else
                        return "📡 无数据";
                }
                color: {
                    if (typeof ControlBridge === "undefined") return "#475569";
                    return ControlBridge.acmiPacketCount > 0 ? "#38bdf8" : "#475569";
                }
                font.pixelSize: 10; font.family: "Arial"
            }

            Text { text: "|"; color: Qt.rgba(0.58, 0.64, 0.72, 0.2); font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }

            Text { text: (typeof ControlBridge !== "undefined" ? ControlBridge.frameMs.toFixed(1) : "0.0") + " MS"; color: "#64d4a0"; font.pixelSize: 10; font.family: "Arial"; font.bold: true }
            Text { text: (typeof ControlBridge !== "undefined" ? ControlBridge.fps : "0") + " FPS"; color: "#64d4a0"; font.pixelSize: 10; font.family: "Arial"; font.bold: true }
        }
    }

    // ==============================================
    // 2. Control Bar (18px)
    // ==============================================
    Item {
        id: controlBar
        width: parent.width
        height: 18
        anchors.top: statusBar.bottom

        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(0.278, 0.333, 0.412, 0.2) }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            // Play/Pause
            Rectangle {
                id: playBtn
                width: 22; height: 16; radius: 2
                property bool isPlaying: (typeof ControlBridge !== "undefined" && ControlBridge.simAnimating)
                color: isPlaying ? Qt.rgba(0.94, 0.27, 0.27, 0.15) : Qt.rgba(0.12, 0.16, 0.23, 0.8)
                border.color: isPlaying ? "#ef4444" : Qt.rgba(0.278, 0.333, 0.412, 0.6)
                Text { text: playBtn.isPlaying ? "||" : "▶"; color: playBtn.border.color; anchors.centerIn: parent; font.pixelSize: 8 }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (typeof ControlBridge !== "undefined")
                            ControlBridge.setSimAnimating(!ControlBridge.simAnimating);
                    }
                }
            }

            // Stop
            Rectangle {
                width: 22; height: 16; radius: 2
                color: Qt.rgba(0.12, 0.16, 0.23, 0.8)
                border.color: Qt.rgba(0.278, 0.333, 0.412, 0.6)
                Text { text: "■"; color: "#94a3b8"; anchors.centerIn: parent; font.pixelSize: 8 }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (typeof ControlBridge !== "undefined")
                            ControlBridge.stopSim();
                    }
                }
            }

            Item { width: 4; height: 1 }

            // Speed Buttons
            Repeater {
                model: [1, 5, 10, 30, 100]
                Rectangle {
                    width: spdTxt.width + 10; height: 16; radius: 2
                    property bool isActive: (typeof ControlBridge !== "undefined" && Math.abs(ControlBridge.simMultiplier - modelData) < 0.01)
                    color: isActive ? Qt.rgba(0.22, 0.74, 0.97, 0.08) : Qt.rgba(0.06, 0.09, 0.16, 0.8)
                    border.color: isActive ? "#38bdf8" : Qt.rgba(0.278, 0.333, 0.412, 0.4)
                    Text {
                        id: spdTxt
                        text: modelData + "×"
                        color: parent.isActive ? "#38bdf8" : "#64748b"
                        anchors.centerIn: parent
                        font.pixelSize: 9; font.family: "Courier New"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (typeof ControlBridge !== "undefined")
                                ControlBridge.setSimMultiplier(modelData);
                        }
                    }
                }
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: typeof ControlBridge !== "undefined" ? ControlBridge.simCurrentTime : ""
            color: "#e2e8f0"
            font.family: "Courier New"
            font.pixelSize: 10
        }
    }

    // ==============================================
    // 3. Timeline (22px)
    // ==============================================
    Item {
        id: timelineOuter
        width: parent.width
        height: 26
        anchors.top: controlBar.bottom

        Rectangle {
            id: zoomLeft
            width: 14; height: 18
            anchors.left: parent.left; anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.rgba(0.06, 0.09, 0.16, 0.6)
            border.color: Qt.rgba(0.278, 0.333, 0.412, 0.3)
            radius: 2
            Text { text: "◀"; anchors.centerIn: parent; color: "#475569"; font.pixelSize: 7 }
        }

        Rectangle {
            id: zoomRight
            width: 14; height: 18
            anchors.right: parent.right; anchors.rightMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.rgba(0.06, 0.09, 0.16, 0.6)
            border.color: Qt.rgba(0.278, 0.333, 0.412, 0.3)
            radius: 2
            Text { text: "▶"; anchors.centerIn: parent; color: "#475569"; font.pixelSize: 7 }
        }

        Rectangle {
            id: timelineBarInner
            anchors.left: zoomLeft.right; anchors.leftMargin: 3
            anchors.right: zoomRight.left; anchors.rightMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            height: 24
            border.color: Qt.rgba(0.278, 0.333, 0.412, 0.6)
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1e2937" }
                GradientStop { position: 1.0; color: "#0f172a" }
            }
            clip: true

            // Buffer fill (White line)
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * ((typeof ControlBridge !== "undefined") ? Math.min(1.0, Math.max(0.0, ControlBridge.maxBufferedProgress)) : 0.0)
                color: Qt.rgba(1, 1, 1, 0.15)
                
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    color: "white"
                    opacity: 0.6
                }
            }

            // Progress fill
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * ((typeof ControlBridge !== "undefined") ? Math.min(1.0, Math.max(0.0, ControlBridge.simProgress)) : 0.0)
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(0.22, 0.74, 0.97, 0.06) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.22, 0.74, 0.97, 0.03) }
                }
            }

            // Ticks
            Canvas {
                id: ticCanvas
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var totalMs = 24 * 3600 * 1000;
                    var mainMs  = 3 * 3600 * 1000;
                    var subMs   = 30 * 60 * 1000;

                    // Sub ticks
                    ctx.fillStyle = "rgba(100,116,139,0.4)";
                    for (var j = 0; j <= totalMs; j += subMs) {
                        if (j % mainMs === 0) continue;
                        var sx = (j / totalMs) * width;
                        ctx.fillRect(sx, 0, 1, 6);
                    }

                    // Main ticks + labels
                    for (var k = 0; k <= totalMs; k += mainMs) {
                        var mx = (k / totalMs) * width;
                        ctx.fillStyle = "rgba(71,85,105,0.35)";
                        ctx.fillRect(mx, 0, 1, height);

                        ctx.textAlign = "center";
                        if (k === 0) {
                            ctx.fillStyle = "#cbd5e1";
                            ctx.font = "11px Courier New";
                        } else {
                            ctx.fillStyle = "#94a3b8";
                            ctx.font = "10px Courier New";
                        }
                        var pct = k / totalMs;
                        if (typeof ControlBridge !== "undefined") {
                            ctx.fillText(ControlBridge.timeStringAt(pct), mx, 19);
                        }
                    }
                }
                Component.onCompleted: requestPaint()
            }

            onWidthChanged: ticCanvas.requestPaint()

            // Repaint ticks when sim range changes
            Connections {
                target: (typeof ControlBridge !== "undefined") ? ControlBridge : null
                function onSimRangeChanged() { ticCanvas.requestPaint(); }
            }

            // Needle
            Item {
                id: needle
                width: 2
                x: (parent.width - 2) * ((typeof ControlBridge !== "undefined") ? Math.min(1.0, Math.max(0.0, ControlBridge.simProgress)) : 0.0)
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                // Red line
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top; anchors.topMargin: 5
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 1
                    width: 2
                    color: "#ef4444"
                }

                // Top triangle
                Canvas {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: 8; height: 5
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, 8, 5);
                        ctx.beginPath();
                        ctx.moveTo(0, 0);
                        ctx.lineTo(8, 0);
                        ctx.lineTo(4, 5);
                        ctx.closePath();
                        ctx.fillStyle = "#ef4444";
                        ctx.fill();
                    }
                }

                // Bottom square
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: 4; height: 4
                    radius: 1
                    color: "#ef4444"
                }
            }

            // Mouse interaction
            MouseArea {
                id: tArea
                anchors.fill: parent
                preventStealing: true
                property bool dragging: false
                
                function updateProgress(mx) {
                    var p = mx / width;
                    p = Math.max(0.0, Math.min(1.0, p));
                    // ★ 限制拖拽进度不超过白线 (已缓冲数据)
                    var maxBuf = (typeof ControlBridge !== "undefined") ? ControlBridge.maxBufferedProgress : 1.0;
                    p = Math.min(p, maxBuf);
                    
                    if (typeof ControlBridge !== "undefined") {
                        ControlBridge.seekToProgress(p);
                    }
                }

                onPressed: {
                    dragging = true;
                    if (typeof ControlBridge !== "undefined") {
                        ControlBridge.setSimAnimating(false);
                    }
                    updateProgress(mouse.x);
                }
                
                onPositionChanged: {
                    var pct = Math.max(0.0, Math.min(1.0, mouseX / width));
                    if (dragging) {
                        updateProgress(mouse.x);
                    } else {
                        hoverTip.visible = true;
                        hoverTip.x = Math.max(0, Math.min(width - hoverTip.width, mouseX - hoverTip.width / 2));
                        hoverTip.y = 1;
                        if (typeof ControlBridge !== "undefined")
                            hoverLabel.text = ControlBridge.timeStringAt(pct);
                    }
                }

                onExited: hoverTip.visible = false

                onReleased: {
                    dragging = false;
                    // Do not auto-resume, let user manually press play if they want.
                }

                onWheel: function(wheel) {
                    if (typeof ControlBridge !== "undefined") {
                        ControlBridge.adjustTimelineCapacity(wheel.angleDelta.y);
                    }
                }

                Rectangle {
                    id: hoverTip
                    visible: false
                    width: hoverLabel.width + 10
                    height: 14
                    color: Qt.rgba(0.04, 0.08, 0.16, 0.95)
                    border.color: Qt.rgba(0.22, 0.74, 0.97, 0.5)
                    radius: 2
                    z: 100
                    Text {
                        id: hoverLabel
                        anchors.centerIn: parent
                        color: "#e2e8f0"
                        font.family: "Courier New"
                        font.pixelSize: 7
                        text: ""
                    }
                }
            }
        }
    }
}
