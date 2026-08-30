#pragma once

#include <QDialog>
#include <QPointer>
#include <QString>

class QShowEvent;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class CustomMessageBox;
}
QT_END_NAMESPACE

class CustomMessageBox final : public QDialog {
  Q_OBJECT

public:
  enum class Type { Information, Warning, Question };

  explicit CustomMessageBox(QWidget* parent = nullptr);
  ~CustomMessageBox() override;

  void setType(Type type);
  void setAnchorWidget(QWidget* widget);
  void setTitle(const QString& title);
  void setText(const QString& text);
  void setInformativeText(const QString& text);
  void setPrimaryButtonText(const QString& text);
  void setSecondaryButtonText(const QString& text);
  void setDetailsMarkdown(const QString& markdown);

  static void warning(QWidget* parent, const QString& title, const QString& text,
                      const QString& informativeText = {}, const QString& details = {},
                      const QString& acceptText = QStringLiteral("知道了"));

  static bool question(QWidget* parent, const QString& title, const QString& text,
                       const QString& informativeText = {}, const QString& details = {},
                       const QString& acceptText = QStringLiteral("确定"),
                       const QString& rejectText = QStringLiteral("取消"));

protected:
  void showEvent(QShowEvent* event) override;

private:
  void updateTypeStyle();
  void centerOnScreen();

private:
  Ui::CustomMessageBox* ui;
  QPointer<QWidget> anchorWidget_;
  Type type_{Type::Information};
};
