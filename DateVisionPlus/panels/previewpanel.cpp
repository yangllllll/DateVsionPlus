#include "previewpanel.h"
#include "imageviewer.h"
#include "ui_previewpanel.h"

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PreviewPanel)
{
    ui->setupUi(this);
    ui->header->setProperty("uiRole", "panelToolbar");
    ui->lblTitle->hide();
    ui->lblInfo->setProperty("uiRole", "numeric");
    ui->lblInfo->setText(QStringLiteral("等待图像"));
    ui->lblInfo->setMinimumWidth(0);
    ui->lblInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->headerSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->headerLayout->setStretch(ui->headerLayout->indexOf(ui->lblInfo), 1);
    ui->btnFit->setProperty("uiRole", "compact");
    ui->btnActualSize->setProperty("uiRole", "compact");
    ui->btnFit->setText(QStringLiteral("适应"));
    ui->btnFit->setToolTip(QStringLiteral("适应窗口显示"));
    ui->btnActualSize->setToolTip(QStringLiteral("按原始像素显示"));
    ui->headerLayout->setContentsMargins(10, 7, 10, 7);

    connect(ui->btnFit, &QPushButton::clicked, ui->viewer, &ImageViewer::fitToWindow);
    connect(ui->btnActualSize, &QPushButton::clicked, ui->viewer, &ImageViewer::zoomActualSize);
}

PreviewPanel::~PreviewPanel()
{
    delete ui;
}

void PreviewPanel::setImage(const cv::Mat &image)
{
    ui->viewer->setImage(image);
    if (image.empty()) {
        ui->lblInfo->setText(QStringLiteral("等待图像"));
        ui->lblInfo->setToolTip(QString());
        return;
    }
    ui->lblInfo->setText(QStringLiteral("%1 × %2  /  %3 CH")
                             .arg(image.cols)
                             .arg(image.rows)
                             .arg(image.channels()));
    ui->lblInfo->setToolTip(ui->lblInfo->text());
}

void PreviewPanel::clearImage()
{
    ui->viewer->clearImage();
    ui->lblInfo->setText(QStringLiteral("等待图像"));
    ui->lblInfo->setToolTip(QString());
}
