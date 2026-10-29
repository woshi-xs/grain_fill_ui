/*
 * HWindow
 * Implementation of the frameless window helper.
 *
 * Author: Huang Xiaosong (woshixs)
 * GitHub: https://github.com/woshi-xs
 * Email: woshixs123@gmail.com
 *
 * Copyright (c) 2026 Huang Xiaosong (woshi-xs)
 * SPDX-License-Identifier: MIT
 * Repository: https://github.com/woshi-xs/HWindow
 */

#include "HWindow.h"

#include <QMouseEvent>

//顶层窗口，负责无边框功能
#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

HWidget::HWidget(QWidget *parent)
    :QWidget(parent)
{
    // 绝对不要用 FramelessWindowHint，否则系统不给贴边！
    //Qt默认是自带最大化最小化按钮的，这里再设置一下保险
    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowMaximizeButtonHint);

    // 这一步让窗口拥有系统阴影，更现代化和美观
    HWND hwnd = (HWND)this->winId();
    /**
     * 通过 告诉 Windows 虽然我看起来像无边框，但其实我的边框往内容区里延伸了 1 个像素
     * 以此来让 Windows 来给我们画阴影和边框
     */
    const MARGINS shadow = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
}

HWidget::~HWidget()
{

}

bool HWidget::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#if defined(Q_OS_WIN)
    MSG* msg = static_cast<MSG*>(message);
    HWND hwnd = (HWND)msg->hwnd;

    switch (msg->message) {
    /**
     * ----------核心---------
     * ------隐藏系统边框------
     * 普通状态下返回 0，告诉 Windows 整个窗口都是客户区
     * 这样系统原本的标题栏和边框就在视觉上消失了
     *
     * ------修正最大化范围------
     * Windows 最大化窗口时，实际窗口矩形会向工作区外扩展几个像素
     * 无边框后这些像素也会成为客户区，导致界面边缘被屏幕或任务栏裁切
     * 最大化时将客户区限制在当前显示器的工作区，真正全屏时则不作限制
     */
    case WM_NCCALCSIZE: {
        if (msg->wParam == TRUE) {
            auto *params =
                reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);

            const bool maximized = IsZoomed(hwnd);
            const bool fullscreen =
                windowState().testFlag(Qt::WindowFullScreen);

            if (maximized && !fullscreen) {
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(MONITORINFO);

                const HMONITOR monitor =
                    MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

                if (GetMonitorInfo(monitor, &monitorInfo)) {
                    params->rgrc[0] = monitorInfo.rcWork;
                }
            }

            *result = 0;
            return true;
        }

        break;
    }

        // 接管鼠标行为
    case WM_NCHITTEST: {
        const int border = 8; // 边缘缩放感应范围
        RECT winRect;
        GetWindowRect(hwnd, &winRect);

        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);

        // --- 判断边缘缩放 (优先级最高) ---
        bool left = x < winRect.left + border;
        bool right = x >= winRect.right - border;
        bool top = y < winRect.top + border;
        bool bottom = y >= winRect.bottom - border;

        if (top && left) { *result = HTTOPLEFT; return true; }
        if (top && right) { *result = HTTOPRIGHT; return true; }
        if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
        if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
        if (left) { *result = HTLEFT; return true; }
        if (right) { *result = HTRIGHT; return true; }
        if (top) { *result = HTTOP; return true; }
        if (bottom) { *result = HTBOTTOM; return true; }

        *result = HTCLIENT;//表示点击的是客户区、HTCAPTION 是标题栏，在和标题栏组件联合使用时会遇到
        return true;
    }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}






//标题栏，负责拖动，贴边和双击最大化/还原功能
HTitleWidget::HTitleWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);//启用自动背景绘制,不开启设置不了背景色
}

void HTitleWidget::setBackgroundColor(QColor BackgroundColor)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window,BackgroundColor);
    setPalette(pal);
}

void HTitleWidget::mouseMoveEvent(QMouseEvent *event)
{
#if defined(Q_OS_WIN)
    if (event->buttons() & Qt::LeftButton) {
        //拖拽
        HWND hwnd = reinterpret_cast<HWND>(this->window()->winId());
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
#else
    QWidget::mouseMoveEvent(event);
#endif
}

void HTitleWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
#if defined(Q_OS_WIN)
    if (event->button() == Qt::LeftButton) {
        //双击最大化/还原
        HWND hwnd = reinterpret_cast<HWND>(this->window()->winId());
        QPoint globalPos = event->globalPosition().toPoint();
        LPARAM lParam = MAKELPARAM(globalPos.x(), globalPos.y());
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDBLCLK, HTCAPTION, lParam);
    }
#else
    QWidget::mouseDoubleClickEvent(event);
#endif
}
