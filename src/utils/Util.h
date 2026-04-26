//
// Created by LMR on 25-8-2.
//

#pragma once
#include <QIcon>
#include <QRect>
#include <QString>

#include <optional>

namespace about {
const QString& IntroductionText();
}

namespace utils {
void LoadStyleSheet(const QString& stylePath);

QString generateDeviceId();

QString macAddress();

QString GetAppName(const QString& appPath);

QIcon GetAppIcon(const QString& appPath);

QString GetClipboardSourceAppPath();

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
std::optional<QRect> GetFocusCaretPosition();
#endif

#ifdef Q_OS_WIN
QString GetProcessPath(HWND hwnd);
#endif

#ifdef Q_OS_MAC
QString GetFrontmostAppPath();

QIcon GetAppIconFromBundle(const QString& appPath);
#endif
} // namespace utils
