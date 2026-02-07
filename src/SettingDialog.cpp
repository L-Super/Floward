//
// Created by LMR on 25-7-26.
//

#include "SettingDialog.h"
#include "ui_SettingDialog.h"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QStyle>
#include <QStyleHints>
#include <QTimer>

#include "QHotkey"

#include "net/SyncServer.h"
#include "utils/AutoStartup.h"
#include "utils/Config.h"
#include "utils/Logger.hpp"
#include "utils/Util.h"

SettingDialog::SettingDialog(QWidget* parent)
    : QWidget(parent), ui(new Ui::SettingDialog), buttonGroup(new QButtonGroup(this)) {
  ui->setupUi(this);

  ui->stackedWidget->setCurrentIndex(0);

  buttonGroup->setExclusive(true);
  buttonGroup->addButton(ui->generalToolButton, 0);
  buttonGroup->addButton(ui->shortcutToolButton, 1);
  buttonGroup->addButton(ui->syncToolButton, 2);

#ifndef ENABLE_SYNC
  ui->syncToolButton->hide();
#endif

  // default is ui->generalToolButton
  ui->generalToolButton->setChecked(true);

  ui->textBrowser->setMarkdown(about::IntroductionText());
  ui->textBrowser->setOpenExternalLinks(true);

  AutoStartup autoStartup;
  ui->autoStartupCheckBox->setChecked(autoStartup.IsAutoStartup());

  ui->warningLabel->setAlignment(Qt::AlignHCenter);
  ui->tipsLabel->setAlignment(Qt::AlignHCenter);

  OnThemeChanged(QGuiApplication::styleHints()->colorScheme());
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &SettingDialog::OnThemeChanged);

  connect(buttonGroup, &QButtonGroup::idClicked, this, [this](auto id) { ui->stackedWidget->setCurrentIndex(id); });

  connect(ui->autoStartupCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
    AutoStartup autoStartup;
    autoStartup.SetAutoStartup(checked);
  });
  connect(ui->confirmButton, &QPushButton::clicked, this, &SettingDialog::OnSyncPageChanged);
  // connect(ui->keySequenceEdit, &QKeySequenceEdit::editingFinished, this,
  //         [this, hotkey]() { qDebug() << "editing finished"; });
  // connect(ui->keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this,
  //         [this](const QKeySequence &keySequence) { qDebug() << "keySequenceChanged" << keySequence; });
  connect(ui->loginButton, &QPushButton::clicked, this, [this]() {
    spdlog::info("Login button clicked");
    auto lineEditText = ui->urlLineEdit->text();
    if (!lineEditText.isEmpty()) {
      QDesktopServices::openUrl(QUrl(lineEditText));
    }
    else if (auto url = Config::instance().get<std::string>("url"); url.has_value()) {
      QDesktopServices::openUrl(QUrl(QString::fromStdString(url.value())));
    }
    else {
      ui->tipsLabel->setText("<span style='color:red;'>服务器 URL 不存在!</span>");
      QTimer::singleShot(2000, [this]() { ui->tipsLabel->clear(); });
    }
  });
}

SettingDialog::~SettingDialog() { delete ui; }

void SettingDialog::SetHotkey(QHotkey* hotkey) {
  this->hotkey = hotkey;

  ui->keySequenceEdit->setKeySequence(this->hotkey->shortcut());

  connect(ui->keySequenceConfirmButton, &QPushButton::clicked, this, [this]() {
    if (!this->hotkey)
      return;

    auto keySequence = ui->keySequenceEdit->keySequence();
    this->hotkey->setShortcut(keySequence, true);

    if (this->hotkey->isRegistered()) {
      ui->warningLabel->setText("快捷键设置成功！");

      // update config
      auto value = keySequence.toString().toStdString();
      Config::instance().set("shortcut", value);
      Config::instance().save();
    }
    else {
      ui->warningLabel->setText("<span style='color:red;'>快捷键设置冲突或不符合规则，请重新设置！</span>");
    }

    ui->warningLabel->show();
    QTimer::singleShot(2000, [this]() { ui->warningLabel->clear(); });
  });
}

void SettingDialog::SetOnlineStatus(bool online) {
  QString statusText = online ? "<span style='color:green;'>在线</span>" : "<span style='color:red;'>离线</span>";
  auto userInfo = Config::instance().getUserInfo();
  if (userInfo.has_value()) {
    QString user = QString::fromStdString(userInfo.value().email);

    ui->accountLabel->setText(user);
    ui->accountStatusLabel->setText(statusText);
    QString deviceName = QString::fromStdString(userInfo.value().device_name);
    ui->deviceNameLineEdit->setPlaceholderText(deviceName);
    options.deviceName = deviceName;
    // if online, hide login button
    ui->loginButton->setVisible(!online);

    if (auto url = Config::instance().get<std::string>("url"); url.has_value()) {
      QString value = QString::fromStdString(url.value());
      ui->urlLineEdit->setPlaceholderText(value);
      options.url = value;
    }
  }
  else {
    ui->accountLabel->clear();
    ui->accountStatusLabel->setText(statusText);
    ui->loginButton->show();
  }
}

void SettingDialog::showEvent(QShowEvent* event) { QWidget::showEvent(event); }

void SettingDialog::closeEvent(QCloseEvent* event) {
  hide();
  QWidget::closeEvent(event);
}

void SettingDialog::OnSyncPageChanged() {
  bool changed{false};

  QString deviceName = ui->deviceNameLineEdit->text();
  if (deviceName.isEmpty()) {
    deviceName = ui->deviceNameLineEdit->placeholderText();
  }

  if (deviceName.isEmpty()) {
    ui->tipsLabel->setText("<span style='color:red;'>设备名为空</span>");
    return;
  }

  if (options.deviceName != deviceName) {
    ui->deviceNameLineEdit->clear();
    ui->deviceNameLineEdit->setPlaceholderText(deviceName);
    options.deviceName = deviceName;
    changed = true;

    auto info = Config::instance().getServerConfig();
    if (info.has_value()) {
      info->device_name = deviceName.toStdString();
      Config::instance().setServerConfig(*info);
    }
  }

  QString url = ui->urlLineEdit->text();
  if (url.isEmpty()) {
    url = ui->urlLineEdit->placeholderText();
  }

  if (url.isEmpty()) {
    ui->tipsLabel->setText("<span style='color:red;'>服务器 URL 为空</span>");
    return;
  }

  if (options.url != url) {
    ui->urlLineEdit->clear();
    ui->urlLineEdit->setPlaceholderText(url);
    options.url = url;
    changed = true;

    Config::instance().set("url", url.toStdString());
  }

  if (changed) {
    ui->tipsLabel->setText("<span style='color:green;'>保存成功！</span>");
    QTimer::singleShot(2000, [this]() { ui->tipsLabel->clear(); });
  }
}

void SettingDialog::OnThemeChanged(Qt::ColorScheme scheme) {
  switch (scheme) {
    case Qt::ColorScheme::Dark: {
      ui->generalToolButton->setIcon(QIcon(":/resources/images/home-white.svg"));
      ui->shortcutToolButton->setIcon(QIcon(":/resources/images/keyboard-white.svg"));
      ui->syncToolButton->setIcon(QIcon(":/resources/images/sync-white.svg"));
    } break;
    case Qt::ColorScheme::Light:
    case Qt::ColorScheme::Unknown: {
      ui->generalToolButton->setIcon(QIcon(":/resources/images/home.svg"));
      ui->shortcutToolButton->setIcon(QIcon(":/resources/images/keyboard.svg"));
      ui->syncToolButton->setIcon(QIcon(":/resources/images/sync.svg"));
    } break;
  }
  qDebug() << "System theme change to" << scheme << "on SettingDialog";
}
