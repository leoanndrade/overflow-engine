#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Scene/Component.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"
#include "Engine/Core/Log.h"

namespace Overflow {

    class HelloWorld : public ScriptableEntity {
    public:
        float Speed = 5.0f;

        void OnCreate() override {
            OVERFLOW_INFO("HelloWorld instanciado na entidade!");
        }

        void OnUpdate(Timestep ts) override {
            if (!HasComponent<TransformComponent>()) return;
            auto& tc = GetComponent<TransformComponent>();
            // Exemplo de movimentacao ou logica de gameplay:
            // tc.Rotation.y += Speed * 10.0f * ts.GetSeconds();
        }

        void OnDestroy() override {
            OVERFLOW_INFO("HelloWorld destruido!");
        }
    };

    // Auto-registro no ScriptRegistry da Engine
    namespace {
        struct HelloWorldAutoRegister {
            HelloWorldAutoRegister() {
                ScriptRegistry::Register<HelloWorld>("HelloWorld", "Script C++ de gameplay customizado");
            }
        };
        static HelloWorldAutoRegister s_HelloWorldAutoReg;
    }

}
