/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** Modified by the webOS ports project
** Copyright (C) 2013 Simon Busch <morphis@gravedo.de>
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qweboswindow.h"

#define MESSAGES_INTERNAL_FILE "SysMgrMessagesInternal.h"
#include <PIpcMessageMacros.h>
#include <PIpcChannel.h>

#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QWebosWindow::QWebosWindow(QWebosWindowManagerClient *client, WebosSurfaceManagerClient *surfaceClient,
                           QWindow *w, QWebosScreen *screen)
    : QPlatformWindow(w),
      OffscreenNativeWindow(w->width(), w->height()),
      mWinid(0),
      mClient(client),
      mSurfaceClient(surfaceClient),
      mScreen(screen),
      mBufferSemaphore(0)
{
    // Register our window with the manager to get a id assigned
    QSize size = geometry().size();
    channel()->sendSyncMessage(new ViewHost_PrepareAddWindow((1 << 1),
                size.width(), size.height(), &mWinid));
    mClient->addWindow(this);

#ifdef QEGL_EXTRA_DEBUG
    qWarning("QWebosWindow %p: %p 0x%x\n", this, w, uint(mWinid));
#endif

    QRect screenGeometry(mScreen->availableGeometry());
    if (w->geometry() != screenGeometry) {
        QWindowSystemInterface::handleGeometryChange(w, screenGeometry);
    }

    createSurface();
}

void QWebosWindow::createSurface()
{
    EGLNativeWindowType nativeWindow = static_cast<ANativeWindow*>(this);

    mEglSurface = eglCreateWindowSurface(mScreen->eglDisplay(), mScreen->eglConfig(),
                                         nativeWindow, NULL);
    assert(mEglSurface != EGL_NO_SURFACE);
}

void QWebosWindow::postBuffer(OffscreenNativeWindowBuffer *buffer)
{
    if (mWinid == -1)
        return;

    mSurfaceClient->postBuffer(mWinid, buffer);
}

void QWebosWindow::waitForBuffer(OffscreenNativeWindowBuffer *buffer)
{
    if (mWinid == -1)
        return;

    if (!mBufferSemaphore)
        mBufferSemaphore = new QSystemSemaphore(QString("EGLWindow%1").arg(mWinid), 3,
                                                QSystemSemaphore::Create);

    mBufferSemaphore->acquire();
}

void QWebosWindow::setGeometry(const QRect &)
{
    // We only support full-screen windows
    QRect rect(screen()->availableGeometry());
    QWindowSystemInterface::handleGeometryChange(window(), rect);

    QPlatformWindow::setGeometry(rect);
}

void QWebosWindow::setVisible(bool visible)
{
    if (visible) {
        std::string winProps = "{ "
                    " 'fullScreen': false, " // defaults to false
                    " 'overlayNotificationsPosition': 'bottom', " // options are left, right, top, bottom
                    " 'subtleLightbar': true, " // defaults to false
                    " 'blockScreenTimeout': true, " // defaults to false
                    " 'fastAccelerometer': true, " // defaults to false
                    " 'suppressBannerMessages': false, " // defaults to false
                    " 'hasPauseUi': true " // defaults to false
                    " }";
        channel()->sendAsyncMessage(new ViewHost_SetWindowProperties(winId(), winProps));
        channel()->sendAsyncMessage(new ViewHost_AddWindow(winId()));
        channel()->sendAsyncMessage(new ViewHost_FocusWindow(winId()));
    }
    else {
        channel()->sendAsyncMessage(new ViewHost_RemoveWindow(winId()));
    }

    QPlatformWindow::setVisible(visible);
}

void QWebosWindow::handleFocus(bool focused)
{
    if (focused)
        QWindowSystemInterface::handleWindowActivated(window());
    else
        QWindowSystemInterface::handleWindowActivated(0);
}

void QWebosWindow::handleResize(int width, int height, bool resizeBuffer)
{
    Q_UNUSED(resizeBuffer)
    setGeometry(QRect(0, 0, width, height));
}

void QWebosWindow::handleFullScreenEnabled()
{
}

void QWebosWindow::handleFullScreenDisabled()
{
}

void QWebosWindow::handlePause()
{
}

void QWebosWindow::handleResume()
{
}

void QWebosWindow::handleInputEvent(const SysMgrEventWrapper& wrapper)
{
    SysMgrEvent* e = wrapper.event;
    QPoint mousePos;

    switch(e->type)
    {
        case SysMgrEvent::PenFlick:
            mousePos = QPoint(e->x, e->y);
            QWindowSystemInterface::handleWheelEvent(window(),mousePos,mousePos,e->z,Qt::Vertical);
            break;
        case SysMgrEvent::PenPressAndHold:
            mousePos = QPoint(e->x, e->y);
            QWindowSystemInterface::handleMouseEvent(window(), mousePos, mousePos, Qt::NoButton);
            break;
        case SysMgrEvent::PenDown:
            mousePos = QPoint(e->x, e->y);
            QWindowSystemInterface::handleMouseEvent(window(), mousePos, mousePos, Qt::LeftButton);
            break;
        case SysMgrEvent::PenUp:
            mousePos = QPoint(e->x, e->y);
            QWindowSystemInterface::handleMouseEvent(window(), mousePos, mousePos, Qt::NoButton);
            break;
        case SysMgrEvent::PenMove:
            mousePos = QPoint(e->x, e->y);
            QWindowSystemInterface::handleMouseEvent(window(), mousePos, mousePos, Qt::LeftButton);
            break;
        case SysMgrEvent::Accelerometer:
        case SysMgrEvent::GestureStart:
        case SysMgrEvent::GestureEnd:
        case SysMgrEvent::GestureCancel:
            break;
        default:
            break;
    }
}

void QWebosWindow::handleTouchEvent(const SysMgrTouchEvent& touchEvent)
{
    Q_UNUSED(touchEvent);
#if 0
    QEvent::Type type = QEvent::None;

    QList<QWindowSystemInterface::TouchPoint> touchPoints;
    for (unsigned int i = 0; i < touchEvent.numTouchPoints; i++) {
        QWindowSystemInterface::TouchPoint touchPoint;
        QPoint pt(touchEvent.touchPoints[i].x, touchEvent.touchPoints[i].y);

        // get size of screen which contains window
        QPlatformScreen *platformScreen = QPlatformScreen::platformScreenForWindow(window());
        QSizeF screenSize = platformScreen->physicalSize();

        touchPoint.id = touchEvent.touchPoints[i].id;

        // update cached position of current touch point
        touchPoint.normalPosition = QPointF( static_cast<qreal>(pt.x()) / screenSize.width(), static_cast<qreal>(pt.y()) / screenSize.height() );
        touchPoint.area = QRectF( pt.x(), pt.y(), 0.0, 0.0 );
        touchPoint.pressure = 1;

        touchPoint.state = static_cast<Qt::TouchPointState>(touchEvent.touchPoints[i].state);

        // FIXME: what if the touchpoints have different states? does this ever happen?
        switch (touchPoint.state) {
            case Qt::TouchPointPressed:
                type = QEvent::TouchBegin;
                break;
            case Qt::TouchPointMoved:
                type = QEvent::TouchUpdate;
                break;
            case Qt::TouchPointReleased:
                type = QEvent::TouchEnd;
                break;
            case Qt::TouchPointStationary:
                type = QEvent::TouchUpdate;
                break;
        }

        touchPoints.append(touchPoint);

        QWindowSystemInterface::handleMouseEvent(window(), pt, pt, (touchPoint.state != Qt::TouchPointReleased ? Qt::LeftButton : Qt::NoButton));
    }

    QWindowSystemInterface::handleTouchEvent(window(), type, QTouchEvent::TouchScreen, touchPoints);
#endif
}

void QWebosWindow::handleKeyEvent(const SysMgrKeyEvent& keyEvent)
{
    QKeyEvent ev = keyEvent.qtEvent();
    Qt::Key key;
    if (ev.key() == 0x01200001)
        key = Qt::Key_Backspace;
    else
        key = (Qt::Key)ev.key();
    QWindowSystemInterface::handleKeyEvent(window(), ev.type(), key, ev.modifiers(), ev.text(), 0, 0);
}

PIpcChannel* QWebosWindow::channel() const
{
    return mClient->channel();
}

QT_END_NAMESPACE
