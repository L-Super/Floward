//
// Created by LMR on 25-8-2.
//

#include "Util.h"

#include <QApplication>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QSysInfo>

#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
#include <oleacc.h>

#pragma comment(lib, "oleacc.lib")
// clang-format on
#endif
namespace about {
const QString& IntroductionText() {
  static const QString aboutText = QStringLiteral(
      "### 简介\n"
      "Floward 是一款跨平台的剪贴板增强工具，提供历史记录管理与多设备实时同步能力，"
      "让复制粘贴在不同电脑之间无缝衔接，显著提升日常工作效率。\n"
      "\n"
      "### 主要特性\n"
      "- 📋 自动记录剪贴板历史，支持文本、图片等多种类型\n"
      "- 🔍 快速检索历史条目，一键回填粘贴\n"
      "- 🔄 多设备实时同步，跨平台无缝协作（Windows / macOS / Linux）\n"
      "- ⚡ 极简界面与全局快捷键，触手可及\n"
      "- 🔒 本地存储，数据安全可控（单机版）\n"
      "\n"
      "### 使用方式\n"
      "按下默认快捷键 `Alt + V` 即可呼出剪贴板面板，使用方向键或鼠标选择条目后回车粘贴。\n"
      "\n"
      "### 版本\n"
      "v%1\n"
      "\n"
      "### 项目地址\n"
      "[L-Super/Floward](https://github.com/L-Super/Floward) — 欢迎 Star、Issue 与 PR")
      .arg(qApp->applicationVersion());

  return aboutText;
}
} // namespace about

namespace utils {
void LoadStyleSheet(const QString& stylePath) {
  QFile style(stylePath);
  if (style.open(QFile::ReadOnly)) {
    qApp->setStyleSheet(style.readAll());
  }
  style.close();
}

QString generateDeviceId() {
  QString result = macAddress();
  // on Linux systems, this ID is usually permanent
  result += QSysInfo::machineUniqueId();
  return result;
}

QString macAddress() {
  QString result;
  foreach (const QNetworkInterface& netInterface, QNetworkInterface::allInterfaces()) {
    if (!(netInterface.flags() & QNetworkInterface::IsLoopBack)) {
      result = netInterface.hardwareAddress();
      break;
    }
  }
  return result;
}

QString GetClipboardSourceAppPath() {
#ifdef Q_OS_WIN
  HWND clipboardOwner = GetClipboardOwner();
  if (!clipboardOwner) {
    return {};
  }

  return GetProcessPath(clipboardOwner);
#elif defined(Q_OS_MAC)
  return GetFrontmostAppPath();
#else
  return {};
#endif
}

#ifdef Q_OS_WIN
QString GetProcessPath(HWND hwnd) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0)
    return {};

  // win 10 later only need PROCESS_QUERY_LIMITED_INFORMATION
  HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!hProc)
    return {};

  wchar_t buf[MAX_PATH] = {};
  DWORD size = MAX_PATH;
  QString result;

  if (QueryFullProcessImageNameW(hProc, 0, buf, &size)) {
    result = QString::fromWCharArray(buf, size);
  }

  CloseHandle(hProc);
  return result;
}

std::optional<QRect> GetFocusCaretPosition() {
  HWND activeWnd = GetForegroundWindow();
  if (!activeWnd || !IsWindow(activeWnd)) {
    qDebug() << "no active hwnd";
    return {};
  }
  qDebug() << "active process" << utils::GetProcessPath(activeWnd);

  HWND focusWnd = nullptr;
  DWORD targetThreadId = GetWindowThreadProcessId(activeWnd, nullptr);
  DWORD currentThreadId = GetCurrentThreadId();

  // 获取目标窗口的焦点句柄
  if (AttachThreadInput(targetThreadId, currentThreadId, TRUE)) {
    focusWnd = GetFocus();
    AttachThreadInput(targetThreadId, currentThreadId, FALSE);
  }
  if (!focusWnd) {
    focusWnd = activeWnd;
  }

  // 1. 通过 IAccessible 尝试获取
  // Valid for the Chromium family
  // Sometimes invalid for JetBrains IDEs; TextInputHost.exe also fails, but it behaves better occasionally.

  // dynamic load
  // static HMODULE hOleacc = LoadLibraryW(L"oleacc.dll");
  // static auto AccessibleObjectFromWindowFn = reinterpret_cast<decltype(::AccessibleObjectFromWindow)*>(
  //     hOleacc ? GetProcAddress(hOleacc, "AccessibleObjectFromWindow") : nullptr);
  //
  // if (AccessibleObjectFromWindowFn)
  {
    IAccessible* pAcc = nullptr;
    // if (AccessibleObjectFromWindowFn(activeWnd, OBJID_CARET, IID_IAccessible, reinterpret_cast<void**>(&pAcc)) ==
    //         S_OK &&
    //     pAcc)
    if (AccessibleObjectFromWindow(focusWnd, OBJID_CARET, IID_IAccessible, reinterpret_cast<void**>(&pAcc)) == S_OK &&
        pAcc) {
      long left = 0, top = 0, width = 0, height = 0;
      VARIANT varCaret;
      varCaret.vt = VT_I4;
      varCaret.lVal = CHILDID_SELF;
      HRESULT hr = pAcc->accLocation(&left, &top, &width, &height, varCaret);
      pAcc->Release();

      if (hr == S_OK && (left != 0 || top != 0)) {
        qDebug() << "AccessibleObjectFromWindow" << left << top;
        return QRect{left, top, width, height};
      }
    }
  }

  // 2. 通过 GUI 线程信息
  // Know info, valid for Everything tool
  do {
    GUITHREADINFO guiThreadInfo{};
    guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
    if (!GetGUIThreadInfo(targetThreadId, &guiThreadInfo)) {
      bool ok{false};
      // 有时需要 AttachThreadInput 才能读取另一个线程的 caret；尝试附加
      if (AttachThreadInput(currentThreadId, targetThreadId, TRUE)) {
        ok = GetGUIThreadInfo(targetThreadId, &guiThreadInfo);
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
      }
      if (!ok) {
        break;
      }
    }

    if (!IsRectEmpty(&guiThreadInfo.rcCaret)) {
      auto rect = guiThreadInfo.rcCaret;
      // Convert Rect to screen coordinates
      MapWindowPoints(guiThreadInfo.hwndCaret, nullptr, reinterpret_cast<POINT*>(&rect), 2);

      // Note: if Rect width is 10, but QRect width is 11
      // The precision error is acceptable here
      return QRect{rect.left,rect.top, rect.right, rect.bottom};
    }
  } while (false);

  // 3. 通过 GetCaretPos（附加输入线程）
  POINT pt;
  if (GetCaretPos(&pt) && (pt.x != 0 || pt.y != 0)) {
    ClientToScreen(focusWnd, &pt);
    qDebug() << "GetCaretPos" << pt.x << pt.y;
    return QRect{pt.x, pt.y, 0, 0};
  }
  qDebug() << "All method failed";
  return {};
}
#endif

QString GetAppName(const QString& appPath) {
  QFileInfo info(appPath);
#ifdef Q_OS_MAC
  // macOS: strip ".app" suffix, e.g. "Safari.app" -> "Safari"
  return info.baseName();
#else
  return info.fileName();
#endif
}

QIcon GetAppIcon(const QString& appPath) {
  QFileIconProvider iconProvider;
  return iconProvider.icon(QFileInfo(appPath));
}
} // namespace utils