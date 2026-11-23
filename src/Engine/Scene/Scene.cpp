#include "Scene.h"
#include "ScriptableEntity.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace Overflow {

    glm::mat4 TransformComponent::GetWorldTransform(Scene* scene) const {
        glm::mat4 local = GetLocalTransform();
        if (ParentID != 0 && scene != nullptr) {
            if (scene->HasComponent<TransformComponent>(ParentID)) {
                const auto& parentTransform = scene->GetComponent<TransformComponent>(ParentID);
                return parentTransform.GetWorldTransform(scene) * local;
            }
        }
        return local;
    }

    // =========================================================================
    // Shaders GLSL Modulares: Atmosfera Físico-Procedural, Nuvens Volumétricas 3D, Céu Cósmico e IBL
    // =========================================================================
    static const char* s_SkyNoiseGLSL = R"(
        // --- 1. Funções de Ruído e Hash Pseudoaleatório 3D ---
        float Hash31(vec3 p) {
            p = fract(p * vec3(443.8975, 397.2973, 491.1871));
            p += dot(p, p.yzx + 19.19);
            return fract((p.x + p.y) * p.z);
        }

        float Noise3D(vec3 p) {
            vec3 i = floor(p);
            vec3 f = fract(p);
            vec3 u = f * f * (3.0 - 2.0 * f);

            return mix(mix(mix(Hash31(i + vec3(0.0, 0.0, 0.0)), Hash31(i + vec3(1.0, 0.0, 0.0)), u.x),
                           mix(Hash31(i + vec3(0.0, 1.0, 0.0)), Hash31(i + vec3(1.0, 1.0, 0.0)), u.x), u.y),
                       mix(mix(Hash31(i + vec3(0.0, 0.0, 1.0)), Hash31(i + vec3(1.0, 0.0, 1.0)), u.x),
                           mix(Hash31(i + vec3(0.0, 1.0, 1.0)), Hash31(i + vec3(1.0, 1.0, 1.0)), u.x), u.y), u.z);
        }
    )";

    static const char* s_SkyCelestialGLSL = R"(
        // --- 2. Campo Estelar Físico Multiespectral com Cintilação Atmosférica ---
        vec3 RenderStarfield(vec3 dir, float time) {
            vec3 p = dir * 280.0;
            vec3 id = floor(p);
            vec3 f = fract(p) - 0.5;
            float h = Hash31(id);
            
            vec3 starColor = vec3(0.0);
            if (h > 0.982) {
                float dist = length(f);
                float temp = fract(h * 43.12);
                vec3 spectralColor = vec3(1.0);
                if (temp < 0.25) spectralColor = vec3(0.70, 0.85, 1.0);      // Azul Quente (Rigel/Vega)
                else if (temp < 0.65) spectralColor = vec3(1.0, 1.0, 1.0); // Branco Puro (Sirius)
                else if (temp < 0.85) spectralColor = vec3(1.0, 0.92, 0.70);// Amarelo Solar (Capella)
                else spectralColor = vec3(1.0, 0.50, 0.30);                // Gigante Vermelha (Betelgeuse)
                
                float twinkleSpeed = 2.0 + fract(h * 89.3) * 6.0;
                float twinkle = 0.75 + 0.25 * sin(time * twinkleSpeed + h * 6.28);
                float mag = pow(fract(h * 117.7), 4.0) * 3.5 + 0.6;
                float star = smoothstep(0.28, 0.02, dist) * mag * twinkle;
                starColor = spectralColor * star;
            }
            return starColor;
        }

        // --- 3. Via Láctea Fotorrealista com Núcleo Galáctico e Rifts de Poeira Cósmica ---
        vec3 RenderMilkyWay(vec3 dir) {
            vec3 galAxis = normalize(vec3(0.45, 0.75, -0.48));
            float distFromPlane = abs(dot(dir, galAxis));
            vec3 coreDir = normalize(vec3(0.7, 0.25, -0.65));
            float coreDot = max(dot(dir, coreDir), 0.0);
            
            float band = exp(-distFromPlane * 7.0);
            float dustRift = 1.0 - exp(-distFromPlane * 35.0) * 0.85 * Noise3D(dir * 12.0);
            float bulge = pow(coreDot, 6.0) * 2.5 * exp(-distFromPlane * 4.0);
            
            vec3 coreColor = vec3(0.18, 0.12, 0.22) * (band * dustRift + bulge);
            coreColor += vec3(0.05, 0.09, 0.16) * band;
            coreColor += vec3(0.12, 0.04, 0.08) * pow(coreDot, 3.0) * band;
            return coreColor * 1.5;
        }

        // --- 4. Lua Fotorrealista Límpida (Disco Branco Puro, Halo Suave e Sem Cortes) ---
        vec3 RenderMoon(vec3 dir, vec3 sunDir) {
            vec3 moonDir = normalize(-sunDir + vec3(0.04, 0.18, -0.02));
            float moonDot = max(dot(dir, moonDir), 0.0);
            
            // Halo de luar suave, contínuo e gradativo (sem corte brusco)
            float moonHalo = pow(moonDot, 32.0) * 0.08 + pow(moonDot, 140.0) * 0.22;
            vec3 haloColor = vec3(0.85, 0.90, 1.0) * moonHalo;
            
            // Disco lunar límpido e nítido (tamanho angular de ~0.52 graus: cos > 0.99965)
            float diskThreshold = 0.99965;
            float disk = smoothstep(diskThreshold - 0.0003, diskThreshold, moonDot);
            vec3 moonColor = vec3(0.96, 0.98, 1.0) * (disk * 4.5);
            
            return haloColor + moonColor;
        }

        // --- 5. Disco Solar com Escurecimento de Bordo e Halo Mie Gradativo (Sem Cortes) ---
        vec3 RenderSun(vec3 dir, vec3 sunDir, float sunElev) {
            float sunDot = max(dot(dir, sunDir), 0.0);
            
            // Disco Solar nítido (~0.53 graus: cos > 0.99965)
            float diskThreshold = 0.99965;
            float disk = smoothstep(diskThreshold - 0.0003, diskThreshold, sunDot);
            
            // Escurecimento de bordo físico (Limb Darkening)
            float r = sqrt(max(0.0, 1.0 - (1.0 - sunDot) / (1.0 - diskThreshold)));
            float limbDarkening = 1.0 - 0.35 * (1.0 - sqrt(max(0.0, 1.0 - r * r)));
            
            // Shifting dinâmico de cor conforme o Sol desce para o horizonte
            vec3 sunColor = mix(vec3(1.0, 0.38, 0.10), vec3(1.0, 0.98, 0.92), clamp(sunElev * 3.0, 0.0, 1.0));
            vec3 sunCore = sunColor * (disk * limbDarkening * 22.0);
            
            // Halo contínuo suave de dispersão frontal Mie
            float g = 0.88;
            float miePhase = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * sunDot, 1.5) * 0.012;
            float sunGlow = pow(sunDot, 18.0) * 0.12 + pow(sunDot, 80.0) * 0.40;
            vec3 corona = sunColor * (miePhase + sunGlow);
            
            float horizonExtinction = smoothstep(-0.06, 0.04, sunElev);
            return (sunCore + corona) * horizonExtinction;
        }
    )";

    static const char* s_SkyAtmosphereGLSL = R"(
        // --- 6. Atmosfera Rayleigh & Ozônio Chappuis (Gradiente Físico Contínuo sem Cores Cartoon) ---
        vec3 CalculateAtmosphere(vec3 dir, vec3 sunDir, float time) {
            float sunElev = sunDir.y;
            float viewZenith = max(dir.y, 0.0);
            
            float dayFactor = smoothstep(-0.10, 0.20, sunElev);
            float sunsetFactor = smoothstep(-0.15, 0.02, sunElev) * smoothstep(0.45, 0.04, sunElev);
            float nightFactor = 1.0 - dayFactor;
            
            // Atmosfera Diurna Física (Azul cerúleo profundo no zênite, névoa atmosférica suave no horizonte)
            vec3 dayZenith = vec3(0.045, 0.13, 0.34);
            vec3 dayHorizon = vec3(0.52, 0.65, 0.80);
            vec3 daySky = mix(dayHorizon, dayZenith, pow(viewZenith, 0.55));
            float sunAlign = max(dot(dir, sunDir), 0.0);
            daySky += vec3(0.18, 0.20, 0.22) * pow(sunAlign, 3.0) * dayFactor;
            
            // Pôr do Sol / Amanhecer / Crepúsculo (Tons naturais de âmbar e ozônio)
            vec3 sunsetHorizon = vec3(0.85, 0.32, 0.06);
            vec3 sunsetMid = vec3(0.45, 0.16, 0.20);
            vec3 sunsetZenith = vec3(0.025, 0.02, 0.07);
            vec3 sunsetSky = mix(sunsetHorizon, sunsetMid, pow(viewZenith, 0.4));
            sunsetSky = mix(sunsetSky, sunsetZenith, pow(viewZenith, 0.8));
            sunsetSky += vec3(0.95, 0.55, 0.18) * pow(sunAlign, 4.0) * 1.4;
            
            // Cinturão de Vênus (Arco crepuscular suave no horizonte anti-solar)
            float antiSunAlign = max(dot(dir, -vec3(sunDir.x, 0.0, sunDir.z)), 0.0);
            vec3 beltOfVenus = vec3(0.50, 0.22, 0.32) * pow(antiSunAlign, 2.5) * exp(-abs(dir.y - 0.08) * 14.0);
            sunsetSky += beltOfVenus * sunsetFactor * 0.8;
            
            // Céu Noturno Cósmico Físico (Espaço negro e límpido, sem brilho azul artificial)
            vec3 nightZenith = vec3(0.0002, 0.0004, 0.0009);
            vec3 nightHorizon = vec3(0.0010, 0.0018, 0.0035);
            vec3 nightSky = mix(nightHorizon, nightZenith, pow(viewZenith, 0.5));
            
            if (dir.y > -0.05) {
                vec3 stars = RenderStarfield(dir, time);
                vec3 milkyWay = RenderMilkyWay(dir);
                nightSky += (stars + milkyWay) * nightFactor * smoothstep(-0.02, 0.12, dir.y);
            }
            
            vec3 sky = mix(nightSky, daySky, dayFactor);
            sky = mix(sky, sunsetSky, sunsetFactor * 0.92);
            
            // Camada de Cirros de Alta Altitude (Wispy Cirrus Feathers no estrato superior)
            if (dir.y > 0.04) {
                vec2 cirrusUV = dir.xz / (dir.y + 0.15) * 1.5 + vec2(time * 0.002, time * 0.001);
                float cirrusNoise = Noise3D(vec3(cirrusUV * 2.5, 3.2)) * 0.7 + Noise3D(vec3(cirrusUV * 6.0, 7.8)) * 0.3;
                float cirrusMask = smoothstep(0.48, 0.82, cirrusNoise);
                if (cirrusMask > 0.001) {
                    vec3 cirrusColor = mix(vec3(0.003, 0.005, 0.010), mix(vec3(0.70, 0.80, 0.90), vec3(0.95, 0.45, 0.20), sunsetFactor), dayFactor);
                    sky = mix(sky, cirrusColor, cirrusMask * 0.50 * smoothstep(0.04, 0.25, dir.y));
                }
            }
            
            return sky;
        }

        // --- 7. Avaliador Geral do Céu (Skybox + Irradiância e Reflexões IBL) ---
        vec3 EvaluateSky(vec3 dir, vec3 sunDir, float time, bool includeDisks) {
            vec3 sky = CalculateAtmosphere(dir, sunDir, time);
            
            if (includeDisks && dir.y > -0.05) {
                vec3 sun = RenderSun(dir, sunDir, sunDir.y);
                vec3 moon = RenderMoon(dir, sunDir) * (1.0 - smoothstep(-0.05, 0.20, sunDir.y));
                sky += sun + moon;
            }
            
            // Perspectiva Aérea do Horizonte com o Chão
            if (dir.y < 0.0) {
                float dayF = smoothstep(-0.10, 0.20, sunDir.y);
                vec3 groundColor = mix(vec3(0.008, 0.010, 0.014), vec3(0.09, 0.10, 0.12), dayF);
                sky = mix(sky, groundColor, pow(clamp(-dir.y * 3.0, 0.0, 1.0), 0.65));
            }
            
            return sky;
        }
    )";

    static const char* s_SkyVolumetricCloudsGLSL = R"(
        uniform int u_EnableClouds;
        uniform float u_CloudBase;
        uniform float u_CloudTop;
        uniform float u_CloudDensityMult;
        uniform float u_CloudCoverage;
        uniform float u_WindSpeed;

        // --- 8. Função de Fase de Henyey-Greenstein com Lóbulos Duplos para Nuvens ---
        float CloudPhase(float cosTheta) {
            float g1 = 0.72;
            float g2 = -0.22;
            float hg1 = (1.0 - g1*g1) / pow(1.0 + g1*g1 - 2.0*g1*cosTheta, 1.5);
            float hg2 = (1.0 - g2*g2) / pow(1.0 + g2*g2 - 2.0*g2*cosTheta, 1.5);
            return (0.75 * hg1 + 0.25 * hg2) / (4.0 * 3.14159265);
        }

        // --- 9. Densidade de Nuvens 3D em Coordenadas de Mundo Real com Mapa Meteorológico ---
        float SampleCloudDensity(vec3 p, vec3 wind) {
            float mult = max(0.0, u_CloudDensityMult);
            if (mult <= 0.001) return 0.0;

            float hBase = (u_CloudBase > 1.0 ? u_CloudBase : 80.0);
            float hTop = (u_CloudTop > hBase + 10.0 ? u_CloudTop : 220.0);
            float h = clamp((p.y - hBase) / (hTop - hBase), 0.0, 1.0);
            
            // Modelagem de altura para cúmulos: base plana nítida, miolo denso e topo suavemente arredondado
            float heightShape = smoothstep(0.0, 0.18, h) * smoothstep(1.0, 0.65, h);
            if (heightShape <= 0.001) return 0.0;
            
            vec3 worldCoord = p + wind;
            
            // Mapa Meteorológico com cobertura dinâmica ajustável (0.0 = sem nuvens, 1.0 = cobertura total)
            vec2 weatherUV = worldCoord.xz * 0.0012;
            float weather = Noise3D(vec3(weatherUV, 0.5));
            float covTarget = clamp(u_CloudCoverage > 0.001 ? u_CloudCoverage : 0.45, 0.0, 1.0);
            float coverageThreshold = mix(0.82, 0.18, covTarget) + (weather - 0.5) * 0.22;
            coverageThreshold = clamp(coverageThreshold, 0.04, 0.96);
            
            // Estrutura volumétrica de lóbulos (Worley/Billow look)
            vec3 q = worldCoord * 0.008;
            float baseFBM = Noise3D(q) * 0.65 + Noise3D(q * 2.1) * 0.35;
            float billow = 1.0 - abs(baseFBM * 2.0 - 1.0);
            float cloudNoise = mix(baseFBM, billow, 0.50);
            
            // Erosão de vento nas bordas
            float detail = Noise3D(q * 4.5) * 0.20;
            float n = cloudNoise - detail;
            
            if (n < coverageThreshold) return 0.0;
            float density = (n - coverageThreshold) / max(1.0 - coverageThreshold, 0.001);
            return density * heightShape * 2.0 * mult;
        }

        // Interseção analítica de raio com a camada de nuvens no espaço de mundo [yMin, yMax]
        bool IntersectCloudLayer(vec3 ro, vec3 rd, float yMin, float yMax, out float t0, out float t1) {
            float invY = 1.0 / (abs(rd.y) > 1e-4 ? rd.y : 1e-4);
            float tA = (yMin - ro.y) * invY;
            float tB = (yMax - ro.y) * invY;
            t0 = min(tA, tB);
            t1 = max(tA, tB);
            
            if (ro.y >= yMin && ro.y <= yMax) {
                // Câmera está DENTRO da camada de nuvens!
                t0 = 0.0;
                t1 = max(t1, 0.0);
            } else {
                if (t1 <= 0.0) return false;
                t0 = max(t0, 0.0);
            }
            
            // Limite máximo de distância para performance e estabilidade
            t1 = min(t1, t0 + 350.0);
            return t1 > t0;
        }

        // --- 10. Raymarching Volumétrico 3D Nativo no Espaço de Mundo ---
        vec4 RaymarchVolumetricClouds(vec3 ro, vec3 dir, vec3 sunDir, float time, vec3 skyBg) {
            if (u_EnableClouds == 0) {
                return vec4(skyBg, 1.0);
            }

            float hBase = (u_CloudBase > 1.0 ? u_CloudBase : 80.0);
            float hTop = (u_CloudTop > hBase + 10.0 ? u_CloudTop : 220.0);
            
            float t0 = 0.0;
            float t1 = 0.0;
            if (!IntersectCloudLayer(ro, dir, hBase, hTop, t0, t1)) {
                return vec4(skyBg, 1.0);
            }
            
            float sunElev = sunDir.y;
            float dayFactor = smoothstep(-0.10, 0.20, sunElev);
            float sunsetFactor = smoothstep(-0.15, 0.02, sunElev) * smoothstep(0.45, 0.04, sunElev);
            
            // Cor da luz direta física (Sol diurno, pôr do sol ou Lua noturna)
            vec3 sunLightColor = mix(vec3(0.95, 0.40, 0.10), vec3(1.0, 0.96, 0.90), clamp(sunElev * 3.5, 0.0, 1.0));
            sunLightColor = mix(vec3(0.50, 0.16, 0.06), sunLightColor, smoothstep(-0.08, 0.05, sunElev));
            vec3 moonLightColor = vec3(0.40, 0.50, 0.68) * 0.35;
            vec3 primaryLightColor = mix(moonLightColor, sunLightColor, dayFactor);
            vec3 primaryLightDir = (sunElev > -0.05) ? sunDir : normalize(-sunDir + vec3(0.04, 0.18, -0.02));
            
            // Cor ambiente física das sombras volumétricas (natural, escura à noite, sem brilho cartoon)
            vec3 dayAmbient = vec3(0.14, 0.22, 0.36);
            vec3 sunsetAmbient = vec3(0.12, 0.08, 0.18);
            vec3 nightAmbient = vec3(0.0005, 0.0010, 0.0020);
            vec3 ambientColor = mix(nightAmbient, mix(dayAmbient, sunsetAmbient, sunsetFactor), dayFactor);
            
            float stepSize = (t1 - t0) / 20.0;
            vec3 wind = vec3(time * 1.5, 0.0, time * 0.8);
            float cosTheta = dot(dir, primaryLightDir);
            float phase = CloudPhase(cosTheta) * 3.14159265;
            
            // Jittering pseudoaleatório para eliminar banding
            float seed = Hash31(dir * 120.0 + fract(time * 0.05));
            float t = t0 + stepSize * seed;
            
            float transmittance = 1.0;
            vec3 scatteredLight = vec3(0.0);
            
            for (int i = 0; i < 20; ++i) {
                if (transmittance < 0.015) break;
                
                vec3 p = ro + dir * t;
                float density = SampleCloudDensity(p, wind);
                
                if (density > 0.002) {
                    // Light Marching: 3 passos para auto-sombreamento volumétrico real
                    float lightOpticalDepth = 0.0;
                    float lightStepSize = 4.5;
                    for (int j = 1; j <= 3; ++j) {
                        vec3 lightP = p + primaryLightDir * (float(j) * lightStepSize);
                        lightOpticalDepth += SampleCloudDensity(lightP, wind);
                    }
                    
                    // Extinção de Beer-Lambert
                    float lightTransmittance = exp(-lightOpticalDepth * 1.4);
                    float powderSugar = 1.0 - exp(-density * 2.2);
                    
                    vec3 directLight = primaryLightColor * (lightTransmittance * phase * 2.6 * powderSugar);
                    vec3 ambientLight = ambientColor * (1.1 + 0.9 * clamp((p.y - hBase) / (hTop - hBase), 0.0, 1.0));
                    vec3 stepLight = directLight + ambientLight;
                    
                    float stepExtinction = exp(-density * stepSize * 0.35);
                    scatteredLight += transmittance * (1.0 - stepExtinction) * stepLight;
                    transmittance *= stepExtinction;
                }
                
                t += stepSize;
            }
            
            vec3 finalColor = skyBg * transmittance + scatteredLight;
            return vec4(finalColor, 1.0 - transmittance);
        }
    )";

    Scene::Scene() {
        // 1. Inicializa malhas e texturas compartilhadas
        m_SharedCube = Model::CreateCube(1.0f);
        m_SharedSphere = Model::CreateSphere(1.0f, 32, 64);
        m_SharedPlane = Model::CreatePlane(12.0f, 12.0f);
        m_SharedTorus = Model::Create("assets/models/torus.obj");

        uint32_t whiteData = 0xffffffff;
        m_WhiteTexture = Texture2D::Create(1, 1);
        m_WhiteTexture->SetData(&whiteData, sizeof(uint32_t));

        // 2. Inicializa ShadowMap de alta resolução (2048x2048)
        m_ShadowMap = std::make_shared<ShadowMap>(2048, 2048);

        // 3. Shader de Mapeamento de Sombra (Passe de Profundidade)
        std::string shadowVertexSrc = R"(
            #version 460 core
            layout(location = 0) in vec3 a_Position;

            uniform mat4 u_LightSpaceMatrix;
            uniform mat4 u_Transform;

            void main() {
                gl_Position = u_LightSpaceMatrix * u_Transform * vec4(a_Position, 1.0);
            }
        )";

        std::string shadowFragmentSrc = R"(
            #version 460 core
            void main() {
                // A GPU atualiza a profundidade no ShadowMap automaticamente
            }
        )";

        m_ShadowShader = Shader::Create("ShadowDepthShader", shadowVertexSrc, shadowFragmentSrc);

        // 4. Shader PBR Completo (Cook-Torrance GGX + Sombras PCF + ACES Tone Mapping + Gamma 2.2)
        std::string pbrVertexSrc = R"(
            #version 460 core

            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec3 a_Normal;
            layout(location = 2) in vec2 a_TexCoord;
            layout(location = 3) in vec4 a_Color;

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Transform;
            uniform mat4 u_LightSpaceMatrix;

            out vec3 v_FragPos;
            out vec3 v_Normal;
            out vec2 v_TexCoord;
            out vec4 v_Color;
            out vec4 v_FragPosLightSpace;

            void main() {
                vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
                v_FragPos = worldPos.xyz;
                v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
                v_TexCoord = a_TexCoord;
                v_Color = a_Color;
                v_FragPosLightSpace = u_LightSpaceMatrix * worldPos;

                gl_Position = u_ViewProjection * worldPos;
            }
        )";

        std::string pbrFragmentSrc = R"(
            #version 460 core

            in vec3 v_FragPos;
            in vec3 v_Normal;
            in vec2 v_TexCoord;
            in vec4 v_Color;
            in vec4 v_FragPosLightSpace;

            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            uniform int u_EntityID;

            uniform sampler2D u_AlbedoTexture;
            uniform sampler2D u_ShadowMap;
            uniform sampler2D u_NormalMap;
            uniform int u_UseNormalMap;

            uniform vec3 u_CameraPosition;
            uniform vec3 u_LightDirection;
            uniform vec3 u_LightColor;
            uniform float u_LightIntensity;
            uniform float u_AmbientIntensity;
            uniform float u_ShadowBias;
            uniform float u_Time;

            uniform vec4 u_BaseColor;
            uniform float u_Metallic;
            uniform float u_Roughness;
            uniform float u_AO;
            uniform float u_Tiling;
            uniform float u_Exposure;
            uniform float u_Vignette;

            uniform vec3 u_EmissionColor;
            uniform float u_EmissionIntensity;

            // -----------------------------------------------------------------
            // Suporte a Múltiplas Luzes (Point Lights & Spot Lights)
            // -----------------------------------------------------------------
            struct PointLight {
                vec3 position;
                vec3 color;
                float intensity;
                float range;
            };

            uniform int u_PointLightCount;
            uniform PointLight u_PointLights[8];

            struct SpotLight {
                vec3 position;
                vec3 direction;
                vec3 color;
                float intensity;
                float range;
                float innerCutoff;
                float outerCutoff;
            };

            uniform int u_SpotLightCount;
            uniform SpotLight u_SpotLights[4];

            const float PI = 3.14159265358979323846;

            float DistributionGGX(vec3 N, vec3 H, float roughness) {
                float a = roughness * roughness;
                float a2 = a * a;
                float NdotH = max(dot(N, H), 0.0);
                float NdotH2 = NdotH * NdotH;

                float num = a2;
                float denom = (NdotH2 * (a2 - 1.0) + 1.0);
                denom = PI * denom * denom;

                return num / max(denom, 0.0000001);
            }

            float GeometrySchlickGGX(float NdotV, float roughness) {
                float r = (roughness + 1.0);
                float k = (r * r) / 8.0;

                float num = NdotV;
                float denom = NdotV * (1.0 - k) + k;

                return num / denom;
            }

            float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
                float NdotV = max(dot(N, V), 0.0);
                float NdotL = max(dot(N, L), 0.0);
                float ggx2 = GeometrySchlickGGX(NdotV, roughness);
                float ggx1 = GeometrySchlickGGX(NdotL, roughness);

                return ggx1 * ggx2;
            }

            vec3 FresnelSchlick(float cosTheta, vec3 F0) {
                return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
            }

            // Sombras Suaves Avançadas via Vogel Disk 16-Samples com Rotação Pseudoaleatória
            float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
                vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
                projCoords = projCoords * 0.5 + 0.5;

                if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
                    return 0.0;

                float bias = max(u_ShadowBias * (1.0 - dot(normal, lightDir)), u_ShadowBias * 0.15);

                float shadow = 0.0;
                vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
                float goldenAngle = 2.39996323;
                float seed = dot(v_FragPos.xy + v_FragPos.z, vec2(12.9898, 78.233));
                float randomAngle = fract(sin(seed) * 43758.5453) * 6.2831853;
                float cosA = cos(randomAngle);
                float sinA = sin(randomAngle);
                mat2 rot = mat2(cosA, -sinA, sinA, cosA);

                for (int i = 0; i < 16; ++i) {
                    float r = sqrt(float(i) + 0.5) / 4.0;
                    float theta = float(i) * goldenAngle;
                    vec2 offset = rot * (vec2(cos(theta), sin(theta)) * r * texelSize * 2.2);
                    float pcfDepth = texture(u_ShadowMap, projCoords.xy + offset).r;
                    shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
                }
                return shadow / 16.0;
            }

            vec3 GetNormalFromMap() {
                vec3 tangentNormal = texture(u_NormalMap, v_TexCoord * u_Tiling).xyz * 2.0 - 1.0;
                vec3 Q1  = dFdx(v_FragPos);
                vec3 Q2  = dFdy(v_FragPos);
                vec2 st1 = dFdx(v_TexCoord * u_Tiling);
                vec2 st2 = dFdy(v_TexCoord * u_Tiling);
                vec3 N   = normalize(v_Normal);
                vec3 T   = normalize(Q1 * st2.t - Q2 * st1.t);
                vec3 B   = -normalize(cross(N, T));
                mat3 TBN = mat3(T, B, N);
                return normalize(TBN * tangentNormal);
            }
        )" + std::string(s_SkyNoiseGLSL) + s_SkyCelestialGLSL + s_SkyAtmosphereGLSL + R"(
            void main() {
                vec4 texColor = texture(u_AlbedoTexture, v_TexCoord * u_Tiling);
                vec3 albedo = pow(texColor.rgb * v_Color.rgb * u_BaseColor.rgb, vec3(2.2));
                float alpha = texColor.a * v_Color.a * u_BaseColor.a;

                vec3 N = (u_UseNormalMap != 0) ? GetNormalFromMap() : normalize(v_Normal);
                vec3 V = normalize(u_CameraPosition - v_FragPos);
                vec3 L = normalize(u_LightDirection);
                vec3 H = normalize(V + L);

                vec3 F0 = vec3(0.04);
                F0 = mix(F0, albedo, u_Metallic);

                // 1. Luz Direcional Principal (Sol / Lua)
                vec3 radiance = u_LightColor * u_LightIntensity;

                float NDF = DistributionGGX(N, H, u_Roughness);
                float G   = GeometrySmith(N, V, L, u_Roughness);
                vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - u_Metallic);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                float NdotL = max(dot(N, L), 0.0);
                vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

                float shadow = CalculateShadow(v_FragPosLightSpace, N, L);
                Lo *= (1.0 - shadow);

                // 2. Contribuição PBR de Point Lights (Atenuação Física Quadrática)
                for (int i = 0; i < u_PointLightCount; ++i) {
                    vec3 toLight = u_PointLights[i].position - v_FragPos;
                    float dist = length(toLight);
                    if (dist > u_PointLights[i].range || dist < 0.001) continue;

                    vec3 L_p = normalize(toLight);
                    vec3 H_p = normalize(V + L_p);

                    float atten = clamp(1.0 - (dist / u_PointLights[i].range), 0.0, 1.0);
                    atten = atten * atten;
                    vec3 rad_p = u_PointLights[i].color * (u_PointLights[i].intensity * atten);

                    float NDF_p = DistributionGGX(N, H_p, u_Roughness);
                    float G_p   = GeometrySmith(N, V, L_p, u_Roughness);
                    vec3 F_p    = FresnelSchlick(max(dot(H_p, V), 0.0), F0);

                    vec3 kS_p = F_p;
                    vec3 kD_p = (vec3(1.0) - kS_p) * (1.0 - u_Metallic);

                    vec3 spec_p = (NDF_p * G_p * F_p) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L_p), 0.0) + 0.0001);
                    float NdotL_p = max(dot(N, L_p), 0.0);
                    Lo += (kD_p * albedo / PI + spec_p) * rad_p * NdotL_p;
                }

                // 3. Contribuição PBR de Spot Lights (Holofotes com Penumbra Suave)
                for (int i = 0; i < u_SpotLightCount; ++i) {
                    vec3 toLight = u_SpotLights[i].position - v_FragPos;
                    float dist = length(toLight);
                    if (dist > u_SpotLights[i].range || dist < 0.001) continue;

                    vec3 L_s = normalize(toLight);
                    vec3 H_s = normalize(V + L_s);

                    float theta = dot(L_s, normalize(-u_SpotLights[i].direction));
                    float epsilon = u_SpotLights[i].innerCutoff - u_SpotLights[i].outerCutoff;
                    float spotIntensity = clamp((theta - u_SpotLights[i].outerCutoff) / max(epsilon, 0.0001), 0.0, 1.0);

                    float atten = clamp(1.0 - (dist / u_SpotLights[i].range), 0.0, 1.0);
                    atten = atten * atten * spotIntensity;
                    vec3 rad_s = u_SpotLights[i].color * (u_SpotLights[i].intensity * atten);

                    float NDF_s = DistributionGGX(N, H_s, u_Roughness);
                    float G_s   = GeometrySmith(N, V, L_s, u_Roughness);
                    vec3 F_s    = FresnelSchlick(max(dot(H_s, V), 0.0), F0);

                    vec3 kS_s = F_s;
                    vec3 kD_s = (vec3(1.0) - kS_s) * (1.0 - u_Metallic);

                    vec3 spec_s = (NDF_s * G_s * F_s) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L_s), 0.0) + 0.0001);
                    float NdotL_s = max(dot(N, L_s), 0.0);
                    Lo += (kD_s * albedo / PI + spec_s) * rad_s * NdotL_s;
                }

                // 4. Image-Based Lighting (IBL) - Irradiância Hemisférica do Céu Atmosférico
                vec3 skyIrradiance = EvaluateSky(N, L, u_Time, false);
                vec3 groundBounce = vec3(0.07, 0.06, 0.05) * clamp(-N.y, 0.0, 1.0);
                vec3 ambientDome = (skyIrradiance * clamp(N.y * 0.5 + 0.5, 0.15, 1.0) + groundBounce) * u_AmbientIntensity;
                vec3 ambientDiffuse = ambientDome * albedo * u_AO * kD;

                // 5. Image-Based Lighting (IBL) - Reflexão Especular do Céu e Nuvens
                vec3 R = reflect(-V, N);
                vec3 skyReflection = EvaluateSky(R, L, u_Time, u_Roughness < 0.25);
                vec3 blurredReflection = mix(skyReflection, ambientDome, clamp(u_Roughness * 1.25, 0.0, 1.0));
                vec3 Fr = F0 + (max(vec3(1.0 - u_Roughness), F0) - F0) * pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 5.0);
                vec3 ambientSpecular = blurredReflection * Fr * u_AO;

                // 6. Emissão de Luz
                vec3 emissive = u_EmissionColor * u_EmissionIntensity;

                vec3 color = ambientDiffuse + ambientSpecular + Lo + emissive;

                // 7. Tone Mapping ACES Film
                color *= u_Exposure;
                float a = 2.51;
                float b = 0.03;
                float c = 2.43;
                float d = 0.59;
                float e = 0.14;
                color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);

                // 8. Gamma Correction 2.2
                color = pow(color, vec3(1.0 / 2.2));

                // 9. Vinheta Cinematográfica
                if (u_Vignette > 0.01) {
                    float dist = length(v_TexCoord - vec2(0.5));
                    float vig = smoothstep(0.75, 0.75 - u_Vignette * 0.35, dist);
                    color *= vig;
                }

                o_Color = vec4(color, alpha);
                o_EntityID = u_EntityID;
            }
        )";

        m_PBRShader = Shader::Create("ScenePBRShader", pbrVertexSrc, pbrFragmentSrc);

        // Shader Unlit / Flat Color & Texture (Sem luzes, ideal para UI, efeitos e cartoon)
        std::string unlitFragmentSrc = R"(
            #version 460 core

            in vec3 v_FragPos;
            in vec3 v_Normal;
            in vec2 v_TexCoord;
            in vec4 v_Color;

            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            uniform int u_EntityID;
            uniform sampler2D u_AlbedoTexture;
            uniform vec4 u_BaseColor;
            uniform float u_Tiling;

            void main() {
                vec4 texColor = texture(u_AlbedoTexture, v_TexCoord * u_Tiling);
                o_Color = texColor * v_Color * u_BaseColor;
                o_EntityID = u_EntityID;
            }
        )";
        m_UnlitShader = Shader::Create("SceneUnlitShader", pbrVertexSrc, unlitFragmentSrc);

        // Shader Blinn-Phong Clássico (Diffuse + Specular + Ambient com IBL Atmosférico)
        std::string blinnPhongFragmentSrc = R"(
            #version 460 core

            in vec3 v_FragPos;
            in vec3 v_Normal;
            in vec2 v_TexCoord;
            in vec4 v_Color;

            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            uniform int u_EntityID;
            uniform sampler2D u_AlbedoTexture;
            uniform vec4 u_BaseColor;
            uniform float u_Tiling;
            uniform float u_Shininess;

            uniform vec3 u_CameraPosition;
            uniform vec3 u_LightDirection;
            uniform vec3 u_LightColor;
            uniform float u_LightIntensity;
            uniform float u_AmbientIntensity;
            uniform float u_Time;
        )" + std::string(s_SkyNoiseGLSL) + s_SkyCelestialGLSL + s_SkyAtmosphereGLSL + R"(
            void main() {
                vec4 texColor = texture(u_AlbedoTexture, v_TexCoord * u_Tiling);
                vec3 diffuseColor = texColor.rgb * v_Color.rgb * u_BaseColor.rgb;

                vec3 N = normalize(v_Normal);
                vec3 L = normalize(u_LightDirection);
                vec3 V = normalize(u_CameraPosition - v_FragPos);
                vec3 H = normalize(L + V);

                vec3 skyAmb = EvaluateSky(N, L, u_Time, false);
                vec3 ambient = skyAmb * diffuseColor * u_AmbientIntensity;
                float diff = max(dot(N, L), 0.0);
                vec3 diffuse = diff * diffuseColor * u_LightColor * u_LightIntensity;

                float spec = pow(max(dot(N, H), 0.0), max(u_Shininess, 1.0));
                vec3 specular = spec * u_LightColor * u_LightIntensity * 0.5;

                vec3 result = ambient + diffuse + specular;
                o_Color = vec4(result, texColor.a * v_Color.a * u_BaseColor.a);
                o_EntityID = u_EntityID;
            }
        )";
        m_BlinnPhongShader = Shader::Create("SceneBlinnPhongShader", pbrVertexSrc, blinnPhongFragmentSrc);

        // Shader Wireframe
        std::string wireframeFragmentSrc = R"(
            #version 460 core

            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            uniform int u_EntityID;
            uniform vec4 u_BaseColor;

            void main() {
                o_Color = u_BaseColor;
                o_EntityID = u_EntityID;
            }
        )";
        m_WireframeShader = Shader::Create("SceneWireframeShader", pbrVertexSrc, wireframeFragmentSrc);

        m_DefaultMaterial = Material::Create("Default-Material", ShaderType::StandardPBR);

        // 5. Shader da Grade Infinita Procedural (Anti-aliased Screen-Space Grid estilo Blender/Unreal)
        std::string gridVertexSrc = R"(
            #version 460 core

            const vec3 s_GridPlaneVertices[6] = vec3[6](
                vec3( 1.0,  1.0, 0.0),
                vec3(-1.0, -1.0, 0.0),
                vec3(-1.0,  1.0, 0.0),
                vec3(-1.0, -1.0, 0.0),
                vec3( 1.0,  1.0, 0.0),
                vec3( 1.0, -1.0, 0.0)
            );

            uniform mat4 u_View;
            uniform mat4 u_Projection;

            out vec3 v_NearPoint;
            out vec3 v_FarPoint;

            vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
                mat4 viewInv = inverse(view);
                mat4 projInv = inverse(proj);
                vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
                return unprojectedPoint.xyz / unprojectedPoint.w;
            }

            void main() {
                vec3 p = s_GridPlaneVertices[gl_VertexID];
                v_NearPoint = UnprojectPoint(p.x, p.y, 0.0, u_View, u_Projection);
                v_FarPoint  = UnprojectPoint(p.x, p.y, 1.0, u_View, u_Projection);
                gl_Position = vec4(p, 1.0);
            }
        )";

        std::string gridFragmentSrc = R"(
            #version 460 core

            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            in vec3 v_NearPoint;
            in vec3 v_FarPoint;

            uniform mat4 u_View;
            uniform mat4 u_Projection;
            uniform float u_Near;
            uniform float u_Far;

            vec4 Grid(vec3 fragPos3D, float scale, bool drawAxis) {
                vec2 coord = fragPos3D.xz * scale;
                vec2 derivative = fwidth(coord);
                vec2 grid = abs(fract(coord - 0.5) - 0.5) / max(derivative, 0.00001);
                float line = min(grid.x, grid.y);
                float minimumz = min(derivative.y, 1.0);
                float minimumx = min(derivative.x, 1.0);
                vec4 color = vec4(0.28, 0.28, 0.32, 1.0 - min(line, 1.0));

                if (drawAxis) {
                    if (fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
                        color = vec4(0.25, 0.45, 0.95, 1.0);
                    if (fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
                        color = vec4(0.95, 0.30, 0.30, 1.0);
                }
                return color;
            }

            float ComputeDepth(vec3 pos) {
                vec4 clip_space_pos = u_Projection * u_View * vec4(pos, 1.0);
                return (clip_space_pos.z / clip_space_pos.w);
            }

            float ComputeLinearDepth(vec3 pos) {
                vec4 clip_space_pos = u_Projection * u_View * vec4(pos, 1.0);
                float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;
                float linearDepth = (2.0 * u_Near * u_Far) / (u_Far + u_Near - clip_space_depth * (u_Far - u_Near));
                return linearDepth / u_Far;
            }

            void main() {
                float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);
                if (t <= 0.0)
                    discard;

                vec3 fragPos3D = v_NearPoint + t * (v_FarPoint - v_NearPoint);

                gl_FragDepth = ((gl_DepthRange.diff * ComputeDepth(fragPos3D)) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;

                float linearDepth = ComputeLinearDepth(fragPos3D);
                float fading = max(0.0, (0.5 - linearDepth));

                vec4 grid1 = Grid(fragPos3D, 1.0, true);
                vec4 grid10 = Grid(fragPos3D, 0.1, true);

                vec4 finalGrid = (grid1.a > 0.0) ? grid1 : grid10;
                finalGrid.a = max(grid1.a * 0.4, grid10.a * 0.7) * fading;

                if (finalGrid.a < 0.01)
                    discard;

                o_Color = finalGrid;
                o_EntityID = -1;
            }
        )";

        m_GridShader = Shader::Create("InfiniteGridShader", gridVertexSrc, gridFragmentSrc);
        glGenVertexArrays(1, &m_GridVAO);

        // 6. Shader do Skybox Atmosférico Fotorrealista (Ciclo Dinâmico Dia/Pôr-do-sol/Noite, Estrelas, Lua, Sol e Nebulosa)
        std::string skyboxVert = R"(
            #version 460 core
            layout(location = 0) in vec3 a_Position;
            uniform mat4 u_View;
            uniform mat4 u_Projection;
            out vec3 v_TexCoord;
            void main() {
                v_TexCoord = a_Position;
                mat4 staticView = mat4(mat3(u_View));
                vec4 pos = u_Projection * staticView * vec4(a_Position, 1.0);
                gl_Position = pos.xyww;
            }
        )";

        std::string skyboxFrag = R"(
            #version 460 core
        )" + std::string(s_SkyNoiseGLSL) + s_SkyCelestialGLSL + s_SkyAtmosphereGLSL + s_SkyVolumetricCloudsGLSL + R"(
            in vec3 v_TexCoord;
            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;

            uniform vec3 u_CameraPosition;
            uniform vec3 u_SunDirection;
            uniform float u_Time;

            void main() {
                vec3 dir = normalize(v_TexCoord);
                vec3 sunDir = normalize(u_SunDirection);
                
                // 1. Atmosfera e Corpos Celestes (Sol, Lua, Estrelas e Via Láctea)
                vec3 skyBg = EvaluateSky(dir, sunDir, u_Time, true);

                // 2. Nuvens Volumétricas 3D Nativas em Coordenadas de Mundo Real
                vec4 cloudResult = RaymarchVolumetricClouds(u_CameraPosition, dir, sunDir, u_Time, skyBg);
                vec3 finalSky = cloudResult.rgb;

                // 3. Tone Mapping ACES Film Cinematográfico
                float a = 2.51;
                float b = 0.03;
                float c = 2.43;
                float d = 0.59;
                float e = 0.14;
                finalSky = clamp((finalSky * (a * finalSky + b)) / (finalSky * (c * finalSky + d) + e), 0.0, 1.0);

                // 4. Gamma Correction 2.2
                finalSky = pow(finalSky, vec3(1.0 / 2.2));

                o_Color = vec4(finalSky, 1.0);
                o_EntityID = -1;
            }
        )";
        m_SkyboxShader = Shader::Create("AtmosphericSkyboxShader", skyboxVert, skyboxFrag);

        // 7. Shader de Linhas e Geometrias de Colisores Wireframe
        std::string lineVert = R"(
            #version 460 core
            layout(location = 0) in vec3 a_Position;
            uniform mat4 u_ViewProjection;
            uniform mat4 u_Transform;
            void main() {
                gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
            }
        )";
        std::string lineFrag = R"(
            #version 460 core
            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;
            uniform vec4 u_Color;
            void main() {
                o_Color = u_Color;
                o_EntityID = -1;
            }
        )";
        m_LineShader = Shader::Create("LineWireframeShader", lineVert, lineFrag);

        // Wireframe de Box Collider (12 arestas = 24 vértices)
        float boxLines[] = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
             0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f
        };
        uint32_t boxVBO = 0;
        glGenVertexArrays(1, &m_ColliderBoxVAO);
        glGenBuffers(1, &boxVBO);
        glBindVertexArray(m_ColliderBoxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(boxLines), boxLines, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // Wireframe de Sphere Collider (3 círculos ortogonais de 32 segmentos cada)
        std::vector<float> sphereLines;
        const int segs = 32;
        for (int i = 0; i < segs; ++i) {
            float a1 = (float)i / segs * 2.0f * 3.14159265f;
            float a2 = (float)(i + 1) / segs * 2.0f * 3.14159265f;
            sphereLines.insert(sphereLines.end(), { cos(a1), sin(a1), 0.0f, cos(a2), sin(a2), 0.0f });
            sphereLines.insert(sphereLines.end(), { cos(a1), 0.0f, sin(a1), cos(a2), 0.0f, sin(a2) });
            sphereLines.insert(sphereLines.end(), { 0.0f, cos(a1), sin(a1), 0.0f, cos(a2), sin(a2) });
        }
        uint32_t sphereVBO = 0;
        glGenVertexArrays(1, &m_ColliderSphereVAO);
        glGenBuffers(1, &sphereVBO);
        glBindVertexArray(m_ColliderSphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, sphereLines.size() * sizeof(float), sphereLines.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // 8. Shader de Partículas 3D Billboard
        std::string partVert = R"(
            #version 460 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec2 a_TexCoord;
            uniform mat4 u_ViewProjection;
            uniform vec3 u_CameraRight;
            uniform vec3 u_CameraUp;
            uniform vec3 u_ParticlePos;
            uniform vec4 u_ParticleColor;
            uniform float u_ParticleSize;
            out vec2 v_TexCoord;
            out vec4 v_Color;
            void main() {
                vec3 worldPos = u_ParticlePos + (u_CameraRight * a_Position.x + u_CameraUp * a_Position.y) * u_ParticleSize;
                gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
                v_TexCoord = a_TexCoord;
                v_Color = u_ParticleColor;
            }
        )";
        std::string partFrag = R"(
            #version 460 core
            in vec2 v_TexCoord;
            in vec4 v_Color;
            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;
            void main() {
                float dist = length(v_TexCoord - vec2(0.5));
                if (dist > 0.5) discard;
                float alpha = smoothstep(0.5, 0.05, dist);
                o_Color = vec4(v_Color.rgb, v_Color.a * alpha);
                o_EntityID = -1;
            }
        )";
        m_ParticleShader = Shader::Create("ParticleBillboardShader", partVert, partFrag);

        float quadVertices[] = {
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };
        uint32_t quadVBO = 0;
        glGenVertexArrays(1, &m_ParticleVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(m_ParticleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);

        // 9. Shader e Geometria para Gizmos de Ícones 3D (Billboard Badges com Mouse Picking)
        std::string iconVert = R"(
            #version 460 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec2 a_TexCoord;
            uniform mat4 u_ViewProjection;
            uniform vec3 u_CameraRight;
            uniform vec3 u_CameraUp;
            uniform vec3 u_Position;
            uniform float u_Size;
            out vec2 v_TexCoord;
            void main() {
                vec3 worldPos = u_Position + (u_CameraRight * a_Position.x + u_CameraUp * a_Position.y) * u_Size;
                gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
                v_TexCoord = a_TexCoord;
            }
        )";
        std::string iconFrag = R"(
            #version 460 core
            in vec2 v_TexCoord;
            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;
            uniform vec4 u_Color;
            uniform int u_EntityID;
            void main() {
                vec2 uv = v_TexCoord - vec2(0.5);
                float dist = length(uv);
                if (dist > 0.48) discard;

                float border = smoothstep(0.48, 0.42, dist);
                float ring = smoothstep(0.42, 0.38, dist);
                float center = smoothstep(0.38, 0.05, dist);

                vec3 col = u_Color.rgb * center + vec3(0.08, 0.08, 0.10) * (ring - center) + vec3(0.95, 0.95, 1.0) * (border - ring);
                o_Color = vec4(col, border * 0.92);
                o_EntityID = u_EntityID;
            }
        )";
        m_GizmoIconShader = Shader::Create("GizmoIconBillboardShader", iconVert, iconFrag);
        m_GizmoIconVAO = m_ParticleVAO; // Reutiliza o quad VAO normalizado

        // 10. Wireframe de Frustum da Câmera (16 segmentos = 32 vértices)
        float nX = 0.40f, nY = 0.25f, nZ = -0.5f;
        float fX = 2.00f, fY = 1.25f, fZ = -2.8f;
        float frustumLines[] = {
            // Linhas da origem aos 4 cantos do Near Plane
            0.0f, 0.0f, 0.0f,  -nX, -nY, nZ,
            0.0f, 0.0f, 0.0f,   nX, -nY, nZ,
            0.0f, 0.0f, 0.0f,   nX,  nY, nZ,
            0.0f, 0.0f, 0.0f,  -nX,  nY, nZ,

            // Retângulo do Near Plane
            -nX, -nY, nZ,   nX, -nY, nZ,
             nX, -nY, nZ,   nX,  nY, nZ,
             nX,  nY, nZ,  -nX,  nY, nZ,
            -nX,  nY, nZ,  -nX, -nY, nZ,

            // Retângulo do Far Plane
            -fX, -fY, fZ,   fX, -fY, fZ,
             fX, -fY, fZ,   fX,  fY, fZ,
             fX,  fY, fZ,  -fX,  fY, fZ,
            -fX,  fY, fZ,  -fX, -fY, fZ,

            // Linhas conectando os 4 cantos do Near aos 4 cantos do Far
            -nX, -nY, nZ,  -fX, -fY, fZ,
             nX, -nY, nZ,   fX, -fY, fZ,
             nX,  nY, nZ,   fX,  fY, fZ,
            -nX,  nY, nZ,  -fX,  fY, fZ
        };

        uint32_t frustumVBO = 0;
        glGenVertexArrays(1, &m_CameraFrustumVAO);
        glGenBuffers(1, &frustumVBO);
        glBindVertexArray(m_CameraFrustumVAO);
        glBindBuffer(GL_ARRAY_BUFFER, frustumVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(frustumLines), frustumLines, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    Scene::~Scene() {
        StopRuntimeScripts();
        if (m_GridVAO) {
            glDeleteVertexArrays(1, &m_GridVAO);
            m_GridVAO = 0;
        }
        if (m_ColliderBoxVAO) {
            glDeleteVertexArrays(1, &m_ColliderBoxVAO);
            m_ColliderBoxVAO = 0;
        }
        if (m_ColliderSphereVAO) {
            glDeleteVertexArrays(1, &m_ColliderSphereVAO);
            m_ColliderSphereVAO = 0;
        }
        if (m_ParticleVAO) {
            glDeleteVertexArrays(1, &m_ParticleVAO);
            m_ParticleVAO = 0;
        }
        if (m_CameraFrustumVAO) {
            glDeleteVertexArrays(1, &m_CameraFrustumVAO);
            m_CameraFrustumVAO = 0;
        }
    }

    Entity Scene::CreateEntity(const std::string& name) {
        Entity entity(m_NextEntityID++, this);
        m_Entities.push_back(entity);

        entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
        entity.AddComponent<TransformComponent>();

        return entity;
    }

    Entity Scene::CreateEntityWithID(uint32_t id, const std::string& name) {
        Entity entity(id, this);
        m_Entities.push_back(entity);

        entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
        entity.AddComponent<TransformComponent>();

        if (id >= m_NextEntityID)
            m_NextEntityID = id + 1;

        return entity;
    }

    std::shared_ptr<Scene> Scene::Copy(const std::shared_ptr<Scene>& other) {
        auto newScene = std::make_shared<Scene>();
        newScene->m_NextEntityID = other->m_NextEntityID;

        for (const auto& entity : other->m_Entities) {
            uint32_t id = entity.GetID();
            Entity newEntity = newScene->CreateEntityWithID(id, "");

            if (other->HasComponent<TagComponent>(id))
                newEntity.GetComponent<TagComponent>() = other->GetComponent<TagComponent>(id);
            if (other->HasComponent<TransformComponent>(id))
                newEntity.GetComponent<TransformComponent>() = other->GetComponent<TransformComponent>(id);
            if (other->HasComponent<MeshComponent>(id))
                newEntity.AddComponent<MeshComponent>() = other->GetComponent<MeshComponent>(id);
            if (other->HasComponent<LightComponent>(id))
                newEntity.AddComponent<LightComponent>() = other->GetComponent<LightComponent>(id);
            if (other->HasComponent<CameraComponent>(id))
                newEntity.AddComponent<CameraComponent>() = other->GetComponent<CameraComponent>(id);
            if (other->HasComponent<RigidbodyComponent>(id))
                newEntity.AddComponent<RigidbodyComponent>() = other->GetComponent<RigidbodyComponent>(id);
            if (other->HasComponent<ColliderComponent>(id))
                newEntity.AddComponent<ColliderComponent>() = other->GetComponent<ColliderComponent>(id);
            if (other->HasComponent<ParticleSystemComponent>(id))
                newEntity.AddComponent<ParticleSystemComponent>() = other->GetComponent<ParticleSystemComponent>(id);
            if (other->HasComponent<AudioSourceComponent>(id))
                newEntity.AddComponent<AudioSourceComponent>() = other->GetComponent<AudioSourceComponent>(id);
            if (other->HasComponent<AudioListenerComponent>(id))
                newEntity.AddComponent<AudioListenerComponent>() = other->GetComponent<AudioListenerComponent>(id);
            if (other->HasComponent<NativeScriptComponent>(id)) {
                auto& copyNsc = newEntity.AddComponent<NativeScriptComponent>() = other->GetComponent<NativeScriptComponent>(id);
                copyNsc.Instance = nullptr; // Cada cópia de simulação instancia seu próprio script limpo
            }
        }

        return newScene;
    }

    void Scene::DestroyEntity(Entity entity) {
        uint32_t id = entity.GetID();

        auto it = std::find(m_Entities.begin(), m_Entities.end(), entity);
        if (it != m_Entities.end()) {
            m_Entities.erase(it);
        }

        m_Tags.erase(id);
        m_Transforms.erase(id);
        m_Meshes.erase(id);
        m_Lights.erase(id);
        m_Cameras.erase(id);
        m_Rigidbodies.erase(id);
        m_Colliders.erase(id);
        m_ParticleSystems.erase(id);
        m_AudioSources.erase(id);
        m_AudioListeners.erase(id);
        if (m_NativeScripts.count(id)) {
            auto& nsc = m_NativeScripts.at(id);
            if (nsc.Instance) {
                nsc.Instance->OnDestroy();
                nsc.DestroyScript(&nsc);
            }
            m_NativeScripts.erase(id);
        }

        for (auto& [childID, transform] : m_Transforms) {
            if (transform.ParentID == id) {
                transform.ParentID = 0;
            }
        }
    }

    void Scene::Clear() {
        StopRuntimeScripts();
        m_Entities.clear();
        m_Tags.clear();
        m_Transforms.clear();
        m_Meshes.clear();
        m_Lights.clear();
        m_Cameras.clear();
        m_Rigidbodies.clear();
        m_Colliders.clear();
        m_ParticleSystems.clear();
        m_AudioSources.clear();
        m_AudioListeners.clear();
        m_NativeScripts.clear();
        m_NextEntityID = 1;
    }

    void Scene::StopRuntimeScripts() {
        for (auto& [id, nsc] : m_NativeScripts) {
            if (nsc.Instance) {
                try {
                    if (nsc.WasActive) {
                        nsc.Instance->OnDisable();
                        nsc.WasActive = false;
                    }
                    nsc.Instance->OnDestroy();
                } catch (...) {}
                if (nsc.DestroyScript) {
                    nsc.DestroyScript(&nsc);
                } else {
                    delete nsc.Instance;
                }
                nsc.Instance = nullptr;
            }
        }
    }

    void Scene::OnUpdate(Timestep ts) {
        float dt = ts.GetSeconds();
        if (dt <= 0.0f || dt > 0.1f) dt = 0.01667f;

        // Atualização de Partículas (Simulação Contínua)
        for (auto& [id, psc] : m_ParticleSystems) {
            if (!psc.IsPlaying) continue;
            glm::vec3 emitterPos = m_Transforms.count(id) ? m_Transforms.at(id).Translation : glm::vec3(0.0f);

            psc.TimeSinceLastEmit += dt;
            float emitInterval = 1.0f / std::max(psc.EmissionRate, 0.1f);
            while (psc.TimeSinceLastEmit >= emitInterval && psc.Pool.size() < (size_t)psc.MaxParticles) {
                psc.TimeSinceLastEmit -= emitInterval;
                Particle p;
                p.Position = emitterPos;
                float rx = ((rand() % 1000) / 500.0f - 1.0f) * 0.4f;
                float ry = ((rand() % 1000) / 1000.0f) * 0.6f;
                float rz = ((rand() % 1000) / 500.0f - 1.0f) * 0.4f;
                p.Velocity = psc.BaseVelocity + glm::vec3(rx, ry, rz);
                p.Color = psc.StartColor;
                p.Size = psc.StartSize;
                p.Lifetime = psc.Lifetime;
                p.RemainingLifetime = psc.Lifetime;
                psc.Pool.push_back(p);
            }

            for (auto it = psc.Pool.begin(); it != psc.Pool.end(); ) {
                it->RemainingLifetime -= dt;
                if (it->RemainingLifetime <= 0.0f) {
                    it = psc.Pool.erase(it);
                    continue;
                }

                it->Velocity += psc.Gravity * dt;
                it->Position += it->Velocity * dt;

                float life = 1.0f - (it->RemainingLifetime / it->Lifetime);
                it->Color = glm::mix(psc.StartColor, psc.EndColor, life);
                it->Size = glm::mix(psc.StartSize, psc.EndSize, life);
                ++it;
            }
        }
    }

    void Scene::OnUpdateRuntime(Timestep ts) {
        float dt = ts.GetSeconds();
        if (dt <= 0.0f || dt > 0.1f) dt = 0.01667f;

        // ---------------------------------------------------------------------
        // 0. Instanciação e Ciclo de Vida de Scripts C++ Nativos (OnInit, OnEnable, OnTick, OnDisable)
        // ---------------------------------------------------------------------
        for (auto& [id, nsc] : m_NativeScripts) {
            bool isActive = m_Tags.count(id) ? m_Tags.at(id).IsActive : true;

            if (!nsc.Instance && nsc.InstantiateScript) {
                try {
                    nsc.Instance = nsc.InstantiateScript();
                    if (nsc.Instance) {
                        nsc.Instance->m_Entity = Entity(id, this);
                        nsc.Instance->OnInit();
                        nsc.Instance->OnCreate();
                        if (isActive) {
                            nsc.Instance->OnEnable();
                            nsc.WasActive = true;
                        }
                    } else {
                        static std::unordered_map<std::string, bool> s_Warned;
                        if (!s_Warned[nsc.ScriptName]) {
                            OVERFLOW_WARN("Script '{0}' anexado a entidade #{1}, mas ainda nao compilado! Compile no Rider ou aperte F5 para Hot-Reload.", nsc.ScriptName, id);
                            s_Warned[nsc.ScriptName] = true;
                        }
                    }
                } catch (const std::exception& e) {
                    OVERFLOW_ERROR("Erro ao instanciar script '{0}': {1}", nsc.ScriptName, e.what());
                    nsc.Instance = nullptr;
                } catch (...) {
                    OVERFLOW_ERROR("Erro desconhecido ao instanciar script '{0}'", nsc.ScriptName);
                    nsc.Instance = nullptr;
                }
            }

            if (nsc.Instance) {
                // Transição de Ativação / Desativação
                if (isActive && !nsc.WasActive) {
                    try { nsc.Instance->OnEnable(); } catch (...) {}
                    nsc.WasActive = true;
                } else if (!isActive && nsc.WasActive) {
                    try { nsc.Instance->OnDisable(); } catch (...) {}
                    nsc.WasActive = false;
                }

                // Loop de Quadro (OnTick) apenas se a entidade estiver ativa
                if (isActive) {
                    try {
                        nsc.Instance->OnTick(ts);
                    } catch (const std::exception& e) {
                        OVERFLOW_ERROR("Erro em OnTick do script '{0}': {1}", nsc.ScriptName, e.what());
                    } catch (...) {
                        OVERFLOW_ERROR("Erro desconhecido em OnTick do script '{0}'", nsc.ScriptName);
                    }
                }
            }
        }

        // Posição de escuta do áudio (AudioListener ou padrão)
        glm::vec3 listenerPos(0.0f, 2.0f, 5.0f);
        for (const auto& [lid, lis] : m_AudioListeners) {
            if (m_Transforms.count(lid)) {
                listenerPos = m_Transforms.at(lid).Translation;
                break;
            }
        }

        // ---------------------------------------------------------------------
        // 1. Integração da Física 3D (Semi-Implicit Euler Integrator)
        // ---------------------------------------------------------------------
        const glm::vec3 gravity(0.0f, -9.81f, 0.0f);

        for (auto& [id, rb] : m_Rigidbodies) {
            if (!m_Transforms.count(id)) continue;
            auto& tc = m_Transforms.at(id);

            if (rb.Type != BodyType::Dynamic || rb.IsKinematic) continue;

            // Aplica Gravidade
            if (rb.UseGravity) {
                rb.LinearVelocity += gravity * dt;
            }

            // Aplica Drag Linear & Angular
            rb.LinearVelocity *= std::clamp(1.0f - (rb.LinearDrag * dt), 0.0f, 1.0f);
            rb.AngularVelocity *= std::clamp(1.0f - (rb.AngularDrag * dt), 0.0f, 1.0f);

            // Constraints de Travamento de Eixos
            if (rb.FreezePositionX) rb.LinearVelocity.x = 0.0f;
            if (rb.FreezePositionY) rb.LinearVelocity.y = 0.0f;
            if (rb.FreezePositionZ) rb.LinearVelocity.z = 0.0f;

            // Atualiza Posição Linear
            tc.Translation += rb.LinearVelocity * dt;

            // Atualiza Rotação Angular
            if (!rb.FreezeRotationX) tc.Rotation.x += glm::degrees(rb.AngularVelocity.x) * dt;
            if (!rb.FreezeRotationY) tc.Rotation.y += glm::degrees(rb.AngularVelocity.y) * dt;
            if (!rb.FreezeRotationZ) tc.Rotation.z += glm::degrees(rb.AngularVelocity.z) * dt;
        }

        // ---------------------------------------------------------------------
        // 2. Detecção e Resolução de Colisões (Chão e Entidades)
        // ---------------------------------------------------------------------
        // A. Colisão com o Chão (Plano Y = 0)
        for (auto& [id, rb] : m_Rigidbodies) {
            if (!m_Transforms.count(id)) continue;
            auto& tc = m_Transforms.at(id);

            float bounciness = 0.5f;
            float friction = 0.3f;
            float bottomY = tc.Translation.y;

            if (m_Colliders.count(id)) {
                const auto& col = m_Colliders.at(id);
                bounciness = col.Bounciness;
                friction = col.Friction;
                if (col.Type == ColliderType::Sphere) {
                    bottomY = tc.Translation.y + col.Center.y - (col.Radius * tc.Scale.y);
                } else {
                    bottomY = tc.Translation.y + col.Center.y - (col.Size.y * 0.5f * tc.Scale.y);
                }
            } else {
                bottomY = tc.Translation.y - (0.5f * tc.Scale.y);
            }

            if (bottomY < 0.0f && rb.LinearVelocity.y < 0.0f) {
                // Áudio físico de impacto no chão
                float impactVel = std::abs(rb.LinearVelocity.y);
                if (impactVel > 1.2f) {
                    float intensity = std::clamp(impactVel / 8.0f, 0.2f, 1.0f);
                    AudioEngine::PlayCollisionSound(intensity, tc.Translation, listenerPos);
                }

                // Correção de penetração
                float penetration = -bottomY;
                tc.Translation.y += penetration;

                // Quique elástico (Restituição)
                rb.LinearVelocity.y = -rb.LinearVelocity.y * bounciness;

                // Atrito no plano XZ
                rb.LinearVelocity.x *= std::clamp(1.0f - friction, 0.0f, 1.0f);
                rb.LinearVelocity.z *= std::clamp(1.0f - friction, 0.0f, 1.0f);

                // Estabilização para repouso
                if (std::abs(rb.LinearVelocity.y) < 0.25f) {
                    rb.LinearVelocity.y = 0.0f;
                }
            }
        }

        // B. Colisão Dinâmica entre Entidades (Esfera-Esfera)
        auto entityIDs = m_Entities;
        for (size_t i = 0; i < entityIDs.size(); ++i) {
            uint32_t idA = entityIDs[i].GetID();
            if (!m_Transforms.count(idA) || !m_Rigidbodies.count(idA)) continue;
            auto& tcA = m_Transforms.at(idA);
            auto& rbA = m_Rigidbodies.at(idA);

            for (size_t j = i + 1; j < entityIDs.size(); ++j) {
                uint32_t idB = entityIDs[j].GetID();
                if (!m_Transforms.count(idB) || !m_Rigidbodies.count(idB)) continue;
                auto& tcB = m_Transforms.at(idB);
                auto& rbB = m_Rigidbodies.at(idB);

                if (rbA.Type != BodyType::Dynamic && rbB.Type != BodyType::Dynamic) continue;

                glm::vec3 centerA = tcA.Translation;
                glm::vec3 centerB = tcB.Translation;
                float radiusA = 0.5f * std::max({ tcA.Scale.x, tcA.Scale.y, tcA.Scale.z });
                float radiusB = 0.5f * std::max({ tcB.Scale.x, tcB.Scale.y, tcB.Scale.z });
                float bounciness = 0.5f;

                if (m_Colliders.count(idA)) {
                    centerA += m_Colliders.at(idA).Center;
                    if (m_Colliders.at(idA).Type == ColliderType::Sphere)
                        radiusA = m_Colliders.at(idA).Radius * std::max({ tcA.Scale.x, tcA.Scale.y, tcA.Scale.z });
                    bounciness = m_Colliders.at(idA).Bounciness;
                }
                if (m_Colliders.count(idB)) {
                    centerB += m_Colliders.at(idB).Center;
                    if (m_Colliders.at(idB).Type == ColliderType::Sphere)
                        radiusB = m_Colliders.at(idB).Radius * std::max({ tcB.Scale.x, tcB.Scale.y, tcB.Scale.z });
                }

                glm::vec3 delta = centerA - centerB;
                float dist = glm::length(delta);
                float minDist = radiusA + radiusB;

                if (dist < minDist && dist > 0.0001f) {
                    glm::vec3 normal = delta / dist;
                    float penetration = minDist - dist;

                    glm::vec3 relVel = rbA.LinearVelocity - rbB.LinearVelocity;
                    float velAlongNormal = glm::dot(relVel, normal);

                    if (std::abs(velAlongNormal) > 0.6f) {
                        float intensity = std::clamp(std::abs(velAlongNormal) / 8.0f, 0.2f, 1.0f);
                        AudioEngine::PlayCollisionSound(intensity, (centerA + centerB) * 0.5f, listenerPos);
                    }

                    if (rbA.Type == BodyType::Dynamic && rbB.Type == BodyType::Dynamic) {
                        tcA.Translation += normal * (penetration * 0.5f);
                        tcB.Translation -= normal * (penetration * 0.5f);

                        if (velAlongNormal < 0.0f) {
                            float impulse = -(1.0f + bounciness) * velAlongNormal * 0.5f;
                            rbA.LinearVelocity += normal * impulse;
                            rbB.LinearVelocity -= normal * impulse;
                        }
                    } else if (rbA.Type == BodyType::Dynamic) {
                        tcA.Translation += normal * penetration;
                        if (velAlongNormal < 0.0f) {
                            rbA.LinearVelocity -= (1.0f + bounciness) * velAlongNormal * normal;
                        }
                    } else if (rbB.Type == BodyType::Dynamic) {
                        tcB.Translation -= normal * penetration;
                        float velAlongNormalB = glm::dot(rbB.LinearVelocity, -normal);
                        if (velAlongNormalB < 0.0f) {
                            rbB.LinearVelocity -= (1.0f + bounciness) * velAlongNormalB * (-normal);
                        }
                    }
                }
            }
        }

        // Rotação suave de nós filhos sem física
        for (auto& [id, transform] : m_Transforms) {
            if (transform.ParentID != 0 && !m_Rigidbodies.count(id)) {
                transform.Rotation.y += 30.0f * dt;
            }
        }

        // Atualiza a simulação de partículas
        OnUpdate(ts);
    }

    void Scene::OnRenderEditor(const PerspectiveCamera& camera, const std::shared_ptr<Framebuffer>& targetFramebuffer, const std::shared_ptr<Shader>& customShader) {
        // ---------------------------------------------------------------------
        // 1. Busca dados da Luz Direcional principal
        // ---------------------------------------------------------------------
        glm::vec3 lightDir = { 0.6f, 1.0f, 0.4f };
        glm::vec3 lightColor = { 1.0f, 0.98f, 0.92f };
        float lightIntensity = 2.5f;
        float ambientIntensity = 0.15f;
        float shadowBias = 0.003f;
        bool hasShadows = false;

        for (const auto& [id, light] : m_Lights) {
            if (light.Type == LightType::Directional) {
                if (m_Transforms.count(id)) {
                    const auto& trans = m_Transforms.at(id);
                    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.x), { 1.0f, 0.0f, 0.0f })
                                  * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.y), { 0.0f, 1.0f, 0.0f })
                                  * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.z), { 0.0f, 0.0f, 1.0f });
                    lightDir = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
                }
                lightColor = light.Color;
                lightIntensity = light.Intensity;
                ambientIntensity = light.AmbientIntensity;
                shadowBias = light.Bias;
                hasShadows = light.CastShadows;
                break;
            }
        }

        float time = static_cast<float>(glfwGetTime());

        // Adaptação física cinematográfica do Sol e da Lua
        glm::vec3 effectiveLightDir = lightDir;
        glm::vec3 effectiveLightColor = lightColor;
        float effectiveLightIntensity = lightIntensity;

        float sunElev = lightDir.y;
        if (sunElev < -0.05f) {
            // Noite Cósmica: A Lua assume como a fonte primária de luz direcional
            glm::vec3 moonDir = glm::normalize(-lightDir + glm::vec3(0.05f, 0.18f, -0.04f));
            effectiveLightDir = moonDir;
            effectiveLightColor = glm::vec3(0.68f, 0.78f, 0.96f); // Luz prateada suave da Lua
            effectiveLightIntensity = lightIntensity * 0.35f;
        } else if (sunElev < 0.25f) {
            // Pôr do Sol / Amanhecer (Golden Hour): tons dourados e avermelhados intensos
            float sunsetAlpha = std::clamp((0.25f - sunElev) / 0.30f, 0.0f, 1.0f);
            glm::vec3 goldenSunset = glm::vec3(1.0f, 0.52f, 0.18f);
            effectiveLightColor = glm::mix(lightColor, goldenSunset, sunsetAlpha);
            effectiveLightIntensity = lightIntensity * glm::mix(1.0f, 0.65f, sunsetAlpha);
        }

        // ---------------------------------------------------------------------
        // PASSO 1: Passe de Sombra (Renderização da Profundidade no ShadowMap)
        // ---------------------------------------------------------------------
        if (hasShadows && m_ShadowMap) {
            m_ShadowMap->CalculateLightSpaceMatrix(effectiveLightDir);
            m_ShadowMap->Bind();

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT); // Front-face culling elimina shadow acne perfeitamente

            m_ShadowShader->Bind();
            m_ShadowShader->SetMat4("u_LightSpaceMatrix", m_ShadowMap->GetLightSpaceMatrix());

            for (const auto& [id, mesh] : m_Meshes) {
                if (!m_Transforms.count(id) || !mesh.ModelAsset)
                    continue;

                if (m_Tags.count(id) && !m_Tags.at(id).IsActive)
                    continue;

                const auto& transform = m_Transforms.at(id);
                glm::mat4 worldTransform = transform.GetWorldTransform(this);

                m_ShadowShader->SetMat4("u_Transform", worldTransform);
                mesh.ModelAsset->Draw(m_ShadowShader);
            }

            glDisable(GL_CULL_FACE);
            m_ShadowMap->Unbind();
        }

        // ---------------------------------------------------------------------
        // PASSO 2: Passe de Iluminação PBR (Cook-Torrance GGX + Sombras + HDR)
        // ---------------------------------------------------------------------
        if (targetFramebuffer) {
            targetFramebuffer->Bind();
            targetFramebuffer->ClearAttachment(1, -1);
        }

        RenderCommand::SetClearColor({ 0.10f, 0.10f, 0.12f, 1.0f });
        RenderCommand::Clear();

        // ---------------------------------------------------------------------
        // PASSO 2A: Skybox Atmosférico Procedural
        // ---------------------------------------------------------------------
        if (m_SkyboxShader && m_SharedCube) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            m_SkyboxShader->Bind();
            m_SkyboxShader->SetMat4("u_View", camera.GetViewMatrix());
            m_SkyboxShader->SetMat4("u_Projection", camera.GetProjection());
            m_SkyboxShader->SetFloat3("u_SunDirection", lightDir);
            m_SkyboxShader->SetFloat3("u_CameraPosition", camera.GetPosition());
            m_SkyboxShader->SetFloat("u_Time", time);
            m_SkyboxShader->SetInt("u_EnableClouds", m_VolumetricCloudsEnabled ? 1 : 0);
            m_SkyboxShader->SetFloat("u_CloudBase", m_CloudBaseAltitude);
            m_SkyboxShader->SetFloat("u_CloudTop", m_CloudTopAltitude);
            m_SkyboxShader->SetFloat("u_CloudDensityMult", m_CloudDensityMultiplier);
            m_SkyboxShader->SetFloat("u_CloudCoverage", m_CloudCoverage);
            m_SkyboxShader->SetFloat("u_WindSpeed", m_WindSpeed);
            m_SharedCube->Draw(m_SkyboxShader);
            glDepthFunc(GL_LESS);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        std::shared_ptr<Shader> activeShader = customShader ? customShader : m_PBRShader;
        activeShader->Bind();

        activeShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
        activeShader->SetMat4("u_LightSpaceMatrix", m_ShadowMap ? m_ShadowMap->GetLightSpaceMatrix() : glm::mat4(1.0f));
        activeShader->SetFloat3("u_CameraPosition", camera.GetPosition());

        activeShader->SetFloat3("u_LightDirection", effectiveLightDir);
        activeShader->SetFloat3("u_LightColor", effectiveLightColor);
        activeShader->SetFloat("u_LightIntensity", effectiveLightIntensity);
        activeShader->SetFloat("u_AmbientIntensity", ambientIntensity);
        activeShader->SetFloat("u_ShadowBias", shadowBias);
        activeShader->SetFloat("u_Exposure", m_Exposure);
        activeShader->SetFloat("u_Vignette", m_Vignette);
        activeShader->SetFloat("u_Time", time);

        // Upload de Múltiplas Luzes (Point Lights e Spot Lights)
        int pointLightCount = 0;
        int spotLightCount = 0;

        for (const auto& [id, light] : m_Lights) {
            glm::vec3 lightPos = m_Transforms.count(id) ? m_Transforms.at(id).Translation : glm::vec3(0.0f);

            if (light.Type == LightType::Point && pointLightCount < 8) {
                std::string prefix = "u_PointLights[" + std::to_string(pointLightCount) + "].";
                activeShader->SetFloat3(prefix + "position", lightPos);
                activeShader->SetFloat3(prefix + "color", light.Color);
                activeShader->SetFloat(prefix + "intensity", light.Intensity);
                activeShader->SetFloat(prefix + "range", light.Range);
                pointLightCount++;
            }
            else if (light.Type == LightType::Spot && spotLightCount < 4) {
                glm::vec3 spotDir = { 0.0f, -1.0f, 0.0f };
                if (m_Transforms.count(id)) {
                    const auto& trans = m_Transforms.at(id);
                    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.x), { 1.0f, 0.0f, 0.0f })
                                  * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.y), { 0.0f, 1.0f, 0.0f })
                                  * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.z), { 0.0f, 0.0f, 1.0f });
                    spotDir = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));
                }

                std::string prefix = "u_SpotLights[" + std::to_string(spotLightCount) + "].";
                activeShader->SetFloat3(prefix + "position", lightPos);
                activeShader->SetFloat3(prefix + "direction", spotDir);
                activeShader->SetFloat3(prefix + "color", light.Color);
                activeShader->SetFloat(prefix + "intensity", light.Intensity);
                activeShader->SetFloat(prefix + "range", light.Range);
                activeShader->SetFloat(prefix + "innerCutoff", glm::cos(glm::radians(light.InnerAngle)));
                activeShader->SetFloat(prefix + "outerCutoff", glm::cos(glm::radians(light.OuterAngle)));
                spotLightCount++;
            }
        }

        activeShader->SetInt("u_PointLightCount", pointLightCount);
        activeShader->SetInt("u_SpotLightCount", spotLightCount);

        activeShader->SetInt("u_AlbedoTexture", 0);
        activeShader->SetInt("u_ShadowMap", 1);

        if (m_ShadowMap) {
            m_ShadowMap->BindTexture(1);
        }

        Renderer::BeginScene(camera);

        for (const auto& [id, mesh] : m_Meshes) {
            if (!m_Transforms.count(id) || !mesh.ModelAsset)
                continue;

            if (m_Tags.count(id) && !m_Tags.at(id).IsActive)
                continue;

            const auto& transform = m_Transforms.at(id);
            glm::mat4 worldTransform = transform.GetWorldTransform(this);

            auto material = mesh.MaterialAsset ? mesh.MaterialAsset : m_DefaultMaterial;
            if (!mesh.MaterialAsset) {
                m_DefaultMaterial->AlbedoColor = mesh.Color;
                m_DefaultMaterial->AlbedoTexture = mesh.TextureAsset;
                m_DefaultMaterial->Metallic = mesh.Metallic;
                m_DefaultMaterial->Roughness = mesh.Roughness;
                m_DefaultMaterial->AO = mesh.AO;
                m_DefaultMaterial->Tiling = mesh.Tiling;
                m_DefaultMaterial->NormalMap = mesh.NormalMapAsset;
                m_DefaultMaterial->UseNormalMap = mesh.UseNormalMap;
                m_DefaultMaterial->EmissionColor = mesh.EmissionColor;
                m_DefaultMaterial->EmissionIntensity = mesh.EmissionIntensity;
            }

            std::shared_ptr<Shader> matShader = customShader ? customShader : GetShaderForType(material->Type);
            matShader->Bind();

            matShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
            matShader->SetMat4("u_LightSpaceMatrix", m_ShadowMap ? m_ShadowMap->GetLightSpaceMatrix() : glm::mat4(1.0f));
            matShader->SetFloat3("u_CameraPosition", camera.GetPosition());

            matShader->SetFloat3("u_LightDirection", effectiveLightDir);
            matShader->SetFloat3("u_LightColor", effectiveLightColor);
            matShader->SetFloat("u_LightIntensity", effectiveLightIntensity);
            matShader->SetFloat("u_AmbientIntensity", ambientIntensity);
            matShader->SetFloat("u_ShadowBias", shadowBias);
            matShader->SetFloat("u_Exposure", m_Exposure);
            matShader->SetFloat("u_Vignette", m_Vignette);
            matShader->SetFloat("u_Time", time);

            matShader->SetInt("u_PointLightCount", pointLightCount);
            matShader->SetInt("u_SpotLightCount", spotLightCount);

            matShader->SetInt("u_AlbedoTexture", 0);
            matShader->SetInt("u_ShadowMap", 1);
            if (m_ShadowMap) {
                m_ShadowMap->BindTexture(1);
            }

            matShader->SetMat4("u_Transform", worldTransform);
            matShader->SetInt("u_EntityID", static_cast<int>(id));

            material->Bind(matShader, m_WhiteTexture);

            if (material->Type == ShaderType::Wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                mesh.ModelAsset->Draw(matShader);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            } else {
                mesh.ModelAsset->Draw(matShader);
            }
        }

        Renderer::EndScene();

        // ---------------------------------------------------------------------
        // PASSO 3: Grade Infinita Procedural (Estilo Blender / Unreal)
        // ---------------------------------------------------------------------
        if (m_ShowGrid && m_GridShader && m_GridVAO) {
            m_GridShader->Bind();
            m_GridShader->SetMat4("u_View", camera.GetViewMatrix());
            m_GridShader->SetMat4("u_Projection", camera.GetProjection());
            m_GridShader->SetFloat("u_Near", camera.GetNearClip());
            m_GridShader->SetFloat("u_Far", camera.GetFarClip());

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glBindVertexArray(m_GridVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }

        // ---------------------------------------------------------------------
        // PASSO 4: Partículas 3D Billboard
        // ---------------------------------------------------------------------
        if (m_ParticleShader && m_ParticleVAO) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending cinematográfico
            glDepthMask(GL_FALSE);

            m_ParticleShader->Bind();
            m_ParticleShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
            m_ParticleShader->SetFloat3("u_CameraRight", camera.GetRight());
            m_ParticleShader->SetFloat3("u_CameraUp", camera.GetUp());

            glBindVertexArray(m_ParticleVAO);
            for (const auto& [id, psc] : m_ParticleSystems) {
                for (const auto& p : psc.Pool) {
                    m_ParticleShader->SetFloat3("u_ParticlePos", p.Position);
                    m_ParticleShader->SetFloat4("u_ParticleColor", p.Color);
                    m_ParticleShader->SetFloat("u_ParticleSize", p.Size);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            }
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        // ---------------------------------------------------------------------
        // PASSO 5: Wireframe Collider Visualizer
        // ---------------------------------------------------------------------
        if (m_ShowColliders && m_LineShader && m_SelectedEntityID != 0 && m_Colliders.count(m_SelectedEntityID)) {
            const auto& col = m_Colliders.at(m_SelectedEntityID);
            if (m_Transforms.count(m_SelectedEntityID)) {
                const auto& trans = m_Transforms.at(m_SelectedEntityID);
                m_LineShader->Bind();
                m_LineShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
                m_LineShader->SetFloat4("u_Color", { 0.2f, 0.95f, 0.25f, 1.0f }); // Verde limão vívido

                glm::mat4 tMat = glm::translate(glm::mat4(1.0f), trans.Translation + col.Center)
                               * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.x), { 1.0f, 0.0f, 0.0f })
                               * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.y), { 0.0f, 1.0f, 0.0f })
                               * glm::rotate(glm::mat4(1.0f), glm::radians(trans.Rotation.z), { 0.0f, 0.0f, 1.0f });

                if (col.Type == ColliderType::Box && m_ColliderBoxVAO) {
                    glm::mat4 colTransform = tMat * glm::scale(glm::mat4(1.0f), trans.Scale * col.Size);
                    m_LineShader->SetMat4("u_Transform", colTransform);
                    glBindVertexArray(m_ColliderBoxVAO);
                    glDrawArrays(GL_LINES, 0, 24);
                    glBindVertexArray(0);
                } else if (col.Type == ColliderType::Sphere && m_ColliderSphereVAO) {
                    float maxScale = std::max({ trans.Scale.x, trans.Scale.y, trans.Scale.z });
                    glm::mat4 colTransform = tMat * glm::scale(glm::mat4(1.0f), glm::vec3(maxScale * col.Radius));
                    m_LineShader->SetMat4("u_Transform", colTransform);
                    glBindVertexArray(m_ColliderSphereVAO);
                    glDrawArrays(GL_LINES, 0, 192);
                    glBindVertexArray(0);
                }
            }
        }

        // ---------------------------------------------------------------------
        // PASSO 6: Frustum da Câmera (Pirâmide de Visão Wireframe estilo Unity/Unreal)
        // ---------------------------------------------------------------------
        if (m_ShowCameraFrustums && m_LineShader && m_CameraFrustumVAO) {
            glEnable(GL_DEPTH_TEST);
            m_LineShader->Bind();
            m_LineShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());

            for (const auto& [id, camComp] : m_Cameras) {
                if (!m_Transforms.count(id)) continue;
                const auto& trans = m_Transforms.at(id);

                bool isSelected = (id == m_SelectedEntityID);
                glm::vec4 frustumColor = isSelected ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.20f, 0.82f, 1.0f, 0.70f);
                m_LineShader->SetFloat4("u_Color", frustumColor);

                glm::mat4 camWorld = trans.GetWorldTransform(this);
                m_LineShader->SetMat4("u_Transform", camWorld);

                glBindVertexArray(m_CameraFrustumVAO);
                glDrawArrays(GL_LINES, 0, 32);
                glBindVertexArray(0);
            }
        }

        // ---------------------------------------------------------------------
        // PASSO 7: Gizmos de Alcance de Luzes (Point/Spot) e Fontes de Áudio
        // ---------------------------------------------------------------------
        if (m_SelectedEntityID != 0 && m_LineShader) {
            if (m_Lights.count(m_SelectedEntityID) && m_Transforms.count(m_SelectedEntityID)) {
                const auto& light = m_Lights.at(m_SelectedEntityID);
                const auto& trans = m_Transforms.at(m_SelectedEntityID);
                glm::mat4 tMat = glm::translate(glm::mat4(1.0f), trans.Translation);

                if (light.Type == LightType::Point && m_ColliderSphereVAO) {
                    m_LineShader->Bind();
                    m_LineShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
                    m_LineShader->SetFloat4("u_Color", { 1.0f, 0.80f, 0.20f, 0.50f }); // Dourado suave
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(light.Range));
                    m_LineShader->SetMat4("u_Transform", tMat * scaleMat);
                    glBindVertexArray(m_ColliderSphereVAO);
                    glDrawArrays(GL_LINES, 0, 192);
                    glBindVertexArray(0);
                }
            }

            if (m_AudioSources.count(m_SelectedEntityID) && m_Transforms.count(m_SelectedEntityID) && m_ColliderSphereVAO) {
                const auto& as = m_AudioSources.at(m_SelectedEntityID);
                const auto& trans = m_Transforms.at(m_SelectedEntityID);
                m_LineShader->Bind();
                m_LineShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
                m_LineShader->SetFloat4("u_Color", { 0.15f, 0.90f, 0.65f, 0.50f }); // Verde água
                glm::mat4 tMat = glm::translate(glm::mat4(1.0f), trans.Translation) * glm::scale(glm::mat4(1.0f), glm::vec3(as.MaxDistance));
                m_LineShader->SetMat4("u_Transform", tMat);
                glBindVertexArray(m_ColliderSphereVAO);
                glDrawArrays(GL_LINES, 0, 192);
                glBindVertexArray(0);
            }
        }

        // ---------------------------------------------------------------------
        // PASSO 8: Ícones 3D na Scene View (Luzes, Câmeras, Áudio, Partículas)
        // Permite clicar diretamente no ícone da câmera ou da luz para selecionar!
        // ---------------------------------------------------------------------
        if (m_ShowGizmoIcons && m_GizmoIconShader && m_GizmoIconVAO) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);

            m_GizmoIconShader->Bind();
            m_GizmoIconShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
            m_GizmoIconShader->SetFloat3("u_CameraRight", camera.GetRight());
            m_GizmoIconShader->SetFloat3("u_CameraUp", camera.GetUp());

            glBindVertexArray(m_GizmoIconVAO);

            for (const auto& entity : m_Entities) {
                uint32_t id = entity.GetID();
                if (!m_Transforms.count(id)) continue;
                const auto& trans = m_Transforms.at(id);

                bool hasLight = m_Lights.count(id);
                bool hasCam = m_Cameras.count(id);
                bool hasAudio = m_AudioSources.count(id);
                bool hasParticles = m_ParticleSystems.count(id);

                if (!hasLight && !hasCam && !hasAudio && !hasParticles) continue;

                glm::vec4 iconColor{ 1.0f, 1.0f, 1.0f, 1.0f };
                float iconSize = 0.50f;

                if (hasCam) {
                    iconColor = { 0.15f, 0.85f, 1.0f, 1.0f }; // Ciano Câmera
                } else if (hasLight) {
                    auto lType = m_Lights.at(id).Type;
                    if (lType == LightType::Directional) iconColor = { 1.0f, 0.88f, 0.25f, 1.0f }; // Sol Dourado
                    else if (lType == LightType::Point) iconColor = { 1.0f, 0.55f, 0.15f, 1.0f };  // Lâmpada Laranja
                    else iconColor = { 0.25f, 0.65f, 1.0f, 1.0f };                                 // Spot Azul
                } else if (hasAudio) {
                    iconColor = { 0.2f, 0.95f, 0.5f, 1.0f };  // Verde Áudio
                } else if (hasParticles) {
                    iconColor = { 0.95f, 0.25f, 0.85f, 1.0f }; // Magenta Partículas
                }

                if (id == m_SelectedEntityID) {
                    iconSize = 0.65f;
                }

                m_GizmoIconShader->SetFloat3("u_Position", trans.Translation);
                m_GizmoIconShader->SetFloat("u_Size", iconSize);
                m_GizmoIconShader->SetFloat4("u_Color", iconColor);
                m_GizmoIconShader->SetInt("u_EntityID", static_cast<int>(id)); // Grava no FBO para clique direto!

                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glBindVertexArray(0);
            glDepthFunc(GL_LESS);
        }

        if (targetFramebuffer) {
            targetFramebuffer->Unbind();
        }
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height) {
        for (auto& [id, cameraComp] : m_Cameras) {
            cameraComp.Camera.SetViewportSize(width, height);
        }
    }

    Entity Scene::CreateCube(const std::string& name, const glm::vec3& position) {
        Entity entity = CreateEntity(name);
        entity.GetComponent<TransformComponent>().Translation = position;

        auto& mesh = entity.AddComponent<MeshComponent>(MeshGeometryType::Cube);
        mesh.ModelAsset = m_SharedCube;
        mesh.Roughness = 0.35f;
        mesh.Metallic = 0.0f;
        return entity;
    }

    Entity Scene::CreateSphere(const std::string& name, const glm::vec3& position) {
        Entity entity = CreateEntity(name);
        entity.GetComponent<TransformComponent>().Translation = position;

        auto& mesh = entity.AddComponent<MeshComponent>(MeshGeometryType::Sphere);
        mesh.ModelAsset = m_SharedSphere;
        mesh.Roughness = 0.2f;
        mesh.Metallic = 0.8f; // Metalizado por padrão para mostrar reflexos Cook-Torrance incríveis!
        return entity;
    }

    Entity Scene::CreatePlane(const std::string& name, const glm::vec3& position) {
        Entity entity = CreateEntity(name);
        entity.GetComponent<TransformComponent>().Translation = position;

        auto& mesh = entity.AddComponent<MeshComponent>(MeshGeometryType::Plane);
        mesh.ModelAsset = m_SharedPlane;
        mesh.Roughness = 0.8f;
        mesh.Metallic = 0.0f;
        return entity;
    }

    Entity Scene::CreateDirectionalLight(const std::string& name, const glm::vec3& /*dir*/) {
        Entity entity = CreateEntity(name);
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 6.0f, 0.0f };
        entity.GetComponent<TransformComponent>().Rotation = { 50.0f, -35.0f, 0.0f };

        entity.AddComponent<LightComponent>();
        return entity;
    }

    Entity Scene::CreateCamera(const std::string& name, const glm::vec3& position) {
        Entity entity = CreateEntity(name);
        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Translation = position;
        tc.Rotation = { 0.0f, 0.0f, 0.0f };

        auto& cam = entity.AddComponent<CameraComponent>();
        cam.Primary = true;
        cam.Camera.SetPosition(position);
        cam.Camera.SetRotation(0.0f, -90.0f);
        return entity;
    }

    Entity Scene::CreateTerrain(const std::string& name, int resolution, float size, float height) {
        Entity entity = CreateEntity(name);
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 0.0f };

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfSize = size * 0.5f;
        float step = size / (resolution - 1);

        for (int z = 0; z < resolution; ++z) {
            for (int x = 0; x < resolution; ++x) {
                float posX = -halfSize + x * step;
                float posZ = -halfSize + z * step;

                float nx = posX * 0.25f;
                float nz = posZ * 0.25f;
                float elevation = (std::sin(nx) * std::cos(nz) + std::sin(nx * 2.1f + 1.2f) * 0.4f + std::cos(nz * 1.8f) * 0.3f) * height;

                Vertex v;
                v.Position = { posX, elevation, posZ };
                float dYdX = (std::cos(nx) * 0.25f * std::cos(nz)) * height;
                float dYdZ = (-std::sin(nx) * std::sin(nz) * 0.25f) * height;
                v.Normal = glm::normalize(glm::vec3(-dYdX, 1.0f, -dYdZ));
                v.TexCoord = { (float)x / (resolution - 1) * 6.0f, (float)z / (resolution - 1) * 6.0f };
                v.Color = { 0.42f, 0.62f, 0.35f, 1.0f };
                vertices.push_back(v);
            }
        }

        for (int z = 0; z < resolution - 1; ++z) {
            for (int x = 0; x < resolution - 1; ++x) {
                int row1 = z * resolution;
                int row2 = (z + 1) * resolution;

                indices.push_back(row1 + x);
                indices.push_back(row2 + x);
                indices.push_back(row1 + x + 1);

                indices.push_back(row1 + x + 1);
                indices.push_back(row2 + x);
                indices.push_back(row2 + x + 1);
            }
        }

        auto terrainModel = Model::CreateFromMesh(Mesh(vertices, indices));
        auto& mesh = entity.AddComponent<MeshComponent>(MeshGeometryType::CustomOBJ);
        mesh.ModelAsset = terrainModel;
        mesh.Roughness = 0.85f;
        mesh.Metallic = 0.05f;
        mesh.Color = { 0.42f, 0.62f, 0.35f, 1.0f };

        auto& rb = entity.AddComponent<RigidbodyComponent>();
        rb.Type = BodyType::Static;
        auto& col = entity.AddComponent<ColliderComponent>(ColliderType::Box);
        col.Size = { size, height * 2.0f, size };

        return entity;
    }

    std::shared_ptr<Shader> Scene::GetShaderForType(ShaderType type) const {
        switch (type) {
            case ShaderType::Unlit:
                return m_UnlitShader ? m_UnlitShader : m_PBRShader;
            case ShaderType::BlinnPhong:
                return m_BlinnPhongShader ? m_BlinnPhongShader : m_PBRShader;
            case ShaderType::Wireframe:
                return m_WireframeShader ? m_WireframeShader : m_PBRShader;
            case ShaderType::StandardPBR:
            default:
                return m_PBRShader;
        }
    }

}
