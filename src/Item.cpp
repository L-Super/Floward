/**
 * Created by LMR on 2023/10/27.
 */

#include "Item.h"
#include "ui_Item.h"
#include <QDebug>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidgetItem>
#include <QPixmap>
#include <QScreen>
#include <QStyleHints>
#include <QVariant>

#include "CustomToolTip.h"

namespace {
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

bool Item::eventFilter(QObject* watched, QEvent* event) {
  if (watched == ui->infoPushButton) {
    if (event->type() == QEvent::Enter) {
      if (QWidget* widget = qobject_cast<QWidget*>(watched)) {
        tipWidget->adjustSize();
        tipWidget->move(AdjustPopupPosition(widget, tipWidget->size()));
        tipWidget->show();
        return true;
      }
    }
    else if (event->type() == QEvent::Leave) {
      tipWidget->hide();
      return true;
    }
  }
  else if (watched == ui->label && metaType == QMetaType::QImage) {
    if (event->type() == QEvent::Enter) {
      ShowImagePreview(ui->label);
      return true;
    }
    else if (event->type() == QEvent::Leave) {
      HideImagePreview();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void Item::ShowImagePreview(QWidget* anchor) {
  if (!imagePreviewLabel) {
    imagePreviewLabel = new QLabel(nullptr);
    imagePreviewLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    imagePreviewLabel->setStyleSheet("QLabel { background-color: palette(window); border: 1px solid palette(mid); "
                                     "border-radius: 6px; padding: 4px; }");
  }

  constexpr int maxPreviewSize = 400;
  qreal dpr = imagePreviewLabel->devicePixelRatioF();
  int scaledMaxSize = static_cast<int>(maxPreviewSize * dpr);
  QImage scaled = latestImage.scaled(scaledMaxSize, scaledMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QPixmap pixmap = QPixmap::fromImage(scaled);
  pixmap.setDevicePixelRatio(dpr);
  imagePreviewLabel->setPixmap(pixmap);
  imagePreviewLabel->adjustSize();

  imagePreviewLabel->move(AdjustPopupPosition(anchor, imagePreviewLabel->size()));
  imagePreviewLabel->show();
}

void Item::HideImagePreview() {
  if (imagePreviewLabel)
    imagePreviewLabel->hide();
}