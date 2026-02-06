/**
 * Created by LMR on 2023/10/24.
 */

#pragma once

#include "Item.h"

#include <QSet>
#include <QSystemTrayIcon>
#include <QWidget>
#include <memory>

class QMenu;
class QHotkey;
class QClipboard;
class QListWidget;
class QListWidgetItem;
class SettingDialog;
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
  bool ignoreNextDataChange{false};
  bool ignoreNetDataChange{false};
#ifdef ENABLE_SYNC
  std::unique_ptr<SyncServer> sync;
#endif
};