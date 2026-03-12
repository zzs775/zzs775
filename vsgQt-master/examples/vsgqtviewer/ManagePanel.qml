import QtQuick
import QtQuick.Layouts 1.15
import qml
import QtQuick.Controls
Rectangle {
    id: root
    width: 800
    height: 500
    color: "#cc000000"
    border.color: theme.neonCyan
    border.width: 2
    radius: 4

    property bool isVisible: true

    QtObject {
        id: theme
        property color neonCyan: "#0efcff"
        property color headerBg: "#1a1a1a"
        property color rowHover: "#2a2a2a"
        property color textNormal: "#ffffff"
        property color textDim: "#aaaaaa"
        property font headerFont: Qt.font({ family: "Microsoft YaHei", bold: true, pixelSize: 14 })
        property font cellFont: Qt.font({ family: "Microsoft YaHei", pixelSize: 13 })
    }

    ListModel {
        id: dummyModel
        ListElement { entityId: "AC001"; entityName: "F-22 Fighter"; entityType: "Aircraft"; entityPos: "39.9, 116.4, 5000" }
        ListElement { entityId: "SH002"; entityName: "Destroyer-Alpha"; entityType: "Ship"; entityPos: "35.2, 139.8, 0" }
        ListElement { entityId: "VH003"; entityName: "Tank-Bravo"; entityType: "Vehicle"; entityPos: "40.1, 116.5, 100" }
        ListElement { entityId: "BD004"; entityName: "Command Center"; entityType: "Building"; entityPos: "39.8, 116.3, 50" }
        ListElement { entityId: "AC005"; entityName: "SU-57 Fighter"; entityType: "Aircraft"; entityPos: "38.5, 115.2, 8000" }
        ListElement { entityId: "SH006"; entityName: "Carrier-Delta"; entityType: "Ship"; entityPos: "32.1, 121.5, 0" }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        Rectangle {
            id: headerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: theme.headerBg
            radius: 2

            Rectangle {
                width: parent.width
                height: 1
                color: theme.neonCyan
                opacity: 0.5
                anchors.bottom: parent.bottom
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 4

                Text {
                    text: "ID"
                    font: theme.headerFont
                    color: theme.neonCyan
                    Layout.fillWidth: true
                    Layout.minimumWidth: 60
                    Layout.preferredWidth: 80
                    horizontalAlignment: Text.AlignLeft
                }
                Text {
                    text: "名称"
                    font: theme.headerFont
                    color: theme.neonCyan
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                    Layout.preferredWidth: 150
                    horizontalAlignment: Text.AlignLeft
                }
                Text {
                    text: "类型"
                    font: theme.headerFont
                    color: theme.neonCyan
                    Layout.fillWidth: true
                    Layout.minimumWidth: 60
                    Layout.preferredWidth: 100
                    horizontalAlignment: Text.AlignLeft
                }
                Text {
                    text: "坐标 (纬度, 经度, 高度)"
                    font: theme.headerFont
                    color: theme.neonCyan
                    Layout.fillWidth: true
                    Layout.minimumWidth: 100
                    Layout.preferredWidth: 200
                    horizontalAlignment: Text.AlignLeft
                }
                Text {
                    text: "操作"
                    font: theme.headerFont
                    color: theme.neonCyan
                    Layout.preferredWidth: 100
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: typeof simModel !== "undefined" ? simModel : dummyModel

            delegate: Rectangle {
                id: delegateItem
                width: listView.width
                height: 36
                color: delegateMouseArea.containsMouse ? theme.rowHover : (index % 2 === 0 ? "transparent" : "#10ffffff")

                Rectangle {
                    width: parent.width
                    height: 1
                    color: theme.neonCyan
                    opacity: 0.1
                    anchors.bottom: parent.bottom
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 4

                    Text {
                        text: entityId
                        font: theme.cellFont
                        color: theme.neonCyan
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        Layout.preferredWidth: 80
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }
                    Text {
                        text: entityName
                        font: theme.cellFont
                        color: theme.textNormal
                        Layout.fillWidth: true
                        Layout.minimumWidth: 80
                        Layout.preferredWidth: 150
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }
                    Text {
                        text: entityType
                        font: theme.cellFont
                        color: theme.textDim
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        Layout.preferredWidth: 100
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }
                    Text {
                        text: entityPos
                        font: theme.cellFont
                        color: theme.textDim
                        Layout.fillWidth: true
                        Layout.minimumWidth: 100
                        Layout.preferredWidth: 200
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 8
                        Layout.preferredWidth: 100
                        Layout.alignment: Qt.AlignRight

                        // 跳转按钮
                        Rectangle {
                            width: 50
                            height: 24
                            color: "#2200ff00"
                            border.color: theme.neonCyan
                            border.width: 1
                            radius: 2

                            Text {
                                text: "跳转"
                                color: "#00ff00"
                                font.pixelSize: 11
                                anchors.centerIn: parent
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (typeof ControlBridge !== "undefined") {
                                        ControlBridge.focusEntity(entityId);
                                    }
                                }
                            }
                        }

                        // 删除按钮
                        Rectangle {
                            width: 40
                            height: 24
                            color: "#22ff0000"
                            border.color: "#ff4444"
                            border.width: 1
                            radius: 2

                            Text {
                                text: "删除"
                                color: "#ff4444"
                                font.pixelSize: 11
                                anchors.centerIn: parent
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (typeof simModel !== "undefined") {
                                        simModel.removeEntity(entityId);
                                    }
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        console.log("[ManagePanel] 选中实体: " + entityId + " - " + entityName)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: "transparent"

        Text {
            text: "模型管理"
            font: Qt.font({ family: "Microsoft YaHei", bold: true, pixelSize: 16 })
            color: theme.neonCyan
            anchors.left: parent.left
            anchors.leftMargin: 15
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 24
            height: 24
            radius: 2
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            color: closeMouseArea.containsMouse ? "#33ff0000" : "transparent"
            border.color: theme.neonCyan
            border.width: 1

            Text {
                text: "X"
                font.pixelSize: 14
                font.bold: true
                color: closeMouseArea.containsMouse ? "#ff4444" : theme.neonCyan
                anchors.centerIn: parent
            }

            MouseArea {
                id: closeMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.visible = false
                    console.log("[ManagePanel] 面板已关闭")
                }
            }
        }
    }
}
