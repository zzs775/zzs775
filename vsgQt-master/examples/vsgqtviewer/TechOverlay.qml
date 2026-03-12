import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: root
    width: 1920
    height: 1080

    readonly property color themeColor: "#0EFCFF"
    readonly property string themeFont: "Microsoft YaHei"

    signal requestAddModel(string modelType)
    signal commandTriggered(string cmd)

    component TechButton: Item {
        id: btn
        property string text: "BUTTON"
        property bool isSelected: false
        signal clicked()

        width: 130
        height: 44

        Image {
            id: bgImg
            anchors.fill: parent
            source: "qrc:/images/顶部UI_模型管理.png"
            fillMode: Image.Stretch
            opacity: mouseArea.pressed || btn.isSelected ? 1.0 : 0.7
        }

        Text {
            anchors.centerIn: parent
            text: btn.text
            color: mouseArea.pressed ? "white" : root.themeColor
            font.family: root.themeFont
            font.pixelSize: 16
            font.bold: true

            layer.enabled: true
            layer.effect: Glow {
                color: root.themeColor
                radius: 4
                samples: 16
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    component TechComboBox: ComboBox {
        id: control
        model: ["飞机", "导弹", "车辆", "舰船"]

        background: Rectangle {
            implicitWidth: 200
            implicitHeight: 40
            color: "#AA000B16"
            border.color: root.themeColor
            border.width: 1
            radius: 4
        }

        contentItem: Text {
            leftPadding: 10
            rightPadding: control.indicator.width + control.spacing
            text: control.displayText
            font.family: root.themeFont
            font.pixelSize: 16
            color: root.themeColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        popup: Popup {
            y: control.height - 1
            width: control.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: control.delegateModel
                currentIndex: control.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                color: "#CC000B16"
                border.color: root.themeColor
                radius: 4
            }
        }

        delegate: ItemDelegate {
            width: control.width
            contentItem: Text {
                text: modelData
                color: highlighted ? "white" : root.themeColor
                font.family: root.themeFont
                font.pixelSize: 14
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: highlighted ? Qt.rgba(0, 252, 255, 0.3) : "transparent"
            }
        }
    }

    Item {
        id: topBar
        width: parent.width
        height: 80
        anchors.top: parent.top

        Image {
            anchors.fill: parent
            source: "qrc:/images/标题栏背景_1 (34).png"
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignTop
        }

        Text {
            text: "多 智 能 体 仿 真 平 台"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 5
            color: root.themeColor
            font.family: root.themeFont
            font.pixelSize: 32
            font.bold: true
            font.letterSpacing: 8

            layer.enabled: true
            layer.effect: Glow {
                color: root.themeColor
                radius: 8
                samples: 32
            }
        }

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: parent.width * 0.15
            anchors.topMargin: 35
            spacing: 20

            TechButton { text: "模型管理"; isSelected: sidePanel.visible; onClicked: { sidePanel.visible = !sidePanel.visible; root.commandTriggered("模型管理") } }
            TechButton { text: "想定管理"; onClicked: root.commandTriggered("想定管理") }
            TechButton { text: "态势控制"; onClicked: root.commandTriggered("态势控制") }
            TechButton { text: "典型场景"; onClicked: root.commandTriggered("典型场景") }
        }

        Row {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: parent.width * 0.15
            anchors.topMargin: 35
            spacing: 20

            TechButton { text: "任务管理"; onClicked: root.commandTriggered("任务管理") }
            TechButton { text: "综合评估"; onClicked: root.commandTriggered("综合评估") }
            TechButton { text: "系统设置"; onClicked: root.commandTriggered("系统设置") }
            TechButton { text: "用户管理"; onClicked: root.commandTriggered("用户管理") }
        }
    }

    Item {
        id: sidePanel
        width: 280
        height: 220
        anchors.left: parent.left
        anchors.top: topBar.bottom
        anchors.leftMargin: 100
        anchors.topMargin: 20
        visible: true

        Rectangle {
            anchors.fill: parent
            color: "#CC000B16"
            border.color: root.themeColor
            border.width: 1
            radius: 6

            Rectangle {
                width: parent.width
                height: 2
                color: root.themeColor
                opacity: 0.5
                anchors.top: parent.top
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "模型列表"
                    color: "white"
                    font.family: root.themeFont
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: "＋  －"
                    color: root.themeColor
                    font.pixelSize: 20
                    font.bold: true
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.requestAddModel(modelSelector.currentText)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.themeColor
                opacity: 0.3
            }

            TechComboBox {
                id: modelSelector
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }

            Item { Layout.fillHeight: true }
        }
    }
}
