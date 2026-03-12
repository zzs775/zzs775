import QtQuick
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent

    Component.onCompleted: {
        console.log(" 引擎启动成功！界面已加载！")
    }

    QtObject {
        id: theme
        property color neonCyan: "#0efcff"
        property font titleFont: Qt.font({ family: "Microsoft YaHei", bold: true, pixelSize: 24, letterSpacing: 4 })
    }

    Item {
        id: topBar
        width: parent.width
        height: 80
        anchors.top: parent.top

        Rectangle { width: parent.width; height: 1; color: theme.neonCyan; opacity: 0.3; anchors.top: parent.top }
        Rectangle { width: parent.width * 0.98; height: 2; color: theme.neonCyan; opacity: 0.8; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 4

            SciFiButton {
                text: "模型管理"
                Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34
                onClicked: {
                    modelListPanel.visible = !modelListPanel.visible
                    if (modelListPanel.visible) {
                        modelListPanel.x = 50
                        modelListPanel.y = 100
                    }
                }
            }
            SciFiButton { text: "想定管理"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
            SciFiButton { text: "态势控制"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
            SciFiButton { text: "典型场景"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }

            Item { Layout.fillWidth: true }

            Text {
                text: "多 智 能 体 仿 真 平 台"
                font: theme.titleFont
                color: theme.neonCyan
                Layout.alignment: Qt.AlignCenter
                elide: Text.ElideNone
            }

            Item { Layout.fillWidth: true }

            SciFiButton { text: "任务管理"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
            SciFiButton { text: "综合评估"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
            SciFiButton { text: "系统设置"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
            SciFiButton { text: "用户管理"; Layout.fillWidth: true; Layout.minimumWidth: 60; Layout.preferredWidth: 100; Layout.maximumWidth: 140; Layout.preferredHeight: 34 }
        }
    }

    // 模型列表面板 (悬浮窗口)
    ModelListPanel {
        id: modelListPanel
        visible: false
        z: 100

        onModelSelected: function(fileName, category) {
            console.log("[TechOverlay] 选择模型: " + fileName + " 分类: " + category)
            if (typeof ControlBridge !== "undefined") {
                ControlBridge.selectModel(fileName, category, "")
            }
        }

        onClosed: {
            console.log("[TechOverlay] 模型列表面板已关闭")
        }
    }
}
