//
// Created by LMR on 2025/12/7.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CustomToolTip.h" resolved

#include "CustomToolTip.h"
#include "ui_CustomToolTip.h"

#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QStyleOption>
#include <QMetaType>

namespace {
QString GetFriendlyTypeName(const QVariant& data) {
  const int typeId = data.metaType().id();

  switch (typeId) {
    case QMetaType::QString:
      return QObject::tr("文本");
    case QMetaType::QImage:
    case QMetaType::QPixmap:
      return QObject::tr("图片");
    case QMetaType::QByteArray:
      return QObject::tr("二进制数据");
    case QMetaType::QUrl:
      return QObject::tr("链接");
    case QMetaType::QStringList:
      return QObject::tr("文本列表");
    case QMetaType::QColor:
      return QObject::tr("颜色");
    default:
      return QObject::tr("其他");
  }
}
} // namespace

CustomToolTip::CustomToolTip(QWidget* parent) : QWidget(parent), ui(new Ui::CustomToolTip) {
  ui->setupUi(this);
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
}

CustomToolTip::~CustomToolTip() { delete ui; }

void CustomToolTip::SetData(const ClipboardSourceInfo& sourceInfo) {
  ui->iconLabel->setPixmap(sourceInfo.icon.pixmap(sourceInfo.icon.actualSize(QSize(16, 16))));
  ui->appLabel->setText(sourceInfo.processName);
  ui->timestampLabel->setText(sourceInfo.timestamp.toString("MM-dd hh:mm:ss"));
  ui->typeLabel->setText(GetFriendlyTypeName(sourceInfo.data));
}

void CustomToolTip::SetSynced(bool synced) {
  if (m_synced != synced) {
    m_synced = synced;
    update(); // 触发重绘
  }
}

void CustomToolTip::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QStyleOption opt;
  opt.initFrom(this);

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  // 圆角背景
  const qreal radius = 10.0;
  QRectF bgRect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath bgPath;
  bgPath.addRoundedRect(bgRect, radius, radius);

  // 透明背景，避免黑底
  painter.fillRect(rect(), Qt::transparent);

  painter.setPen(Qt::NoPen);
  painter.setBrush(opt.palette.window());
  painter.drawPath(bgPath);

  // 已同步时绘制左侧竖条
  if (m_synced) {
    constexpr qreal barWidth = 3.0;
    constexpr qreal barMargin = 1.0;
    QRectF barRect(bgRect.left() + barMargin, bgRect.top() + radius,
                   barWidth, bgRect.height() - 2 * radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#4CAF50"));
    painter.drawRoundedRect(barRect, barWidth / 2, barWidth / 2);
  }

  // 轻描边，增强边缘清晰度
  painter.setPen(QPen(opt.palette.mid(), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(bgPath);

  // 应用圆角遮罩，去掉矩形四角
  setMask(QRegion(bgPath.toFillPolygon().toPolygon()));
}