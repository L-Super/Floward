//
// Created by LMR on 25-7-27.
//

#pragma once

#include <QObject>

class AutoStartup : public QObject {
  Q_OBJECT
public:
  AutoStartup();
  ~AutoStartup() override = default;
  void SetAutoStartup(bool enable = false);
  bool IsAutoStartup();

private:
#if defined(Q_OS_WIN)
  void SetWinAutoStartup(bool enable);
  bool IsWinAutoStartup();
#elif defined(Q_OS_LINUX)
  void SetLinuxAutoStartup(bool enable);
  bool IsLinuxAutoStartup();
#elif defined(Q_OS_MACOS)
  void SetMacAutoStartup(bool enable);
  bool IsMacAutoStartup();
#endif

private:
  const QString winRegistry{R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"};
  // const QString winRegistry{"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"}; // 需要管理员
};
