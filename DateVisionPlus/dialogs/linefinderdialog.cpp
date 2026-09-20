#include "linefinderdialog.h"
#include "roigraphics.h"
#include "ui_linefinderdialog.h"

#include "../plugins/linefinderplugin.h"

#include <QComboBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QTableWidgetItem>

LineFinderDialog::LineFinderDialog(OVP::LineFinderPlugin *plugin, const cv::Mat &inputImage,
                                   QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LineFinderDialog)
    , m_plugin(plugin)
    , m_sourceImage(inputImage.clone())
{
    ui->setupUi(this);

    m_scene = new RoiImageScene(this);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    connect(ui->btnDetect, &QPushButton::clicked, this, &LineFinderDialog::onDetectClicked);
    connect(ui->btnLearn, &QPushButton::clicked, this, &LineFinderDialog::onLearnClicked);
    connect(ui->btnFit, &QPushButton::clicked, this, &LineFinderDialog::onFitClicked);
    connect(ui->btnClearRoi, &QPushButton::clicked, this, &LineFinderDialog::onClearRoiClicked);
    connect(ui->btnDeleteRoi, &QPushButton::clicked, this, &LineFinderDialog::onDeleteRoiClicked);
    connect(ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_scene, &RoiImageScene::roiChanged, this, &LineFinderDialog::onRoiChanged);
    connect(ui->listRoi, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        onRoiListClicked(ui->listRoi->row(item));
    });

    ui->tblLines->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadParams();

    if (!m_sourceImage.empty()) {
        m_scene->setImage(m_sourceImage);
        for (const QRect &roi : m_plugin->rois())
            m_scene->addRoi(roi);
    }
    updateRoiList();
}

LineFinderDialog::~LineFinderDialog()
{
    delete ui;
}

void LineFinderDialog::loadParams()
{
    if (!m_plugin)
        return;

    ui->sliderEdgeT1->setValue(m_plugin->paramInt(QStringLiteral("edge_threshold1"), 30));
    ui->sliderEdgeT2->setValue(m_plugin->paramInt(QStringLiteral("edge_threshold2"), 90));
    ui->sliderHough->setValue(m_plugin->paramInt(QStringLiteral("hough_threshold"), 30));
    ui->sliderMinLen->setValue(m_plugin->paramInt(QStringLiteral("min_line_length"), 30));
    ui->sliderMaxGap->setValue(m_plugin->paramInt(QStringLiteral("max_line_gap"), 15));
    ui->spinBlur->setValue(m_plugin->paramInt(QStringLiteral("blur_ksize"), 3));
    ui->spinThickness->setValue(m_plugin->paramInt(QStringLiteral("line_thickness"), 2));
    ui->chkLearnMode->setChecked(m_plugin->paramBool(QStringLiteral("learn_mode"), false));

    const auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0)
            combo->setCurrentIndex(index);
    };
    setCombo(ui->comboSearchDir, m_plugin->paramString(QStringLiteral("search_direction"), QStringLiteral("垂直")));
    setCombo(ui->comboPolarity, m_plugin->paramString(QStringLiteral("edge_polarity"), QStringLiteral("明到暗")));
    setCombo(ui->comboDrawColor, m_plugin->paramString(QStringLiteral("draw_color"), QStringLiteral("绿色")));
}

void LineFinderDialog::saveParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("edge_threshold1"), ui->sliderEdgeT1->value());
    m_plugin->setParam(QStringLiteral("edge_threshold2"), ui->sliderEdgeT2->value());
    m_plugin->setParam(QStringLiteral("hough_threshold"), ui->sliderHough->value());
    m_plugin->setParam(QStringLiteral("min_line_length"), ui->sliderMinLen->value());
    m_plugin->setParam(QStringLiteral("max_line_gap"), ui->sliderMaxGap->value());
    m_plugin->setParam(QStringLiteral("blur_ksize"), ui->spinBlur->value());
    m_plugin->setParam(QStringLiteral("line_thickness"), ui->spinThickness->value());
    m_plugin->setParam(QStringLiteral("search_direction"), ui->comboSearchDir->currentText());
    m_plugin->setParam(QStringLiteral("edge_polarity"), ui->comboPolarity->currentText());
    m_plugin->setParam(QStringLiteral("learn_mode"), ui->chkLearnMode->isChecked());
    m_plugin->setParam(QStringLiteral("draw_color"), ui->comboDrawColor->currentText());
}

void LineFinderDialog::onDetectClicked()
{
    if (!m_plugin || m_sourceImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("无图像"),
                             QStringLiteral("请先连接图像源节点，然后双击线查找节点打开此对话框"));
        return;
    }

    saveParams();
    m_plugin->setRois(m_scene->rois());
    m_plugin->setInput(QStringLiteral("input"), OVP::imageValue(m_sourceImage));

    if (m_plugin->execute()) {
        m_detectedLines = m_plugin->detectedLines();
        const cv::Mat output = OVP::toMat(m_plugin->output(QStringLiteral("output")));
        if (!output.empty())
            m_scene->updateBackground(output);

        updateLineTable();
        ui->lblLineCount->setText(QStringLiteral("线段: %1").arg(m_detectedLines.size()));
        if (m_detectedLines.isEmpty()) {
            ui->lblLineCount->setStyleSheet(QStringLiteral("color: #ff9800; font-size: 12px;"));
            ui->lblLineCount->setToolTip(QStringLiteral("未检测到线段，请降低边缘阈值或霍夫阈值"));
        } else {
            ui->lblLineCount->setStyleSheet(QStringLiteral("color: #4caf50; font-size: 12px;"));
            ui->lblLineCount->setToolTip(QString());
        }
    } else {
        const QString err = m_plugin->lastError();
        QMessageBox::warning(this, QStringLiteral("检测失败"),
                             err.isEmpty() ? QStringLiteral("线查找检测失败，请检查参数")
                                           : QStringLiteral("线查找检测失败：\n\n%1").arg(err));
    }
}

void LineFinderDialog::onLearnClicked()
{
    if (!m_plugin || m_sourceImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("无图像"), QStringLiteral("请先连接图像源节点"));
        return;
    }

    const QList<QRect> rois = m_scene->rois();
    if (rois.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无ROI"), QStringLiteral("请先在图像上绘制ROI框"));
        return;
    }

    saveParams();

    bool ok = false;
    const QVector<double> result = m_plugin->autoLearn(m_sourceImage, rois.first(), &ok);
    if (!ok || result.size() < 6) {
        QMessageBox::warning(this, QStringLiteral("学习失败"),
                             QStringLiteral("无法在ROI内找到有效边缘，请调整ROI位置或检查图像"));
        return;
    }

    const int edgeT1 = static_cast<int>(result[4]);
    const int edgeT2 = static_cast<int>(result[5]);
    ui->sliderEdgeT1->setValue(edgeT1);
    ui->sliderEdgeT2->setValue(edgeT2);

    m_plugin->setRois(rois);
    m_plugin->setInput(QStringLiteral("input"), OVP::imageValue(m_sourceImage));
    m_plugin->execute();

    const cv::Mat output = OVP::toMat(m_plugin->output(QStringLiteral("output")));
    if (!output.empty())
        m_scene->updateBackground(output);

    m_detectedLines = m_plugin->detectedLines();
    updateLineTable();
    ui->lblLineCount->setText(QStringLiteral("线段: %1 (已学习)").arg(m_detectedLines.size()));
    ui->lblLineCount->setStyleSheet(QStringLiteral("color: #ce93d8; font-size: 12px;"));
    ui->lblLineCount->setToolTip(QStringLiteral("学习完成\n阈值: [%1, %2]\n线: (%3,%4) → (%5,%6)")
                                     .arg(edgeT1)
                                     .arg(edgeT2)
                                     .arg(result[0], 0, 'f', 0)
                                     .arg(result[1], 0, 'f', 0)
                                     .arg(result[2], 0, 'f', 0)
                                     .arg(result[3], 0, 'f', 0));
}

void LineFinderDialog::onFitClicked()
{
    ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void LineFinderDialog::onClearRoiClicked()
{
    m_scene->clearRois();
    if (!m_sourceImage.empty())
        m_scene->setImage(m_sourceImage);
    ui->tblLines->setRowCount(0);
    m_detectedLines.clear();
    ui->lblLineCount->setText(QStringLiteral("线段: 0"));
}

void LineFinderDialog::onDeleteRoiClicked()
{
    m_scene->removeSelectedRoi();
}

void LineFinderDialog::onRoiChanged()
{
    updateRoiList();
}

void LineFinderDialog::updateRoiList()
{
    ui->listRoi->clear();
    const QList<QRect> rois = m_scene->rois();
    for (int i = 0; i < rois.size(); ++i) {
        const QRect &r = rois.at(i);
        ui->listRoi->addItem(QStringLiteral("ROI %1: (%2,%3) %4x%5")
                                 .arg(i + 1)
                                 .arg(r.x())
                                 .arg(r.y())
                                 .arg(r.width())
                                 .arg(r.height()));
    }
    ui->lblRoiCount->setText(QStringLiteral("ROI: %1").arg(rois.size()));
}

void LineFinderDialog::onRoiListClicked(int row)
{
    RoiRectItem *item = m_scene->roiItemAt(row);
    if (!item)
        return;
    m_scene->clearSelection();
    item->setSelected(true);
    ui->graphicsView->centerOn(item);
}

void LineFinderDialog::updateLineTable()
{
    ui->tblLines->setRowCount(0);
    for (int i = 0; i < m_detectedLines.size(); ++i) {
        const QVector<double> &line = m_detectedLines.at(i);
        ui->tblLines->insertRow(i);
        ui->tblLines->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        for (int c = 0; c < 4 && c < line.size(); ++c)
            ui->tblLines->setItem(i, c + 1, new QTableWidgetItem(QString::number(line.at(c), 'f', 1)));
    }
}

void LineFinderDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        m_scene->removeSelectedRoi();
        return;
    }
    QDialog::keyPressEvent(event);
}
