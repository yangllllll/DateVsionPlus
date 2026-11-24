#include "calibdata.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QVariantList>

#include <cmath>

namespace CALIB {
namespace {

const char *const kType = "npoint_affine_2d";
constexpr int kVersion = 1;
constexpr int kMinPoints = 3;

void assignError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QVariantList pairList(double first, double second)
{
    return QVariantList{first, second};
}

} // namespace

QString fileFilter()
{
    return QStringLiteral("标定文件 (*.ncal *.json);;所有文件 (*.*)");
}

bool CalibData::compute(QString *error)
{
    valid = false;

    if (points.size() < kMinPoints) {
        assignError(error, QStringLiteral("至少需要 %1 组标定点，当前 %2 组")
                               .arg(kMinPoints)
                               .arg(points.size()));
        return false;
    }

    const int count = points.size();
    cv::Mat lhs(2 * count, 6, CV_64F, cv::Scalar(0.0));
    cv::Mat rhs(2 * count, 1, CV_64F);

    for (int i = 0; i < count; ++i) {
        const double u = points.at(i).image.x();
        const double v = points.at(i).image.y();

        lhs.at<double>(i * 2, 0) = u;
        lhs.at<double>(i * 2, 1) = v;
        lhs.at<double>(i * 2, 2) = 1.0;

        lhs.at<double>(i * 2 + 1, 3) = u;
        lhs.at<double>(i * 2 + 1, 4) = v;
        lhs.at<double>(i * 2 + 1, 5) = 1.0;

        rhs.at<double>(i * 2, 0) = points.at(i).robot.x();
        rhs.at<double>(i * 2 + 1, 0) = points.at(i).robot.y();
    }

    cv::Mat solution;
    if (!cv::solve(lhs, rhs, solution, cv::DECOMP_SVD)) {
        assignError(error, QStringLiteral("标定方程求解失败，请检查标定点数据"));
        return false;
    }

    const double a = solution.at<double>(0, 0);
    const double b = solution.at<double>(1, 0);
    const double c = solution.at<double>(2, 0);
    const double d = solution.at<double>(3, 0);
    const double e = solution.at<double>(4, 0);
    const double f = solution.at<double>(5, 0);

    // 图像点共线时矩阵退化，变换不可用
    if (std::fabs(a * e - b * d) < 1e-12) {
        assignError(error, QStringLiteral("标定点共线或分布过于集中，无法计算有效变换，"
                                          "请把标定点分散到画面不同位置"));
        return false;
    }

    matrix = cv::Matx23d(a, b, c, d, e, f);

    double sum = 0.0;
    for (const CalibPoint &point : points) {
        const QPointF mapped = apply(point.image);
        const double dx = mapped.x() - point.robot.x();
        const double dy = mapped.y() - point.robot.y();
        sum += dx * dx + dy * dy;
    }
    rms = std::sqrt(sum / count);

    valid = true;
    if (error)
        error->clear();
    return true;
}

QPointF CalibData::apply(const QPointF &imagePoint) const
{
    return QPointF(matrix(0, 0) * imagePoint.x() + matrix(0, 1) * imagePoint.y() + matrix(0, 2),
                   matrix(1, 0) * imagePoint.x() + matrix(1, 1) * imagePoint.y() + matrix(1, 2));
}

QVariantMap CalibData::toVariantMap() const
{
    QVariantList pointList;
    pointList.reserve(points.size());
    for (const CalibPoint &point : points) {
        QVariantMap entry;
        entry.insert(QStringLiteral("image"), pairList(point.image.x(), point.image.y()));
        entry.insert(QStringLiteral("robot"), pairList(point.robot.x(), point.robot.y()));
        pointList.append(entry);
    }

    QVariantMap map;
    map.insert(QStringLiteral("type"), QString::fromLatin1(kType));
    map.insert(QStringLiteral("version"), kVersion);
    map.insert(QStringLiteral("valid"), valid);
    map.insert(QStringLiteral("rms"), rms);
    map.insert(QStringLiteral("imageWidth"), imageSize.width());
    map.insert(QStringLiteral("imageHeight"), imageSize.height());
    map.insert(QStringLiteral("matrix"),
               QVariantList{matrix(0, 0), matrix(0, 1), matrix(0, 2),
                            matrix(1, 0), matrix(1, 1), matrix(1, 2)});
    map.insert(QStringLiteral("points"), pointList);
    return map;
}

bool CalibData::fromVariantMap(const QVariantMap &map, QString *error)
{
    points.clear();
    valid = false;
    rms = 0.0;
    matrix = cv::Matx23d(1, 0, 0, 0, 1, 0);
    imageSize = QSize();

    const QString type = map.value(QStringLiteral("type")).toString();
    if (!type.isEmpty() && type != QString::fromLatin1(kType)) {
        assignError(error, QStringLiteral("不是 N 点标定文件（type = %1）").arg(type));
        return false;
    }

    const QVariantList pointList = map.value(QStringLiteral("points")).toList();
    for (const QVariant &entry : pointList) {
        const QVariantMap pointMap = entry.toMap();
        const QVariantList image = pointMap.value(QStringLiteral("image")).toList();
        const QVariantList robot = pointMap.value(QStringLiteral("robot")).toList();
        if (image.size() < 2 || robot.size() < 2)
            continue;

        CalibPoint point;
        point.image = QPointF(image.at(0).toDouble(), image.at(1).toDouble());
        point.robot = QPointF(robot.at(0).toDouble(), robot.at(1).toDouble());
        points.append(point);
    }

    imageSize = QSize(map.value(QStringLiteral("imageWidth")).toInt(),
                      map.value(QStringLiteral("imageHeight")).toInt());
    rms = map.value(QStringLiteral("rms")).toDouble();

    const QVariantList matrixList = map.value(QStringLiteral("matrix")).toList();
    bool hasMatrix = (matrixList.size() >= 6);
    if (hasMatrix) {
        matrix = cv::Matx23d(matrixList.at(0).toDouble(), matrixList.at(1).toDouble(),
                             matrixList.at(2).toDouble(), matrixList.at(3).toDouble(),
                             matrixList.at(4).toDouble(), matrixList.at(5).toDouble());
        valid = true;
    }

    if (!hasMatrix) {
        if (points.size() < kMinPoints) {
            assignError(error, QStringLiteral("标定文件缺少变换矩阵，且标定点不足 %1 组")
                                   .arg(kMinPoints));
            return false;
        }
        QString computeError;
        if (!compute(&computeError)) {
            assignError(error, computeError);
            return false;
        }
    }

    if (error)
        error->clear();
    return true;
}

bool CalibData::save(const QString &path, QString *error) const
{
    if (path.isEmpty()) {
        assignError(error, QStringLiteral("标定文件路径为空"));
        return false;
    }

    const QJsonObject object = QJsonObject::fromVariantMap(toVariantMap());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        assignError(error, QStringLiteral("无法写入标定文件：%1\n(%2)")
                               .arg(path, file.errorString()));
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        assignError(error, QStringLiteral("保存标定文件失败：%1\n(%2)")
                               .arg(path, file.errorString()));
        return false;
    }

    if (error)
        error->clear();
    return true;
}

bool CalibData::load(const QString &path, CalibData &data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        assignError(error, QStringLiteral("无法打开标定文件：%1\n(%2)")
                               .arg(path, file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        assignError(error, QStringLiteral("标定文件格式错误：%1\n(%2)")
                               .arg(path, parseError.errorString()));
        return false;
    }

    CalibData loaded;
    if (!loaded.fromVariantMap(document.object().toVariantMap(), error))
        return false;

    loaded.sourceFile = path;
    data = loaded;
    return true;
}

std::shared_ptr<const CalibData> loadCached(const QString &path, QString *error)
{
    static QMutex mutex;
    struct Entry
    {
        QDateTime modified;
        std::shared_ptr<CalibData> data;
    };
    static QHash<QString, Entry> cache;

    const QFileInfo info(path);
    if (!info.exists()) {
        assignError(error, QStringLiteral("标定文件不存在：%1").arg(path));
        return nullptr;
    }

    const QString key = info.absoluteFilePath();
    const QDateTime modified = info.lastModified();

    QMutexLocker locker(&mutex);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd() && it->modified == modified)
        return it->data;

    std::shared_ptr<CalibData> data(new CalibData);
    QString loadError;
    if (!CalibData::load(key, *data, &loadError)) {
        assignError(error, loadError);
        return nullptr;
    }

    Entry entry;
    entry.modified = modified;
    entry.data = data;
    cache.insert(key, entry);

    if (error)
        error->clear();
    return data;
}

} // namespace CALIB
