//
// Created by LMR on 25-8-2.
//

#include "Util.h"

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

namespace utils {
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
  return info.fileName();
}

QIcon GetAppIcon(const QString& appPath) {
  QFileIconProvider iconProvider;
  return iconProvider.icon(QFileInfo(appPath));
}
} // namespace utils