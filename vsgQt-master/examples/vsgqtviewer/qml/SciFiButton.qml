import QtQuick

// 贴图驱动的科幻按钮 — 使用 BorderImage 九宫格自适应拉伸
Item {
    id: btn

    // 默认尺寸
    width: 110
    height: 34

    // 自定义属性
    property string text: "按钮"
    property color mainColor: "#0efcff"      // 文字色
    property color hoverColor: "#ffffff"     // 悬停文字色
    property bool mirrored: false            // 镜像翻转（右侧按钮用）

    // 信号
    signal clicked()

    // ========== 图片边框（九宫格） ==========
    BorderImage {
        id: borderBg
        anchors.fill: parent
        source: "qrc:///images/btn_border.png"

        // 九宫格切图参数：保护边角不被拉伸
        border.left:   12
        border.right:  12
        border.top:    10
        border.bottom: 10

        // 右侧按钮水平镜像翻转
        mirror: btn.mirrored

        // 悬停效果：亮度变化
        opacity: mouseArea.containsMouse ? 1.0 : 0.85

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }
    }

    // ========== 悬停高亮叠加层 ==========
    Rectangle {
        anchors.fill: parent
        color: "#0efcff"
        opacity: mouseArea.containsMouse ? 0.08 : 0
        radius: 2

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }
    }

    // ========== 文字（绝对居中） ==========
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 0
        text: btn.text
        font.family: "Microsoft YaHei"
        font.bold: true
        font.pixelSize: 13
        color: mouseArea.containsMouse ? btn.hoverColor : btn.mainColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        padding: 0
        topPadding: 0
        bottomPadding: 0
        leftPadding: 0
        rightPadding: 0

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    // ========== 鼠标交互 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            console.log("[按钮点击] " + btn.text)
            btn.clicked()
        }
    }
}
