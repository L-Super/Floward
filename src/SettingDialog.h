//
// Created by LMR on 25-7-26.
//

#pragma once

#include <QWidget>
class QButtonGroup;
class QHotkey;

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingDialog;
}
QT_END_NAMESPACE

class SettingDialog : public QWidget {
  Q_OBJECT

public:
  explicit SettingDialog(QWidget* parent = nullptr);
  ~SettingDialog() override;

  void SetHotkey(QHotkey* hotkey);
  void SetOnlineStatus(bool online);

protected:
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

protected:
  struct Options {
    QString url;
    QString deviceName;
  };

  void OnSyncPageChanged();
  void OnThemeChanged(Qt::ColorScheme scheme);

private:
  Ui::SettingDialog* ui;
  QButtonGroup* buttonGroup{};
  QHotkey* hotkey{};
  Options options;
};
