/**
 * Renderer.cpp
 *
 * Advanced voxel rendering system with physically-based shading, normal mapping,
 * and Perlin noise-influenced volumetric fog.
 *
 * Features:
 *  - Global sun directional illumination (diffuse + Blinn-Phong specular)
 *  - Placeholder support for per-block emissive light via lightMap sampler
 *  - Perlin noise-based depth fog with volumetric appearance
 *  - Normal mapping using tangent-space normals with computed TBN matrix
 *  - Specular and roughness sampling from texture atlases
 *  - Proper gamma handling (linearization for lighting, re-gamma for output)
 *  - Mipmap generation with improved minification filtering to reduce shimmering
 *
 * The shader pipeline processes vertex/fragment operations in linear space,
 * allowing accurate lighting calculations before final sRGB conversion for display.
 */

#include "Renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stbimage/stb_image.h"
#include <iostream>
#include <vector>

/**
 * Vertex Shader Source
 *
 * Handles per-vertex transformation and tangent-space basis computation.
 * Transforms positions, normals, and tangents from model to world space,
 * enabling accurate normal mapping and lighting calculations in fragment shader.
 *
 * Key operations:
 *  - Project vertex position to clip space (MVP matrix)
 *  - Pass world-space fragment position for distance-based fog calculations
 *  - Compute tangent-normal-bitangent (TBN) matrix for tangent-space normal mapping
 *
 * Input attributes:
 *  - aPos: vertex position (model space)
 *  - aUV: texture coordinates [0, 1] for atlas sampling
 *  - aNormal: vertex normal (model space, should be normalized by loader)
 *  - aTangent: vertex tangent (model space, required for normal mapping)
 *
 * Output varyings:
 *  - TexCoord: texture coordinates passed to fragment shader
 *  - FragPos: world-space position for lighting and fog
 *  - Normal, Tangent, Bitangent: world-space basis vectors for TBN matrix
 */
static const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
/**
 * aTangent: Tangent vector in tangent space for normal mapping
 *
 * For voxel cubes, each face should have consistent tangent vector pointing
 * along one of the primary axes. Example: +X face has tangent = (+Z, 0, 0)
 */
layout (location = 3) in vec3 aTangent;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 Bitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    mat4 mv = view * model;
    gl_Position = projection * mv * vec4(aPos, 1.0);

    /**
     * World-space fragment position
     *
     * Used for:
     *  - Distance calculation in fog (fog-based on depth to camera)
     *  - World-space lighting and normal mapping
     */
    FragPos = vec3(model * vec4(aPos, 1.0));

    /**
     * Transform normals and tangents to world space
     *
     * Uses inverse-transpose of model's 3x3 component to properly handle
     * non-uniform scaling (though voxel grids typically use uniform scale).
     * This ensures normals remain perpendicular to surfaces after transformation.
     */
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalize(normalMatrix * aNormal);
    Tangent = normalize(normalMatrix * aTangent);
    /**
     * Bitangent (binormal)
     *
     * Computed via cross product to ensure orthogonal basis for TBN matrix.
     * Order: cross(Normal, Tangent) produces correct handedness for right-handed system.
     */
    Bitangent = normalize(cross(Normal, Tangent));

    TexCoord = aUV;
}
)";

/**
 * Fragment Shader Source
 *
 * Core rendering pipeline combining multiple lighting models and effects:
 *  1. Albedo sampling with gamma linearization
 *  2. Normal mapping via tangent-space transformation
 *  3. Diffuse lighting (Lambert model)
 *  4. Specular lighting (Blinn-Phong with roughness control)
 *  5. Block light contributions (placeholder for voxel-emissive support)
 *  6. Ambient fallback lighting
 *  7. Perlin noise-based depth fog with volumetric effect
 *  8. Final gamma correction for display
 */
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec3 Bitangent;

uniform sampler2D albedoAtlas;
uniform sampler2D normalAtlas;
uniform sampler2D specularAtlas;
uniform sampler2D lightMap; // placeholder: future support for per-block emissive/light values

// Global sun (directional) lighting
uniform vec3 sunDirection; // direction TO light (should be normalized)
uniform vec3 sunColor;     // e.g. vec3(1.0, 0.98, 0.9)
uniform float sunIntensity; // e.g. 1.0

// Ambient / environment
uniform vec3 ambientColor; // base ambient (weak)
uniform float ambientIntensity;

// Camera
uniform vec3 cameraPos;

// Specular parameters
uniform float defaultRoughness; // fallback roughness if spec map not provided (0..1)
uniform float defaultMetalness; // not used here but left for expansion

// Fog (depth-based) driven by Perlin noise
uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogColor;
uniform float fogDensity; // frequency/scale for noise
uniform float fogStrength; // overall blend

// Perlin noise parameters
// (These allow tuning noise scale and animation if desired)
uniform float noiseScale;
uniform float noiseAmplitude;
uniform float time; // optional time for animated fog if desired

// Toggles
uniform bool useNormalMap;
uniform bool useSpecularMap;
uniform bool useLightMap;

// Gamma (we will linearize textures on sample then re-gamma at end)
const float GAMMA = 2.2;

// --- Utility: convert color sampled from sRGB to linear for lighting computations
vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(GAMMA));
}
vec3 linearToSrgb(vec3 c) {
    return pow(c, vec3(1.0 / GAMMA));
}

// --- Simple 3D Perlin noise (compact, classic)
vec4 permute(vec4 x) { return mod(((x*34.0)+1.0)*x, 289.0); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

float perlin3d(vec3 P) {
    vec3 Pi0 = floor(P); // Integer part for indexing
    vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
    Pi0 = mod(Pi0, 289.0);
    Pi1 = mod(Pi1, 289.0);
    vec3 Pf0 = fract(P); // Fractional part for interpolation
    vec3 Pf1 = Pf0 - vec3(1.0);
    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
    vec4 iz0 = vec4(Pi0.z);
    vec4 iz1 = vec4(Pi1.z);

    vec4 ixy = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);

    vec4 gx0 = fract(ixy0 * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 gy0 = abs(gx0) - 0.5;
    vec4 tx0 = floor(gx0 + 0.5);
    gx0 = gx0 - tx0;

    vec4 gx1 = fract(ixy1 * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 gy1 = abs(gx1) - 0.5;
    vec4 tx1 = floor(gx1 + 0.5);
    gx1 = gx1 - tx1;

    vec3 g000 = vec3(gx0.x, gy0.x, gx0.y);
    vec3 g100 = vec3(gx0.z, gy0.z, gx0.w);
    vec3 g010 = vec3(gx0.y, gy0.y, gx0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gx0.x);

    vec3 g001 = vec3(gx1.x, gy1.x, gx1.y);
    vec3 g101 = vec3(gx1.z, gy1.z, gx1.w);
    vec3 g011 = vec3(gx1.y, gy1.y, gx1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gx1.x);

    vec4 norm0 = taylorInvSqrt(vec4(dot(g000,g000), dot(g010,g010), dot(g100,g100), dot(g110,g110)));
    g000 *= norm0.x; g010 *= norm0.y; g100 *= norm0.z; g110 *= norm0.w;
    vec4 norm1 = taylorInvSqrt(vec4(dot(g001,g001), dot(g011,g011), dot(g101,g101), dot(g111,g111)));
    g001 *= norm1.x; g011 *= norm1.y; g101 *= norm1.z; g111 *= norm1.w;

    float n000 = dot(g000, Pf0);
    float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.x, Pf1.y, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
    float n111 = dot(g111, Pf1);

    vec3 fade_xyz = Pf0 * Pf0 * Pf0 * (Pf0 * (Pf0 * 6.0 - 15.0) + 10.0);
    float n_z0 = mix(mix(n000, n100, fade_xyz.x), mix(n010, n110, fade_xyz.x), fade_xyz.y);
    float n_z1 = mix(mix(n001, n101, fade_xyz.x), mix(n011, n111, fade_xyz.x), fade_xyz.y);
    float n_xyz = mix(n_z0, n_z1, fade_xyz.z);
    return n_xyz;
}

// --- Normal mapping helper
vec3 getNormalFromMap(vec2 uv, vec3 N, vec3 T, vec3 B) {
    vec3 normalSample = texture(normalAtlas, uv).rgb;
    // normal map encoded in [0,1] -> [-1,1]
    vec3 n = normalize(normalSample * 2.0 - 1.0);
    mat3 TBN = mat3(normalize(T), normalize(B), normalize(N));
    return normalize(TBN * n);
}

// --- main
void main() {
    // --- Sample albedo and linearize
    vec3 albedoSample = texture(albedoAtlas, TexCoord).rgb;
    vec3 albedo = srgbToLinear(albedoSample);

    // --- Normal (with normal map)
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    vec3 B = normalize(Bitangent);
    if (useNormalMap) {
        N = getNormalFromMap(TexCoord, N, T, B);
    }

    // --- View and light directions (world-space)
    vec3 V = normalize(cameraPos - FragPos);
    vec3 L = normalize(sunDirection); // sunDirection should point to light

    // --- Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * sunColor * sunIntensity * NdotL;

    // --- Specular (Blinn-Phong with roughness from specular atlas alpha channel or R channel)
    float specIntensity = 1.0;
    float roughness = defaultRoughness;
    if (useSpecularMap) {
        vec4 specSample = texture(specularAtlas, TexCoord); // e.g. R = specular strength, A = roughness
        specIntensity = specSample.r;
        roughness = clamp(specSample.a, 0.02, 1.0);
    }
    // Blinn-Phong
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    // convert roughness (0..1) to shininess exponent (simple heuristic)
    float shininess = mix(4.0, 2000.0, 1.0 - roughness); // smooth -> high shininess
    float specFactor = pow(NdotH, shininess);
    vec3 specular = sunColor * specIntensity * specFactor * sunIntensity;

    // --- Lightmap / block lights contribution (future support)
    vec3 blockLight = vec3(0.0);
    if (useLightMap) {
        // For now sample a lightmap at TexCoord; user can replace with voxel-space light texture later.
        vec3 lm = texture(lightMap, TexCoord).rgb;
        // lightmap assumed already linear (or low dynamic)
        blockLight = srgbToLinear(lm);
    }

    // --- Ambient
    vec3 ambient = ambientColor * ambientIntensity * albedo;

    // --- Final lighting in linear space
    vec3 colorLinear = ambient + diffuse + specular + blockLight;

    // --- Perlin depth-based fog (apply to linear color)
    float dist = length(cameraPos - FragPos);

    // Evaluate noise at world position scaled by noiseScale and optionally animated
    float n = perlin3d((FragPos * noiseScale) + vec3(0.0, time * 0.0, 0.0));
    // map noise [-1,1] from algorithm to [0,1]
    n = clamp(n * 0.5 + 0.5, 0.0, 1.0);
    // Use noise to perturb fog density with amplitude
    float fogFactorByDist = clamp((fogEnd - dist) / max(0.0001, fogEnd - fogStart), 0.0, 1.0);
    // Combine with noise: when noise high -> less fog (holes), when low -> more fog
    float noiseMod = mix(1.0 - noiseAmplitude, 1.0 + noiseAmplitude, n);
    float fogMix = clamp(1.0 - fogFactorByDist * (1.0 / noiseMod), 0.0, 1.0);
    fogMix = clamp(fogMix * fogStrength, 0.0, 1.0);

    vec3 fogLinear = srgbToLinear(fogColor);

    vec3 finalLinear = mix(colorLinear, fogLinear, fogMix);

    // --- Gamma correction back to sRGB for output
    vec3 finalColor = linearToSrgb(finalLinear);

    FragColor = vec4(finalColor, 1.0);
}
)";

// ---------------------------------------------------------------------------

Renderer::Renderer()
    : shaderProgram(0),
      atlasTexture(0),
      normalAtlasTexture(0),
      specularAtlasTexture(0),
      lightMapTexture(0) {
}

Renderer::~Renderer() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (atlasTexture) glDeleteTextures(1, &atlasTexture);
    if (normalAtlasTexture) glDeleteTextures(1, &normalAtlasTexture);
    if (specularAtlasTexture) glDeleteTextures(1, &specularAtlasTexture);
    if (lightMapTexture) glDeleteTextures(1, &lightMapTexture);
}

unsigned int Renderer::compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cout << "Shader Error: " << infoLog << std::endl;
    }
    return shader;
}

unsigned int Renderer::createShaderProgram() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cout << "Linker Error: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // set texture units once (convenience)
    glUseProgram(program);
    GLint loc;
    loc = glGetUniformLocation(program, "albedoAtlas"); if (loc >= 0) glUniform1i(loc, 0);
    loc = glGetUniformLocation(program, "normalAtlas"); if (loc >= 0) glUniform1i(loc, 1);
    loc = glGetUniformLocation(program, "specularAtlas"); if (loc >= 0) glUniform1i(loc, 2);
    loc = glGetUniformLocation(program, "lightMap"); if (loc >= 0) glUniform1i(loc, 3);
    glUseProgram(0);

    return program;
}

GLuint Renderer::loadTexture(const char* path, bool hasAlpha, bool isNormalMap) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (!data) {
        std::cout << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB8;
    if (channels == 1)      { format = GL_RED; internalFormat = GL_R8; }
    else if (channels == 3) { format = GL_RGB; internalFormat = GL_RGB8; }
    else if (channels == 4) { format = GL_RGBA; internalFormat = GL_RGBA8; }

    // For normal maps we keep linear formats; for albedo we will do manual gamma in shader
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Upload texture
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Sampler settings:
    // - Keep MAG filter nearest to preserve crisp pixels (Minecraft style)
    // - Use NEAREST_MIPMAP_LINEAR for MIN filter to reduce shimmer at distance
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);

    // Wrap: repeat by default, but atlas UVs usually stay within [0,1]
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // For normal maps, ensure no SRGB conversion by using linear internal formats above.
    // If you want GPU SRGB handling for albedo, upload with GL_SRGB8_ALPHA8 and sample as usual.
    // We instead perform manual linearization in the shader to keep control.

    stbi_image_free(data);
    return textureID;
}

bool Renderer::initialize() {
    shaderProgram = createShaderProgram();
    if (!shaderProgram) return false;

    // Load required textures (paths are examples; adjust to your project layout)
    atlasTexture         = loadTexture("../src/textures/atlas.png", true, false);
    normalAtlasTexture   = loadTexture("../src/textures/atlas_normal.png", true, true);
    specularAtlasTexture = loadTexture("../src/textures/atlas_specular.png", true, false);

    // Create placeholder 1x1 black lightmap for now
    unsigned char black[4] = {0,0,0,255};
    glGenTextures(1, &lightMapTexture);
    glBindTexture(GL_TEXTURE_2D, lightMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!atlasTexture || !normalAtlasTexture || !specularAtlasTexture) {
        std::cout << "One or more textures failed to load." << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    return true;
}
