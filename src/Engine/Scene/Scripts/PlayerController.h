#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Component.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"

namespace Overflow {

    class PlayerController : public ScriptableEntity {
    public:
        float MoveSpeed = 6.0f;
        float JumpForce = 6.5f;

        virtual void OnCreate() override {
        }

        virtual void OnUpdate(Timestep ts) override {
            if (!HasComponent<TransformComponent>()) return;

            auto& tc = GetComponent<TransformComponent>();
            float dt = ts.GetSeconds();

            // Movimentação WASD
            glm::vec3 moveDir{ 0.0f };
            if (Input::IsKeyPressed(Key::W)) moveDir.z -= 1.0f;
            if (Input::IsKeyPressed(Key::S)) moveDir.z += 1.0f;
            if (Input::IsKeyPressed(Key::A)) moveDir.x -= 1.0f;
            if (Input::IsKeyPressed(Key::D)) moveDir.x += 1.0f;

            if (glm::length(moveDir) > 0.001f) {
                moveDir = glm::normalize(moveDir);
                if (HasComponent<RigidbodyComponent>()) {
                    auto& rb = GetComponent<RigidbodyComponent>();
                    rb.LinearVelocity.x = moveDir.x * MoveSpeed;
                    rb.LinearVelocity.z = moveDir.z * MoveSpeed;
                } else {
                    tc.Translation += moveDir * (MoveSpeed * dt);
                }
            }

            // Pulo com Barra de Espaço
            if (Input::IsKeyPressed(Key::Space)) {
                if (HasComponent<RigidbodyComponent>()) {
                    auto& rb = GetComponent<RigidbodyComponent>();
                    // Pula se estiver no chão ou quase no chão
                    if (tc.Translation.y <= 0.65f && std::abs(rb.LinearVelocity.y) < 0.2f) {
                        rb.LinearVelocity.y = JumpForce;
                    }
                }
            }
        }
    };

}
