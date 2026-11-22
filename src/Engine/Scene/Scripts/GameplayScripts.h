#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Component.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"
#include "Engine/Core/Log.h"
#include "PlayerController.h"

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace Overflow {

    // -------------------------------------------------------------------------
    // Script: Rotator (Gira o objeto continuamente no espaço 3D)
    // -------------------------------------------------------------------------
    class RotatorScript : public ScriptableEntity {
    public:
        glm::vec3 RotationSpeed{ 0.0f, 45.0f, 0.0f }; // Graus por segundo

        void OnUpdate(Timestep ts) override {
            if (!HasComponent<TransformComponent>()) return;
            auto& tc = GetComponent<TransformComponent>();
            tc.Rotation += RotationSpeed * ts.GetSeconds();
        }
    };

    // -------------------------------------------------------------------------
    // Script: FloatingBob (Flutuação senoidal suave, estilo colecionáveis)
    // -------------------------------------------------------------------------
    class FloatingBobScript : public ScriptableEntity {
    public:
        float Frequency = 2.0f;
        float Amplitude = 0.35f;
        float RotationSpeed = 60.0f;

    private:
        float m_InitialY = 0.0f;
        float m_Timer = 0.0f;
        bool m_Initialized = false;

    public:
        void OnCreate() override {
            if (HasComponent<TransformComponent>()) {
                m_InitialY = GetComponent<TransformComponent>().Translation.y;
                m_Initialized = true;
            }
        }

        void OnUpdate(Timestep ts) override {
            if (!HasComponent<TransformComponent>()) return;
            auto& tc = GetComponent<TransformComponent>();
            if (!m_Initialized) {
                m_InitialY = tc.Translation.y;
                m_Initialized = true;
            }

            m_Timer += ts.GetSeconds();
            tc.Translation.y = m_InitialY + std::sin(m_Timer * Frequency) * Amplitude;
            tc.Rotation.y += RotationSpeed * ts.GetSeconds();
        }
    };

    // -------------------------------------------------------------------------
    // Script: EntitySpawner (Cria entidades dinâmicas com física em tempo de execução)
    // -------------------------------------------------------------------------
    class EntitySpawnerScript : public ScriptableEntity {
    public:
        float SpawnInterval = 2.5f;
        int MaxSpawns = 8;

    private:
        float m_Timer = 0.0f;
        int m_CurrentSpawnCount = 0;

    public:
        void OnUpdate(Timestep ts) override {
            m_Timer += ts.GetSeconds();
            if (m_Timer >= SpawnInterval && m_CurrentSpawnCount < MaxSpawns) {
                m_Timer = 0.0f;
                m_CurrentSpawnCount++;

                if (HasComponent<TransformComponent>()) {
                    auto& myTc = GetComponent<TransformComponent>();
                    Scene* scene = GetEntity().GetScene();
                    if (scene) {
                        std::string name = "Spawned Ball #" + std::to_string(m_CurrentSpawnCount);
                        auto ball = scene->CreateSphere(name, myTc.Translation + glm::vec3(0.0f, 1.0f, 0.0f));
                        ball.GetComponent<TransformComponent>().Scale = { 0.4f, 0.4f, 0.4f };
                        
                        auto& rb = ball.AddComponent<RigidbodyComponent>();
                        rb.Type = BodyType::Dynamic;
                        rb.Mass = 1.0f;
                        
                        // Impulso inicial para a frente e cima
                        rb.LinearVelocity = glm::vec3(((rand() % 100) / 50.0f - 1.0f) * 2.0f, 4.0f, ((rand() % 100) / 50.0f - 1.0f) * 2.0f);

                        auto& col = ball.AddComponent<ColliderComponent>(ColliderType::Sphere);
                        col.Radius = 0.4f;
                        col.Bounciness = 0.8f;

                        OVERFLOW_INFO("Spawner gerou entidade dinamica: {0}", name);
                    }
                }
            }
        }
    };

    // -------------------------------------------------------------------------
    // Script: ThirdPersonFollowCamera (Câmera suave que segue o alvo)
    // -------------------------------------------------------------------------
    class ThirdPersonFollowCameraScript : public ScriptableEntity {
    public:
        glm::vec3 Offset{ 0.0f, 3.5f, 6.5f };
        float SmoothSpeed = 6.0f;

        void OnUpdate(Timestep ts) override {
            if (!HasComponent<TransformComponent>()) return;
            auto& myTc = GetComponent<TransformComponent>();
            Scene* scene = GetEntity().GetScene();
            if (!scene) return;

            // Encontra a primeira entidade dinâmica (ex: Player ou Esfera)
            for (auto entity : scene->GetEntities()) {
                if (entity != GetEntity() && entity.HasComponent<RigidbodyComponent>()) {
                    auto& targetTc = entity.GetComponent<TransformComponent>();
                    glm::vec3 targetPos = targetTc.Translation + Offset;
                    myTc.Translation = glm::mix(myTc.Translation, targetPos, std::clamp(SmoothSpeed * ts.GetSeconds(), 0.0f, 1.0f));
                    break;
                }
            }
        }
    };

    inline void RegisterEngineScripts() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        ScriptRegistry::Register<PlayerController>("PlayerController", "Controle jogavel WASD com pulo e fisica dinamica");
        ScriptRegistry::Register<RotatorScript>("Rotator", "Gira o objeto continuamente no espaco 3D");
        ScriptRegistry::Register<FloatingBobScript>("FloatingBob", "Flutuacao senoidal suave e rotacao, estilo colecionaveis");
        ScriptRegistry::Register<EntitySpawnerScript>("EntitySpawner", "Gera novas entidades fisicas na cena dinamicamente");
        ScriptRegistry::Register<ThirdPersonFollowCameraScript>("FollowCamera", "Camera de terceira pessoa que segue o jogador suavemente");
    }

}
