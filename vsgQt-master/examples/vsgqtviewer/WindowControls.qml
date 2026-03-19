import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root
    width: 36
    height: 36
    
    // 自动获取所在的窗口
    property var targetWindow: Window.window

    // 收起态：三点圆形按钮
    Rectangle {
        id: triggerDot
        width: 36
        height: 36
        radius: 18
        color: dotMouseArea.containsMouse ? Qt.rgba(1,1,1,0.25) : Qt.rgba(1,1,1,0.15)
        border.color: dotMouseArea.containsMouse ? Qt.rgba(1,1,1,0.6) : Qt.rgba(1,1,1,0.35)
        border.width: 1
        visible: !controlsBar.visible

        Behavior on color { ColorAnimation { duration: 150 } }

        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle { width: 4; height: 4; radius: 2; color: "white" }
            }
        }

        MouseArea {
            id: dotMouseArea
            anchors.fill: parent
            hoverEnabled: true

            property point pressPos: Qt.point(0,0)
            property bool isDragging: false

            onPressed: (mouse) => {
                pressPos = Qt.point(mouse.x, mouse.y)
                isDragging = false
            }

            onPositionChanged: (mouse) => {
                if (pressed && !isDragging) {
                    if (Math.abs(mouse.x - pressPos.x) > 3 || Math.abs(mouse.y - pressPos.y) > 3) {
                        isDragging = true
                        if (typeof ControlBridge !== "undefined") ControlBridge.startWindowDrag()
                    }
                }
            }

            onClicked: {
                if (!isDragging) {
                    controlsBar.visible = true
                    root.width = controlsBar.width
                    root.height = 46
                }
            }
        }
    }

    // 展开态：控制栏
    Rectangle {
        id: controlsBar
        visible: false
        width: 180
        height: 46
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.13)
        border.color: Qt.rgba(1, 1, 1, 0.28)
        border.width: 1

        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        MouseArea {
            anchors.fill: parent
            z: -1 // 确保在按钮下方

            property point pressPos: Qt.point(0,0)
            property bool isDragging: false

            onPressed: (mouse) => {
                pressPos = Qt.point(mouse.x, mouse.y)
                isDragging = false
            }

            onPositionChanged: (mouse) => {
                if (pressed && !isDragging) {
                    if (Math.abs(mouse.x - pressPos.x) > 3 || Math.abs(mouse.y - pressPos.y) > 3) {
                        isDragging = true
                        if (typeof ControlBridge !== "undefined") ControlBridge.startWindowDrag()
                    }
                }
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 2

            // 收起
            WinBtn {
                btnIcon: "collapse"
                onBtnClicked: {
                    controlsBar.visible = false
                    root.width = 36
                    root.height = 36
                }
            }

            Rectangle { width: 1; height: 18; color: Qt.rgba(1,1,1,0.25); anchors.verticalCenter: parent.verticalCenter }

            // 最小化
            WinBtn {
                btnIcon: "minimize"
                onBtnClicked: {
                    if (typeof ControlBridge !== "undefined") ControlBridge.minimizeWindow()
                    else if (targetWindow) targetWindow.showMinimized()
                }
            }

            // 最大化/还原
            WinBtn {
                id: maxBtn
                btnIcon: (targetWindow && targetWindow.visibility === Window.Maximized) ? "restore" : "maximize"
                onBtnClicked: {
                    if (typeof ControlBridge !== "undefined") {
                        ControlBridge.maximizeWindow()
                    } else if (targetWindow) {
                        if (targetWindow.visibility === Window.Maximized) {
                            targetWindow.showNormal()
                        } else {
                            targetWindow.showMaximized()
                        }
                    }
                }
            }

            Rectangle { width: 1; height: 18; color: Qt.rgba(1,1,1,0.25); anchors.verticalCenter: parent.verticalCenter }

            // 关闭
            WinBtn {
                btnIcon: "close"
                isCloseBtn: true
                onBtnClicked: {
                    if (typeof ControlBridge !== "undefined") ControlBridge.closeWindow()
                    else Qt.quit()
                }
            }
        }
    }
}
