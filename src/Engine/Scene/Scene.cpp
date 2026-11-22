#include "Scene.h"
#include "ScriptableEntity.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>
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

            float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
                vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
                projCoords = projCoords * 0.5 + 0.5;

                if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
                    return 0.0;

                float bias = max(u_ShadowBias * (1.0 - dot(normal, lightDir)), u_ShadowBias * 0.2);

                // PCF (Percentage-Closer Filtering) 3x3 suave
                float shadow = 0.0;
                vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
                for(int x = -1; x <= 1; ++x) {
                    for(int y = -1; y <= 1; ++y) {
                        float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(float(x), float(y)) * texelSize).r;
                        shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
                    }
                }
                shadow /= 9.0;
                return shadow;
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

                vec3 radiance = u_LightColor * u_LightIntensity;

                float NDF = DistributionGGX(N, H, u_Roughness);
                float G   = GeometrySmith(N, V, L, u_Roughness);
                vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - u_Metallic;

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                float NdotL = max(dot(N, L), 0.0);
                vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

                float shadow = CalculateShadow(v_FragPosLightSpace, N, L);
                Lo *= (1.0 - shadow);

                // Contribuição PBR de Point Lights (Atenuação Física Quadrática)
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

                // Contribuição PBR de Spot Lights (Holofotes com Penumbra Suave)
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

                vec3 ambient = u_AmbientIntensity * albedo * u_AO;
                vec3 emissive = u_EmissionColor * u_EmissionIntensity;
                vec3 color = ambient + Lo + emissive;

                // Tone Mapping ACES Film
                color *= u_Exposure;
                float a = 2.51;
                float b = 0.03;
                float c = 2.43;
                float d = 0.59;
                float e = 0.14;
                color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);

                // Gamma Correction 2.2
                color = pow(color, vec3(1.0 / 2.2));

                // Vinheta Cinematográfica
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

        // 6. Shader do Skybox Atmosférico Procedural (Atmosfera Dinâmica com Disco Solar)
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
            in vec3 v_TexCoord;
            layout(location = 0) out vec4 o_Color;
            layout(location = 1) out int o_EntityID;
            uniform vec3 u_SunDirection;
            void main() {
                vec3 dir = normalize(v_TexCoord);
                vec3 sunDir = normalize(u_SunDirection);

                vec3 zenith = vec3(0.18, 0.42, 0.85);
                vec3 horizon = vec3(0.82, 0.72, 0.62);
                vec3 ground = vec3(0.12, 0.12, 0.14);

                vec3 sky = (dir.y > 0.0) ? mix(horizon, zenith, pow(dir.y, 0.65)) : mix(horizon, ground, pow(-dir.y, 0.5));
                float sunDot = max(dot(dir, sunDir), 0.0);
                float sunDisk = smoothstep(0.998, 0.9995, sunDot) * 3.5;
                float sunGlow = pow(sunDot, 16.0) * 0.45;
                vec3 sun = vec3(1.0, 0.95, 0.82) * (sunDisk + sunGlow);

                o_Color = vec4(sky + sun, 1.0);
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

        // ---------------------------------------------------------------------
        // PASSO 1: Passe de Sombra (Renderização da Profundidade no ShadowMap)
        // ---------------------------------------------------------------------
        if (hasShadows && m_ShadowMap) {
            m_ShadowMap->CalculateLightSpaceMatrix(lightDir);
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

        activeShader->SetFloat3("u_LightDirection", lightDir);
        activeShader->SetFloat3("u_LightColor", lightColor);
        activeShader->SetFloat("u_LightIntensity", lightIntensity);
        activeShader->SetFloat("u_AmbientIntensity", ambientIntensity);
        activeShader->SetFloat("u_ShadowBias", shadowBias);
        activeShader->SetFloat("u_Exposure", m_Exposure);
        activeShader->SetFloat("u_Vignette", m_Vignette);

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

            activeShader->SetMat4("u_Transform", worldTransform);
            activeShader->SetInt("u_EntityID", static_cast<int>(id));
            activeShader->SetFloat4("u_BaseColor", mesh.Color);
            activeShader->SetFloat("u_Metallic", mesh.Metallic);
            activeShader->SetFloat("u_Roughness", mesh.Roughness);
            activeShader->SetFloat("u_AO", mesh.AO);
            activeShader->SetFloat("u_Tiling", mesh.Tiling);
            activeShader->SetFloat3("u_EmissionColor", mesh.EmissionColor);
            activeShader->SetFloat("u_EmissionIntensity", mesh.EmissionIntensity);

            if (mesh.TextureAsset) {
                mesh.TextureAsset->Bind(0);
            } else {
                m_WhiteTexture->Bind(0);
            }

            if (mesh.UseNormalMap && mesh.NormalMapAsset) {
                mesh.NormalMapAsset->Bind(2);
                activeShader->SetInt("u_NormalMap", 2);
                activeShader->SetInt("u_UseNormalMap", 1);
            } else {
                activeShader->SetInt("u_UseNormalMap", 0);
            }

            mesh.ModelAsset->Draw(activeShader);
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

}
