//
// Created by LMR on 25-9-07.
//

#pragma once

#include <QObject>
#include <QString>

#include "ProtocolCommon.h"

class ProtocolRegistry : public QObject {
  Q_OBJECT
public:
  ProtocolRegistry();
  ~ProtocolRegistry() override = default;

  // 注册自定义协议
  void RegisterProtocol(const QString& protocolName = ProtocolConstants::DEFAULT_PROTOCOL_SCHEME);

  // 取消注册自定义协议
  void UnregisterProtocol(const QString& protocolName = ProtocolConstants::DEFAULT_PROTOCOL_SCHEME);

  // 检查协议是否已注册
  bool IsProtocolRegistered(const QString& protocolName = ProtocolConstants::DEFAULT_PROTOCOL_SCHEME);

  // 获取协议URL
  QString GetProtocolUrl(const QString& protocolName = ProtocolConstants::DEFAULT_PROTOCOL_SCHEME);

private:
#if defined(Q_OS_WIN)
  void RegisterWinProtocol(const QString& protocolName);
  void UnregisterWinProtocol(const QString& protocolName);
  bool IsWinProtocolRegistered(const QString& protocolName);
#elif defined(Q_OS_LINUX)
  void RegisterLinuxProtocol(const QString& protocolName);
  void UnregisterLinuxProtocol(const QString& protocolName);
  bool IsLinuxProtocolRegistered(const QString& protocolName);
#elif defined(Q_OS_MACOS)
  // macOS custom URL schemes are declared statically in the app bundle's
  // Info.plist and are registered with Launch Services by macOS.
  void RegisterMacProtocol(const QString& protocolName);
  void UnregisterMacProtocol(const QString& protocolName);
  bool IsMacProtocolRegistered(const QString& protocolName);
#endif
};
