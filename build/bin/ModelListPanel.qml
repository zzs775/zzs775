import QtQuick
import QtQuick.Layouts 1.15
import QtQuick.Controls

Rectangle {
    id: root
    width: 400
    height: 500
    color: "#dd000000"
    border.color: theme.neonCyan
    border.width: 2
    radius: 6

    property string selectedModel: ""
    property string selectedCategory: ""

    signal modelSelected(string fileName, string category)
    signal closed()

    QtObject {
        id: theme
        property color neonCyan: "#0efcff"
        property color neonGreen: "#00ff88"
        property color neonOrange: "#ff8800"
        property color headerBg: "#1a1a1a"
        property color rowHover: "#2a2a2a"
        property color rowSelected: "#1a3a4a"
        property color textNormal: "#ffffff"
        property color textDim: "#aaaaaa"
        property font headerFont: Qt.font({ family: "Microsoft YaHei", bold: true, pixelSize: 14 })
        property font cellFont: Qt.font({ family: "Microsoft YaHei", pixelSize: 13 })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // 标题栏
        Rectangle {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: theme.headerBg
            radius: 4

            Rectangle {
                width: parent.width
                height: 1
                color: theme.neonCyan
                opacity: 0.5
                anchors.bottom: parent.bottom
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 10

                Text {
                    text: "模型列表"
                    font: theme.headerFont
                    color: theme.neonCyan
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 24
                    height: 24
                    radius: 2
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
                            root.closed()
                        }
                    }
                }
            }
        }

        // 分类标签栏
        Rectangle {
            id: categoryBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#0a0a0a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Repeater {
                    model: ["全部", "飞机", "导弹", "其他"]

                    Rectangle {
                        Layout.preferredWidth: categoryText.implicitWidth + 16
                        Layout.preferredHeight: 26
                        radius: 3
                        color: selectedCategory === modelData ? theme.neonCyan : (catMouseArea.containsMouse ? "#2a2a2a" : "#1a1a1a")
                        border.color: selectedCategory === modelData ? theme.neonCyan : "#333333"
                        border.width: 1

                        Text {
                            id: categoryText
                            text: modelData
                            font.pixelSize: 12
                            color: selectedCategory === modelData ? "#000000" : theme.textNormal
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: catMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                selectedCategory = modelData
                                filterModels()
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // 模型列表
        ListView {
            id: modelListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: modelListModel
            spacing: 2

            delegate: Rectangle {
                id: delegateItem
                width: modelListView.width
                height: 50
                color: selectedModel === fileName ? theme.rowSelected : (delegateMouseArea.containsMouse ? theme.rowHover : "transparent")

                Rectangle {
                    width: parent.width
                    height: 1
                    color: theme.neonCyan
                    opacity: 0.1
                    anchors.bottom: parent.bottom
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15
                    spacing: 10

                    // 分类图标
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 4
                        color: category === "飞机" ? "#1a4a6a" : (category === "导弹" ? "#4a1a1a" : "#3a3a3a")
                        border.color: category === "飞机" ? theme.neonCyan : (category === "导弹" ? theme.neonOrange : "#555555")
                        border.width: 1

                        Text {
                            text: category === "飞机" ? "✈" : (category === "导弹" ? "🚀" : "📦")
                            font.pixelSize: 16
                            color: "#ffffff"
                            anchors.centerIn: parent
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: displayName
                            font: theme.cellFont
                            font.bold: true
                            color: theme.textNormal
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: category + " | " + fileName
                            font.pixelSize: 11
                            color: theme.textDim
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    // 选择按钮
                    Rectangle {
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 28
                        radius: 3
                        color: selectMouseArea.containsMouse ? theme.neonGreen : "transparent"
                        border.color: theme.neonGreen
                        border.width: 1

                        Text {
                            text: "选择"
                            font.pixelSize: 12
                            color: selectMouseArea.containsMouse ? "#000000" : theme.neonGreen
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: selectMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                selectedModel = fileName
                                root.modelSelected(fileName, category)
                                root.visible = false
                                root.closed()
                            }
                        }
                    }
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }
        }
    }

    // 空状态提示
    Text {
        text: "暂无模型数据"
        font.pixelSize: 14
        color: theme.textDim
        anchors.centerIn: parent
        visible: modelListView.count === 0
    }

    function filterModels() {
        // 这里可以扩展过滤逻辑
        // 目前由 QML 自动处理
    }

    Component.onCompleted: {
        console.log("[ModelListPanel] 面板已加载")
    }
}
