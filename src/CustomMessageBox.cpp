#include "CustomMessageBox.h"
#include "ui_CustomMessageBox.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QLayout>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStyle>
#include <QTextBrowser>
#include <QWidget>
#include <QWindow>

#include <algorithm>

namespace {
constexpr int kPreferredWidth = 540;
constexpr int kMinimumWidth = 420;
constexpr int kScreenMargin = 24;

void RefreshStyle(QWidget* widget) {
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}

QRect AvailableGeometryFor(QWidget* widget) {
  const auto geometryForWidget = [](QWidget* candidate) -> QRect {
    if (!candidate) {
      return {};
    }

    QWidget* topLevel = candidate->window();
    if (!topLevel) {
      topLevel = candidate;
    }

    if (!topLevel->isVisible()) {
      return {};
    }

    if (QScreen* screen = QGuiApplication::screenAt(topLevel->frameGeometry().center())) {
      return screen->availableGeometry();
    }

    if (QWindow* handle = topLevel->windowHandle(); handle && handle->screen()) {
      return handle->screen()->availableGeometry();
    }

    return {};
  };

  if (const QRect geometry = geometryForWidget(widget); geometry.isValid()) {
    return geometry;
  }

  if (const QRect geometry = geometryForWidget(QApplication::activeWindow()); geometry.isValid()) {
    return geometry;
  }

  if (QScreen* screen = QGuiApplication::screenAt(QCursor::pos())) {
    return screen->availableGeometry();
  }

  if (QScreen* screen = QGuiApplication::primaryScreen()) {
    return screen->availableGeometry();
  }

  return {};
}
} // namespace

CustomMessageBox::CustomMessageBox(QWidget* parent)
    : QDialog(nullptr), ui(new Ui::CustomMessageBox), anchorWidget_(parent) {
  ui->setupUi(this);

  Qt::WindowFlags flags = Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint |
                          Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint;

  setWindowFlags(flags);
  setAttribute(Qt::WA_StyledBackground, true);

  ui->panelFrame->setAttribute(Qt::WA_StyledBackground, true);
  ui->bodyLabel->clear();
  ui->informativeLabel->hide();
  ui->detailsBrowser->hide();
  ui->secondaryButton->hide();

  connect(ui->primaryButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(ui->secondaryButton, &QPushButton::clicked, this, &QDialog::reject);

  ui->primaryButton->setDefault(true);
  ui->primaryButton->setAutoDefault(true);

  updateTypeStyle();
}

CustomMessageBox::~CustomMessageBox() { delete ui; }

void CustomMessageBox::setType(Type type) {
  type_ = type;
  updateTypeStyle();
}

void CustomMessageBox::setAnchorWidget(QWidget* widget) { anchorWidget_ = widget; }

void CustomMessageBox::setTitle(const QString& title) {
  ui->titleLabel->setText(title);
}

void CustomMessageBox::setText(const QString& text) {
  ui->bodyLabel->setText(text);
  adjustSize();
}

void CustomMessageBox::setInformativeText(const QString& text) {
  ui->informativeLabel->setVisible(!text.trimmed().isEmpty());
  ui->informativeLabel->setText(text);
  adjustSize();
}

void CustomMessageBox::setPrimaryButtonText(const QString& text) {
  ui->primaryButton->setVisible(!text.isEmpty());
  ui->primaryButton->setText(text);
}

void CustomMessageBox::setSecondaryButtonText(const QString& text) {
  ui->secondaryButton->setVisible(!text.isEmpty());
  ui->secondaryButton->setText(text);
}

void CustomMessageBox::setDetailsMarkdown(const QString& markdown) {
  ui->detailsBrowser->setVisible(!markdown.trimmed().isEmpty());
  ui->detailsBrowser->setMarkdown(markdown);
  adjustSize();
}

void CustomMessageBox::warning(QWidget* parent, const QString& title, const QString& text,
                               const QString& informativeText, const QString& details, const QString& acceptText) {
  CustomMessageBox box(parent);
  box.setType(Type::Warning);
  box.setTitle(title);
  box.setText(text);
  box.setInformativeText(informativeText);
  box.setPrimaryButtonText(acceptText);
  box.setSecondaryButtonText({});
  box.setDetailsMarkdown(details);
  box.exec();
}

bool CustomMessageBox::question(QWidget* parent, const QString& title, const QString& text,
                                const QString& informativeText, const QString& details, const QString& acceptText,
                                const QString& rejectText) {
  CustomMessageBox box(parent);
  box.setType(Type::Question);
  box.setTitle(title);
  box.setText(text);
  box.setInformativeText(informativeText);
  box.setPrimaryButtonText(acceptText);
  box.setSecondaryButtonText(rejectText);
  box.setDetailsMarkdown(details);
  return box.exec() == QDialog::Accepted;
}

void CustomMessageBox::showEvent(QShowEvent* event) {
  centerOnScreen();
  QDialog::showEvent(event);
}

void CustomMessageBox::updateTypeStyle() {
  QString typeName;
  QString symbol;
  switch (type_) {
    case Type::Information:
      typeName = "information";
      symbol = "i";
      break;
    case Type::Warning:
      typeName = "warning";
      symbol = "!";
      break;
    case Type::Question:
      typeName = "question";
      symbol = "?";
      break;
  }

  setProperty("messageType", typeName);
  ui->badgeLabel->setProperty("messageType", typeName);
  ui->panelFrame->setProperty("messageType", typeName);
  ui->badgeLabel->setText(symbol);

  RefreshStyle(this);
  RefreshStyle(ui->badgeLabel);
  RefreshStyle(ui->panelFrame);
}

void CustomMessageBox::centerOnScreen() {
  const QRect availableGeometry = AvailableGeometryFor(anchorWidget_);
  if (!availableGeometry.isValid()) {
    return;
  }

  const int maxWidth = std::max(320, availableGeometry.width() - kScreenMargin * 2);
  const int targetWidth = maxWidth >= kMinimumWidth ? std::min(kPreferredWidth, maxWidth) : maxWidth;
  setFixedWidth(targetWidth);

  const int maxHeight = std::max(220, availableGeometry.height() - kScreenMargin * 2);
  const int targetHeight = std::min(sizeHint().height(), maxHeight);
  resize(targetWidth, targetHeight);
  move(availableGeometry.center() - rect().center());
}
