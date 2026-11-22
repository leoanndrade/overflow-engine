#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Component.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"
#include "Engine/Core/Log.h"

#include <cmath>

namespace Overflow
{
    class MyBehavior : public OBehaviour
    {
    public:
        float JumpForce = 7.5f;     // Intensidade do impulso para cima
        float MovementSpeed = 5.0f; // Velocidade de movimentação horizontal

    protected:
        void OnInit() override
        {
            OVERFLOW_INFO("MyBehavior inicializado na entidade!");
        }

        void OnEnable() override
        {
            OVERFLOW_INFO("MyBehavior ativado!");
        }

        void OnTick(Timestep ts) override
        {
            if (!HasComponent<TransformComponent>())
            {
                return;
            }
            auto& tc = GetComponent<TransformComponent>();

            // -------------------------------------------------------------
            // Aplica Força / Impulso para cima com a Barra de Espaço
            // -------------------------------------------------------------
            if (Input::IsKeyPressed(Key::Space))
            {
                if (HasComponent<PhysicsBodyComponent>())
                {
                    auto& pb = GetComponent<PhysicsBodyComponent>();

                    // Verifica se o objeto está no chão para evitar pulo infinito
                    if (std::abs(pb.LinearVelocity.y) < 0.2f)
                    {
                        pb.AddImpulse(glm::vec3(0.0f, JumpForce, 0.0f));
                        OVERFLOW_INFO("Impulso fisico aplicado para cima!");
                    }
                }
            }

            // Movimentação horizontal suave (WASD)
            if (Input::IsKeyPressed(Key::W))
            {
                tc.Translation.z -= MovementSpeed * ts.GetSeconds();
            }
            if (Input::IsKeyPressed(Key::S))
            {
                tc.Translation.z += MovementSpeed * ts.GetSeconds();
            }
            if (Input::IsKeyPressed(Key::A))
            {
                tc.Translation.x -= MovementSpeed * ts.GetSeconds();
            }
            if (Input::IsKeyPressed(Key::D))
            {
                tc.Translation.x += MovementSpeed * ts.GetSeconds();
            }
        }

        void OnDisable() override
        {
            OVERFLOW_INFO("MyBehavior desativado!");
        }

        void OnDestroy() override
        {
            OVERFLOW_INFO("MyBehavior descartado!");
        }
    };
}
