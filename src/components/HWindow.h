/*
 * HWindow
 * Lightweight frameless window helper for Qt Widgets on Windows.
 *
 * Author: Huang Xiaosong (woshixs)
 * GitHub: https://github.com/woshi-xs
 * Email: woshixs123@gmail.com
 *
 * Copyright (c) 2026 Huang Xiaosong (woshi-xs)
 * SPDX-License-Identifier: MIT
 * Repository: https://github.com/woshi-xs/HWindow
 */

/*
 * 强烈建议设置背景色时不要样式表和函数混用
 * 推荐使用函数接口设置背景色
 */

#ifndef HWINDOW_HXS
#define HWINDOW_HXS

#include <QWidget>

//顶层窗口，负责无边框功能
class HWidget : public QWidget
{
    Q_OBJECT

public:
    HWidget(QWidget *parent = nullptr);
    ~HWidget();
protected:
    ///
    /// \brief nativeEvent  拦截Win系统底层消息，类似于mouseEvent就是高层消息
    /// \param eventType    Qt的事件类型标识
    /// \param message      指向操作系统消息的指针，配合操作系统判断
    /// \param result       通过它告诉操作系统这个消息的返回值，例如拖动、缩放或客户区
    /// \return
    ///
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
};

//标题栏，负责拖动，贴边和双击最大化/还原功能
class HTitleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HTitleWidget(QWidget *parent = nullptr);
    ///
    /// \brief setBackgroundColor   指定背景颜色
    /// \param BackgroundColor      背景颜色
    ///
    void setBackgroundColor(QColor BackgroundColor);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

#endif // HWINDOW_HXS
