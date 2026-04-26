//
// Created by LMR on 25-7-26.
//

#pragma once

#include <QWidget>
class QButtonGroup;

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
  void SetOnlineStatus(bool online);

  Ui::SettingDialog* ui;
  QButtonGroup* buttonGroup{};
  Options options;
};
