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
        height: 70
        anchors.top: parent.top

        // ========== 贴图背景横幅 ==========
        Image {
            id: bannerBg
            anchors.fill: parent
            source: "qrc:///images/top_banner_bg.png"
            fillMode: Image.Stretch
            z: 0

            onStatusChanged: {
                if (status === Image.Error)
                    console.warn("[TechOverlay] 横幅背景加载失败，source=" + source)
                else if (status === Image.Ready)
                    console.log("[TechOverlay] 横幅背景加载成功")
            }
        }

        // ========== 按钮和标题布局 ==========
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 6
            anchors.leftMargin: 60
            anchors.rightMargin: 72
            height: 50
            spacing: 12
            z: 1

            // ===== 左侧四个按钮 =====
            SciFiButton {
                text: "模型管理"
                mirrored: false
                Layout.preferredWidth: 120; Layout.preferredHeight: 46
                onClicked: {
                    modelListPanel.visible = !modelListPanel.visible
                    if (modelListPanel.visible) {
                        modelListPanel.x = 50
                        modelListPanel.y = 100
                    }
                }
            }
            SciFiButton { text: "想定管理"; mirrored: false; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
            SciFiButton { text: "态势控制"; mirrored: false; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
            SciFiButton { text: "典型场景"; mirrored: false; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }

            // ===== 中间弹性区域（给装饰斜线让路） =====
            Item { Layout.fillWidth: true; Layout.minimumWidth: 60 }

            // ===== 中间标题 =====
            Text {
                text: "多 智 能 体 仿 真 平 台"
                font: theme.titleFont
                color: theme.neonCyan
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideNone
            }

            // ===== 中间弹性区域（给装饰斜线让路） =====
            Item { Layout.fillWidth: true; Layout.minimumWidth: 60 }

            // ===== 右侧四个按钮（镜像翻转） =====
            SciFiButton { text: "任务管理"; mirrored: true; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
            SciFiButton { text: "综合评估"; mirrored: true; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
            SciFiButton { text: "系统设置"; mirrored: true; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
            SciFiButton { text: "用户管理"; mirrored: true; Layout.preferredWidth: 120; Layout.preferredHeight: 46 }
        }
    }

    // 模型列表面板 (悬浮窗口，已合并"已添加模型"功能)
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

    // ========== 底部时间轴与状态栏综合面板 ==========
    TimelineBar {
        id: timelineBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64
    }
}
