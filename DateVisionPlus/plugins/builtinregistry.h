#ifndef BUILTINREGISTRY_H
#define BUILTINREGISTRY_H

namespace OVP {
class PluginManager;
}

/** 注册所有内置视觉工具插件 */
void registerBuiltinPlugins(OVP::PluginManager &manager);

#endif // BUILTINREGISTRY_H
