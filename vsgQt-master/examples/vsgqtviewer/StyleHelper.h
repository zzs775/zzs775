#pragma once
#include <QString>

inline QString getTechStyleSheet() {
    return R"(
        /* 全局按钮默认样式 */
        QPushButton {
            background-color: rgba(0, 30, 60, 180); /* 半透明深蓝底 */
            border: 2px solid #00FFFF;             /* 青色边框 */
            border-radius: 5px;                    /* 切角效果可以通过 border-image 图片实现，这里用圆角代替 */
            color: #00FFFF;                        /* 青色文字 */
            font-family: "Microsoft YaHei";
            font-size: 16px;
            font-weight: bold;
            padding: 5px;
        }

        /* 鼠标悬停效果 */
        QPushButton:hover {
            background-color: rgba(0, 255, 255, 50); /* 变亮 */
            border: 2px solid #FFFFFF;
            color: #FFFFFF;
        }

        /* 鼠标按下效果 */
        QPushButton:pressed {
            background-color: rgba(0, 255, 255, 100);
            border-color: #008888;
        }

        /* 特殊标题样式 (模拟你图中的大标题) */
        QPushButton#TitleBtn {
            background-color: transparent;
            border: none;
            font-size: 24px;
            color: #00FFFF;
            border-bottom: 2px solid #00FFFF; /* 只有下划线 */
        }

        /* 针对悬浮面板的样式 */
        FloatingPanel {
            /* 使用 border-image 自动拉伸图片 */
            border-image: url(:/images/image_4ce07d.png) 15 15 15 15 stretch;
            border-width: 15px; /* 对应上面的切片大小 */
            background-color: rgba(0, 30, 60, 220); /* 稍微深一点的背景，防止文字看不清 */
        }
    )";
}
