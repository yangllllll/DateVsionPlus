#include "randompickdialog.h"

#include "calibdata.h"
#include "cvutils.h"

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

RandomPickDialog::RandomPickDialog(OVP::RandomPickPlugin *plugin, const cv::Mat &image,
                                   QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
    , m_image(image)
{
    setWindowTitle(QStringLiteral("标定转换 - 设置"));
    resize(1000, 680);
    setMinimumSize(760, 500);

    setupUi();
    loadParams();
    showImage(m_image);
}

void RandomPickDialog::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);

    // ---- 左侧参数 ----
    auto *left = new QWidget(this);
    left->setFixedWidth(360);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 6, 0);
    leftLayout->setSpacing(6);

    auto *calibGroup = new QGroupBox(QStringLiteral("标定文件"), left);
    calibGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *calibLayout = new QVBoxLayout(calibGroup);
    calibLayout->setSpacing(6);

    auto *calibRow = new QHBoxLayout();
    m_calibEdit = new QLineEdit(calibGroup);
    m_calibEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_calibEdit->setPlaceholderText(QStringLiteral("N点标定保存的 .ncal 文件"));
    auto *browseButton = new QPushButton(QStringLiteral("..."), calibGroup);
    browseButton->setMaximumWidth(32);
    browseButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(browseButton, &QPushButton::clicked, this, &RandomPickDialog::onBrowseCalib);
    calibRow->addWidget(m_calibEdit);
    calibRow->addWidget(browseButton);
    calibLayout->addLayout(calibRow);

    auto *calibHint = new QLabel(
        QStringLiteral("标定文件由「N点标定」插件保存；文件中包含仿射变换矩阵。"),
        calibGroup);
    calibHint->setWordWrap(true);
    calibHint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    calibLayout->addWidget(calibHint);

    leftLayout->addWidget(calibGroup);

    auto *paramGroup = new QGroupBox(QStringLiteral("抓取参数"), left);
    paramGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *paramForm = new QFormLayout(paramGroup);
    paramForm->setSpacing(6);

    m_indexSpin = new QSpinBox(paramGroup);
    m_indexSpin->setRange(0, 99);
    m_indexSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_indexSpin->setToolTip(QStringLiteral("0 = 第一个（置信度最高）"));
    paramForm->addRow(QStringLiteral("目标序号"), m_indexSpin);

    m_scoreSpin = new QDoubleSpinBox(paramGroup);
    m_scoreSpin->setRange(0.0, 1.0);
    m_scoreSpin->setSingleStep(0.01);
    m_scoreSpin->setDecimals(2);
    m_scoreSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("最小置信度"), m_scoreSpin);

    m_classFilterEdit = new QLineEdit(paramGroup);
    m_classFilterEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_classFilterEdit->setPlaceholderText(QStringLiteral("如 0,2 或 person,car（留空=全部）"));
    paramForm->addRow(QStringLiteral("类别过滤"), m_classFilterEdit);

    m_thicknessSpin = new QSpinBox(paramGroup);
    m_thicknessSpin->setRange(1, 5);
    m_thicknessSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("线宽"), m_thicknessSpin);

    m_drawCheck = new QCheckBox(QStringLiteral("绘制抓取点"), paramGroup);
    paramForm->addRow(QString(), m_drawCheck);

    leftLayout->addWidget(paramGroup);
    leftLayout->addStretch();

    m_statusLabel = new QLabel(QStringLiteral("参数调整后点击「测试转换」"), left);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    leftLayout->addWidget(m_statusLabel);

    auto *buttonRow = new QHBoxLayout();
    auto *testButton = new QPushButton(QStringLiteral("测试转换"), left);
    testButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(testButton, &QPushButton::clicked, this, &RandomPickDialog::onTest);
    auto *okButton = new QPushButton(QStringLiteral("保存并关闭"), left);
    okButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0d7377; color: #fff; border: 1px solid #0a5c5f; "
        "padding: 4px 10px; } QPushButton:hover { background: #0a5c5f; }"));
    connect(okButton, &QPushButton::clicked, this, &RandomPickDialog::onAccept);
    buttonRow->addWidget(testButton);
    buttonRow->addWidget(okButton);
    leftLayout->addLayout(buttonRow);

    mainLayout->addWidget(left);

    // ---- 右侧预览 ----
    m_previewLabel = new QLabel(QStringLiteral("（无图像）"), this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(480, 380);
    m_previewLabel->setStyleSheet(
        QStringLiteral("background: #1e1e1e; border: 1px solid #3e3e42; color: #666;"));
    mainLayout->addWidget(m_previewLabel, 1);
}

void RandomPickDialog::loadParams()
{
    if (!m_plugin)
        return;

    m_calibEdit->setText(m_plugin->paramString(QStringLiteral("calib_file")));
    m_indexSpin->setValue(m_plugin->paramInt(QStringLiteral("pick_index"), 0));
    m_scoreSpin->setValue(m_plugin->paramDouble(QStringLiteral("min_score"), 0.0));
    m_classFilterEdit->setText(m_plugin->paramString(QStringLiteral("class_filter")));
    m_thicknessSpin->setValue(m_plugin->paramInt(QStringLiteral("line_thickness"), 2));
    m_drawCheck->setChecked(m_plugin->paramBool(QStringLiteral("draw"), true));
}

void RandomPickDialog::applyParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("calib_file"), m_calibEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("pick_index"), m_indexSpin->value());
    m_plugin->setParam(QStringLiteral("min_score"), m_scoreSpin->value());
    m_plugin->setParam(QStringLiteral("class_filter"), m_classFilterEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("line_thickness"), m_thicknessSpin->value());
    m_plugin->setParam(QStringLiteral("draw"), m_drawCheck->isChecked());
}

void RandomPickDialog::onBrowseCalib()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择标定文件"),
                                                      m_calibEdit->text(), CALIB::fileFilter());
    if (path.isEmpty())
        return;

    m_calibEdit->setText(path);

    CALIB::CalibData data;
    QString error;
    if (CALIB::CalibData::load(path, data, &error)) {
        setStatus(QStringLiteral("标定文件有效：%1 个点 | RMS %2")
                      .arg(data.points.size())
                      .arg(data.rms, 0, 'f', 3),
                  false);
    } else {
        setStatus(error, true);
    }
}

void RandomPickDialog::onTest()
{
    applyParams();

    if (!m_plugin) {
        setStatus(QStringLiteral("插件实例无效"), true);
        return;
    }
    if (m_calibEdit->text().trimmed().isEmpty()) {
        setStatus(QStringLiteral("请先选择标定文件"), true);
        return;
    }

    setStatus(QStringLiteral("正在转换，请稍候..."), false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    bool success = false;
    QString failure;
    try {
        if (!m_image.empty())
            m_plugin->setInput(QStringLiteral("input"), OVP::imageValue(m_image));
        success = m_plugin->execute();
    } catch (const cv::Exception &e) {
        success = false;
        failure = QStringLiteral("OpenCV 异常：%1").arg(QString::fromUtf8(e.what()));
    } catch (const std::exception &e) {
        success = false;
        failure = QStringLiteral("异常：%1").arg(QString::fromUtf8(e.what()));
    }
    QApplication::restoreOverrideCursor();

    const cv::Mat output = OVP::toMat(m_plugin->output(QStringLiteral("output")));
    if (!output.empty())
        showImage(output);

    if (!success) {
        const QString error = failure.isEmpty() ? m_plugin->lastError() : failure;
        setStatus(error.isEmpty() ? QStringLiteral("转换失败") : error, true);
        return;
    }

    const QVariant name = m_plugin->output(QStringLiteral("class_name"));
    setStatus(QStringLiteral("目标 #%1 %2 (%3) → 机器人 (%4, %5)")
                  .arg(m_plugin->output(QStringLiteral("pick_index")).toInt())
                  .arg(name.toString().isEmpty()
                           ? QString::number(m_plugin->output(QStringLiteral("class_id")).toInt())
                           : name.toString())
                  .arg(m_plugin->output(QStringLiteral("confidence")).toDouble(), 0, 'f', 2)
                  .arg(m_plugin->output(QStringLiteral("robot_x")).toDouble(), 0, 'f', 3)
                  .arg(m_plugin->output(QStringLiteral("robot_y")).toDouble(), 0, 'f', 3),
              false);
}

void RandomPickDialog::onAccept()
{
    applyParams();
    accept();
}

void RandomPickDialog::setStatus(const QString &text, bool isError)
{
    const QString firstLine = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    m_statusLabel->setText(firstLine.isEmpty() ? text : firstLine);
    m_statusLabel->setToolTip(text);
    m_statusLabel->setStyleSheet(
        isError ? QStringLiteral("color: #ff6b6b; font-size: 11px;")
                : QStringLiteral("color: #7fd18b; font-size: 11px;"));
}

void RandomPickDialog::showImage(const cv::Mat &image)
{
    if (image.empty()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("（无图像）"));
        return;
    }

    const QPixmap pixmap = OVP::matToQPixmap(image);
    if (pixmap.isNull())
        return;

    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
}
