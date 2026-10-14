/**
 * Created by LMR on 2023/10/24.
 */

#pragma once

#include "Item.h"

#include <QElapsedTimer>
#include <QSystemTrayIcon>
#include <QWidget>
#include <memory>

class QMenu;
class QHotkey;
class QClipboard;
class QListWidget;
class QListWidgetItem;
class SettingDialog;
class QTimer;
#ifdef ENABLE_SYNC
class SyncServer;
#endif

class Clipboard : public QWidget {
  Q_OBJECT

public:
  explicit Clipboard(QWidget* parent = nullptr);
  ~Clipboard() override;

#ifdef ENABLE_SYNC
  void ReloadSyncServer();
#endif

protected:
  void AddItem(const ClipboardSourceInfo& sourceInfo, const QByteArray& hash);
  void MoveItemToTop(const QByteArray& hashValue);
  void RemoveOlderItems();
  QPoint adjustPosition(const QRect& rect);
  void pasteText();

protected slots:
  void DataChanged();
  void ClearItems();
  void RemoveItem(QListWidgetItem* item);
  void StayOnTop();
  void TrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void OnItemClicked(QListWidgetItem *item);

private:
  void InitTrayMenu();
  void CreateTrayAction();
  void InitShortcut();
#ifdef ENABLE_SYNC
  bool InitSyncServer();
#endif

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

private:
  QClipboard* clipboard;
  QSystemTrayIcon* trayIcon;
  QMenu* trayMenu;
  QHotkey* hotkey;
  QListWidget* listWidget;
  QHash<QByteArray, QListWidgetItem*> hashItemMap;
  SettingDialog* homeWidget;
  QString configFilePath;
  int maxHistoryCount{100};
  bool ignoreNextDataChange{false};
  bool ignoreNetDataChange{false};

#ifdef Q_OS_MACOS
  QTimer* clipboardPollTimer{};
  int lastChangeCount{};
#endif
#ifdef ENABLE_SYNC
  std::unique_ptr<SyncServer> sync;
  // Guard against sync echo-loops: when we write synced content into the
  // clipboard we record its hash + timestamp. If the resulting dataChanged
  // signal arrives within the window with matching content we treat it as
  // an echo and skip re-uploading. The single-shot ignoreNetDataChange flag
  // is unreliable on platforms that deliver dataChanged asynchronously
  // (macOS polling, delayed Windows signals).
  QByteArray lastSyncedHash;
  QElapsedTimer lastSyncedTimer;
#endif
};