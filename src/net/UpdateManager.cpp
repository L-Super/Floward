#include "UpdateManager.h"

#include "version.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

namespace {
constexpr auto kManifestUrl =
    "https://github.com/L-Super/Floward/releases/latest/download/latest.json";

QString Sha256ForFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};

  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    const QByteArray chunk = file.read(1024 * 1024);
    if (chunk.isEmpty() && !file.atEnd())
      return {};
    hash.addData(chunk);
  }
  return QString::fromLatin1(hash.result().toHex()).toLower();
}

QString DownloadFileName(const QUrl& url) {
  QString name = QFileInfo(url.path()).fileName();
  return name.isEmpty() ? "Floward-update" : name;
}

void ConfigureRequest(QNetworkRequest& request) {
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QString("Floward/%1").arg((VERSION_STR)));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
}

bool IsSha256(const QString& value) {
  if (value.size() != 64)
    return false;
  for (const QChar character : value) {
    const QChar lower = character.toLower();
    if (!((lower >= QLatin1Char('0') && lower <= QLatin1Char('9')) ||
          (lower >= QLatin1Char('a') && lower <= QLatin1Char('f'))))
      return false;
  }
  return true;
}
} // namespace

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent),
      networkManager_(new QNetworkAccessManager(this)),
      manifestUrl_(QString::fromLatin1(kManifestUrl)) {}

QString UpdateManager::PlatformKey() const {
#if defined(Q_OS_WIN)
#  if defined(Q_PROCESSOR_ARM_64)
  return "windows-arm64";
#  else
  return "windows-x86_64";
#  endif
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
  return "macos-universal";
#elif defined(Q_OS_LINUX)
#  if defined(Q_PROCESSOR_ARM_64)
  return "linux-arm64";
#  else
  return "linux-x86_64";
#  endif
#else
  return {};
#endif
}

bool UpdateManager::IsNewerVersion(const QString& remoteVersion) const {
  const QString normalized = remoteVersion.startsWith('v') ? remoteVersion.mid(1) : remoteVersion;
  const QVersionNumber remote = QVersionNumber::fromString(normalized);
  const QVersionNumber current = QVersionNumber::fromString(QStringLiteral(VERSION_STR));
  return !remote.isNull() && !current.isNull() && QVersionNumber::compare(remote, current) > 0;
}

void UpdateManager::CheckForUpdates() {
  QNetworkRequest request{QUrl(manifestUrl_)};
  ConfigureRequest(request);
  auto* reply = networkManager_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] { HandleManifest(reply); });
}

// template json:
// {
//   "version": "3.0.7.0",
//   "notes": "release note",
//   "platforms": {
//     "windows-x86_64": {
//       "url": "https://github.com/{repository}/releases/download/{tag_name}/{filename}",
//       "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
//     },
//     "macos-universal": {
//       "url": "https://github.com/{repository}/releases/download/{tag_name}/{filename}",
//       "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
//     },
//     "linux-x86_64": {
//       "url": "https://github.com/{repository}/releases/download/{tag_name}/{filename}",
//       "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
//     }
//   }
// }
void UpdateManager::HandleManifest(QNetworkReply* reply) {
  reply->deleteLater();
  if (reply->error() != QNetworkReply::NoError) {
    emit failed(QString("检查更新失败：%1").arg(reply->errorString()));
    return;
  }

  QJsonParseError parseError{};
  const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    emit failed("更新清单格式无效");
    return;
  }

  const QJsonObject root = document.object();
  const QString remoteVersion = root.value("version").toString();
  const QJsonObject platforms = root.value("platforms").toObject();
  const QJsonObject platform = platforms.value(PlatformKey()).toObject();
  const QString url = platform.value("url").toString();
  const QString sha256 = platform.value("sha256").toString().toLower();
  const QUrl packageUrl(url);
  if (remoteVersion.isEmpty() || !packageUrl.isValid() || packageUrl.scheme() != "https" || !IsSha256(sha256)) {
    emit failed("更新清单缺少当前平台的 url 或 sha256");
    return;
  }

  if (!IsNewerVersion(remoteVersion)) {
    emit upToDate();
    return;
  }

  pendingUpdate_ = {remoteVersion, root.value("notes").toString(), url, sha256};
  emit updateAvailable(pendingUpdate_);
}

void UpdateManager::DownloadAndInstall() {
  if (pendingUpdate_.url.isEmpty()) {
    emit failed("没有可下载的更新");
    return;
  }

  const QUrl url(pendingUpdate_.url);
  const QString tempDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation).isEmpty()
                                    ? QDir::tempPath()
                                    : QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  packagePath_ = QDir(tempDirectory).filePath(DownloadFileName(url));
  QFile::remove(packagePath_);
  packageFile_.setFileName(packagePath_);
  if (!packageFile_.open(QIODevice::WriteOnly)) {
    emit failed("无法创建更新安装包临时文件");
    return;
  }

  QNetworkRequest request{url};
  ConfigureRequest(request);
  auto* reply = networkManager_->get(request);
  connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
  connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
    const QByteArray chunk = reply->readAll();
    if (packageFile_.write(chunk) != chunk.size())
      reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply] { HandlePackage(reply); });
}

void UpdateManager::HandlePackage(QNetworkReply* reply) {
  reply->deleteLater();
  if (packageFile_.isOpen())
    packageFile_.close();
  if (reply->error() != QNetworkReply::NoError) {
    QFile::remove(packagePath_);
    emit failed(QString("下载更新失败：%1").arg(reply->errorString()));
    return;
  }

  const QByteArray trailing = reply->readAll();
  if (!trailing.isEmpty()) {
    // readyRead may not have fired for the final bytes.
    QFile file(packagePath_);
    if (!file.open(QIODevice::Append) || file.write(trailing) != trailing.size()) {
      QFile::remove(packagePath_);
      emit failed("无法保存更新安装包");
      return;
    }
  }
  if (!QFileInfo::exists(packagePath_) || QFileInfo(packagePath_).size() == 0) {
    QFile::remove(packagePath_);
    emit failed("无法保存更新安装包");
    return;
  }

  if (Sha256ForFile(packagePath_) != pendingUpdate_.sha256) {
    QFile::remove(packagePath_);
    emit failed("更新安装包 SHA-256 校验失败");
    return;
  }

  emit updateReady(packagePath_);

#if defined(Q_OS_WIN)
  bool launched = false;
  if (packagePath_.endsWith(".msi", Qt::CaseInsensitive)) {
    launched = QProcess::startDetached("msiexec.exe",
                                       {"/i", packagePath_},
                                       QFileInfo(packagePath_).absolutePath());
  }
  else {
    launched = QProcess::startDetached(packagePath_, {}, QFileInfo(packagePath_).absolutePath());
  }
  if (!launched) {
    emit fail("无法启动 Windows 安装程序");
    return;
  }
  QCoreApplication::quit();
#elif defined(Q_OS_LINUX)
  QFile::setPermissions(packagePath_, QFile::permissions(packagePath_) | QFileDevice::ExeOwner |
                                                   QFileDevice::ExeGroup | QFileDevice::ExeOther);
  if (packagePath_.endsWith(".AppImage", Qt::CaseInsensitive) &&
      QProcess::startDetached(packagePath_, {}, QFileInfo(packagePath_).absolutePath())) {
    QCoreApplication::quit();
    return;
  }
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(packagePath_)))
    emit fail("无法打开更新安装包");
#else
  // macOS DMG/ZIP 交给系统打开，由用户完成安装。
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(packagePath_)))
    emit failed("无法打开更新安装包");
#endif
}
