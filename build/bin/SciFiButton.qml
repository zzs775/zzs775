import QtQuick

// 只定义"一个按钮"长什么样
Item {
    id: btn
    
    // 默认尺寸
    width: 110
    height: 34

    // 自定义属性
    property string text: "按钮"
    property color mainColor: "#0efcff"      // 边框色
    property color hoverColor: "#ffffff"     // 悬停文字色
    property color bgHoverColor: "#1a1a1a"   // 悬停背景色（深灰）

    // 信号
    signal clicked()

    // 背景框
    Rectangle {
        id: bgRect
        anchors.fill: parent
        
        // 🌟 绝对关键：默认颜色必须是 transparent，不能是 white
        color: mouseArea.containsMouse ? btn.bgHoverColor : "transparent"
        
        border.color: btn.mainColor
        border.width: 1
        radius: 2
        opacity: mouseArea.containsMouse ? 1.0 : 0.9
    }

    // 文字
    Text {
        anchors.centerIn: parent
        text: btn.text
        font.family: "Microsoft YaHei"
        font.bold: true
        font.pixelSize: 13
        color: mouseArea.containsMouse ? btn.hoverColor : btn.mainColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        clip: true
    }

    // 鼠标交互
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            console.log("[按钮点击] " + btn.text)
            btn.clicked() // 触发信号
        }
    }
}
