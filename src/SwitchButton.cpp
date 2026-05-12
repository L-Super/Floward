#include "SwitchButton.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

// ======= Primitive helpers =======
static void drawRoundedRectBorder(QPainter* p, const QRect& rect, const QColor& color, qreal borderWidth,
                                  qreal radius) {
  if (borderWidth <= 0.0)
    return;
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(QPen(color, borderWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p->setBrush(Qt::NoBrush);
  const qreal half = borderWidth / 2.0;
  QRectF r = QRectF(rect).marginsRemoved({half, half, half, half});
  const qreal rr = std::max(0.0, radius - half);
  p->drawRoundedRect(r, rr, rr);
}

static void drawCheckBoxIndicator(const QRect& rect, QPainter* p, qreal progress) {
  const auto w = rect.width();
  const auto h = rect.width();
  const auto x = rect.x();
  const auto y = rect.y();
  constexpr auto intendedSize = 16.0;

  const QPointF p1{(4.5 / intendedSize) * w + x, (8.5 / intendedSize) * h + y};
  const QPointF p2{(7.0 / intendedSize) * w + x, (11.0 / intendedSize) * h + y};
  const QPointF p3{(11.5 / intendedSize) * w + x, (5.5 / intendedSize) * h + y};

  QPainterPath path;
  path.moveTo(p1);
  path.lineTo(p2);
  path.lineTo(p3);

  if (1.0 - progress > 0.01) {
    const auto last = path.pointAtPercent(progress);
    QPainterPath trimmed;
    trimmed.moveTo(p1);
    if (progress < 0.5) {
      trimmed.lineTo(last);
    }
    else {
      trimmed.lineTo(p2);
      trimmed.lineTo(last);
    }
    path = trimmed;
  }
  p->drawPath(path);
}

static void drawCloseIndicator(const QRect& rect, QPainter* p) {
  constexpr auto intendedSize = 16.0;
  const auto w = rect.width();
  const auto h = rect.width();
  const auto x = rect.x();
  const auto y = rect.y();

  p->drawLine(QPointF{(4.0 / intendedSize) * w + x, (4.0 / intendedSize) * h + y},
              QPointF{(12.0 / intendedSize) * w + x, (12.0 / intendedSize) * h + y});
  p->drawLine(QPointF{(12.0 / intendedSize) * w + x, (4.0 / intendedSize) * h + y},
              QPointF{(4.0 / intendedSize) * w + x, (12.0 / intendedSize) * h + y});
}

// ======= SwitchButton::Theme =======
static QColor withAlpha(QColor c, int a) {
  c.setAlpha(a);
  return c;
}

SwitchButton::Theme SwitchButton::Theme::fromPalette(const QPalette& pal) {
  Theme t;
  t.backgroundColorMain1 = pal.color(QPalette::Base);
  t.backgroundColorMain2 = pal.color(QPalette::Window);
  t.backgroundColorMain3 = pal.color(QPalette::Mid);

  t.primaryColor = pal.color(QPalette::Highlight);
  t.primaryColorHovered = t.primaryColor.lighter(110);
  t.primaryColorPressed = t.primaryColor.darker(110);
  t.primaryColorDisabled = withAlpha(t.primaryColor, 120);

  t.primaryColorForeground = pal.color(QPalette::HighlightedText);
  t.primaryColorForegroundHovered = t.primaryColorForeground;
  t.primaryColorForegroundPressed = t.primaryColorForeground;
  t.primaryColorForegroundDisabled = withAlpha(t.primaryColorForeground, 120);

  t.secondaryColor = pal.color(QPalette::Text);
  t.secondaryColorHovered = t.secondaryColor;
  t.secondaryColorPressed = t.secondaryColor;
  t.secondaryColorDisabled = pal.color(QPalette::Disabled, QPalette::Text);

  // On some Linux themes (e.g. Adwaita/Yaru), QPalette::Mid is too close to Base,
  // making the switch border invisible. Ensure minimum contrast against the background.
  t.borderColor = pal.color(QPalette::Dark);
  t.borderColorHovered = pal.color(QPalette::Shadow);
  t.borderColorPressed = pal.color(QPalette::Shadow);
  t.borderColorDisabled = pal.color(QPalette::Disabled, QPalette::Dark);

  t.focusColor = pal.color(QPalette::Highlight);
  return t;
}

const QColor& SwitchButton::Theme::switchGrooveColor(MouseState mouse, CheckState checked) const {
  if (checked != CheckState::NotChecked) {
    switch (mouse) {
      case MouseState::Pressed:
        return primaryColorPressed;
      case MouseState::Hovered:
        return primaryColorHovered;
      case MouseState::Disabled:
        return primaryColorDisabled;
      default:
        return primaryColor;
    }
  }
  switch (mouse) {
    case MouseState::Pressed:
      return backgroundColorMain3;
    case MouseState::Disabled:
      return backgroundColorMain2;
    default:
      return backgroundColorMain1;
  }
}

const QColor& SwitchButton::Theme::switchGrooveBorderColor(MouseState mouse, FocusState focus, CheckState checked) const {
  if (checked != CheckState::NotChecked)
    return switchGrooveColor(mouse, checked);
  if (focus == FocusState::Focused)
    return primaryColor;
  switch (mouse) {
    case MouseState::Hovered:
      return borderColorHovered;
    case MouseState::Pressed:
      return borderColorPressed;
    case MouseState::Disabled:
      return borderColorDisabled;
    default:
      return borderColor;
  }
}

const QColor& SwitchButton::Theme::switchHandleColor(MouseState mouse, CheckState checked) const {
  const bool primary = checked != CheckState::NotChecked;
  switch (mouse) {
    case MouseState::Pressed:
      return primary ? primaryColorForegroundPressed : secondaryColorPressed;
    case MouseState::Hovered:
      return primary ? primaryColorForegroundHovered : secondaryColorHovered;
    case MouseState::Disabled:
      return primary ? primaryColorForegroundDisabled : secondaryColorDisabled;
    default:
      return primary ? primaryColorForeground : secondaryColor;
  }
}

const QColor& SwitchButton::Theme::labelForegroundColor(MouseState mouse) const {
  return (mouse == MouseState::Disabled) ? secondaryColorDisabled : secondaryColor;
}

// ======= SwitchButton =======
SwitchButton::SwitchButton(QWidget* parent) : QAbstractButton(parent) {
  setCheckable(true);
  setChecked(false);
  setAutoRepeat(false);

  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  updateTheme();

  fullHandlePadding_ = theme_.borderWidth * 2;

  setupAnimation();
}

SwitchButton::MouseState SwitchButton::getMouseState(bool pressed, bool hovered, bool enabled) {
  if (!enabled)
    return MouseState::Disabled;
  if (pressed)
    return MouseState::Pressed;
  if (hovered)
    return MouseState::Hovered;
  return MouseState::Normal;
}

SwitchButton::CheckState SwitchButton::getCheckState(Qt::CheckState s) {
  switch (s) {
    case Qt::PartiallyChecked:
      return CheckState::Indeterminate;
    case Qt::Checked:
      return CheckState::Checked;
    case Qt::Unchecked:
    default:
      return CheckState::NotChecked;
  }
}

void SwitchButton::setTheme(const Theme& t) {
  theme_ = t;
  themeOverride_ = true;
  fullHandlePadding_ = theme_.borderWidth * 2;

  startAnimation();
}

void SwitchButton::updateTheme() {
  if (themeOverride_)
    return;
  theme_ = Theme::fromPalette(palette());
}

QSize SwitchButton::sizeHint() const {
  const auto fm = fontMetrics();
  const auto textW = fm.horizontalAdvance(text());
  const auto textH = fm.height();
  const auto spacing = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  const auto switchRect = getSwitchRect();
  auto w = (textW > 0 ? textW + spacing : 0) + switchRect.width();

  const auto iconSize = this->iconSize();
  const auto hasIcon = !icon().isNull() || iconSize.isEmpty();
  const auto iconH = hasIcon ? iconSize.height() : 0;
  if (hasIcon)
    w += iconSize.width() + spacing;

  const auto h = std::max({textH, switchRect.height(), iconH, theme_.controlHeightMedium});
  return {w, h};
}

void SwitchButton::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const auto spacing = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  const auto contentRect = rect();
  const auto iconSize = this->iconSize();
  const auto hasIcon = !icon().isNull() || iconSize.isEmpty();
  const auto hasText = !text().isEmpty();

  // Draw switch button.
  const auto switchRect = getSwitchRect();
  const auto switchRadius = switchRect.height() / 2.0;

  const auto bgColor = bgAnimation_.currentValue().value<QColor>();
  const auto fgColor = fgAnimation_.currentValue().value<QColor>();
  const auto borderColor = borderAnimation_.currentValue().value<QColor>();
  const auto textColor = getTextColor();

  constexpr auto borderW = 1.0;
  constexpr auto halfBorderW = borderW / 2.0;
  const auto bgRect = QRectF(switchRect).marginsRemoved({halfBorderW, halfBorderW, halfBorderW, halfBorderW});
  p.setPen(Qt::NoPen);
  p.setBrush(bgColor);
  p.drawRoundedRect(bgRect, switchRadius, switchRadius);
  drawRoundedRectBorder(&p, switchRect, borderColor, borderW, switchRadius);

  // Draw handle.
  const auto handleXRatio = handleAnimation_.currentValue().toDouble();
  const auto handlePadding = handlePaddingAnimation_.currentValue().toInt();
  const auto handleDiameter = static_cast<double>(switchRect.height() - handlePadding * 2);
  const auto handleGrooveWidth = switchRect.width() - handlePadding * 2 - handleDiameter;
  const auto handleX = switchRect.x() + handlePadding + handleGrooveWidth * handleXRatio;
  const auto handleY = static_cast<double>(switchRect.y() + handlePadding);
  const auto handleRect = QRectF{handleX, handleY, handleDiameter, handleDiameter};
  p.setPen(Qt::NoPen);
  p.setBrush(fgColor);
  p.drawEllipse(handleRect);

  auto availableX = switchRect.x() + switchRect.width() + spacing;
  auto availableW = contentRect.width() - switchRect.width() - spacing;

  // Draw accessibility symbols.
  if (showAccessibilitySymbols() && checkState() != Qt::PartiallyChecked) {
    constexpr auto checkThickness = 1.01;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen{bgColor, checkThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
    if (checkState() == Qt::Checked) {
      drawCheckBoxIndicator(handleRect.toRect(), &p, symbolAnimation_.currentValue().toReal());
    }
    else {
      drawCloseIndicator(handleRect.toRect().marginsRemoved({1, 1, 1, 1}), &p);
    }
  }

  // Draw icon.
  const auto extent = iconSize.height();
  if (hasIcon && availableW >= extent) {
    const auto iconX = availableX;
    const auto iconY = contentRect.y() + (contentRect.height() - extent) / 2;
    const auto iconRect = QRect{iconX, iconY, extent, extent};
    p.drawPixmap(iconRect, icon().pixmap(extent, extent));

    availableX += extent + spacing;
    availableW -= extent + spacing;
  }

  // Draw text.
  if (hasText && availableW > 0) {
    const auto textRect = QRect{availableX, contentRect.y(), availableW, contentRect.height()};
    const auto elidedText = fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width(), Qt::TextSingleLine);
    p.setBrush(Qt::NoBrush);
    p.setPen(textColor);
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);
  }
}

void SwitchButton::enterEvent(QEnterEvent* e) {
  QAbstractButton::enterEvent(e);
  isMouseOver_ = true;
  startAnimation();
}

void SwitchButton::leaveEvent(QEvent* e) {
  QAbstractButton::leaveEvent(e);
  isMouseOver_ = false;
  startAnimation();
}

void SwitchButton::changeEvent(QEvent* e) {
  QAbstractButton::changeEvent(e);
  const auto type = e->type();
  if (type == QEvent::EnabledChange || type == QEvent::PaletteChange || type == QEvent::ApplicationPaletteChange) {
    updateTheme();
    startAnimation();
  }
}

void SwitchButton::focusInEvent(QFocusEvent* e) {
  QAbstractButton::focusInEvent(e);
  startAnimation();
}

void SwitchButton::focusOutEvent(QFocusEvent* e) {
  QAbstractButton::focusOutEvent(e);
  startAnimation();
}

void SwitchButton::checkStateSet() {
  if (blockRefresh_)
    return;
  intermediate_ = false;
  const auto state = checkState();
  if (state != publishedState_) {
    publishedState_ = state;
    Q_EMIT checkStateChanged(state);
  }
  startAnimation();
}

void SwitchButton::nextCheckState() {
  if (tristate_) {
    setCheckState(static_cast<Qt::CheckState>((checkState() + 1) % 3));
  }
  else {
    QAbstractButton::nextCheckState();
    checkStateSet();
  }
}

void SwitchButton::setupAnimation() {
  constexpr auto animationDuration = 0;

  bgAnimation_.setDuration(animationDuration);
  bgAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  bgAnimation_.setStartValue(getBgColor());
  bgAnimation_.setEndValue(getBgColor());
  connect(&bgAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });

  borderAnimation_.setDuration(animationDuration);
  borderAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  borderAnimation_.setStartValue(getBorderColor());
  borderAnimation_.setEndValue(getBorderColor());
  connect(&borderAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });

  fgAnimation_.setDuration(animationDuration);
  fgAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  fgAnimation_.setStartValue(getFgColor());
  fgAnimation_.setEndValue(getFgColor());
  connect(&fgAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });

  connect(this, &QAbstractButton::pressed, this, [this] { startAnimation(); });
  connect(this, &QAbstractButton::released, this, [this] { startAnimation(); });
  connect(this, &QAbstractButton::toggled, this, [this] { startAnimation(); });
  connect(this, &SwitchButton::checkStateChanged, this, [this] { startAnimation(); });

  handleAnimation_.setDuration(animationDuration);
  handleAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  handleAnimation_.setStartValue(0.);
  handleAnimation_.setEndValue(0.);
  connect(&handleAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });

  handlePaddingAnimation_.setDuration(animationDuration);
  handlePaddingAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  handlePaddingAnimation_.setStartValue(fullHandlePadding_);
  handlePaddingAnimation_.setEndValue(fullHandlePadding_);
  connect(&handlePaddingAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });

  symbolAnimation_.setDuration(animationDuration);
  symbolAnimation_.setEasingCurve(QEasingCurve::OutCubic);
  symbolAnimation_.setStartValue(0.);
  symbolAnimation_.setEndValue(0.);
  connect(&symbolAnimation_, &QVariantAnimation::valueChanged, this, [this] { update(); });
}

void SwitchButton::startAnimation() {
  const auto animationDuration = isVisible() ? style()->styleHint(QStyle::SH_Widget_Animation_Duration) : 0;

  auto currentBg = bgAnimation_.currentValue();
  bgAnimation_.stop();
  bgAnimation_.setDuration(animationDuration);
  bgAnimation_.setStartValue(currentBg);
  bgAnimation_.setEndValue(getBgColor());
  bgAnimation_.start();

  auto currentBorder = borderAnimation_.currentValue();
  borderAnimation_.stop();
  borderAnimation_.setDuration(animationDuration);
  borderAnimation_.setStartValue(currentBorder);
  borderAnimation_.setEndValue(getBorderColor());
  borderAnimation_.start();

  auto currentFg = fgAnimation_.currentValue();
  fgAnimation_.stop();
  fgAnimation_.setDuration(animationDuration);
  fgAnimation_.setStartValue(currentFg);
  fgAnimation_.setEndValue(getFgColor());
  fgAnimation_.start();

  auto currentXRatio = handleAnimation_.currentValue();
  const auto state = checkState();
  handleAnimation_.stop();
  handleAnimation_.setDuration(animationDuration);
  handleAnimation_.setStartValue(currentXRatio);
  handleAnimation_.setEndValue(state == Qt::Checked ? 1. : (state == Qt::Unchecked ? 0. : 0.5));
  handleAnimation_.start();

  auto currentPadding = handlePaddingAnimation_.currentValue();
  handlePaddingAnimation_.stop();
  handlePaddingAnimation_.setDuration(animationDuration);
  handlePaddingAnimation_.setStartValue(currentPadding);
  handlePaddingAnimation_.setEndValue(fullHandlePadding_ * (state == Qt::PartiallyChecked ? 3. : 1.));
  handlePaddingAnimation_.start();

  auto currentSymbol = symbolAnimation_.currentValue();
  symbolAnimation_.stop();
  symbolAnimation_.setDuration(animationDuration);
  symbolAnimation_.setStartValue(currentSymbol);
  symbolAnimation_.setEndValue(1.);
  symbolAnimation_.start();
}

QColor SwitchButton::getBgColor() const {
  const auto mouse = getMouseState(isDown(), isMouseOver_, isEnabled());
  const auto check = getCheckState(checkState());
  return theme_.switchGrooveColor(mouse, check);
}

QColor SwitchButton::getBorderColor() const {
  const auto mouse = getMouseState(isDown(), isMouseOver_, isEnabled());
  const auto focus = hasFocus() ? FocusState::Focused : FocusState::NotFocused;
  const auto check = getCheckState(checkState());
  return theme_.switchGrooveBorderColor(mouse, focus, check);
}

QColor SwitchButton::getFgColor() const {
  const auto mouse = getMouseState(isDown(), isMouseOver_, isEnabled());
  const auto check = getCheckState(checkState());
  return theme_.switchHandleColor(mouse, check);
}

QColor SwitchButton::getTextColor() const {
  const auto mouse = getMouseState(isDown(), isMouseOver_, isEnabled());
  return theme_.labelForegroundColor(mouse);
}

QRect SwitchButton::getSwitchRect() const {
  const auto contentRect = rect();
  const auto switchW = theme_.controlHeightLarge;
  const auto switchH = theme_.controlHeightSmall;
  const auto switchX = contentRect.x();
  const auto switchY = contentRect.y() + (contentRect.height() - switchH) / 2;
  return {switchX, switchY, switchW, switchH};
}

void SwitchButton::setTristate(bool tristate) {
  tristate_ = tristate;
  update();
  Q_EMIT tristateChanged(tristate);
}

bool SwitchButton::isTristate() const { return tristate_; }

void SwitchButton::setShowAccessibilitySymbols(bool showAccessibilitySymbols) {
  showAccessibilitySymbols_ = showAccessibilitySymbols;
  update();
  Q_EMIT showAccessibilitySymbolsChanged(showAccessibilitySymbols);
}

bool SwitchButton::showAccessibilitySymbols() const { return showAccessibilitySymbols_; }

Qt::CheckState SwitchButton::checkState() const {
  if (tristate_ && intermediate_)
    return Qt::PartiallyChecked;
  return isChecked() ? Qt::Checked : Qt::Unchecked;
}

void SwitchButton::setCheckState(Qt::CheckState state) {
  if (state == Qt::PartiallyChecked) {
    tristate_ = true;
    intermediate_ = true;
  }
  else {
    intermediate_ = false;
  }
  blockRefresh_ = true;
  setChecked(state != Qt::Unchecked);
  blockRefresh_ = false;
  update();

  if (state != publishedState_) {
    publishedState_ = state;
    Q_EMIT checkStateChanged(state);
  }
}
