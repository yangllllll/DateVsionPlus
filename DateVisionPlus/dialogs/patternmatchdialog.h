#ifndef PATTERNMATCHDIALOG_H
#define PATTERNMATCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointF>

#include <opencv2/core.hpp>

namespace OVP {
class PatternMatchPlugin;
}

namespace Ui {
class PatternMatchDialog;
}

class RoiImageScene;

/** 模板匹配对话框：ROI 框选训练模板 + 检测 */
class PatternMatchDialog : public QDialog
{
    Q_OBJECT

public:
    PatternMatchDialog(OVP::PatternMatchPlugin *plugin, const cv::Mat &inputImage,
                       QWidget *parent = nullptr);
    ~PatternMatchDialog() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onTrainClicked();
    void onDetectClicked();
    void onFitClicked();
    void onClearClicked();

private:
    void loadParams();
    void saveParams();
    void updateResultTable();

    Ui::PatternMatchDialog *ui = nullptr;
    OVP::PatternMatchPlugin *m_plugin = nullptr;
    RoiImageScene *m_scene = nullptr;
    cv::Mat m_sourceImage;
    QList<QPointF> m_matchPositions;
};

#endif // PATTERNMATCHDIALOG_H
