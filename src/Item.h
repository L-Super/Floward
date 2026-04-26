/**
 * Created by LMR on 2023/10/27.
 */

#ifndef ITEM_H
#define ITEM_H

#include <QDateTime>
#include <QWidget>

struct ClipboardSourceInfo {
  QIcon icon;          // 可执行文件图标
  QString processName; // 可执行文件名
  QString processPath; // 可执行文件路径
  QDateTime timestamp; // 时间戳
  QVariant data;       // 剪贴板数据
};

class QListWidgetItem;
class QLabel;
class CustomToolTip;

QT_BEGIN_NAMESPACE
namespace Ui {
class Item;
}
QT_END_NAMESPACE

class Item : public QWidget {
  Q_OBJECT

public:
  explicit Item(QWidget* parent = nullptr);
  explicit Item(const QString& text, QWidget* parent = nullptr);

  ~Item() override;

  void SetData(const ClipboardSourceInfo& data, const QByteArray& hash);
  void SetListWidgetItem(QListWidgetItem* listWidgetItem);
  QListWidgetItem* GetListWidgetItem() const;

  void SetText(const QString& text);
  QString GetText() const;
  QImage GetImage() const;
  QByteArray GetHashValue() const;
  int GetMetaType() const;

protected:
  void DeleteButtonClicked();
  void ApplyTheme(Qt::ColorScheme colorScheme);
  bool eventFilter(QObject *watched, QEvent *event) override;
  void ShowImagePreview(QWidget* anchor);
  void HideImagePreview();

signals:
  void itemClickedSignal(const QString& text);
  void deleteButtonClickedSignal(QListWidgetItem*);

private:
  Ui::Item* ui;
  QListWidgetItem* listItem{};
  QImage latestImage;
  QByteArray hashValue;
  int metaType{0}; // default is UnknownType
  CustomToolTip* tipWidget;
  QLabel* imagePreviewLabel{};
};

#endif // ITEM_H
