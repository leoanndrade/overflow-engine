#include "SceneSerializer.h"
#include "Entity.h"
#include "Component.h"
#include "Engine/Core/Log.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace Overflow {

    SceneSerializer::SceneSerializer(const std::shared_ptr<Scene>& scene)
        : m_Scene(scene) {}

    bool SceneSerializer::Serialize(const std::string& filepath) {
        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            OVERFLOW_CORE_ERROR("Nao foi possivel abrir o arquivo para salvar cena: '{0}'", filepath);
            return false;
        }

        fout << "Scene: OverflowScene\n";
        fout << "Entities:\n";

        for (const auto& entity : m_Scene->GetEntities()) {
            uint32_t id = entity.GetID();
            fout << "  - Entity: " << id << "\n";

            if (m_Scene->HasComponent<TagComponent>(id)) {
                const auto& tag = m_Scene->GetComponent<TagComponent>(id);
                fout << "    TagComponent:\n";
                fout << "      Tag: " << tag.Tag << "\n";
                fout << "      IsActive: " << (tag.IsActive ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<TransformComponent>(id)) {
                const auto& tc = m_Scene->GetComponent<TransformComponent>(id);
                fout << "    TransformComponent:\n";
                fout << "      Translation: [" << tc.Translation.x << ", " << tc.Translation.y << ", " << tc.Translation.z << "]\n";
                fout << "      Rotation: [" << tc.Rotation.x << ", " << tc.Rotation.y << ", " << tc.Rotation.z << "]\n";
                fout << "      Scale: [" << tc.Scale.x << ", " << tc.Scale.y << ", " << tc.Scale.z << "]\n";
                fout << "      ParentID: " << tc.ParentID << "\n";
            }

            if (m_Scene->HasComponent<MeshComponent>(id)) {
                const auto& mc = m_Scene->GetComponent<MeshComponent>(id);
                fout << "    MeshComponent:\n";
                fout << "      GeometryType: " << static_cast<int>(mc.Geometry) << "\n";
                fout << "      Color: [" << mc.Color.r << ", " << mc.Color.g << ", " << mc.Color.b << ", " << mc.Color.a << "]\n";
                fout << "      Metallic: " << mc.Metallic << "\n";
                fout << "      Roughness: " << mc.Roughness << "\n";
                fout << "      AO: " << mc.AO << "\n";
                fout << "      Tiling: " << mc.Tiling << "\n";
                fout << "      EmissionColor: [" << mc.EmissionColor.r << ", " << mc.EmissionColor.g << ", " << mc.EmissionColor.b << "]\n";
                fout << "      EmissionIntensity: " << mc.EmissionIntensity << "\n";
                fout << "      TexturePath: " << (mc.TextureAsset ? mc.TextureAsset->GetPath() : "") << "\n";
                fout << "      NormalMapPath: " << (mc.NormalMapAsset ? mc.NormalMapAsset->GetPath() : "") << "\n";
                fout << "      UseNormalMap: " << (mc.UseNormalMap ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<LightComponent>(id)) {
                const auto& lc = m_Scene->GetComponent<LightComponent>(id);
                fout << "    LightComponent:\n";
                fout << "      Type: " << static_cast<int>(lc.Type) << "\n";
                fout << "      Color: [" << lc.Color.r << ", " << lc.Color.g << ", " << lc.Color.b << "]\n";
                fout << "      Intensity: " << lc.Intensity << "\n";
                fout << "      Range: " << lc.Range << "\n";
                fout << "      InnerAngle: " << lc.InnerAngle << "\n";
                fout << "      OuterAngle: " << lc.OuterAngle << "\n";
                fout << "      AmbientIntensity: " << lc.AmbientIntensity << "\n";
                fout << "      CastShadows: " << (lc.CastShadows ? 1 : 0) << "\n";
                fout << "      Bias: " << lc.Bias << "\n";
            }

            if (m_Scene->HasComponent<CameraComponent>(id)) {
                const auto& cc = m_Scene->GetComponent<CameraComponent>(id);
                fout << "    CameraComponent:\n";
                fout << "      Primary: " << (cc.Primary ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<RigidbodyComponent>(id)) {
                const auto& rb = m_Scene->GetComponent<RigidbodyComponent>(id);
                fout << "    RigidbodyComponent:\n";
                fout << "      BodyType: " << static_cast<int>(rb.Type) << "\n";
                fout << "      Mass: " << rb.Mass << "\n";
                fout << "      LinearDrag: " << rb.LinearDrag << "\n";
                fout << "      AngularDrag: " << rb.AngularDrag << "\n";
                fout << "      UseGravity: " << (rb.UseGravity ? 1 : 0) << "\n";
                fout << "      IsKinematic: " << (rb.IsKinematic ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<ColliderComponent>(id)) {
                const auto& col = m_Scene->GetComponent<ColliderComponent>(id);
                fout << "    ColliderComponent:\n";
                fout << "      ColliderType: " << static_cast<int>(col.Type) << "\n";
                fout << "      Center: [" << col.Center.x << ", " << col.Center.y << ", " << col.Center.z << "]\n";
                fout << "      Size: [" << col.Size.x << ", " << col.Size.y << ", " << col.Size.z << "]\n";
                fout << "      Radius: " << col.Radius << "\n";
                fout << "      Bounciness: " << col.Bounciness << "\n";
                fout << "      Friction: " << col.Friction << "\n";
            }

            if (m_Scene->HasComponent<ParticleSystemComponent>(id)) {
                const auto& ps = m_Scene->GetComponent<ParticleSystemComponent>(id);
                fout << "    ParticleSystemComponent:\n";
                fout << "      MaxParticles: " << ps.MaxParticles << "\n";
                fout << "      EmissionRate: " << ps.EmissionRate << "\n";
                fout << "      Lifetime: " << ps.Lifetime << "\n";
                fout << "      StartColor: [" << ps.StartColor.r << ", " << ps.StartColor.g << ", " << ps.StartColor.b << ", " << ps.StartColor.a << "]\n";
                fout << "      EndColor: [" << ps.EndColor.r << ", " << ps.EndColor.g << ", " << ps.EndColor.b << ", " << ps.EndColor.a << "]\n";
                fout << "      StartSize: " << ps.StartSize << "\n";
                fout << "      EndSize: " << ps.EndSize << "\n";
                fout << "      BaseVelocity: [" << ps.BaseVelocity.x << ", " << ps.BaseVelocity.y << ", " << ps.BaseVelocity.z << "]\n";
                fout << "      Gravity: [" << ps.Gravity.x << ", " << ps.Gravity.y << ", " << ps.Gravity.z << "]\n";
                fout << "      IsPlaying: " << (ps.IsPlaying ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<AudioSourceComponent>(id)) {
                const auto& as = m_Scene->GetComponent<AudioSourceComponent>(id);
                fout << "    AudioSourceComponent:\n";
                fout << "      ClipPath: " << as.ClipPath << "\n";
                fout << "      Volume: " << as.Volume << "\n";
                fout << "      Pitch: " << as.Pitch << "\n";
                fout << "      Loop: " << (as.Loop ? 1 : 0) << "\n";
                fout << "      PlayOnAwake: " << (as.PlayOnAwake ? 1 : 0) << "\n";
                fout << "      Spatial3D: " << (as.Spatial3D ? 1 : 0) << "\n";
            }

            if (m_Scene->HasComponent<AudioListenerComponent>(id)) {
                const auto& al = m_Scene->GetComponent<AudioListenerComponent>(id);
                fout << "    AudioListenerComponent:\n";
                fout << "      Active: " << (al.Active ? 1 : 0) << "\n";
            }
        }

        OVERFLOW_CORE_INFO("Cena salva com sucesso em '{0}'", filepath);
        return true;
    }

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        std::ifstream stream(filepath);
        if (!stream.is_open()) {
            OVERFLOW_CORE_ERROR("Nao foi possivel carregar arquivo de cena: '{0}'", filepath);
            return false;
        }

        m_Scene->Clear();

        std::string line;
        Entity currentEntity(0, nullptr);
        uint32_t currentID = 0;

        auto parseVec3 = [](const std::string& str) -> glm::vec3 {
            size_t start = str.find('[');
            size_t end = str.find(']');
            if (start == std::string::npos || end == std::string::npos) return { 0.0f, 0.0f, 0.0f };
            std::string sub = str.substr(start + 1, end - start - 1);
            std::stringstream ss(sub);
            char c;
            glm::vec3 v{ 0.0f };
            ss >> v.x >> c >> v.y >> c >> v.z;
            return v;
        };

        auto parseVec4 = [](const std::string& str) -> glm::vec4 {
            size_t start = str.find('[');
            size_t end = str.find(']');
            if (start == std::string::npos || end == std::string::npos) return { 1.0f, 1.0f, 1.0f, 1.0f };
            std::string sub = str.substr(start + 1, end - start - 1);
            std::stringstream ss(sub);
            char c;
            glm::vec4 v{ 1.0f };
            ss >> v.r >> c >> v.g >> c >> v.b >> c >> v.a;
            return v;
        };

        while (std::getline(stream, line)) {
            if (line.find("  - Entity: ") != std::string::npos) {
                currentID = static_cast<uint32_t>(std::stoul(line.substr(12)));
                currentEntity = m_Scene->CreateEntityWithID(currentID, "Entity");
            } else if (currentEntity && line.find("      Tag: ") != std::string::npos) {
                std::string tag = line.substr(11);
                currentEntity.GetComponent<TagComponent>().Tag = tag;
            } else if (currentEntity && line.find("      IsActive: ") != std::string::npos) {
                currentEntity.GetComponent<TagComponent>().IsActive = (std::stoi(line.substr(16)) != 0);
            } else if (currentEntity && line.find("      Translation: ") != std::string::npos) {
                currentEntity.GetComponent<TransformComponent>().Translation = parseVec3(line.substr(19));
            } else if (currentEntity && line.find("      Rotation: ") != std::string::npos) {
                currentEntity.GetComponent<TransformComponent>().Rotation = parseVec3(line.substr(16));
            } else if (currentEntity && line.find("      Scale: ") != std::string::npos) {
                currentEntity.GetComponent<TransformComponent>().Scale = parseVec3(line.substr(13));
            } else if (currentEntity && line.find("      ParentID: ") != std::string::npos) {
                currentEntity.GetComponent<TransformComponent>().ParentID = static_cast<uint32_t>(std::stoul(line.substr(16)));
            } else if (currentEntity && line.find("    MeshComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<MeshComponent>()) {
                    currentEntity.AddComponent<MeshComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<MeshComponent>()) {
                auto& mc = currentEntity.GetComponent<MeshComponent>();
                if (line.find("      GeometryType: ") != std::string::npos) {
                    mc.Geometry = static_cast<MeshGeometryType>(std::stoi(line.substr(20)));
                    if (mc.Geometry == MeshGeometryType::Cube) mc.ModelAsset = m_Scene->GetSharedCube();
                    else if (mc.Geometry == MeshGeometryType::Sphere) mc.ModelAsset = m_Scene->GetSharedSphere();
                    else if (mc.Geometry == MeshGeometryType::Plane) mc.ModelAsset = m_Scene->GetSharedPlane();
                    else if (mc.Geometry == MeshGeometryType::Torus) mc.ModelAsset = m_Scene->GetSharedTorus();
                } else if (line.find("      Color: ") != std::string::npos) {
                    mc.Color = parseVec4(line.substr(13));
                } else if (line.find("      Metallic: ") != std::string::npos) {
                    mc.Metallic = std::stof(line.substr(16));
                } else if (line.find("      Roughness: ") != std::string::npos) {
                    mc.Roughness = std::stof(line.substr(17));
                } else if (line.find("      AO: ") != std::string::npos) {
                    mc.AO = std::stof(line.substr(10));
                } else if (line.find("      Tiling: ") != std::string::npos) {
                    mc.Tiling = std::stof(line.substr(14));
                } else if (line.find("      TexturePath: ") != std::string::npos) {
                    std::string texPath = line.substr(19);
                    if (!texPath.empty() && std::filesystem::exists(texPath)) {
                        mc.TextureAsset = Texture2D::Create(texPath);
                    }
                } else if (line.find("      EmissionColor: ") != std::string::npos) {
                    mc.EmissionColor = parseVec3(line.substr(21));
                } else if (line.find("      EmissionIntensity: ") != std::string::npos) {
                    mc.EmissionIntensity = std::stof(line.substr(25));
                } else if (line.find("      NormalMapPath: ") != std::string::npos) {
                    std::string nPath = line.substr(21);
                    if (!nPath.empty() && std::filesystem::exists(nPath)) {
                        mc.NormalMapAsset = Texture2D::Create(nPath);
                    }
                } else if (line.find("      UseNormalMap: ") != std::string::npos) {
                    mc.UseNormalMap = (std::stoi(line.substr(20)) != 0);
                }
            } else if (currentEntity && line.find("    LightComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<LightComponent>()) {
                    currentEntity.AddComponent<LightComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<LightComponent>()) {
                auto& lc = currentEntity.GetComponent<LightComponent>();
                if (line.find("      Type: ") != std::string::npos) {
                    lc.Type = static_cast<LightType>(std::stoi(line.substr(12)));
                } else if (line.find("      Color: ") != std::string::npos) {
                    lc.Color = parseVec3(line.substr(13));
                } else if (line.find("      Intensity: ") != std::string::npos) {
                    lc.Intensity = std::stof(line.substr(17));
                } else if (line.find("      Range: ") != std::string::npos) {
                    lc.Range = std::stof(line.substr(13));
                } else if (line.find("      InnerAngle: ") != std::string::npos) {
                    lc.InnerAngle = std::stof(line.substr(18));
                } else if (line.find("      OuterAngle: ") != std::string::npos) {
                    lc.OuterAngle = std::stof(line.substr(18));
                } else if (line.find("      AmbientIntensity: ") != std::string::npos) {
                    lc.AmbientIntensity = std::stof(line.substr(24));
                } else if (line.find("      CastShadows: ") != std::string::npos) {
                    lc.CastShadows = (std::stoi(line.substr(19)) != 0);
                } else if (line.find("      Bias: ") != std::string::npos) {
                    lc.Bias = std::stof(line.substr(12));
                }
            } else if (currentEntity && line.find("    CameraComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<CameraComponent>()) {
                    currentEntity.AddComponent<CameraComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<CameraComponent>()) {
                auto& cc = currentEntity.GetComponent<CameraComponent>();
                if (line.find("      Primary: ") != std::string::npos) {
                    cc.Primary = (std::stoi(line.substr(15)) != 0);
                }
            } else if (currentEntity && line.find("    RigidbodyComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<RigidbodyComponent>()) {
                    currentEntity.AddComponent<RigidbodyComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<RigidbodyComponent>()) {
                auto& rb = currentEntity.GetComponent<RigidbodyComponent>();
                if (line.find("      BodyType: ") != std::string::npos) {
                    rb.Type = static_cast<BodyType>(std::stoi(line.substr(16)));
                } else if (line.find("      Mass: ") != std::string::npos) {
                    rb.Mass = std::stof(line.substr(12));
                } else if (line.find("      LinearDrag: ") != std::string::npos) {
                    rb.LinearDrag = std::stof(line.substr(18));
                } else if (line.find("      AngularDrag: ") != std::string::npos) {
                    rb.AngularDrag = std::stof(line.substr(19));
                } else if (line.find("      UseGravity: ") != std::string::npos) {
                    rb.UseGravity = (std::stoi(line.substr(18)) != 0);
                } else if (line.find("      IsKinematic: ") != std::string::npos) {
                    rb.IsKinematic = (std::stoi(line.substr(19)) != 0);
                }
            } else if (currentEntity && line.find("    ColliderComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<ColliderComponent>()) {
                    currentEntity.AddComponent<ColliderComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<ColliderComponent>()) {
                auto& col = currentEntity.GetComponent<ColliderComponent>();
                if (line.find("      ColliderType: ") != std::string::npos) {
                    col.Type = static_cast<ColliderType>(std::stoi(line.substr(20)));
                } else if (line.find("      Center: ") != std::string::npos) {
                    col.Center = parseVec3(line.substr(14));
                } else if (line.find("      Size: ") != std::string::npos) {
                    col.Size = parseVec3(line.substr(12));
                } else if (line.find("      Radius: ") != std::string::npos) {
                    col.Radius = std::stof(line.substr(14));
                } else if (line.find("      Bounciness: ") != std::string::npos) {
                    col.Bounciness = std::stof(line.substr(18));
                } else if (line.find("      Friction: ") != std::string::npos) {
                    col.Friction = std::stof(line.substr(16));
                }
            } else if (currentEntity && line.find("    ParticleSystemComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<ParticleSystemComponent>()) {
                    currentEntity.AddComponent<ParticleSystemComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<ParticleSystemComponent>()) {
                auto& ps = currentEntity.GetComponent<ParticleSystemComponent>();
                if (line.find("      MaxParticles: ") != std::string::npos) {
                    ps.MaxParticles = std::stoi(line.substr(20));
                } else if (line.find("      EmissionRate: ") != std::string::npos) {
                    ps.EmissionRate = std::stof(line.substr(20));
                } else if (line.find("      Lifetime: ") != std::string::npos) {
                    ps.Lifetime = std::stof(line.substr(16));
                } else if (line.find("      StartColor: ") != std::string::npos) {
                    ps.StartColor = parseVec4(line.substr(18));
                } else if (line.find("      EndColor: ") != std::string::npos) {
                    ps.EndColor = parseVec4(line.substr(16));
                } else if (line.find("      StartSize: ") != std::string::npos) {
                    ps.StartSize = std::stof(line.substr(17));
                } else if (line.find("      EndSize: ") != std::string::npos) {
                    ps.EndSize = std::stof(line.substr(15));
                } else if (line.find("      BaseVelocity: ") != std::string::npos) {
                    ps.BaseVelocity = parseVec3(line.substr(20));
                } else if (line.find("      Gravity: ") != std::string::npos) {
                    ps.Gravity = parseVec3(line.substr(15));
                } else if (line.find("      IsPlaying: ") != std::string::npos) {
                    ps.IsPlaying = (std::stoi(line.substr(17)) != 0);
                }
            } else if (currentEntity && line.find("    AudioSourceComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<AudioSourceComponent>()) {
                    currentEntity.AddComponent<AudioSourceComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<AudioSourceComponent>()) {
                auto& as = currentEntity.GetComponent<AudioSourceComponent>();
                if (line.find("      ClipPath: ") != std::string::npos) {
                    as.ClipPath = line.substr(16);
                } else if (line.find("      Volume: ") != std::string::npos) {
                    as.Volume = std::stof(line.substr(14));
                } else if (line.find("      Pitch: ") != std::string::npos) {
                    as.Pitch = std::stof(line.substr(13));
                } else if (line.find("      Loop: ") != std::string::npos) {
                    as.Loop = (std::stoi(line.substr(12)) != 0);
                } else if (line.find("      PlayOnAwake: ") != std::string::npos) {
                    as.PlayOnAwake = (std::stoi(line.substr(19)) != 0);
                } else if (line.find("      Spatial3D: ") != std::string::npos) {
                    as.Spatial3D = (std::stoi(line.substr(17)) != 0);
                }
            } else if (currentEntity && line.find("    AudioListenerComponent:") != std::string::npos) {
                if (!currentEntity.HasComponent<AudioListenerComponent>()) {
                    currentEntity.AddComponent<AudioListenerComponent>();
                }
            } else if (currentEntity && currentEntity.HasComponent<AudioListenerComponent>()) {
                auto& al = currentEntity.GetComponent<AudioListenerComponent>();
                if (line.find("      Active: ") != std::string::npos) {
                    al.Active = (std::stoi(line.substr(14)) != 0);
                }
            }
        }

        OVERFLOW_CORE_INFO("Cena carregada com sucesso de '{0}'", filepath);
        return true;
    }

    bool SceneSerializer::SerializeEntity(Entity entity, const std::string& filepath) {
        if (!entity) return false;
        auto tempScene = std::make_shared<Scene>();
        Entity copy = tempScene->CreateEntityWithID(1, "PrefabEntity");

        uint32_t id = entity.GetID();
        Scene* srcScene = entity.GetScene();

        if (srcScene->HasComponent<TagComponent>(id)) copy.GetComponent<TagComponent>() = srcScene->GetComponent<TagComponent>(id);
        if (srcScene->HasComponent<TransformComponent>(id)) copy.GetComponent<TransformComponent>() = srcScene->GetComponent<TransformComponent>(id);
        if (srcScene->HasComponent<MeshComponent>(id)) copy.AddComponent<MeshComponent>() = srcScene->GetComponent<MeshComponent>(id);
        if (srcScene->HasComponent<LightComponent>(id)) copy.AddComponent<LightComponent>() = srcScene->GetComponent<LightComponent>(id);
        if (srcScene->HasComponent<CameraComponent>(id)) copy.AddComponent<CameraComponent>() = srcScene->GetComponent<CameraComponent>(id);
        if (srcScene->HasComponent<RigidbodyComponent>(id)) copy.AddComponent<RigidbodyComponent>() = srcScene->GetComponent<RigidbodyComponent>(id);
        if (srcScene->HasComponent<ColliderComponent>(id)) copy.AddComponent<ColliderComponent>() = srcScene->GetComponent<ColliderComponent>(id);
        if (srcScene->HasComponent<ParticleSystemComponent>(id)) copy.AddComponent<ParticleSystemComponent>() = srcScene->GetComponent<ParticleSystemComponent>(id);
        if (srcScene->HasComponent<AudioSourceComponent>(id)) copy.AddComponent<AudioSourceComponent>() = srcScene->GetComponent<AudioSourceComponent>(id);
        if (srcScene->HasComponent<AudioListenerComponent>(id)) copy.AddComponent<AudioListenerComponent>() = srcScene->GetComponent<AudioListenerComponent>(id);
        if (srcScene->HasComponent<NativeScriptComponent>(id)) copy.AddComponent<NativeScriptComponent>() = srcScene->GetComponent<NativeScriptComponent>(id);

        SceneSerializer serializer(tempScene);
        return serializer.Serialize(filepath);
    }

    Entity SceneSerializer::DeserializeEntity(const std::string& filepath, Scene* scene) {
        if (!scene || !std::filesystem::exists(filepath)) return Entity(0, nullptr);

        auto tempScene = std::make_shared<Scene>();
        SceneSerializer serializer(tempScene);
        if (!serializer.Deserialize(filepath)) return Entity(0, nullptr);

        auto entities = tempScene->GetEntities();
        if (entities.empty()) return Entity(0, nullptr);

        Entity srcEntity = entities[0];
        uint32_t srcId = srcEntity.GetID();

        std::string name = tempScene->HasComponent<TagComponent>(srcId) ? tempScene->GetComponent<TagComponent>(srcId).Tag : "Prefab Instance";
        Entity newEntity = scene->CreateEntity(name);

        if (tempScene->HasComponent<TagComponent>(srcId)) newEntity.GetComponent<TagComponent>() = tempScene->GetComponent<TagComponent>(srcId);
        if (tempScene->HasComponent<TransformComponent>(srcId)) newEntity.GetComponent<TransformComponent>() = tempScene->GetComponent<TransformComponent>(srcId);
        if (tempScene->HasComponent<MeshComponent>(srcId)) newEntity.AddComponent<MeshComponent>() = tempScene->GetComponent<MeshComponent>(srcId);
        if (tempScene->HasComponent<LightComponent>(srcId)) newEntity.AddComponent<LightComponent>() = tempScene->GetComponent<LightComponent>(srcId);
        if (tempScene->HasComponent<CameraComponent>(srcId)) newEntity.AddComponent<CameraComponent>() = tempScene->GetComponent<CameraComponent>(srcId);
        if (tempScene->HasComponent<RigidbodyComponent>(srcId)) newEntity.AddComponent<RigidbodyComponent>() = tempScene->GetComponent<RigidbodyComponent>(srcId);
        if (tempScene->HasComponent<ColliderComponent>(srcId)) newEntity.AddComponent<ColliderComponent>() = tempScene->GetComponent<ColliderComponent>(srcId);
        if (tempScene->HasComponent<ParticleSystemComponent>(srcId)) newEntity.AddComponent<ParticleSystemComponent>() = tempScene->GetComponent<ParticleSystemComponent>(srcId);
        if (tempScene->HasComponent<AudioSourceComponent>(srcId)) newEntity.AddComponent<AudioSourceComponent>() = tempScene->GetComponent<AudioSourceComponent>(srcId);
        if (tempScene->HasComponent<AudioListenerComponent>(srcId)) newEntity.AddComponent<AudioListenerComponent>() = tempScene->GetComponent<AudioListenerComponent>(srcId);
        if (tempScene->HasComponent<NativeScriptComponent>(srcId)) newEntity.AddComponent<NativeScriptComponent>() = tempScene->GetComponent<NativeScriptComponent>(srcId);

        return newEntity;
    }

}
