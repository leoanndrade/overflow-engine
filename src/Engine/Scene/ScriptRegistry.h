#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Overflow {

    struct ScriptInfo {
        std::string Name;
        std::string Description;
        std::function<ScriptableEntity*()> Factory;
    };

    class ScriptRegistry {
    public:
        static void Register(const std::string& name, const std::string& description, std::function<ScriptableEntity*()> factory) {
            GetRegistry()[name] = { name, description, factory };
        }

        template<typename T>
        static void Register(const std::string& name, const std::string& description) {
            Register(name, description, []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); });
        }

        static const std::unordered_map<std::string, ScriptInfo>& GetScripts() {
            return GetRegistry();
        }

        static ScriptableEntity* Create(const std::string& name) {
            auto& reg = GetRegistry();
            if (reg.find(name) != reg.end()) {
                return reg[name].Factory();
            }
            return nullptr;
        }

        static bool Exists(const std::string& name) {
            return GetRegistry().find(name) != GetRegistry().end();
        }

    private:
        static std::unordered_map<std::string, ScriptInfo>& GetRegistry() {
            static std::unordered_map<std::string, ScriptInfo> s_Registry;
            return s_Registry;
        }
    };

    // Macro facilitadora para auto-registro limpo de comportamentos da Overflow Engine
    #define OVERFLOW_REGISTER_BEHAVIOR(ClassName, Description) \
        namespace \
        { \
            struct ClassName##AutoReg \
            { \
                ClassName##AutoReg() \
                { \
                    ::Overflow::ScriptRegistry::Register<ClassName>(#ClassName, Description); \
                } \
            }; \
            static ClassName##AutoReg s_##ClassName##_AutoReg; \
        }

}
