#include "patternmatchdialog.h"
#include "roigraphics.h"
#include "ui_patternmatchdialog.h"

#include "../core/cvutils.h"
#include "../core/plugintypes.h"
#include "../plugins/patternmatchplugin.h"

#include <QComboBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QTableWidgetItem>

PatternMatchDialog::PatternMatchDialog(OVP::PatternMatchPlugin *plugin, const cv::Mat &inputImage,
                                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PatternMatchDialog)
    , m_plugin(plugin)
    , m_sourceImage(inputImage.clone())
{
    ui->setupUi(this);

    m_scene = new RoiImageScene(this);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    connect(ui->btnTrain, &QPushButton::clicked, this, &PatternMatchDialog::onTrainClicked);
    connect(ui->btnDetect, &QPushButton::clicked, this, &PatternMatchDialog::onDetectClicked);
    connect(ui->btnFit, &QPushButton::clicked, this, &PatternMatchDialog::onFitClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &PatternMatchDialog::onClearClicked);
    connect(ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    ui->tblResults->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadParams();

    if (!m_sourceImage.empty()) {
        m_scene->setImage(m_sourceImage);
        const QRect roi = m_plugin->templateRoi();
        if (roi.isValid())
            m_scene->addRoi(roi);

        const cv::Mat tmpl = m_plugin->templateImage();
        if (!tmpl.empty()) {
            ui->lblTemplatePreview->setPixmap(OVP::matToQPixmap(tmpl).scaled(
                ui->lblTemplatePreview->width(), 120, Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
            ui->lblTemplateSize->setText(QStringLiteral("模板尺寸: %1 x %2")
                                             .arg(tmpl.cols)
                                             .arg(tmpl.rows));
        }
    }
}

PatternMatchDialog::~PatternMatchDialog()
{
    delete ui;
}

void PatternMatchDialog::loadParams()
{
    if (!m_plugin)
        return;

    const auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0)
            combo->setCurrentIndex(index);
    };

    setCombo(ui->comboMethod,
             m_plugin->paramString(QStringLiteral("method"), QStringLiteral("CCOEFF_NORMED")));
    setCombo(ui->comboDrawColor, m_plugin->paramString(QStringLiteral("draw_color"), QStringLiteral("绿色")));
    ui->sliderThreshold->setValue(
        static_cast<int>(m_plugin->paramDouble(QStringLiteral("threshold"), 0.8) * 100.0));
    ui->sliderMaxMatches->setValue(m_plugin->paramInt(QStringLiteral("max_matches"), 10));
    ui->sliderThickness->setValue(m_plugin->paramInt(QStringLiteral("line_thickness"), 2));
}

void PatternMatchDialog::saveParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("method"), ui->comboMethod->currentText());
    m_plugin->setParam(QStringLiteral("threshold"), ui->sliderThreshold->value() / 100.0);
    m_plugin->setParam(QStringLiteral("max_matches"), ui->sliderMaxMatches->value());
    m_plugin->setParam(QStringLiteral("draw_color"), ui->comboDrawColor->currentText());
    m_plugin->setParam(QStringLiteral("line_thickness"), ui->sliderThickness->value());
}

void PatternMatchDialog::onTrainClicked()
{
    if (!m_plugin || m_sourceImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("无图像"), QStringLiteral("请先连接图像源节点"));
        return;
    }

    const QList<QRect> rois = m_scene->rois();
    if (rois.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无ROI"), QStringLiteral("请先在图像上框选模板区域"));
        return;
    }

    const cv::Mat tmpl = m_plugin->trainTemplate(m_sourceImage, rois.first());
    if (tmpl.empty()) {
        QMessageBox::warning(this, QStringLiteral("训练失败"), QStringLiteral("无法提取模板，请确保ROI大小合适"));
        return;
    }

    const QPixmap preview = OVP::matToQPixmap(tmpl).scaled(250, 120, Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation);
    ui->lblTemplatePreview->setPixmap(preview);
    ui->lblTemplateSize->setText(QStringLiteral("模板尺寸: %1 x %2").arg(tmpl.cols).arg(tmpl.rows));
    ui->lblStatus->setText(QStringLiteral("模板已训练 (%1x%2)").arg(tmpl.cols).arg(tmpl.rows));
    ui->lblStatus->setStyleSheet(QStringLiteral("color: #ce93d8; font-size: 12px;"));
}

void PatternMatchDialog::onDetectClicked()
{
    if (!m_plugin || m_sourceImage.empty())
        return;

    saveParams();

    if (m_plugin->templateImage().empty()) {
        QMessageBox::warning(this, QStringLiteral("无模板"), QStringLiteral("请先点击\"训练\"按钮提取模板"));
        return;
    }

    m_plugin->setInput(QStringLiteral("input"), OVP::imageValue(m_sourceImage));
    if (m_plugin->execute()) {
        const cv::Mat output = OVP::toMat(m_plugin->output(QStringLiteral("output")));
        if (!output.empty()) {
            m_scene->updateBackground(output);
            for (const QRect &roi : m_scene->rois())
                m_scene->addRoi(roi);
        }

        m_matchPositions = OVP::unpackPoints(m_plugin->output(QStringLiteral("match_positions")));
        updateResultTable();
        ui->lblStatus->setText(QStringLiteral("匹配到 %1 个位置").arg(m_matchPositions.size()));
        ui->lblStatus->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px;")
                .arg(m_matchPositions.isEmpty() ? QStringLiteral("#ff9800") : QStringLiteral("#4caf50")));
    } else {
        QMessageBox::warning(this, QStringLiteral("检测失败"), m_plugin->lastError());
    }
}

void PatternMatchDialog::onFitClicked()
{
    ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void PatternMatchDialog::onClearClicked()
{
    m_scene->clearRois();
    if (!m_sourceImage.empty())
        m_scene->setImage(m_sourceImage);

    ui->lblTemplatePreview->setPixmap(QPixmap());
    ui->lblTemplatePreview->setText(QStringLiteral("(未训练)"));
    ui->lblTemplateSize->clear();
    ui->tblResults->setRowCount(0);
    m_matchPositions.clear();
    ui->lblStatus->setText(QStringLiteral("请绘制ROI框"));
    ui->lblStatus->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
}

void PatternMatchDialog::updateResultTable()
{
    ui->tblResults->setRowCount(0);
    for (int i = 0; i < m_matchPositions.size(); ++i) {
        const QPointF &p = m_matchPositions.at(i);
        ui->tblResults->insertRow(i);
        ui->tblResults->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        ui->tblResults->setItem(i, 1, new QTableWidgetItem(QString::number(p.x(), 'f', 0)));
        ui->tblResults->setItem(i, 2, new QTableWidgetItem(QString::number(p.y(), 'f', 0)));
    }
}

void PatternMatchDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        m_scene->removeSelectedRoi();
        return;
    }
    QDialog::keyPressEvent(event);
}
