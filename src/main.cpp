#include "Clipboard.h"
#include "CustomMessageBox.h"
#include "SingleApplication"
#ifdef ENABLE_SYNC
#include "net/ProtocolHandler.h"
#include "net/ProtocolRegistry.h"
#include "net/UpdateManager.h"
#endif
#include "utils/Config.h"
#include "utils/Logger.hpp"
#include "utils/Util.h"
#include "version.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QPalette>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleHints>
#include <QUrl>

void SetAppColorScheme(bool isDark) {
  QPalette palette = qApp->palette();

  if (isDark) {
    palette.setColor(QPalette::Window, QColor("#1f2329"));
  }
  else {
    palette.setColor(QPalette::Window, QColor("#f2f4f6"));
  }

  qApp->setPalette(palette);
}

void ApplyTheme(Qt::ColorScheme scheme) {
  switch (scheme) {
    case Qt::ColorScheme::Dark: {
      SetAppColorScheme(true);
      utils::LoadStyleSheet(":/qss/resources/style_dark.css");
    } break;
    case Qt::ColorScheme::Light:
    case Qt::ColorScheme::Unknown: {
      SetAppColorScheme(false);
      utils::LoadStyleSheet(":/qss/resources/style.css");
    } break;
  }
  qDebug() << "ApplyTheme to" << scheme;
}

int main(int argc, char* argv[]) {
  SingleApplication a(argc, argv, true);

  auto logFilePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs/app.log";
  initLogging(logFilePath.toStdString());

  // 处理命令行参数中的协议 URL
  QString protocolUrl;
  for (int i = 1; i < argc; ++i) {
    const QString argument = QString::fromLocal8Bit(argv[i]);
    if (argument.startsWith("floward://")) {
      protocolUrl = argument;
    }
  }

  if (a.isSecondary()) {
    qDebug() << "Primary instance PID: " << a.primaryPid();
    qDebug() << "Primary instance user: " << a.primaryUser();
    spdlog::info("Secondary app is launching.");
    if (!protocolUrl.isEmpty()) {
      a.sendMessage(protocolUrl.toUtf8());
    }

    return 0;
  }

#ifdef Q_OS_MACOS
  a.setWindowIcon(QIcon(":/resources/icon-for-mac.png"));
#else
  a.setWindowIcon(QIcon(":/resources/icon.png"));
#endif

  spdlog::info("App launched, version:{}", VERSION_STR);

  a.setApplicationVersion(VERSION_STR);

  ApplyTheme(QGuiApplication::styleHints()->colorScheme());

  // 控制着当最后一个可视的窗口退出时候，程序是否退出，默认是true
  QApplication::setQuitOnLastWindowClosed(false);

#ifdef ENABLE_SYNC
  // 注册自定义协议
  ProtocolRegistry protocolRegistry;
  if (!protocolRegistry.IsProtocolRegistered())
    protocolRegistry.RegisterProtocol();
#endif

  auto configFilePath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/clipboard_settings.json";
  if (auto re = Config::instance().load(configFilePath.toStdString()); !re.has_value()) {
    spdlog::warn("Config file is unexpected. {}", re.error());
  }
  Clipboard c;
  c.show();

#ifdef ENABLE_SYNC
  UpdateManager updateManager(&a);
  QObject::connect(&updateManager, &UpdateManager::updateAvailable, &c,
                   [&updateManager, &c](const UpdateInfo& info) {
                     const bool accepted = CustomMessageBox::question(
                         &c,
                         "Floward 更新",
                         QString("发现新版本 %1").arg(info.version),
                         "是否立即更新？会在后台下载，下载完成后弹出安装。",
                         info.notes,
                         "立即更新",
                         "稍后");
                     if (accepted)
                       updateManager.DownloadAndInstall();
                   });
  QObject::connect(&updateManager, &UpdateManager::failed, &c,
                   [&c](const QString& message) {
                     spdlog::warn("Update failed: {}", message.toStdString());

                     if (message.contains("下载") || message.contains("SHA-256") ||
                         message.contains("安装包")) {
                       CustomMessageBox::warning(&c, "Floward 更新", message);
                     }
                   });
  // 仅在用户开启「检查更新」时启动检测（默认开启）
  if (Config::instance().get<bool>("check_update").value_or(true)) {
    QTimer::singleShot(1500, &updateManager, &UpdateManager::CheckForUpdates);
  }
#endif

  QObject::connect(&a, &SingleApplication::instanceStarted, &c, &Clipboard::show);
  // 连接系统主题变化信号 Qt 6.5 support
  QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, [](Qt::ColorScheme scheme) {
    qDebug() << "System theme change to" << scheme;
    ApplyTheme(scheme);
  });

#ifdef ENABLE_SYNC
  // 创建协议处理器
  ProtocolHandler protocolHandler;
  QObject::connect(&a, &SingleApplication::receivedMessage, &protocolHandler,
                   [&protocolHandler](int instanceId, QByteArray message) {
                     qDebug() << "instance id:" << instanceId << "message:" << message;
                     protocolHandler.HandleProtocolUrl(message);
                   });

  // 连接协议处理器的信号到剪贴板对象
  QObject::connect(&protocolHandler, &ProtocolHandler::loginDataReceived, &c,
                   [&c](const UserInfo& userInfo, const QVariantMap& additionalData) {
                     spdlog::info("Data received from custom protocol");
                     qDebug() << "Additional data:" << additionalData;
                     QString url = additionalData.value("api_url", "").toString();
                     if (!url.isEmpty()) {
                       Config::instance().set("url", url.toStdString());
                     }

                     Config::instance().setUserInfo(userInfo);
                     (void)Config::instance().save();

                     c.ReloadSyncServer();

                     // 显示主窗口
                     c.show();
                     c.raise();
                     c.activateWindow();
                   });

  QObject::connect(&protocolHandler, &ProtocolHandler::errorOccurred,
                   [](const QString& errorMessage) { spdlog::error("Protocol url wrong. error:{}", errorMessage); });

  if (!protocolUrl.isEmpty()) {
    protocolHandler.HandleProtocolUrl(protocolUrl.toUtf8());
  }
#endif

  return QApplication::exec();
}
