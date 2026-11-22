#include "AudioEngine.h"
#include "Engine/Core/Log.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace Overflow {

    std::vector<uint8_t> AudioEngine::s_CollisionWavBuffer;
    bool AudioEngine::s_Initialized = false;

    void AudioEngine::Init() {
        if (s_Initialized) return;
        OVERFLOW_CORE_INFO("Inicializando Overflow 3D Audio Engine...");
        GenerateCollisionWav();
        s_Initialized = true;
    }

    void AudioEngine::Shutdown() {
        s_CollisionWavBuffer.clear();
        s_Initialized = false;
    }

    void AudioEngine::GenerateCollisionWav() {
        // Gera um áudio WAV PCM 16-bit 22050Hz de impacto de colisão físico realista
        const int sampleRate = 22050;
        const float duration = 0.16f; // 160ms
        const int numSamples = static_cast<int>(sampleRate * duration);
        const int dataSize = numSamples * sizeof(int16_t);

        s_CollisionWavBuffer.resize(44 + dataSize);
        uint8_t* p = s_CollisionWavBuffer.data();

        // RIFF Header
        memcpy(p, "RIFF", 4); p += 4;
        uint32_t fileSizeMinus8 = 36 + dataSize;
        memcpy(p, &fileSizeMinus8, 4); p += 4;
        memcpy(p, "WAVE", 4); p += 4;

        // fmt subchunk
        memcpy(p, "fmt ", 4); p += 4;
        uint32_t subchunk1Size = 16;
        memcpy(p, &subchunk1Size, 4); p += 4;
        uint16_t audioFormat = 1; // PCM
        memcpy(p, &audioFormat, 2); p += 2;
        uint16_t numChannels = 1; // Mono
        memcpy(p, &numChannels, 2); p += 2;
        uint32_t sr = sampleRate;
        memcpy(p, &sr, 4); p += 4;
        uint32_t byteRate = sampleRate * sizeof(int16_t);
        memcpy(p, &byteRate, 4); p += 4;
        uint16_t blockAlign = sizeof(int16_t);
        memcpy(p, &blockAlign, 2); p += 2;
        uint16_t bitsPerSample = 16;
        memcpy(p, &bitsPerSample, 2); p += 2;

        // data subchunk
        memcpy(p, "data", 4); p += 4;
        uint32_t ds = dataSize;
        memcpy(p, &ds, 4); p += 4;

        // Gera forma de onda: impacto com queda exponencial e frequência de 160Hz decaindo para 80Hz
        int16_t* samples = reinterpret_cast<int16_t*>(p);
        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float freq = 180.0f * (1.0f - (t / duration) * 0.5f);
            float envelope = std::exp(-t * 28.0f);
            float wave = std::sin(2.0f * 3.14159265f * freq * t);
            // Adiciona pequeno clique de ruído no início para simular contato rígido
            if (t < 0.02f) {
                float noise = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 0.35f;
                wave += noise;
            }
            float val = wave * envelope * 28000.0f;
            samples[i] = static_cast<int16_t>(std::clamp(val, -32767.0f, 32767.0f));
        }
    }

    void AudioEngine::PlayCollisionSound(float intensity, const glm::vec3& soundPos, const glm::vec3& listenerPos) {
#ifdef _WIN32
        if (!s_Initialized || s_CollisionWavBuffer.empty()) return;

        // Atenuação de distância 3D
        float dist = glm::length(soundPos - listenerPos);
        float attenuation = 1.0f / (1.0f + 0.15f * dist);
        float volumeFactor = std::clamp(intensity * attenuation, 0.0f, 1.0f);

        if (volumeFactor < 0.05f) return;

        PlaySoundA(reinterpret_cast<LPCSTR>(s_CollisionWavBuffer.data()), NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#endif
    }

    void AudioEngine::PlayBeep(float frequency, int durationMs) {
#ifdef _WIN32
        Beep(static_cast<DWORD>(frequency), static_cast<DWORD>(durationMs));
#endif
    }

}
