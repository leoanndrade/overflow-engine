#pragma once

#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/Material.h"

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <typeinfo>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Overflow {

    class Scene;

    struct TagComponent {
        std::string Tag;
        bool IsActive = true;
        std::string Layer = "Default";

        TagComponent() = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct TransformComponent {
        glm::vec3 Translation{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation{ 0.0f, 0.0f, 0.0f }; // Ângulos de Euler em graus
        glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

        uint32_t ParentID = 0; // 0 = Sem pai (Raiz do Grafo de Cena)

        TransformComponent() = default;
        TransformComponent(const glm::vec3& translation)
            : Translation(translation) {}

        glm::mat4 GetLocalTransform() const {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), { 1.0f, 0.0f, 0.0f })
                                * glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), { 0.0f, 1.0f, 0.0f })
                                * glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), { 0.0f, 0.0f, 1.0f });

            return glm::translate(glm::mat4(1.0f), Translation)
                 * rotation
                 * glm::scale(glm::mat4(1.0f), Scale);
        }

        glm::mat4 GetWorldTransform(Scene* scene) const;
    };

    enum class MeshGeometryType {
        Cube = 0,
        Sphere,
        Plane,
        Torus,
        CustomOBJ
    };

    struct MeshComponent {
        MeshGeometryType Geometry = MeshGeometryType::Cube;
        std::shared_ptr<Model> ModelAsset;

        // Material (.ovmat)
        std::shared_ptr<Material> MaterialAsset;
        std::string MaterialPath = "";

        std::shared_ptr<Texture2D> TextureAsset;
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float Tiling = 1.0f;
        std::string CustomFilePath;

        // Propriedades PBR (Physically Based Rendering)
        float Metallic = 0.0f;   // 0.0 = Dielétrico, 1.0 = Metal
        float Roughness = 0.4f;  // 0.0 = Espelhado liso, 1.0 = Fosco rugoso
        float AO = 1.0f;         // Ambient Occlusion

        // Efeitos de Luz Emissiva (Neon / Glow Bloom)
        glm::vec3 EmissionColor{ 0.0f };
        float EmissionIntensity = 0.0f;

        // Mapeamento de Normais (Normal Mapping em Espaço Tangente)
        std::shared_ptr<Texture2D> NormalMapAsset;
        bool UseNormalMap = false;

        MeshComponent() = default;
        MeshComponent(MeshGeometryType geom) : Geometry(geom) {}
    };

    enum class LightType {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    struct LightComponent {
        LightType Type = LightType::Directional;
        glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
        float Intensity = 1.5f;
        float AmbientIntensity = 0.15f;

        // Propriedades para Point Light e Spot Light (Atenuação Física)
        float Range = 12.0f;
        float InnerAngle = 15.0f; // Para Spot Light
        float OuterAngle = 25.0f; // Para Spot Light

        // Propriedades de Sombra em Tempo Real
        bool CastShadows = true;
        float Bias = 0.003f;

        LightComponent() = default;
    };

    struct CameraComponent {
        PerspectiveCamera Camera;
        bool Primary = true;
        bool DrawFrustum = true;

        CameraComponent()
            : Camera(45.0f, 1.778f, 0.1f, 1000.0f) {}
    };

    // -------------------------------------------------------------------------
    // Sistema de Física 3D (Rigidbody & Colliders estilo Unity / Unreal Engine)
    // -------------------------------------------------------------------------
    enum class BodyType {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    };

    struct RigidbodyComponent {
        BodyType Type = BodyType::Dynamic;
        float Mass = 1.0f;
        float LinearDrag = 0.05f;
        float AngularDrag = 0.05f;
        bool UseGravity = true;
        bool IsKinematic = false;

        // Velocidades físicas calculadas pelo integrador em tempo real
        glm::vec3 LinearVelocity{ 0.0f, 0.0f, 0.0f };
        glm::vec3 AngularVelocity{ 0.0f, 0.0f, 0.0f };

        // Constraints de travamento de eixos (estilo Unity Rigidbody Constraints)
        bool FreezePositionX = false;
        bool FreezePositionY = false;
        bool FreezePositionZ = false;
        bool FreezeRotationX = false;
        bool FreezeRotationY = false;
        bool FreezeRotationZ = false;

        RigidbodyComponent() = default;

        // Métodos de aplicação de forças e impulsos físicos
        void AddForce(const glm::vec3& force) {
            if (Mass > 0.0001f) {
                LinearVelocity += (force / Mass);
            }
        }

        void AddImpulse(const glm::vec3& impulse) {
            LinearVelocity += impulse;
        }

        void AddTorque(const glm::vec3& torque) {
            if (Mass > 0.0001f) {
                AngularVelocity += (torque / Mass);
            }
        }
    };

    // Identidade da Overflow Engine para o componente de corpo físico
    using PhysicsBodyComponent = RigidbodyComponent;

    enum class ColliderType {
        Box = 0,
        Sphere = 1
    };

    struct ColliderComponent {
        ColliderType Type = ColliderType::Box;
        glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Size{ 1.0f, 1.0f, 1.0f }; // Dimensões do Box Collider
        float Radius = 0.5f;                // Raio do Sphere Collider
        bool IsTrigger = false;
        float Bounciness = 0.6f;            // Restituição (Elasticidade de colisão)
        float Friction = 0.3f;              // Atrito de superfície

        ColliderComponent() = default;
        ColliderComponent(ColliderType type) : Type(type) {}
    };

    // -------------------------------------------------------------------------
    // Sistema de Partículas 3D (VFX / Niagara Lite)
    // -------------------------------------------------------------------------
    struct Particle {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Velocity{ 0.0f };
        glm::vec4 Color{ 1.0f };
        float Life = 1.0f;
        float MaxLife = 1.0f;
        float Size = 0.2f;
        float RemainingLifetime = 1.0f;
        float Lifetime = 1.0f;
    };

    struct ParticleSystemComponent {
        int MaxParticles = 250;
        float EmissionRate = 35.0f; // Partículas emitidas por segundo
        float Lifetime = 1.8f;
        float Speed = 2.5f;
        float GravityMultiplier = -0.6f;
        glm::vec3 Gravity{ 0.0f, -4.0f, 0.0f };
        glm::vec3 BaseVelocity{ 0.0f, 2.5f, 0.0f };
        glm::vec3 ConeDirection{ 0.0f, 1.0f, 0.0f };
        float ConeAngle = 35.0f; // Abertura do cone de dispersão em graus
        glm::vec4 ColorBegin{ 1.0f, 0.65f, 0.15f, 1.0f };
        glm::vec4 ColorEnd{ 0.85f, 0.15f, 0.05f, 0.0f };
        glm::vec4 StartColor{ 1.0f, 0.65f, 0.15f, 1.0f };
        glm::vec4 EndColor{ 0.85f, 0.15f, 0.05f, 0.0f };
        float SizeBegin = 0.35f;
        float SizeEnd = 0.05f;
        float StartSize = 0.35f;
        float EndSize = 0.05f;
        bool IsPlaying = true;
        float TimeSinceLastEmit = 0.0f;

        std::vector<Particle> Pool;
        float EmitAccumulator = 0.0f;

        ParticleSystemComponent() = default;
    };

    // -------------------------------------------------------------------------
    // Sistema de Áudio 3D Espacial (AudioSource & AudioListener)
    // -------------------------------------------------------------------------
    struct AudioSourceComponent {
        std::string ClipName = "CollisionImpact";
        std::string ClipPath = "CollisionImpact";
        float Volume = 1.0f;
        float Pitch = 1.0f;
        bool Loop = false;
        bool PlayOnStart = false;
        bool PlayOnAwake = false;
        bool Spatial3D = true;
        float MinDistance = 1.0f;
        float MaxDistance = 30.0f;
        bool IsPlaying = false;

        AudioSourceComponent() = default;
    };

    struct AudioListenerComponent {
        bool Active = true;

        AudioListenerComponent() = default;
    };

    // -------------------------------------------------------------------------
    // Sistema de Scripting Nativo (C++ Behavior / Gameplay Scripts)
    // -------------------------------------------------------------------------
}
#include "ScriptableEntity.h"
#include "ScriptRegistry.h"

namespace Overflow {

    struct NativeScriptComponent {
        ScriptableEntity* Instance = nullptr;
        std::string ScriptName = "None";
        bool WasActive = false;

        std::function<ScriptableEntity*()> InstantiateScript = nullptr;
        std::function<void(NativeScriptComponent*)> DestroyScript = nullptr;

        template<typename T>
        void Bind(const std::string& name = "") {
            ScriptName = name.empty() ? typeid(T).name() : name;
            InstantiateScript = []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); };
            DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
        }

        void BindByName(const std::string& name) {
            ScriptName = name;
            InstantiateScript = [name]() -> ScriptableEntity* {
                return ScriptRegistry::Create(name);
            };
            DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
        }
    };

}
