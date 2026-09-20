#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

#include <QHash>
#include <QVariant>
#include <QWidget>

namespace OVP {
class PluginBase;
class NodeItem;
struct ParamDef;
}

namespace Ui {
class PropertiesPanel;
}

class QFormLayout;
class QLineEdit;

/** 属性面板：根据插件的参数定义动态生成控件 */
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);
    ~PropertiesPanel() override;

public slots:
    void setNode(OVP::NodeItem *node);
    void clear();

signals:
    void paramChanged(const QString &nodeId, const QString &paramName, const QVariant &value);

private:
    void clearForm();
    void addParamWidget(const OVP::ParamDef &def);
    void browseFile(QLineEdit *lineEdit, const QString &paramName);
    void onValueChanged(const QString &paramName, const QVariant &value);

    Ui::PropertiesPanel *ui = nullptr;
    QFormLayout *m_form = nullptr;
    OVP::NodeItem *m_node = nullptr;
    QHash<QString, QWidget *> m_widgets;
};

#endif // PROPERTIESPANEL_H
