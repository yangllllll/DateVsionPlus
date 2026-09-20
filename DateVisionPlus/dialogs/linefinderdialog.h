#ifndef LINEFINDERDIALOG_H
#define LINEFINDERDIALOG_H

#include <QDialog>
#include <QList>
#include <QVector>
#include <QVariantMap>

#include <opencv2/core.hpp>

namespace OVP {
class LineFinderPlugin;
}

namespace Ui {
class LineFinderDialog;
}

class RoiImageScene;

/** 线查找工具对话框：ROI 绘制 + 自动学习 + 手动检测 */
class LineFinderDialog : public QDialog
{
    Q_OBJECT

public:
    LineFinderDialog(OVP::LineFinderPlugin *plugin, const cv::Mat &inputImage,
                     QWidget *parent = nullptr);
    ~LineFinderDialog() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onDetectClicked();
    void onLearnClicked();
    void onFitClicked();
    void onClearRoiClicked();
    void onDeleteRoiClicked();
    void onRoiChanged();
    void onRoiListClicked(int row);

private:
    void loadParams();
    void saveParams();
    void updateRoiList();
    void updateLineTable();

    Ui::LineFinderDialog *ui = nullptr;
    OVP::LineFinderPlugin *m_plugin = nullptr;
    RoiImageScene *m_scene = nullptr;
    cv::Mat m_sourceImage;
    QList<QVector<double>> m_detectedLines;
};

#endif // LINEFINDERDIALOG_H
