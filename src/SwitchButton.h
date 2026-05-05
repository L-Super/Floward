#pragma once
#include <QAbstractButton>
#include <QVariantAnimation>

class SwitchButton final : public QAbstractButton {
  Q_OBJECT
  Q_PROPERTY(bool tristate READ isTristate WRITE setTristate NOTIFY tristateChanged)
  Q_PROPERTY(bool showAccessibilitySymbols READ showAccessibilitySymbols WRITE setShowAccessibilitySymbols NOTIFY
                 showAccessibilitySymbolsChanged)

public:
  enum class MouseState { Normal, Hovered, Pressed, Disabled };
  enum class FocusState { Focused, NotFocused };
  enum class CheckState { NotChecked, Checked, Indeterminate };

  struct Theme {
    // sizes
    int controlHeightLarge = 28;
    int controlHeightSmall = 16;
    int controlHeightMedium = 24;
    int spacing = 6;
    int borderWidth = 1;
    int focusBorderWidth = 2;
    double borderRadius = 8.0;

    // colors
    QColor backgroundColorMain1;
    QColor backgroundColorMain2;
    QColor backgroundColorMain3;

    QColor primaryColor;
    QColor primaryColorHovered;
    QColor primaryColorPressed;
    QColor primaryColorDisabled;

    QColor primaryColorForeground;
    QColor primaryColorForegroundHovered;
    QColor primaryColorForegroundPressed;
    QColor primaryColorForegroundDisabled;

    QColor secondaryColor;
    QColor secondaryColorHovered;
    QColor secondaryColorPressed;
    QColor secondaryColorDisabled;

    QColor borderColor;
    QColor borderColorHovered;
    QColor borderColorPressed;
    QColor borderColorDisabled;

    QColor focusColor;

    static Theme fromPalette(const QPalette& pal);

    const QColor& switchGrooveColor(MouseState mouse, CheckState checked) const;
    const QColor& switchGrooveBorderColor(MouseState mouse, FocusState focus, CheckState checked) const;
    const QColor& switchHandleColor(MouseState mouse, CheckState checked) const;
    const QColor& labelForegroundColor(MouseState mouse) const;
  };

public:
  explicit SwitchButton(QWidget* parent = nullptr);

  QSize sizeHint() const override;

  void setTristate(bool);
  bool isTristate() const;

  bool showAccessibilitySymbols() const;
  void setShowAccessibilitySymbols(bool showAccessibilitySymbols);

  Qt::CheckState checkState() const;
  void setCheckState(Qt::CheckState state);

  void setTheme(const Theme& t);
  const Theme& theme() const { return theme_; }

Q_SIGNALS:
  void checkStateChanged(Qt::CheckState);
  void tristateChanged(bool);
  void showAccessibilitySymbolsChanged(bool);

protected:
  void paintEvent(QPaintEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
  void changeEvent(QEvent*) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void checkStateSet() override;
  void nextCheckState() override;

private:
  static MouseState getMouseState(bool pressed, bool hovered, bool enabled);
  static CheckState getCheckState(Qt::CheckState s);

  void setupAnimation();
  void startAnimation();
  QRect getSwitchRect() const;

  QColor getBgColor() const;
  QColor getBorderColor() const;
  QColor getFgColor() const;
  QColor getTextColor() const;

  void updateTheme();

private:
  double fullHandlePadding_{2.0};
  bool isMouseOver_{false};
  bool tristate_{false};
  bool intermediate_{false};
  bool blockRefresh_{false};
  bool showAccessibilitySymbols_{false};
  Qt::CheckState publishedState_{Qt::Unchecked};

  QVariantAnimation handleAnimation_;
  QVariantAnimation handlePaddingAnimation_;
  QVariantAnimation bgAnimation_;
  QVariantAnimation borderAnimation_;
  QVariantAnimation fgAnimation_;
  QVariantAnimation symbolAnimation_;

  Theme theme_{};
  bool themeOverride_{false};
};
