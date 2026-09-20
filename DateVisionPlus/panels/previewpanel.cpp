#include "previewpanel.h"
#include "imageviewer.h"
#include "ui_previewpanel.h"

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PreviewPanel)
{
    ui->setupUi(this);

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
        ui->lblInfo->clear();
        return;
    }
    ui->lblInfo->setText(QStringLiteral("%1 x %2  通道: %3")
                             .arg(image.cols)
                             .arg(image.rows)
                             .arg(image.channels()));
}

void PreviewPanel::clearImage()
{
    ui->viewer->clearImage();
    ui->lblInfo->clear();
}
