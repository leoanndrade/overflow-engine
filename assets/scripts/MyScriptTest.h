#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Component.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"
#include "Engine/Core/Log.h"

namespace Overflow
{
    class MyScriptTest : public OBehaviour
    {
    public:
        float MovementSpeed = 5.0f;

    protected:
        void OnInit() override
        {
            OVERFLOW_INFO("MyScriptTest inicializado no mundo!");
        }

        void OnEnable() override
        {
            OVERFLOW_INFO("MyScriptTest habilitado!");
        }

        void OnTick(Timestep ts) override
        {
            if (!HasComponent<TransformComponent>())
            {
                return;
            }
            auto& tc = GetComponent<TransformComponent>();
        }

        void OnDisable() override
        {
            OVERFLOW_INFO("MyScriptTest desabilitado!");
        }

        void OnDestroy() override
        {
            OVERFLOW_INFO("MyScriptTest destruido!");
        }
    };
}
