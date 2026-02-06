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
QString generateDeviceId();

QString macAddress();

QString GetAppName(const QString& appPath);

QIcon GetAppIcon(const QString& appPath);

QString GetClipboardSourceAppPath();

#ifdef Q_OS_WIN
QString GetProcessPath(HWND hwnd);

std::optional<QRect> GetFocusCaretPosition();
#endif
} // namespace utils
