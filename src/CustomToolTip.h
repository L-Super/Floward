//
// Created by LMR on 2025/12/7.
//

#pragma once

#include <QWidget>

#include "Item.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CustomToolTip;
}
QT_END_NAMESPACE

class CustomToolTip : public QWidget {
  Q_OBJECT

public:
  explicit CustomToolTip(QWidget* parent = nullptr);
  ~CustomToolTip() override;

  void SetData(const ClipboardSourceInfo& sourceInfo);
  void SetSynced(bool synced);

protected:
  void paintEvent(QPaintEvent* event) override;
private:
  Ui::CustomToolTip* ui;
  bool m_synced{false};
};
