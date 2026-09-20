#include "builtinregistry.h"

#include "../core/pluginmanager.h"

#include "blobanalysisplugin.h"
#include "edgedetectionplugin.h"
#include "grayscaleplugin.h"
#include "imagefilterplugin.h"
#include "imageoutputplugin.h"
#include "imagesourceplugin.h"
#include "linedistanceplugin.h"
#include "linefinderplugin.h"
#include "measureplugin.h"
#include "morphologyplugin.h"
#include "patternmatchplugin.h"
#include "thresholdplugin.h"

void registerBuiltinPlugins(OVP::PluginManager &manager)
{
    using OVP::createPluginInstance;

    // 输入输出
    manager.registerFactory(&createPluginInstance<OVP::ImageSourcePlugin>);
    manager.registerFactory(&createPluginInstance<OVP::ImageOutputPlugin>);

    // 图像处理
    manager.registerFactory(&createPluginInstance<OVP::GrayscalePlugin>);
    manager.registerFactory(&createPluginInstance<OVP::ThresholdPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::EdgeDetectionPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::MorphologyPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::ImageFilterPlugin>);

    // 检测定位
    manager.registerFactory(&createPluginInstance<OVP::BlobAnalysisPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::PatternMatchPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::MeasurePlugin>);
    manager.registerFactory(&createPluginInstance<OVP::LineFinderPlugin>);
    manager.registerFactory(&createPluginInstance<OVP::LineDistancePlugin>);
}
