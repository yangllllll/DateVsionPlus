#include "yolodetectdialog.h"

#include "cvutils.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QVBoxLayout>

#include <exception>

namespace {

const char *const kGroupStyle =
    "QGroupBox { color: #aaa; border: 1px solid #3e3e42; margin-top: 8px; } "
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }";
const char *const kInputStyle =
    "background: #333; color: #ccc; border: 1px solid #555; padding: 2px;";
const char *const kButtonStyle =
    "QPushButton { background: #3e3e42; color: #ddd; border: 1px solid #555; padding: 4px 10px; } "
    "QPushButton:hover { background: #505050; }";

} // namespace

YoloDetectDialog::YoloDetectDialog(OVP::YoloDetectPlugin *plugin, const cv::Mat &image,
                                   QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
    , m_inputImage(image)
    , m_previewImage(image)
{
    setWindowTitle(QStringLiteral("YOLO目标识别 - 设置"));
    resize(1000, 680);
    setMinimumSize(760, 500);

    setupUi();
    loadParams();
    updatePreview();
}

// ------------------------------------------------------------------- 界面

void YoloDetectDialog::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);

    // ---- 左侧预览 ----
    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(480, 420);
    m_previewLabel->setStyleSheet(
        QStringLiteral("background: #1e1e1e; border: 1px solid #3e3e42; color: #666;"));
    mainLayout->addWidget(m_previewLabel, 1);

    // ---- 右侧参数 ----
    auto *right = new QWidget(this);
    right->setFixedWidth(320);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto *modelGroup = new QGroupBox(QStringLiteral("模型"), right);
    modelGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *modelForm = new QFormLayout(modelGroup);
    modelForm->setSpacing(6);

    auto *modelRow = new QHBoxLayout();
    m_modelEdit = new QLineEdit(modelGroup);
    m_modelEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_modelEdit->setPlaceholderText(QStringLiteral("yolo11n.onnx 或模型绝对路径"));
    auto *browseButton = new QPushButton(QStringLiteral("..."), modelGroup);
    browseButton->setMaximumWidth(32);
    browseButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(browseButton, &QPushButton::clicked, this, &YoloDetectDialog::onBrowseModel);
    modelRow->addWidget(m_modelEdit);
    modelRow->addWidget(browseButton);
    modelForm->addRow(QStringLiteral("模型文件"), modelRow);

    m_backendCombo = new QComboBox(modelGroup);
    m_backendCombo->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_backendCombo->addItems({QStringLiteral("自动"), QStringLiteral("OpenCV DNN"),
                              QStringLiteral("ONNX Runtime")});
    m_backendCombo->setToolTip(
        QStringLiteral("自动：可用 ONNX Runtime 时优先使用（支持 GPU），否则 OpenCV DNN"));
    modelForm->addRow(QStringLiteral("推理后端"), m_backendCombo);

    auto *ortRow = new QHBoxLayout();
    m_ortPathEdit = new QLineEdit(modelGroup);
    m_ortPathEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_ortPathEdit->setPlaceholderText(QStringLiteral("可选：onnxruntime.dll 或其目录"));
    auto *ortButton = new QPushButton(QStringLiteral("..."), modelGroup);
    ortButton->setMaximumWidth(32);
    ortButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(ortButton, &QPushButton::clicked, this, &YoloDetectDialog::onBrowseOrt);
    ortRow->addWidget(m_ortPathEdit);
    ortRow->addWidget(ortButton);
    modelForm->addRow(QStringLiteral("ONNX Runtime"), ortRow);

    m_deviceCombo = new QComboBox(modelGroup);
    m_deviceCombo->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_deviceCombo->addItems({QStringLiteral("自动"), QStringLiteral("CPU"),
                             QStringLiteral("CUDA:0"), QStringLiteral("CUDA:1"),
                             QStringLiteral("CUDA:2"), QStringLiteral("CUDA:3")});
    m_deviceCombo->setToolTip(QStringLiteral("自动：检测到可用 GPU 时使用 CUDA，否则使用 CPU"));
    modelForm->addRow(QStringLiteral("推理设备"), m_deviceCombo);

    auto *namesRow = new QHBoxLayout();
    m_namesEdit = new QLineEdit(modelGroup);
    m_namesEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_namesEdit->setPlaceholderText(QStringLiteral("可选：类别名称文件"));
    auto *namesButton = new QPushButton(QStringLiteral("..."), modelGroup);
    namesButton->setMaximumWidth(32);
    namesButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(namesButton, &QPushButton::clicked, this, &YoloDetectDialog::onBrowseNames);
    namesRow->addWidget(m_namesEdit);
    namesRow->addWidget(namesButton);
    modelForm->addRow(QStringLiteral("类别名称"), namesRow);

    rightLayout->addWidget(modelGroup);

    auto *paramGroup = new QGroupBox(QStringLiteral("参数"), right);
    paramGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *paramForm = new QFormLayout(paramGroup);
    paramForm->setSpacing(6);

    m_imgszSpin = new QSpinBox(paramGroup);
    m_imgszSpin->setRange(32, 2048);
    m_imgszSpin->setSingleStep(32);
    m_imgszSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("推理尺寸"), m_imgszSpin);

    m_confSpin = new QDoubleSpinBox(paramGroup);
    m_confSpin->setRange(0.0, 1.0);
    m_confSpin->setSingleStep(0.01);
    m_confSpin->setDecimals(2);
    m_confSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("置信度"), m_confSpin);

    m_iouSpin = new QDoubleSpinBox(paramGroup);
    m_iouSpin->setRange(0.0, 1.0);
    m_iouSpin->setSingleStep(0.01);
    m_iouSpin->setDecimals(2);
    m_iouSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("IoU阈值"), m_iouSpin);

    m_maxDetSpin = new QSpinBox(paramGroup);
    m_maxDetSpin->setRange(1, 1000);
    m_maxDetSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("最大目标数"), m_maxDetSpin);

    m_classFilterEdit = new QLineEdit(paramGroup);
    m_classFilterEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_classFilterEdit->setPlaceholderText(QStringLiteral("如 0,2 或 person,car（留空=全部）"));
    paramForm->addRow(QStringLiteral("类别过滤"), m_classFilterEdit);

    m_formatCombo = new QComboBox(paramGroup);
    m_formatCombo->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_formatCombo->addItems({QStringLiteral("自动"), QStringLiteral("YOLOv8/v11"),
                             QStringLiteral("YOLOv5"), QStringLiteral("已NMS")});
    m_formatCombo->setToolTip(QStringLiteral("模型输出张量格式；识别异常时手动指定"));
    paramForm->addRow(QStringLiteral("输出格式"), m_formatCombo);

    m_thicknessSpin = new QSpinBox(paramGroup);
    m_thicknessSpin->setRange(1, 5);
    m_thicknessSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("线宽"), m_thicknessSpin);

    m_halfCheck = new QCheckBox(QStringLiteral("半精度推理(FP16)"), paramGroup);
    m_halfCheck->setToolTip(QStringLiteral("仅 CUDA 有效，可提升速度"));
    paramForm->addRow(QString(), m_halfCheck);

    m_emptyNgCheck = new QCheckBox(QStringLiteral("空结果判NG"), paramGroup);
    paramForm->addRow(QString(), m_emptyNgCheck);

    m_drawCheck = new QCheckBox(QStringLiteral("绘制检测框"), paramGroup);
    paramForm->addRow(QString(), m_drawCheck);

    m_labelCheck = new QCheckBox(QStringLiteral("显示类别标签"), paramGroup);
    paramForm->addRow(QString(), m_labelCheck);

    rightLayout->addWidget(paramGroup);
    rightLayout->addStretch();

    m_statusLabel = new QLabel(QStringLiteral("参数调整后点击“测试识别”预览结果"), right);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    rightLayout->addWidget(m_statusLabel);

    auto *buttonRow = new QHBoxLayout();
    auto *testButton = new QPushButton(QStringLiteral("测试识别"), right);
    testButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(testButton, &QPushButton::clicked, this, &YoloDetectDialog::onTest);

    auto *saveButton = new QPushButton(QStringLiteral("保存并关闭"), right);
    saveButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0d7377; color: #fff; border: 1px solid #0a5c5f; "
        "padding: 4px 10px; } QPushButton:hover { background: #0a5c5f; }"));
    connect(saveButton, &QPushButton::clicked, this, &YoloDetectDialog::onSave);

    buttonRow->addWidget(testButton);
    buttonRow->addWidget(saveButton);
    rightLayout->addLayout(buttonRow);

    mainLayout->addWidget(right);
}

// ------------------------------------------------------------- 参数读写

void YoloDetectDialog::loadParams()
{
    if (!m_plugin)
        return;

    m_modelEdit->setText(m_plugin->paramString(QStringLiteral("model_path")));

    const QString device = m_plugin->paramString(QStringLiteral("device"), QStringLiteral("自动"));
    const int deviceIndex = m_deviceCombo->findText(device);
    if (deviceIndex >= 0) {
        m_deviceCombo->setCurrentIndex(deviceIndex);
    } else {
        m_deviceCombo->addItem(device);
        m_deviceCombo->setCurrentText(device);
    }

    const QString backend = m_plugin->paramString(QStringLiteral("backend"),
                                                  QStringLiteral("自动"));
    const int backendIndex = m_backendCombo->findText(backend);
    if (backendIndex >= 0)
        m_backendCombo->setCurrentIndex(backendIndex);

    m_ortPathEdit->setText(m_plugin->paramString(QStringLiteral("ort_path")));
    m_namesEdit->setText(m_plugin->paramString(QStringLiteral("class_names")));
    m_imgszSpin->setValue(m_plugin->paramInt(QStringLiteral("imgsz"), 640));
    m_confSpin->setValue(m_plugin->paramDouble(QStringLiteral("conf"), 0.25));
    m_iouSpin->setValue(m_plugin->paramDouble(QStringLiteral("iou"), 0.45));
    m_maxDetSpin->setValue(m_plugin->paramInt(QStringLiteral("max_det"), 300));
    m_classFilterEdit->setText(m_plugin->paramString(QStringLiteral("class_filter")));

    const QString format = m_plugin->paramString(QStringLiteral("output_format"),
                                                 QStringLiteral("自动"));
    const int formatIndex = m_formatCombo->findText(format);
    if (formatIndex >= 0)
        m_formatCombo->setCurrentIndex(formatIndex);

    m_thicknessSpin->setValue(m_plugin->paramInt(QStringLiteral("line_thickness"), 2));
    m_halfCheck->setChecked(m_plugin->paramBool(QStringLiteral("half"), false));
    m_emptyNgCheck->setChecked(m_plugin->paramBool(QStringLiteral("empty_as_ng"), true));
    m_drawCheck->setChecked(m_plugin->paramBool(QStringLiteral("draw"), true));
    m_labelCheck->setChecked(m_plugin->paramBool(QStringLiteral("show_label"), true));
}

void YoloDetectDialog::applyParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("model_path"), m_modelEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("device"), m_deviceCombo->currentText());
    m_plugin->setParam(QStringLiteral("backend"), m_backendCombo->currentText());
    m_plugin->setParam(QStringLiteral("ort_path"), m_ortPathEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("class_names"), m_namesEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("imgsz"), m_imgszSpin->value());
    m_plugin->setParam(QStringLiteral("conf"), m_confSpin->value());
    m_plugin->setParam(QStringLiteral("iou"), m_iouSpin->value());
    m_plugin->setParam(QStringLiteral("max_det"), m_maxDetSpin->value());
    m_plugin->setParam(QStringLiteral("class_filter"), m_classFilterEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("output_format"), m_formatCombo->currentText());
    m_plugin->setParam(QStringLiteral("line_thickness"), m_thicknessSpin->value());
    m_plugin->setParam(QStringLiteral("half"), m_halfCheck->isChecked());
    m_plugin->setParam(QStringLiteral("empty_as_ng"), m_emptyNgCheck->isChecked());
    m_plugin->setParam(QStringLiteral("draw"), m_drawCheck->isChecked());
    m_plugin->setParam(QStringLiteral("show_label"), m_labelCheck->isChecked());
}

// ------------------------------------------------------------------- 交互

void YoloDetectDialog::onBrowseModel()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 YOLO 模型"), m_modelEdit->text(),
        QStringLiteral("YOLO模型 (*.onnx);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_modelEdit->setText(path);
}

void YoloDetectDialog::onBrowseNames()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择类别名称文件"), m_namesEdit->text(),
        QStringLiteral("类别文件 (*.txt *.names);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_namesEdit->setText(path);
}

void YoloDetectDialog::onBrowseOrt()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 onnxruntime.dll"), m_ortPathEdit->text(),
        QStringLiteral("ONNX Runtime (onnxruntime.dll);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_ortPathEdit->setText(path);
}

void YoloDetectDialog::onTest()
{
    applyParams();

    if (!m_plugin) {
        setStatus(QStringLiteral("插件实例无效"), true);
        return;
    }
    if (m_inputImage.empty()) {
        setStatus(QStringLiteral("没有输入图像，请先连接上游图像节点后再测试"), true);
        return;
    }

    setStatus(QStringLiteral("正在识别，请稍候..."), false);
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QCoreApplication::processEvents();

    bool success = false;
    QString failure;
    QElapsedTimer timer;
    timer.start();
    try {
        m_plugin->setInput(QStringLiteral("input"), OVP::imageValue(m_inputImage));
        success = m_plugin->execute();
    } catch (const cv::Exception &e) {
        success = false;
        failure = QStringLiteral("OpenCV 异常：%1").arg(QString::fromUtf8(e.what()));
    } catch (const std::exception &e) {
        success = false;
        failure = QStringLiteral("异常：%1").arg(QString::fromUtf8(e.what()));
    }
    const qint64 elapsed = timer.elapsed();
    QApplication::restoreOverrideCursor();

    const cv::Mat output = OVP::toMat(m_plugin->output(QStringLiteral("output")));
    if (!output.empty()) {
        m_previewImage = output;
        updatePreview();
    }

    if (success) {
        const int count = m_plugin->output(QStringLiteral("det_count")).toInt();
        const QString device = m_plugin->output(QStringLiteral("device_used")).toString();
        setStatus(QStringLiteral("识别成功：%1 个目标 | 设备：%2 | 耗时 %3 ms")
                      .arg(count)
                      .arg(device.isEmpty() ? QStringLiteral("cpu") : device)
                      .arg(elapsed),
                  false);
    } else {
        const QString error = failure.isEmpty() ? m_plugin->lastError() : failure;
        setStatus(error.isEmpty() ? QStringLiteral("识别失败") : error, true);
    }
}

void YoloDetectDialog::onSave()
{
    applyParams();
    accept();
}

void YoloDetectDialog::setStatus(const QString &text, bool isError)
{
    const QString firstLine = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    m_statusLabel->setText(firstLine.isEmpty() ? text : firstLine);
    m_statusLabel->setToolTip(text);
    m_statusLabel->setStyleSheet(
        isError ? QStringLiteral("color: #ff6b6b; font-size: 11px;")
                : QStringLiteral("color: #7fd18b; font-size: 11px;"));
}

// ------------------------------------------------------------------- 预览

void YoloDetectDialog::updatePreview()
{
    if (m_previewImage.empty()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("(无图像)"));
        return;
    }

    const QPixmap pixmap = OVP::matToQPixmap(m_previewImage);
    if (pixmap.isNull()) {
        m_previewLabel->setText(QStringLiteral("(图像格式不支持预览)"));
        return;
    }

    m_previewPixmap = pixmap;
    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
}

void YoloDetectDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!m_previewPixmap.isNull()) {
        m_previewLabel->setPixmap(m_previewPixmap.scaled(m_previewLabel->size(),
                                                         Qt::KeepAspectRatio,
                                                         Qt::SmoothTransformation));
    }
}
