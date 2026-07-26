#include "ClipboardApiClient.h"
#include "../utils/Crypto.h"
#include "../utils/Logger.hpp"
#include "magic_enum/magic_enum.hpp"

#include <QBuffer>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>

ClipboardApiClient::ClipboardApiClient(const QUrl& baseUrl, QObject* parent)
    : QObject(parent), manager(new QNetworkAccessManager(this)), baseUrl(baseUrl) {
  connect(manager, &QNetworkAccessManager::finished, this, &ClipboardApiClient::onNetworkReply);
}

ClipboardApiClient::~ClipboardApiClient() = default;

void ClipboardApiClient::setUrl(const QUrl& url) { baseUrl = url; }

void ClipboardApiClient::registerUser(const QString& email, const QString& password) {
  QUrl url = baseUrl.resolved(QUrl("/auth/register"));
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject body;
  body["email"] = email;
  body["password"] = password;
  QNetworkReply* reply = manager->post(req, QJsonDocument(body).toJson());
  replyMap.insert(reply, Endpoint::Register);
}

void ClipboardApiClient::login(const User& user) {
  QUrl url = baseUrl.resolved(QUrl("/auth/login"));
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject body;
  body["email"] = user.email;
  body["password"] = user.password;
  body["device_id"] = user.deviceId;
  body["device_name"] = user.deviceName;
  auto deviceType = magic_enum::enum_name(user.deviceType);
  body["device_type"] = QString::fromStdString(deviceType.data());

  QNetworkReply* reply = manager->post(req, QJsonDocument(body).toJson());
  replyMap.insert(reply, Endpoint::Login);
}

void ClipboardApiClient::verifyToken(const QString& authToken) {
  QUrl url = baseUrl.resolved(QUrl("/auth/verify-token"));
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QString("Bearer %1").arg(authToken).toUtf8());

  QNetworkReply* reply = manager->get(req);
  replyMap.insert(reply, Endpoint::VerifyToken);
}

void ClipboardApiClient::uploadClipboard(const ClipboardData& data, const QString& authToken) {
  QUrl url = baseUrl.resolved(QUrl("/clipboard"));
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QString("Bearer %1").arg(authToken).toUtf8());

  QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  QHttpPart typePart;
  typePart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"type\"");

  if (data.type == ClipboardDataType::text) {
    typePart.setBody("text");

    // Text is end-to-end encrypted: the server only sees ciphertext.
    // Wire format: base64([12B nonce][ciphertext][16B GCM tag]).
    // Base64 keeps the value ASCII-safe so the server can store and forward
    // it inside JSON strings without any binary handling on its side.
    const QByteArray encrypted = Crypto::Encrypt(data.data).toBase64();
    if (encrypted.isEmpty()) {
      emit uploadFinished(false, "Encryption failed");
      delete multiPart;
      return;
    }

    QHttpPart dataPart;
    dataPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"data\"");
    dataPart.setBody(encrypted);

    multiPart->append(typePart);
    multiPart->append(dataPart);
  }
  else {
    // Image/file is uploaded as-is: the server downloads it, stores it, and
    // hands back a URL. The server must be able to read the bytes, so E2EE
    // does not apply here — transport security relies on TLS instead.
    auto type = QString::fromStdString(std::string(magic_enum::enum_name(data.type)));

    typePart.setBody(type.toUtf8()); // image 或 "file"

    QHttpPart filePart;
    // TODO: add file meta
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, R"(form-data; name="file"; filename="test.png")");
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "image/png"); // 或其他类型

    QBuffer* buffer = new QBuffer();
    buffer->setData(data.data);
    buffer->open(QIODevice::ReadOnly);
    filePart.setBodyDevice(buffer);
    buffer->setParent(multiPart);

    multiPart->append(typePart);
    multiPart->append(filePart);
  }

  QNetworkReply* reply = manager->post(req, multiPart);
  multiPart->setParent(reply); // 自动释放
  replyMap.insert(reply, Endpoint::Upload);
}

void ClipboardApiClient::downloadImage(const QString& imageUrl, const QString& authToken) {
  QUrl url = baseUrl.resolved(imageUrl);

  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QString("Bearer %1").arg(authToken).toUtf8());

  QNetworkReply* reply = manager->get(req);
  replyMap.insert(reply, Endpoint::DownloadImage);
}

void ClipboardApiClient::onNetworkReply(QNetworkReply* reply) {
  Endpoint ep = replyMap.take(reply);

  if (reply->error() != QNetworkReply::NoError) {
    QString err = reply->errorString();
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    spdlog::error("On network reply. Url: {} status code: {} error type: {} error message: {}", reply->url().toString(),
                  statusCode.isValid() ? statusCode.toInt() : -1, magic_enum::enum_name(reply->error()), err);

    switch (ep) {
    case Endpoint::Register:
      emit registrationFinished(false, err);
      break;
    case Endpoint::Login:
      emit loginFinished(false, {}, err);
      break;
    case Endpoint::Upload:
      emit uploadFinished(false, err);
      break;
    case Endpoint::DownloadImage:
      emit imageDownloadFinished(false, QImage(), err);
      break;
    case Endpoint::VerifyToken:
      emit verifyTokenFinished(false, err);
      break;
    }
    reply->deleteLater();
    return;
  }

  // 处理图片下载
  if (ep == Endpoint::DownloadImage) {
    handleImageDownload(reply);
    reply->deleteLater();
    return;
  }

  // 解析 JSON 统一响应格式：{ "code":0, "message":"...", "token":"...", ... }
  handleJsonResponse(reply, ep);
  reply->deleteLater();
}

void ClipboardApiClient::handleJsonResponse(QNetworkReply* reply, Endpoint ep) {
  const auto bytes = reply->readAll();
  const auto doc = QJsonDocument::fromJson(bytes);
  const auto obj = doc.object();

  switch (ep) {
  case Endpoint::Register: {
    bool success = obj.value("code").toInt() == 0;
    QString message = obj.value("message").toString();
    emit registrationFinished(success, message);
  } break;
  case Endpoint::Login: {
    QString accessToken = obj.value("access_token").toString();
    QString refreshToken = obj.value("refresh_token").toString();
    emit loginFinished(true, {.accessToken = accessToken, .refreshToken = refreshToken}, "");
  } break;
  case Endpoint::Upload: {
    emit uploadFinished(true, bytes);
  } break;
  case Endpoint::VerifyToken: {
    bool success = obj.value("code").toInt() == 0;
    QString message = obj.value("message").toString();
    emit verifyTokenFinished(success, message);
  } break;
  default:
    break;
  }
}

void ClipboardApiClient::handleImageDownload(QNetworkReply* reply) {
  const auto bytes = reply->readAll();

  // Images are not E2E-encrypted — the server stores and serves them in
  // plaintext so it can manage the image lifecycle. Transport security
  // is provided by TLS.
  QImage image = QImage::fromData(bytes);

  if (image.isNull()) {
    emit imageDownloadFinished(false, QImage(), "Failed to load image data");
  }
  else {
    emit imageDownloadFinished(true, image, "");
  }
}
