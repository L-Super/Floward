#include "ClipboardWebSocketClient.h"
#include "../utils/Logger.hpp"
#include <QDebug>
#include <QRandomGenerator>

#include <magic_enum/magic_enum.hpp>

ClipboardWebSocketClient::ClipboardWebSocketClient(const QUrl& url, QObject* parent) : QObject(parent), serverUrl(url) {
  // 连接底层信号
  connect(&webSocket, &QWebSocket::connected, this, &ClipboardWebSocketClient::onConnected);
  connect(&webSocket, &QWebSocket::errorOccurred, this, &ClipboardWebSocketClient::onError);
  connect(&webSocket, &QWebSocket::disconnected, this, &ClipboardWebSocketClient::onDisconnected);

  // 重连定时器（首次不启动，按需设置动态间隔）
  reconnectTimer.setSingleShot(true);
  connect(&reconnectTimer, &QTimer::timeout, this, &ClipboardWebSocketClient::tryReconnect);

  connect(&webSocket, &QWebSocket::textMessageReceived, this, &ClipboardWebSocketClient::onTextMessageReceived);
  connect(&webSocket, &QWebSocket::binaryMessageReceived, this, &ClipboardWebSocketClient::onBinaryMessageReceived);
}

ClipboardWebSocketClient::~ClipboardWebSocketClient() { disconnectFromServer(); }

void ClipboardWebSocketClient::connectToServer() {
  if (webSocket.state() == QAbstractSocket::ConnectedState || webSocket.state() == QAbstractSocket::ConnectingState) {
    return;
  }
  userRequestedDisconnect = false;
  QUrl url(serverUrl);
  url.setQuery(QString()); // 清空查询

  spdlog::info("Connecting to websocket {}", url.toString());
  webSocket.open(serverUrl);
}

void ClipboardWebSocketClient::disconnectFromServer() {
  userRequestedDisconnect = true;
  reconnectTimer.stop();
  reconnectAttempt = 0;
  webSocket.close();
}

bool ClipboardWebSocketClient::isConnected() const {
  return webSocket.state() == QAbstractSocket::ConnectedState;
}

void ClipboardWebSocketClient::onConnected() {
  emit connected();
  spdlog::info("WebSocket connected.");
  // 成功连接后重置重试状态
  reconnectAttempt = 0;

  // 订阅 /sync/notify（如果服务端需要额外握手可以在这里发送订阅消息）
  // webSocket.sendTextMessage(QStringLiteral("{\"action\":\"subscribe\",\"topic\":\"/sync/notify\"}"));
}

void ClipboardWebSocketClient::onTextMessageReceived(const QString& message) {
  qDebug() << __func__ << "Websocket text message:" << message;
  emit notifyMessageReceived(message);
}

void ClipboardWebSocketClient::onBinaryMessageReceived(const QByteArray& message) {
  qDebug() << __func__ << "Websocket binary message received; length =" << message.size();
  // 如果服务端以二进制发推送，可以转换成 QString 或 QJsonDocument
  emit notifyMessageReceived(QString::fromUtf8(message));
}

void ClipboardWebSocketClient::onError(QAbstractSocket::SocketError error) {
  emit errorOccurred(error);

  spdlog::error("WebSocket error:{} {}", magic_enum::enum_name(error), webSocket.errorString());

  // 启动重连
  scheduleReconnect();
}

void ClipboardWebSocketClient::onDisconnected() {
  emit disconnected();
  spdlog::warn("WebSocket disconnected.");

  // 尝试重连
  scheduleReconnect();
}

void ClipboardWebSocketClient::tryReconnect() {
  spdlog::warn("Reconnecting...");
  connectToServer();
}

void ClipboardWebSocketClient::scheduleReconnect() {
  if (userRequestedDisconnect) {
    spdlog::info("Skip reconnect: user requested disconnect.");
    return;
  }
  if (webSocket.state() == QAbstractSocket::ConnectedState || webSocket.state() == QAbstractSocket::ConnectingState) {
    return;
  }
  if (reconnectMaxAttempts > 0 && reconnectAttempt >= reconnectMaxAttempts) {
    spdlog::error("Max reconnect attempts reached ({}). Stop trying.", reconnectMaxAttempts);
    emit reconnectExhausted();
    return;
  }

  // 计算指数退避间隔，带抖动
  const int backoffPow = std::min(reconnectAttempt, 16);                        // 防溢出保护
  qint64 interval = static_cast<qint64>(reconnectBaseIntervalMs) << backoffPow; // base * 2^attempt
  if (interval > reconnectMaxIntervalMs)
    interval = reconnectMaxIntervalMs;
  // 抖动：在 [0.5x, 1.0x] 之间随机
  double jitter = 0.5 + (QRandomGenerator::global()->bounded(51) / 100.0); // 0.5~1.0
  int delayMs = static_cast<int>(interval * jitter);
  if (delayMs < reconnectBaseIntervalMs)
    delayMs = reconnectBaseIntervalMs;

  reconnectAttempt++;
  reconnectTimer.stop();
  reconnectTimer.setInterval(delayMs);

  spdlog::warn("Schedule reconnect #{} in {} ms", reconnectAttempt, delayMs);

  if (!reconnectTimer.isActive())
    reconnectTimer.start();
}
