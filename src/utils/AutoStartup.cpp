//
// Created by LMR on 25-7-27.
//

#include "AutoStartup.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString plistTemplate = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>%1</string>
    <key>ProgramArguments</key>
    <array>
        <string>%2</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <false/>
</dict>
</plist>
)";
}

AutoStartup::AutoStartup() {}

void AutoStartup::SetAutoStartup(bool enable) {
#if defined(Q_OS_WIN)
  SetWinAutoStartup(enable);
#elif defined(Q_OS_LINUX)
  SetLinuxAutoStartup(enable);
#elif defined(Q_OS_MACOS)
  SetMacAutoStartup(enable);
#endif
}

bool AutoStartup::IsAutoStartup() {
#if defined(Q_OS_WIN)
  return IsWinAutoStartup();
#elif defined(Q_OS_LINUX)
  return IsLinuxAutoStartup();
#elif defined(Q_OS_MACOS)
  return IsMacAutoStartup();
#endif

  // another return true
  return true;
}

#if defined(Q_OS_WIN)
void AutoStartup::SetWinAutoStartup(bool enable) {
  QSettings settings(winRegistry, QSettings::Format::NativeFormat);
  auto appPath = QCoreApplication::applicationFilePath();
  // 程序名称作为注册表中的键,根据键获取对应的值（程序路径）
  QFileInfo fileInfo(appPath);
  // the name as a key
  QString name = fileInfo.baseName();
  if (enable) {
    // second modify
    if (settings.contains(name)) {
      // 如果注册表中的路径和当前程序路径不一样，则表示没有设置自启动或本自启动程序已经更换了路径
      // 获取注册表的路径
      QString oldPath = settings.value(name).toString();
      qDebug() << oldPath;
      // '/' to '\'
      QString newPath = QDir::toNativeSeparators(appPath);
      if (oldPath != newPath)
        settings.setValue(name, newPath);
    }
    else // first setting
    {
      settings.setValue(name, QDir::toNativeSeparators(appPath));
    }
  }
  else {
    // cancel auto startup
    settings.remove(name);
  }
}

bool AutoStartup::IsWinAutoStartup() {
  QSettings settings(winRegistry, QSettings::Format::NativeFormat);
  auto appPath = QCoreApplication::applicationFilePath();
  // 程序名称作为注册表中的键,根据键获取对应的值（程序路径）
  QFileInfo fileInfo(appPath);
  // the name as a key
  QString name = fileInfo.baseName();
  if (settings.contains(name)) {
    return true;
  }
  return false;
}

#elif defined(Q_OS_LINUX)
void AutoStartup::SetLinuxAutoStartup(bool enable) {
  QString text{R"([Desktop Entry]
Categories=Network;Office;
Name=com.Floward.Floward
Name[zh_CN]=Floward
Keywords=clipboard
Keywords[zh_CN]=剪贴板
Type=Application
Exec=/opt/apps/com.Floward.Floward/files/Floward
Icon=logo
X-Deepin-CreatedBy=com.deepin.SessionManager
X-Deepin-AppID=com.Floward.Floward
Hidden=)"};
  // enable=false -> Hidden=true | enable=true -> Hidden=false
  text.append(enable ? "false\n" : "true\n");

  // need /home/{user}/.config/autostart/com.xxx.desktop path
  auto path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  QFile file(path + "/autostart/com.Floward.Floward.desktop");

  try {
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qDebug() << "Autostart file write failed";
      return;
    }
    QTextStream out(&file);

    // write data
    out << text;
  }
  catch (...) {
    qDebug() << "AutoStart file write fail, raising Exception";
  }

  file.close();
}

bool AutoStartup::IsLinuxAutoStartup() {
  auto path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

  QFile file(path + "/autostart/com.Floward.Floward.desktop");
  try {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "AutoStart file open failed or not exist";
      return false;
    }
  }
  catch (...) {
    qDebug() << "AutoStart file open failed or not exist, raising Exception";
  }

  QTextStream in(&file);
  QString lastLine;
  QString value;
  while (!in.atEnd()) {
    lastLine = in.readLine();

    auto pos = lastLine.indexOf("Hidden");
    if (pos != -1) {
      // method1:输出右边第N个字符串
      // value = (lastLine.right(lastLine.size() - pos - 7)).trimmed();
      QStringList kv = lastLine.split("=");
      value = kv[1].trimmed();
    }
  }

  file.close();
  return value == "false" ? true : false;
}

#elif defined(Q_OS_MACOS)
void AutoStartup::SetMacAutoStartup(bool enable) {
  QString launchAgentDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Library/LaunchAgents";
  // 尝试从App的Info.plist中读取CFBundleIdentifier
  QSettings settings(QCoreApplication::applicationDirPath() + "/../Info.plist", QSettings::NativeFormat);
  QString bundleIdentifier = settings.value("CFBundleIdentifier").toString();
  if (bundleIdentifier.isEmpty()) {
    bundleIdentifier = "com.Floward.Floward";
  }
  QString plistFileName = bundleIdentifier + ".plist";
  QString plistFilePath = launchAgentDirPath + "/" + plistFileName;

  if (enable) {
    QDir dir(launchAgentDirPath);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        qWarning() << "Failed to create LaunchAgents directory";
        return;
      }
    }

    QString appPath = QCoreApplication::applicationFilePath();

    // fill template string
    QString plistContent = plistTemplate.arg(bundleIdentifier, appPath);

    QFile plistFile(plistFilePath);
    if (!plistFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qWarning() << "Failed to open plist file for writing:" << plistFilePath;
      return;
    }
    plistFile.write(plistContent.toUtf8());
    plistFile.close();

    qDebug() << "Auto-start enabled. Plist file created at:" << plistFilePath;
  }
  else {
    QFile plistFile(plistFilePath);
    if (plistFile.exists()) {
      if (!plistFile.remove()) {
        qWarning() << "Failed to remove plist file:" << plistFilePath;
      }
      qDebug() << "Auto-start disabled. Plist file removed.";
    }
  }
}

bool AutoStartup::IsMacAutoStartup() {
  QString launchAgentDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Library/LaunchAgents";
  // 尝试从App的Info.plist中读取CFBundleIdentifier
  QSettings settings(QCoreApplication::applicationDirPath() + "/../Info.plist", QSettings::NativeFormat);
  QString bundleIdentifier = settings.value("CFBundleIdentifier").toString();
  if (bundleIdentifier.isEmpty()) {
    bundleIdentifier = "com.Floward.Floward";
  }
  QString plistFileName = bundleIdentifier + ".plist";
  QString plistFilePath = launchAgentDirPath + "/" + plistFileName;

  QFile plistFile(plistFilePath);
  if (plistFile.exists()) {
    return true;
  }
  return false;
}
#endif