#pragma once

#include "Entity.h"
#include "Component.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/ShadowMap.h"
#include "Engine/Renderer/Framebuffer.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <stdexcept>

namespace Overflow {

    class Scene {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = "Entity");
        Entity CreateEntityWithID(uint32_t id, const std::string& name = "Entity");
        void DestroyEntity(Entity entity);
        void Clear();

        void OnUpdate(Timestep ts);
        void OnUpdateRuntime(Timestep ts);
        void OnRenderEditor(const PerspectiveCamera& camera, const std::shared_ptr<Framebuffer>& targetFramebuffer = nullptr, const std::shared_ptr<Shader>& shader = nullptr);
        void OnViewportResize(uint32_t width, uint32_t height);

        void SetExposure(float exposure) { m_Exposure = exposure; }
        float GetExposure() const { return m_Exposure; }
        void SetShowGrid(bool show) { m_ShowGrid = show; }
        bool GetShowGrid() const { return m_ShowGrid; }
        void SetShowColliders(bool show) { m_ShowColliders = show; }
        bool GetShowColliders() const { return m_ShowColliders; }
        void SetShowGizmoIcons(bool show) { m_ShowGizmoIcons = show; }
        bool GetShowGizmoIcons() const { return m_ShowGizmoIcons; }
        void SetShowCameraFrustums(bool show) { m_ShowCameraFrustums = show; }
        bool GetShowCameraFrustums() const { return m_ShowCameraFrustums; }

        void SetVolumetricCloudsEnabled(bool enabled) { m_VolumetricCloudsEnabled = enabled; }
        bool GetVolumetricCloudsEnabled() const { return m_VolumetricCloudsEnabled; }
        void SetCloudAltitudes(float baseAlt, float topAlt) { m_CloudBaseAltitude = baseAlt; m_CloudTopAltitude = topAlt; }
        float GetCloudBaseAltitude() const { return m_CloudBaseAltitude; }
        float GetCloudTopAltitude() const { return m_CloudTopAltitude; }
        void SetCloudDensityMultiplier(float mult) { m_CloudDensityMultiplier = mult; }
        float GetCloudDensityMultiplier() const { return m_CloudDensityMultiplier; }
        void SetCloudCoverage(float coverage) { m_CloudCoverage = coverage; }
        float GetCloudCoverage() const { return m_CloudCoverage; }
        void SetWindSpeed(float speed) { m_WindSpeed = speed; }
        float GetWindSpeed() const { return m_WindSpeed; }
        void SetSelectedEntity(uint32_t id) { m_SelectedEntityID = id; }
        void SetVignette(float v) { m_Vignette = v; }
        float GetVignette() const { return m_Vignette; }
        void StopRuntimeScripts();

        const std::vector<Entity>& GetEntities() const { return m_Entities; }

        Entity GetEntityByID(uint32_t id) {
            for (const auto& entity : m_Entities) {
                if (entity.GetID() == id)
                    return entity;
            }
            return Entity(0, nullptr);
        }

        bool HasEntity(uint32_t id) const {
            for (const auto& entity : m_Entities) {
                if (entity.GetID() == id)
                    return true;
            }
            return false;
        }

        Entity GetPrimaryCameraEntity() {
            for (const auto& entity : m_Entities) {
                if (HasComponent<CameraComponent>(entity.GetID())) {
                    if (GetComponent<CameraComponent>(entity.GetID()).Primary)
                        return entity;
                }
            }
            for (const auto& entity : m_Entities) {
                if (HasComponent<CameraComponent>(entity.GetID()))
                    return entity;
            }
            return Entity(0, nullptr);
        }

        bool HasActiveCamera() const {
            return !m_Cameras.empty();
        }

        static std::shared_ptr<Scene> Copy(const std::shared_ptr<Scene>& other);

        // Construtores de Entidades Primitivas Prontas
        Entity CreateCube(const std::string& name = "3D Cube", const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
        Entity CreateSphere(const std::string& name = "3D Sphere", const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
        Entity CreatePlane(const std::string& name = "3D Plane", const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
        Entity CreateDirectionalLight(const std::string& name = "Directional Light", const glm::vec3& dir = { 0.6f, 1.0f, 0.4f });
        Entity CreateCamera(const std::string& name = "Main Camera", const glm::vec3& position = { 0.0f, 2.0f, 8.0f });

        // Acesso aos recursos compartilhados
        std::shared_ptr<Model> GetSharedCube() const { return m_SharedCube; }
        std::shared_ptr<Model> GetSharedSphere() const { return m_SharedSphere; }
        std::shared_ptr<Model> GetSharedPlane() const { return m_SharedPlane; }
        std::shared_ptr<Model> GetSharedTorus() const { return m_SharedTorus; }
        std::shared_ptr<Texture2D> GetWhiteTexture() const { return m_WhiteTexture; }
        std::shared_ptr<ShadowMap> GetShadowMap() const { return m_ShadowMap; }
        std::shared_ptr<Shader> GetPBRShader() const { return m_PBRShader; }
        std::shared_ptr<Shader> GetUnlitShader() const { return m_UnlitShader; }
        std::shared_ptr<Shader> GetBlinnPhongShader() const { return m_BlinnPhongShader; }
        std::shared_ptr<Shader> GetShaderForType(ShaderType type) const;
        std::shared_ptr<Material> GetDefaultMaterial() const { return m_DefaultMaterial; }

    public:
        // ---------------------------------------------------------------------
        // Gerenciamento de Componentes (Modern C++20 if constexpr Registry)
        // ---------------------------------------------------------------------
        template<typename T, typename... Args>
        T& AddComponent(uint32_t entityID, Args&&... args) {
            if constexpr (std::is_same_v<T, TagComponent>) {
                return m_Tags[entityID] = TagComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, TransformComponent>) {
                return m_Transforms[entityID] = TransformComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, MeshComponent>) {
                return m_Meshes[entityID] = MeshComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, LightComponent>) {
                return m_Lights[entityID] = LightComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, CameraComponent>) {
                return m_Cameras[entityID] = CameraComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
                return m_Rigidbodies[entityID] = RigidbodyComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, ColliderComponent>) {
                return m_Colliders[entityID] = ColliderComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, ParticleSystemComponent>) {
                return m_ParticleSystems[entityID] = ParticleSystemComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, AudioSourceComponent>) {
                return m_AudioSources[entityID] = AudioSourceComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, AudioListenerComponent>) {
                return m_AudioListeners[entityID] = AudioListenerComponent(std::forward<Args>(args)...);
            } else if constexpr (std::is_same_v<T, NativeScriptComponent>) {
                return m_NativeScripts[entityID] = NativeScriptComponent(std::forward<Args>(args)...);
            }
        }

        template<typename T>
        T& GetComponent(uint32_t entityID) {
            if constexpr (std::is_same_v<T, TagComponent>) {
                return m_Tags.at(entityID);
            } else if constexpr (std::is_same_v<T, TransformComponent>) {
                return m_Transforms.at(entityID);
            } else if constexpr (std::is_same_v<T, MeshComponent>) {
                return m_Meshes.at(entityID);
            } else if constexpr (std::is_same_v<T, LightComponent>) {
                return m_Lights.at(entityID);
            } else if constexpr (std::is_same_v<T, CameraComponent>) {
                return m_Cameras.at(entityID);
            } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
                return m_Rigidbodies.at(entityID);
            } else if constexpr (std::is_same_v<T, ColliderComponent>) {
                return m_Colliders.at(entityID);
            } else if constexpr (std::is_same_v<T, ParticleSystemComponent>) {
                return m_ParticleSystems.at(entityID);
            } else if constexpr (std::is_same_v<T, AudioSourceComponent>) {
                return m_AudioSources.at(entityID);
            } else if constexpr (std::is_same_v<T, AudioListenerComponent>) {
                return m_AudioListeners.at(entityID);
            } else if constexpr (std::is_same_v<T, NativeScriptComponent>) {
                return m_NativeScripts.at(entityID);
            }
        }

        template<typename T>
        const T& GetComponent(uint32_t entityID) const {
            if constexpr (std::is_same_v<T, TagComponent>) {
                return m_Tags.at(entityID);
            } else if constexpr (std::is_same_v<T, TransformComponent>) {
                return m_Transforms.at(entityID);
            } else if constexpr (std::is_same_v<T, MeshComponent>) {
                return m_Meshes.at(entityID);
            } else if constexpr (std::is_same_v<T, LightComponent>) {
                return m_Lights.at(entityID);
            } else if constexpr (std::is_same_v<T, CameraComponent>) {
                return m_Cameras.at(entityID);
            } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
                return m_Rigidbodies.at(entityID);
            } else if constexpr (std::is_same_v<T, ColliderComponent>) {
                return m_Colliders.at(entityID);
            } else if constexpr (std::is_same_v<T, ParticleSystemComponent>) {
                return m_ParticleSystems.at(entityID);
            } else if constexpr (std::is_same_v<T, AudioSourceComponent>) {
                return m_AudioSources.at(entityID);
            } else if constexpr (std::is_same_v<T, AudioListenerComponent>) {
                return m_AudioListeners.at(entityID);
            } else if constexpr (std::is_same_v<T, NativeScriptComponent>) {
                return m_NativeScripts.at(entityID);
            }
        }

        template<typename T>
        bool HasComponent(uint32_t entityID) const {
            if constexpr (std::is_same_v<T, TagComponent>) {
                return m_Tags.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, TransformComponent>) {
                return m_Transforms.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, MeshComponent>) {
                return m_Meshes.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, LightComponent>) {
                return m_Lights.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, CameraComponent>) {
                return m_Cameras.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
                return m_Rigidbodies.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, ColliderComponent>) {
                return m_Colliders.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, ParticleSystemComponent>) {
                return m_ParticleSystems.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, AudioSourceComponent>) {
                return m_AudioSources.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, AudioListenerComponent>) {
                return m_AudioListeners.count(entityID) > 0;
            } else if constexpr (std::is_same_v<T, NativeScriptComponent>) {
                return m_NativeScripts.count(entityID) > 0;
            } else {
                return false;
            }
        }

        template<typename T>
        void RemoveComponent(uint32_t entityID) {
            if constexpr (std::is_same_v<T, TagComponent>) {
                m_Tags.erase(entityID);
            } else if constexpr (std::is_same_v<T, TransformComponent>) {
                m_Transforms.erase(entityID);
            } else if constexpr (std::is_same_v<T, MeshComponent>) {
                m_Meshes.erase(entityID);
            } else if constexpr (std::is_same_v<T, LightComponent>) {
                m_Lights.erase(entityID);
            } else if constexpr (std::is_same_v<T, CameraComponent>) {
                m_Cameras.erase(entityID);
            } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
                m_Rigidbodies.erase(entityID);
            } else if constexpr (std::is_same_v<T, ColliderComponent>) {
                m_Colliders.erase(entityID);
            } else if constexpr (std::is_same_v<T, ParticleSystemComponent>) {
                m_ParticleSystems.erase(entityID);
            } else if constexpr (std::is_same_v<T, AudioSourceComponent>) {
                m_AudioSources.erase(entityID);
            } else if constexpr (std::is_same_v<T, AudioListenerComponent>) {
                m_AudioListeners.erase(entityID);
            } else if constexpr (std::is_same_v<T, NativeScriptComponent>) {
                m_NativeScripts.erase(entityID);
            }
        }

    private:
        uint32_t m_NextEntityID = 1;
        std::vector<Entity> m_Entities;

        // Tabelas de Componentes
        std::unordered_map<uint32_t, TagComponent> m_Tags;
        std::unordered_map<uint32_t, TransformComponent> m_Transforms;
        std::unordered_map<uint32_t, MeshComponent> m_Meshes;
        std::unordered_map<uint32_t, LightComponent> m_Lights;
        std::unordered_map<uint32_t, CameraComponent> m_Cameras;
        std::unordered_map<uint32_t, RigidbodyComponent> m_Rigidbodies;
        std::unordered_map<uint32_t, ColliderComponent> m_Colliders;
        std::unordered_map<uint32_t, ParticleSystemComponent> m_ParticleSystems;
        std::unordered_map<uint32_t, AudioSourceComponent> m_AudioSources;
        std::unordered_map<uint32_t, AudioListenerComponent> m_AudioListeners;
        std::unordered_map<uint32_t, NativeScriptComponent> m_NativeScripts;

        // Visualizador de Colisores & Seleção
        bool m_ShowColliders = true;
        uint32_t m_SelectedEntityID = 0;

        // Recursos compartilhados de malha e textura
        std::shared_ptr<Model> m_SharedCube;
        std::shared_ptr<Model> m_SharedSphere;
        std::shared_ptr<Model> m_SharedPlane;
        std::shared_ptr<Model> m_SharedTorus;
        std::shared_ptr<Texture2D> m_WhiteTexture;

        // Pipeline PBR & Sombras (Fase 6)
        std::shared_ptr<ShadowMap> m_ShadowMap;
        std::shared_ptr<Shader> m_ShadowShader;
        std::shared_ptr<Shader> m_PBRShader;
        std::shared_ptr<Shader> m_UnlitShader;
        std::shared_ptr<Shader> m_BlinnPhongShader;
        std::shared_ptr<Shader> m_WireframeShader;
        std::shared_ptr<Material> m_DefaultMaterial;

        // Grade Infinita Procedural (Fase 7)
        uint32_t m_GridVAO = 0;
        std::shared_ptr<Shader> m_GridShader;
        bool m_ShowGrid = true;
        float m_Exposure = 1.0f;
        float m_Vignette = 0.35f;

        // Skybox Atmosférico Procedural
        std::shared_ptr<Shader> m_SkyboxShader;
        bool m_VolumetricCloudsEnabled = true;
        float m_CloudBaseAltitude = 80.0f;
        float m_CloudTopAltitude = 220.0f;
        float m_CloudDensityMultiplier = 1.0f;
        float m_CloudCoverage = 0.5f;
        float m_WindSpeed = 1.5f;

        // Wireframe de Colisores
        uint32_t m_ColliderBoxVAO = 0;
        uint32_t m_ColliderSphereVAO = 0;
        std::shared_ptr<Shader> m_LineShader;

        // Gizmos de Ícones 3D (Billboard Badges) e Frustum de Câmera
        uint32_t m_GizmoIconVAO = 0;
        std::shared_ptr<Shader> m_GizmoIconShader;
        uint32_t m_CameraFrustumVAO = 0;
        bool m_ShowGizmoIcons = true;
        bool m_ShowCameraFrustums = true;

        // Sistema de Partículas 3D
        uint32_t m_ParticleVAO = 0;
        std::shared_ptr<Shader> m_ParticleShader;

    public:
        Entity CreateTerrain(const std::string& name = "Procedural Terrain", int resolution = 40, float size = 20.0f, float height = 2.5f);
    };

    // -------------------------------------------------------------------------
    // Implementações inline dos métodos de Entity
    // -------------------------------------------------------------------------
    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args) {
        return m_Scene->AddComponent<T>(m_EntityID, std::forward<Args>(args)...);
    }

    template<typename T>
    T& Entity::GetComponent() {
        return m_Scene->GetComponent<T>(m_EntityID);
    }

    template<typename T>
    const T& Entity::GetComponent() const {
        return m_Scene->GetComponent<T>(m_EntityID);
    }

    template<typename T>
    bool Entity::HasComponent() const {
        return m_Scene->HasComponent<T>(m_EntityID);
    }

    template<typename T>
    void Entity::RemoveComponent() {
        m_Scene->RemoveComponent<T>(m_EntityID);
    }

}
