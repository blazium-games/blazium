#include "register_types.h"

#include "yaml.h"

#include "core/object/class_db.h"

void initialize_yaml_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    GDREGISTER_CLASS(YAML);
}

void uninitialize_yaml_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}