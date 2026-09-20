#ifndef PREVIEWPANEL_H
#define PREVIEWPANEL_H

#include <QWidget>

#include <opencv2/core.hpp>

namespace Ui {
class PreviewPanel;
}

/** 图像预览面板 */
class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget *parent = nullptr);
    ~PreviewPanel() override;

public slots:
    void setImage(const cv::Mat &image);
    void clearImage();

private:
    Ui::PreviewPanel *ui = nullptr;
};

#endif // PREVIEWPANEL_H
