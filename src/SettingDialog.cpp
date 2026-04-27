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

  // 初始化最大历史记录条数
  int maxHistory = Config::instance().get<int>("max_history").value_or(100);
  ui->maxHistorySpinBox->setValue(maxHistory);

  connect(ui->maxHistorySpinBox, &QSpinBox::valueChanged, this, [](int value) {
    Config::instance().set("max_history", value);
    Config::instance().save();
  });

  ui->warningLabel->setAlignment(Qt::AlignHCenter);
  ui->tipsLabel->setAlignment(Qt::AlignHCenter);

  ui->urlLineEdit->setClearButtonEnabled(true);
  ui->deviceNameLineEdit->setClearButtonEnabled(true);

  // 初始化快捷键显示
  QString shortcutStr = "Alt+V";
  if (auto op = Config::instance().get<std::string>("shortcut"); op.has_value() && !op->empty()) {
    shortcutStr = QString::fromStdString(*op);
  }
  ui->keySequenceEdit->setKeySequence(QKeySequence(shortcutStr));

  OnThemeChanged(QGuiApplication::styleHints()->colorScheme());
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &SettingDialog::OnThemeChanged);

  connect(buttonGroup, &QButtonGroup::idClicked, this, [this](auto id) { ui->stackedWidget->setCurrentIndex(id); });

  connect(ui->autoStartupCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
    AutoStartup autoStartup;
    autoStartup.SetAutoStartup(checked);
  });
  connect(ui->confirmButton, &QPushButton::clicked, this, &SettingDialog::OnSyncPageChanged);

  connect(ui->keySequenceConfirmButton, &QPushButton::clicked, this, [this]() {
    auto keySequence = ui->keySequenceEdit->keySequence();
    auto currentShortcut = Config::instance().get<std::string>("shortcut");
    if (currentShortcut.has_value() && QKeySequence(QString::fromStdString(*currentShortcut)) == keySequence) {
      ui->warningLabel->setText("快捷键未改变");
      ui->warningLabel->show();
      QTimer::singleShot(2000, [this]() { ui->warningLabel->clear(); });
      return;
    }
    QHotkey testHotkey(keySequence, true);
    if (testHotkey.isRegistered()) {
      ui->warningLabel->setText("快捷键设置成功！");
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

#ifdef ENABLE_SYNC
  Config::instance().addObserver("online_status", [this]() {
    if (auto v = Config::instance().get<bool>("online_status"); v.has_value())
      SetOnlineStatus(v.value());
  });
#endif
}

SettingDialog::~SettingDialog() { delete ui; }

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

void SettingDialog::showEvent(QShowEvent* event) {
#ifdef ENABLE_SYNC
  if (auto v = Config::instance().get<bool>("online_status"); v.has_value()) {
    SetOnlineStatus(v.value());
  }
#endif
  QWidget::showEvent(event);
}

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
