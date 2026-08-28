/**
 * Created by LMR on 2023/10/27.
 */

#include "Item.h"
#include "ui_Item.h"
#include <QDebug>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QStyleHints>
#include <QVariant>

#include "CustomToolTip.h"

namespace {
class RoundedPreviewLabel final : public QLabel {
public:
  explicit RoundedPreviewLabel(QWidget* parent = nullptr) : QLabel(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setContentsMargins(6, 6, 6, 6);
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF backgroundRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath backgroundPath;
    backgroundPath.addRoundedRect(backgroundRect, 10.0, 10.0);

    painter.fillPath(backgroundPath, palette().color(QPalette::Window));
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.drawPath(backgroundPath);
    painter.setClipPath(backgroundPath);

    QLabel::paintEvent(event);
  }
};

QPoint AdjustPopupPosition(QWidget* anchor, const QSize& popupSize) {
  QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height()));

  QScreen* screen = QGuiApplication::screenAt(pos);
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  QRect screenRect = screen->availableGeometry();

  if (pos.x() + popupSize.width() > screenRect.right())
    pos.setX(screenRect.right() - popupSize.width());
  if (pos.y() + popupSize.height() > screenRect.bottom())
    pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - popupSize.height());

  return pos;
}

bool IsPreviewable(int metaType) { return metaType == QMetaType::QImage || metaType == QMetaType::QString; }
} // namespace

Item::Item(QWidget* parent) : QWidget(parent), ui(new Ui::Item) {
  ui->setupUi(this);

  tipWidget = new CustomToolTip(this);
  ui->label->setWordWrap(true);
  ui->label->setAlignment(Qt::AlignTop);
  ui->deletePushButton->setIcon(QIcon(":/resources/images/delete.svg"));
  ui->infoPushButton->setIcon(QIcon(":/resources/images/info.svg"));
  ui->infoPushButton->setEnabled(false);

  ui->infoPushButton->installEventFilter(this);

  ApplyTheme(QGuiApplication::styleHints()->colorScheme());

  // 连接系统主题变化信号 Qt 6.5 support
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &Item::ApplyTheme);
  connect(ui->deletePushButton, &QPushButton::clicked, this, &Item::DeleteButtonClicked);
}

Item::Item(const QString& text, QWidget* parent) : Item(parent) {
  //
  ui->label->setText(text);
}

Item::~Item() { delete ui; }

void Item::SetData(const ClipboardSourceInfo& sourceInfo, const QByteArray& hash) {
  metaType = sourceInfo.data.userType();

  if (metaType == QMetaType::QString) {
    SetText(sourceInfo.data.toString());
    ui->label->installEventFilter(this);
  }
  else if (metaType == QMetaType::QPixmap) {
    qDebug() << "Item add  Pixmap";

    auto pixmap = sourceInfo.data.value<QPixmap>();
    ui->label->setPixmap(pixmap);
  }
  else if (metaType == QMetaType::QImage) {
    qDebug() << "Item add QImage";

    latestImage = sourceInfo.data.value<QImage>();

    auto pixmap = latestImage.scaled(this->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    ui->label->setPixmap(QPixmap::fromImage(pixmap));

    // Enable hover preview for image items
    ui->label->installEventFilter(this);
  }

  hashValue = hash;

  tipWidget->SetData(sourceInfo);
}

void Item::SetText(const QString& text) { ui->label->setText(text); }

QString Item::GetText() const { return ui->label->text(); }

void Item::SetListWidgetItem(QListWidgetItem* listWidgetItem) { listItem = listWidgetItem; }

QListWidgetItem* Item::GetListWidgetItem() const { return listItem == nullptr ? nullptr : listItem; }

void Item::DeleteButtonClicked() { emit deleteButtonClickedSignal(GetListWidgetItem()); }

void Item::ApplyTheme(Qt::ColorScheme scheme) {
  switch (scheme) {
    case Qt::ColorScheme::Dark: {
      ui->deletePushButton->setIcon(QIcon(":/resources/images/delete-white.svg"));
      ui->infoPushButton->setIcon(QIcon(":/resources/images/info-white.svg"));
    } break;
    case Qt::ColorScheme::Light:
    case Qt::ColorScheme::Unknown: {
      ui->deletePushButton->setIcon(QIcon(":/resources/images/delete.svg"));
      ui->infoPushButton->setIcon(QIcon(":/resources/images/info.svg"));
    } break;
  }
}

QImage Item::GetImage() const { return latestImage; }

QByteArray Item::GetHashValue() const { return hashValue; }

int Item::GetMetaType() const { return metaType; }

void Item::SetUploadStatus(bool success) { tipWidget->SetSynced(success); }

bool Item::eventFilter(QObject* watched, QEvent* event) {
  const auto type = event->type();
  if (type != QEvent::Enter && type != QEvent::Leave)
    return QWidget::eventFilter(watched, event);

  if (watched == ui->infoPushButton) {
    if (type == QEvent::Enter) {
      tipWidget->adjustSize();
      tipWidget->move(AdjustPopupPosition(ui->infoPushButton, tipWidget->size()));
      tipWidget->show();
      return true;
    }
    else if (type == QEvent::Leave) {
      tipWidget->hide();
      return true;
    }
  }
  else if (watched == ui->label && IsPreviewable(metaType)) {
    if (type == QEvent::Enter) {
      if (metaType == QMetaType::QString) {
        // 只在文字溢出时才显示预览
        QFontMetrics fm(ui->label->font());
        const QRect contentRect = ui->label->contentsRect();
        QRect needed = fm.boundingRect(contentRect, Qt::TextWordWrap, ui->label->text());
        if (needed.height() <= contentRect.height())
          // 文本没有溢出，不需要预览
          return true;
      }
      ShowPreview(ui->label);
      return true;
    }
    else if (type == QEvent::Leave) {
      HidePreview();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void Item::EnsurePreviewLabel() {
  if (previewLabel)
    return;
  previewLabel = new RoundedPreviewLabel();
}

void Item::ShowPreview(QWidget* anchor) {
  EnsurePreviewLabel();

  if (metaType == QMetaType::QImage) {
    constexpr int maxPreviewSize = 400;
    qreal dpr = previewLabel->devicePixelRatioF();
    int scaledMaxSize = static_cast<int>(maxPreviewSize * dpr);
    QImage scaled = latestImage.scaled(scaledMaxSize, scaledMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap pixmap = QPixmap::fromImage(scaled);
    pixmap.setDevicePixelRatio(dpr);
    previewLabel->setPixmap(pixmap);
  }
  else {
    previewLabel->setWordWrap(true);
    previewLabel->setTextFormat(Qt::PlainText);
    previewLabel->setMaximumWidth(400);
    previewLabel->setMaximumHeight(300);
    previewLabel->setText(ui->label->text());
  }

  previewLabel->adjustSize();
  previewLabel->move(AdjustPopupPosition(anchor, previewLabel->size()));
  previewLabel->show();
}

void Item::HidePreview() {
  if (previewLabel)
    previewLabel->hide();
}