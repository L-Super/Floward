/**
 * Created by LMR on 2023/10/31.
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include "utils/Util.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AboutDialog) {
  ui->setupUi(this);
  ui->imageLabel->setPixmap(QPixmap(":/resources/icon.png").scaled({80, 80}, Qt::KeepAspectRatio, Qt::SmoothTransformation));

  ui->textBrowser->setMarkdown(about::IntroductionText());

  ui->textBrowser->setOpenExternalLinks(true);
}

AboutDialog::~AboutDialog() { delete ui; }
