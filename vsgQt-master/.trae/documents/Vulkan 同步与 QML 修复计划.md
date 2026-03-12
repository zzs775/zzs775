## 修复计划

### 一、main.cpp 修复 (4处修改)

**修复1: Resize 逻辑添加 GPU 同步**
- 位置: 第 285 行附近 `if (curW != lastW || curH != lastH)` 块内
- 操作: 在尺寸变化时先调用 `viewer->deviceWaitIdle()` 等待 GPU 完成

**修复2: 添加窗口暴露检测**
- 位置: 第 267 行 lambda 开头
- 操作: 添加 `!vsgWindow->isExposed()` 判断，跳过未暴露窗口的渲染

**修复3: 退出时安全清理**
- 位置: 第 318 行 `return app.exec();` 之前
- 操作: 连接 `QCoreApplication::aboutToQuit` 信号，调用 `viewer->deviceWaitIdle()`

**修复4: 改进异常处理**
- 位置: 第 306-315 行 `try-catch` 块
- 操作: 替换空 `catch(...)` 为具体的异常捕获和日志输出

---

### 二、ModelListPanel.qml 修复 (1处修改)

**修复5: 添加缺失的 QML 导入**
- 位置: 文件头部第 1-2 行
- 操作: 添加 `import QtQuick.Controls` 导入 ScrollBar 组件