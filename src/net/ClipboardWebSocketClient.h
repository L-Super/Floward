#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

class ClipboardWebSocketClient : public QObject {
  Q_OBJECT

public:
  explicit ClipboardWebSocketClient(const QUrl& url, QObject* parent = nullptr);
  ~ClipboardWebSocketClient() override;

  // 发起/断开连接
  void connectToServer();
  void disconnectFromServer();

  // 检查连接状态
  bool isConnected() const;

  // 发送一段文本消息到服务端（用于回 ACK / 心跳等）
  void sendTextMessage(const QString& message);

signals:
  void connected();
  void disconnected();
  void notifyMessageReceived(const QString& message);
  void errorOccurred(QAbstractSocket::SocketError error);
  void reconnectExhausted();

private slots:
  void onConnected();
  void onTextMessageReceived(const QString& message);
  void onBinaryMessageReceived(const QByteArray& message);
  void onError(QAbstractSocket::SocketError error);
  void onDisconnected();
  void tryReconnect();

private:
  // 安排下一次重连（带退避与抖动）
  void scheduleReconnect();

private:
  QWebSocket webSocket;
  QUrl serverUrl;
  QTimer reconnectTimer;
  // 退避重连参数
  const int reconnectBaseIntervalMs{2000}; // 初始重试间隔
  const int reconnectMaxIntervalMs{60000}; // 最大退避间隔
  int reconnectAttempt{0};                 // 已重试次数
  int reconnectMaxAttempts{12};            // 最大重试次数（到达即停止）
  bool userRequestedDisconnect{false};     // 用户主动断开则不再自动重连
};
