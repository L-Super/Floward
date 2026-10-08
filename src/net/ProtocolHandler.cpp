//
// Created by LMR on 25-9-07.
//

#include "ProtocolHandler.h"

#include "../utils/Logger.hpp"
#include "ClipboardStruct.h"
#include "ProtocolCommon.h"

#ifdef Q_OS_MACOS
#include <QCoreApplication>
#include <QEvent>
#include <QFileOpenEvent>
#endif
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
const QString LOGIN_ACTION = "login";
} // namespace

ProtocolHandler::ProtocolHandler(QObject* parent) : QObject(parent) {}

void ProtocolHandler::InstallUrlEventFilter() {
#ifdef Q_OS_MACOS
  // 仅 macOS 需要：协议 URL 经 Apple Event → QFileOpenEvent 投递；
  // Windows/Linux 的 URL 走 argv / SingleApplication 转发，无需监听
  if (auto* app = QCoreApplication::instance())
    app->installEventFilter(this);
#endif
}

#ifdef Q_OS_MACOS
bool ProtocolHandler::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::FileOpen) {
    const auto* openEvent = static_cast<const QFileOpenEvent*>(event);
    const QUrl url = openEvent->url();
    if (url.isValid() &&
        url.scheme().compare(ProtocolConstants::DEFAULT_PROTOCOL_SCHEME, Qt::CaseInsensitive) == 0) {

      spdlog::info("Protocol url from QFileOpenEvent: {}", url.toString().toStdString());

      HandleProtocolUrl(url.toString());
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}
#endif  // Q_OS_MACOS

void ProtocolHandler::HandleProtocolUrl(const QString& url) {
  if (!ValidateUrl(url)) {
    emit errorOccurred("Invalid protocol URL format");
    return;
  }

  QVariantMap parameters = ParseUrlParameters(url);

  // 从URL路径中提取操作类型
  QUrl qurl(url);
  QString actionType = qurl.host();

  if (actionType.isEmpty()) {
    // 对于 PROTOCOL_SCHEME://login? 格式，手动提取操作类型
    // 移除 "PROTOCOL_SCHEME://"
    QString urlWithoutScheme = url.mid(ProtocolConstants::DEFAULT_PROTOCOL_SCHEME.length() + 3);
    int queryIndex = urlWithoutScheme.indexOf('?');
    if (queryIndex > 0) {
      actionType = urlWithoutScheme.left(queryIndex);
    }
  }

  // 检查是否是登录操作
  if (actionType == LOGIN_ACTION) {
    QString email = parameters.value("email", "").toString();
    QString token = parameters.value("token", "").toString();
    QString deviceName = parameters.value("device_name", "").toString();

    if (email.isEmpty() || token.isEmpty()) {
      emit errorOccurred("Email or token is missing");
      return;
    }

    UserInfo userInfo{
        .email = email.toStdString(), .token = token.toStdString(), .device_name = deviceName.toStdString()};

    // 移除敏感信息，只保留其他参数
    QVariantMap additionalData = parameters;
    additionalData.remove("email");
    additionalData.remove("token");
    additionalData.remove("device_name");

    emit loginDataReceived(userInfo, additionalData);
  }
  else {
    emit errorOccurred(QString("Unknown action: %1").arg(url));
  }
}

QVariantMap ProtocolHandler::ParseUrlParameters(const QString& url) {
  QVariantMap parameters;

  try {
    QUrlQuery query(QUrl{url});

    // 解析查询参数
    for (const auto& item : query.queryItems()) {
      QString key = DecodeUrlParameter(item.first);
      QString value = DecodeUrlParameter(item.second);
      parameters.insert(key, value);
    }
  }
  catch (const std::exception& e) {
    qDebug() << "Error parsing URL:" << e.what();
    spdlog::error("Error parsing url:{}", e.what());
  }

  return parameters;
}

bool ProtocolHandler::ValidateUrl(const QString& url) {
  // 检查URL是否以正确的协议开头
  if (!url.startsWith(ProtocolConstants::DEFAULT_PROTOCOL_SCHEME + "://")) {
    return false;
  }

  // 使用QUrl验证URL格式
  QUrl qurl(url);
  return qurl.isValid() && qurl.scheme() == ProtocolConstants::DEFAULT_PROTOCOL_SCHEME;
}

QString ProtocolHandler::DecodeUrlParameter(const QString& parameter) {
  return QUrl::fromPercentEncoding(parameter.toUtf8());
}

QString ProtocolHandler::EncodeUrlParameter(const QString& parameter) {
  return QString::fromUtf8(QUrl::toPercentEncoding(parameter));
}
