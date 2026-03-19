import QtQuick
import QtQuick.Layouts 1.15
import QtQuick.Controls

Rectangle {
    id: root
    width: 450
    height: 560
    color: "#dd000000"
    border.color: theme.neonCyan
    border.width: 2
    radius: 6

    property string selectedModel: ""
    property string selectedCategory: ""
    property var expandedItems: ({})
    property string lockedModelId: ""
    // 已添加模型分类
    property string placedCategory: "全部"

    signal modelSelected(string fileName, string category)
    signal closed()

    // 是否显示"已添加模型" Tab
    property bool showPlacedTab: false

    function toggleExpand(itemId) {
        var copy = expandedItems
        if (copy[itemId]) {
            delete copy[itemId]
        } else {
            copy[itemId] = true
        }
        expandedItems = copy
    }

    function isExpanded(itemId) {
        return expandedItems[itemId] === true
    }

    // 监听 C++ 的视角解除锁定信号
    Connections {
        target: typeof ControlBridge !== "undefined" ? ControlBridge : null
        function onTargetUntethered() {
            root.lockedModelId = ""
            console.log("[QML] 收到目标跟随解除信号，已重置锁定按钮状态")
        }
    }

    QtObject {
        id: theme
        property color neonCyan: "#0efcff"
        property color neonGreen: "#00ff88"
        property color neonOrange: "#ff8800"
        property color neonRed: "#ff4444"
        property color headerBg: "#1a1a1a"
        property color rowHover: "#2a2a2a"
        property color rowSelected: "#1a3a4a"
        property color textNormal: "#ffffff"
        property color textDim: "#aaaaaa"
        property color detailBg: "#0d1a1f"
        property font headerFont: Qt.font({ family: "Microsoft YaHei", bold: true, pixelSize: 14 })
        property font cellFont: Qt.font({ family: "Microsoft YaHei", pixelSize: 13 })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // ═══════ 标题栏（可拖动） ═══════
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

            // 拖动区域 (覆盖整个标题栏，但让子项优先)
            MouseArea {
                id: dragArea
                anchors.fill: parent
                property point lastPos: Qt.point(0, 0)
                cursorShape: Qt.SizeAllCursor
                
                onPressed: (mouse) => {
                    lastPos = Qt.point(mouse.x, mouse.y)
                }
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var delta = Qt.point(mouse.x - lastPos.x, mouse.y - lastPos.y)
                        root.x += delta.x
                        root.y += delta.y
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 10

                Text {
                    text: showPlacedTab ? "已添加模型" : "模型列表"
                    font: theme.headerFont
                    color: theme.neonCyan
                }

                Item { Layout.fillWidth: true }

                // 已添加模型计数
                Text {
                    text: showPlacedTab ? (placedListView.count + " 个") : ""
                    font.pixelSize: 11
                    color: theme.textDim
                    visible: showPlacedTab
                }

                Rectangle {
                    width: 30
                    height: 30
                    radius: 4
                    color: closeMouseArea.containsMouse ? "#33ff0000" : "transparent"
                    border.color: theme.neonCyan
                    border.width: 1

                    Text {
                        text: "X"
                        font.pixelSize: 16
                        font.bold: true
                        color: closeMouseArea.containsMouse ? "#ff4444" : theme.neonCyan
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: closeMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: 10 // 确保在拖动层之上
                        onClicked: {
                            console.log("[ModelListPanel] 正在关闭面板")
                            root.visible = false
                            root.closed()
                        }
                    }
                }
            }
        }

        // ═══════ 分类标签栏 ═══════
        Rectangle {
            id: categoryBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#0a0a0a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Repeater {
                    model: ["全部", "飞机", "导弹", "其他", "已添加模型"]

                    Rectangle {
                        property bool isActive: {
                            if (modelData === "已添加模型") return showPlacedTab
                            if (showPlacedTab) return false
                            return selectedCategory === modelData
                        }

                        Layout.preferredWidth: catBtnText.implicitWidth + 14
                        Layout.preferredHeight: 26
                        radius: 3
                        color: isActive ? theme.neonCyan : (catBtnMouseArea.containsMouse ? "#2a2a2a" : "#1a1a1a")
                        border.color: isActive ? theme.neonCyan : "#333333"
                        border.width: 1

                        Text {
                            id: catBtnText
                            text: modelData
                            font.pixelSize: 11
                            color: isActive ? "#000000" : theme.textNormal
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: catBtnMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData === "已添加模型") {
                                    showPlacedTab = true
                                    placedCategory = "全部"
                                } else {
                                    showPlacedTab = false
                                    selectedCategory = modelData
                                    filterModels()
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ═══════ 已添加模型子分类栏 ═══════
        Rectangle {
            id: placedSubCategoryBar
            Layout.fillWidth: true
            Layout.preferredHeight: showPlacedTab ? 32 : 0
            color: "#111111"
            visible: showPlacedTab
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Repeater {
                    model: ["全部", "飞机", "导弹", "其他"]

                    Rectangle {
                        Layout.preferredWidth: subCatText.implicitWidth + 12
                        Layout.preferredHeight: 22
                        radius: 2
                        color: placedCategory === modelData ? "#0efcff" : (subCatMouse.containsMouse ? "#222" : "#181818")
                        border.color: placedCategory === modelData ? "#0efcff" : "#2a2a2a"
                        border.width: 1

                        Text {
                            id: subCatText
                            text: modelData
                            font.pixelSize: 10
                            color: placedCategory === modelData ? "#000" : "#ccc"
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: subCatMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                placedCategory = modelData
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ═══════ 主列表区域 ═══════
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: showPlacedTab ? 1 : 0

            // ────── Page 0: 模型目录列表 ──────
            ListView {
                id: modelListView
                clip: true
                model: modelListModel
                spacing: 2

                delegate: Rectangle {
                    id: catalogDelegate
                    width: modelListView.width
                    height: 50
                    color: selectedModel === fileName ? theme.rowSelected : (catalogHover.containsMouse ? theme.rowHover : "transparent")

                    Rectangle {
                        width: parent.width; height: 1
                        color: theme.neonCyan; opacity: 0.1
                        anchors.bottom: parent.bottom
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15; anchors.rightMargin: 15
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            radius: 4
                            color: category === "飞机" ? "#1a4a6a" : (category === "导弹" ? "#4a1a1a" : "#3a3a3a")
                            border.color: category === "飞机" ? theme.neonCyan : (category === "导弹" ? theme.neonOrange : "#555555")
                            border.width: 1
                            Text {
                                text: category === "飞机" ? "✈" : (category === "导弹" ? "🚀" : "📦")
                                font.pixelSize: 16; color: "#ffffff"; anchors.centerIn: parent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text {
                                text: displayName
                                font.family: "Microsoft YaHei"; font.bold: true; font.pixelSize: 13
                                color: theme.textNormal; Layout.fillWidth: true; elide: Text.ElideRight
                            }
                            Text {
                                text: category + " | " + fileName
                                font.pixelSize: 11; color: theme.textDim
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 60; Layout.preferredHeight: 28; radius: 3
                            color: selectBtnMouse.containsMouse ? theme.neonGreen : "transparent"
                            border.color: theme.neonGreen; border.width: 1
                            Text {
                                text: "选择"; font.pixelSize: 12
                                color: selectBtnMouse.containsMouse ? "#000000" : theme.neonGreen
                                anchors.centerIn: parent
                            }
                            MouseArea {
                                id: selectBtnMouse
                                anchors.fill: parent; hoverEnabled: true
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
                        id: catalogHover
                        anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton
                    }
                }

                ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
            }

            // ────── Page 1: 已添加模型列表 ──────
            Item {
                ListView {
                    id: placedListView
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: batchDeleteBar.top
                    clip: true
                    model: typeof simModel !== "undefined" ? simModel : null
                    spacing: 1

                    ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }

                    delegate: Column {
                        id: delegateCol
                        width: placedListView.width

                        // 根据分类过滤项
                        property bool isMatch: {
                            if (placedCategory === "全部") return true;
                            if (placedCategory === "飞机" && entityType === "Aircraft") return true;
                            if (placedCategory === "导弹" && entityType === "Missile") return true;
                            if (placedCategory === "其他" && entityType !== "Aircraft" && entityType !== "Missile") return true;
                            return false;
                        }

                        visible: isMatch
                        height: isMatch ? (isExpanded(entityId) ? 44 + detailCol.implicitHeight + 16 : 44) : 0

                        // 模型条目行
                        Rectangle {
                            width: parent.width
                            height: 44
                            color: rowClickArea.containsMouse ? theme.rowHover : (index % 2 === 0 ? "transparent" : "#08ffffff")

                            Rectangle {
                                width: parent.width; height: 1
                                color: theme.neonCyan; opacity: 0.1; anchors.bottom: parent.bottom
                            }

                            // 整行点击区域（已移除双击跟随，改为单选或空操作）
                            MouseArea {
                                id: rowClickArea
                                anchors.fill: parent
                                anchors.rightMargin: 100 // 为锁定按钮留出更多空间
                                hoverEnabled: true
                                z: 0
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10; anchors.rightMargin: 10
                                spacing: 8
                                z: 1

                                // 展开/折叠箭头
                                Text {
                                    text: isExpanded(entityId) ? "▼" : "▶"
                                    font.pixelSize: 11; color: theme.neonCyan
                                    Layout.preferredWidth: 16
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.margins: -10
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: { toggleExpand(entityId) }
                                    }
                                }

                                // 类型图标
                                Rectangle {
                                    Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 4
                                    color: entityType === "Aircraft" ? "#1a4a6a" : (entityType === "Missile" ? "#4a1a1a" : "#3a3a3a")
                                    border.color: entityType === "Aircraft" ? theme.neonCyan : (entityType === "Missile" ? theme.neonOrange : "#555555")
                                    border.width: 1
                                    Text {
                                        text: entityType === "Aircraft" ? "✈" : (entityType === "Missile" ? "🚀" : "📦")
                                        font.pixelSize: 14; color: "#fff"; anchors.centerIn: parent
                                    }
                                }

                                // 名称
                                Text {
                                    text: entityName
                                    font.family: "Microsoft YaHei"; font.bold: true; font.pixelSize: 13
                                    color: theme.textNormal; Layout.fillWidth: true; elide: Text.ElideRight
                                }

                                // 6位ID标签
                                Rectangle {
                                    Layout.preferredWidth: idLabel.implicitWidth + 12
                                    Layout.preferredHeight: 20; radius: 3
                                    color: "#1a2a2a"; border.color: "#333"; border.width: 1
                                    Text {
                                        id: idLabel
                                        text: entityId
                                        font.pixelSize: 11; font.family: "Consolas"
                                        color: theme.neonCyan; anchors.centerIn: parent
                                    }
                                }

                                // 数据按钮 (新增)
                                Rectangle {
                                    id: dataBtn
                                    property bool isObserving: typeof ControlBridge !== "undefined" && ControlBridge.activeDataEntityId === entityId
                                    Layout.preferredWidth: 44; Layout.preferredHeight: 24; radius: 3
                                    color: isObserving ? theme.neonCyan : (dataBtnMouse.containsMouse ? "#2244bb" : "transparent")
                                    border.color: isObserving ? theme.neonCyan : theme.neonCyan
                                    border.width: 1

                                    Text {
                                        text: "数据"
                                        font.pixelSize: 11; font.bold: true
                                        color: dataBtn.isObserving ? "#000000" : (dataBtnMouse.containsMouse ? "#fff" : theme.neonCyan)
                                        anchors.centerIn: parent
                                    }

                                    MouseArea {
                                        id: dataBtnMouse
                                        anchors.fill: parent; hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            console.log("[PlacedModel] 展示遥测数据 ID:" + entityId)
                                            if (typeof ControlBridge !== "undefined") {
                                                ControlBridge.showTelemetry(entityId)
                                            }
                                        }
                                    }
                                }

                                // 锁定/取消按钮
                                Rectangle {
                                    id: lockBtn
                                    property bool isLocked: root.lockedModelId === entityId
                                    Layout.preferredWidth: 60; Layout.preferredHeight: 24; radius: 3
                                    color: isLocked ? theme.neonOrange : (lockBtnMouse.containsMouse ? "#224444" : "transparent")
                                    border.color: isLocked ? theme.neonOrange : theme.neonCyan
                                    border.width: 1

                                    Text {
                                        text: parent.isLocked ? "取消" : "锁定"
                                        font.pixelSize: 11; font.bold: true
                                        color: parent.isLocked ? "#000" : (lockBtnMouse.containsMouse ? "#fff" : theme.neonCyan)
                                        anchors.centerIn: parent
                                    }

                                    MouseArea {
                                        id: lockBtnMouse
                                        anchors.fill: parent; hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (lockBtn.isLocked) {
                                                console.log("[PlacedModel] 取消锁定 ID:" + entityId)
                                                root.lockedModelId = ""
                                                if (typeof ControlBridge !== "undefined") ControlBridge.untether()
                                            } else {
                                                console.log("[PlacedModel] 锁定模型 ID:" + entityId)
                                                root.lockedModelId = entityId
                                                if (typeof ControlBridge !== "undefined") ControlBridge.focusEntity(entityId)
                                            }
                                        }
                                    }
                                }

                                // 删除按钮
                                Rectangle {
                                    Layout.preferredWidth: 24; Layout.preferredHeight: 24; radius: 3
                                    color: delBtnMouse.containsMouse ? "#44ff0000" : "transparent"
                                    border.color: theme.neonRed; border.width: 1
                                    z: 2
                                    Text {
                                        text: "✕"; font.pixelSize: 12; font.bold: true
                                        color: delBtnMouse.containsMouse ? "#ff2222" : "#884444"
                                        anchors.centerIn: parent
                                    }
                                    MouseArea {
                                        id: delBtnMouse
                                        anchors.fill: parent; hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        z: 3
                                        onClicked: {
                                            console.log("[PlacedModel] 删除模型 ID:" + entityId)
                                            if (typeof simModel !== "undefined") {
                                                simModel.removeEntity(entityId)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 展开的详情区域
                        Rectangle {
                            width: parent.width
                            height: isExpanded(entityId) ? detailCol.implicitHeight + 16 : 0
                            color: theme.detailBg
                            clip: true
                            visible: height > 0

                            Behavior on height {
                                NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                            }

                            Rectangle {
                                width: parent.width; height: 1
                                color: theme.neonCyan; opacity: 0.15; anchors.top: parent.top
                            }

                            Column {
                                id: detailCol
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8; anchors.leftMargin: 40
                                spacing: 3

                                Repeater {
                                    model: [
                                        { label: "ID", value: entityId },
                                        { label: "类型", value: entityType },
                                        { label: "名称", value: entityName },
                                        { label: "纬度", value: entityLat !== undefined ? entityLat.toFixed(6) : "N/A" },
                                        { label: "经度", value: entityLon !== undefined ? entityLon.toFixed(6) : "N/A" },
                                        { label: "高度", value: entityAlt !== undefined ? entityAlt.toFixed(1) + " m" : "N/A" },
                                        { label: "航向", value: entityHeading !== undefined ? entityHeading.toFixed(1) + "°" : "N/A" },
                                        { label: "速度", value: entitySpeed !== undefined ? entitySpeed.toFixed(1) + " m/s" : "N/A" },
                                        { label: "方位角", value: entityYaw !== undefined ? entityYaw.toFixed(2) + "°" : "N/A" },
                                        { label: "俯仰角", value: entityPitch !== undefined ? entityPitch.toFixed(2) + "°" : "N/A" },
                                        { label: "滚转角", value: entityRoll !== undefined ? entityRoll.toFixed(2) + "°" : "N/A" }
                                    ]

                                    Row {
                                        spacing: 6
                                        Text {
                                            text: modelData.label + ":"
                                            font.pixelSize: 11; color: theme.neonCyan
                                            width: 50; horizontalAlignment: Text.AlignRight
                                        }
                                        Text {
                                            text: modelData.value
                                            font.pixelSize: 11; font.family: "Consolas"
                                            color: theme.textNormal
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 批量删除栏
                Rectangle {
                    id: batchDeleteBar
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: showPlacedTab && placedListView.count > 0 ? 40 : 0
                    visible: showPlacedTab && placedListView.count > 0
                    color: "#111111"

                    Rectangle {
                        width: parent.width; height: 1
                        color: theme.neonCyan; opacity: 0.2; anchors.top: parent.top
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15; anchors.rightMargin: 15

                        Text {
                            text: "共 " + placedListView.count + " 个模型"
                            font.pixelSize: 12; color: theme.textDim
                        }

                        Item { Layout.fillWidth: true }

                        // 批量删除按钮
                        Rectangle {
                            Layout.preferredWidth: batchDelText.implicitWidth + 20
                            Layout.preferredHeight: 26; radius: 3
                            color: batchDelMouse.containsMouse ? "#44ff0000" : "transparent"
                            border.color: theme.neonRed; border.width: 1

                            Text {
                                id: batchDelText
                                text: "清空全部"
                                font.pixelSize: 12
                                color: batchDelMouse.containsMouse ? "#ff2222" : "#aa4444"
                                anchors.centerIn: parent
                            }

                            MouseArea {
                                id: batchDelMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    console.log("[PlacedModel] 批量删除全部模型")
                                    if (typeof simModel !== "undefined") {
                                        simModel.clearAll()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 空状态提示
    Text {
        text: showPlacedTab ? "暂无已添加模型" : "暂无模型数据"
        font.pixelSize: 14
        color: theme.textDim
        anchors.centerIn: parent
        visible: showPlacedTab ? (placedListView.count === 0) : (modelListView.count === 0)
    }

    function filterModels() {
        if (typeof modelListModel !== "undefined") {
            modelListModel.setCategoryFilter(selectedCategory)
        }
    }

    Component.onCompleted: {
        console.log("[ModelListPanel] 面板已加载")
    }
}

