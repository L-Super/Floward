#pragma once

#include <QObject>
#include <QFile>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct UpdateInfo {
  QString version;
  QString notes;
  QString url;
  QString sha256;
};

class UpdateManager final : public QObject {
  Q_OBJECT

public:
  explicit UpdateManager(QObject* parent = nullptr);

  void CheckForUpdates();
  void DownloadAndInstall();

signals:
  void updateAvailable(const UpdateInfo& info);
  void updateReady(const QString& packagePath);
  void upToDate();
  void failed(const QString& message);
  void downloadProgress(qint64 received, qint64 total);

private:
  void HandleManifest(QNetworkReply* reply);
  void HandlePackage(QNetworkReply* reply);
  QString PlatformKey() const;
  bool IsNewerVersion(const QString& remoteVersion) const;

  QNetworkAccessManager* networkManager_;
  UpdateInfo pendingUpdate_;
  QString manifestUrl_;
  QString packagePath_;
  QFile packageFile_;
};
