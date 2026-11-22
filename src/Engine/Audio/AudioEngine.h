#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Overflow {

    class AudioEngine {
    public:
        static void Init();
        static void Shutdown();

        static void PlayCollisionSound(float intensity = 1.0f, const glm::vec3& soundPos = { 0.0f, 0.0f, 0.0f }, const glm::vec3& listenerPos = { 0.0f, 0.0f, 0.0f });
        static void PlayBeep(float frequency = 440.0f, int durationMs = 120);

    private:
        static void GenerateCollisionWav();
    private:
        static std::vector<uint8_t> s_CollisionWavBuffer;
        static bool s_Initialized;
    };

}
