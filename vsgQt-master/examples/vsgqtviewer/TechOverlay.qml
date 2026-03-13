import QtQuick
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent

    // 圆角裁剪容器
    Rectangle {
        id: mainClip
        anchors.fill: parent
        color: "transparent"
        radius: 12
        clip: true
    }

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

        // ========== 贴图背景横幅 (已注释) ==========
        /*
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
        */

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

            // ===== 左侧四个按钮 (仅保留模型管理) =====
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
            /*
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
            */
        }
    }

    // 右上角三点控制组件
    WindowControls {
        id: winControls
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 14
        z: 999
    }

    // 点击外部自动收起 (WindowControls 内部已有逻辑，但为确保全局逻辑统一，保留或调整)
    MouseArea {
        anchors.fill: parent
        enabled: winControls.width > 36 // 处于展开态
        z: winControls.z - 1
        onClicked: {
            // 这里可以触发 winControls 的收起逻辑，但由于 WindowControls.qml 是独立组件，
            // 建议在组件内处理或通过信号透传。
            // 简单起见，如果 winControls.qml 的 Rectangle 有 id 可以直接操作
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
