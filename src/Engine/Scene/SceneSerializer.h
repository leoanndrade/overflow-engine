#pragma once

#include "Scene.h"
#include <memory>
#include <string>

namespace Overflow {

    class SceneSerializer {
    public:
        SceneSerializer(const std::shared_ptr<Scene>& scene);

        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

        // Sistema de Prefabs (.prefab)
        static bool SerializeEntity(Entity entity, const std::string& filepath);
        static Entity DeserializeEntity(const std::string& filepath, Scene* scene);

    private:
        std::shared_ptr<Scene> m_Scene;
    };

}
