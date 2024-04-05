#version 420 core

out vec4 FragColor;

in vec3 WorldPos;

uniform samplerCube envCubemap;

const float PI = 3.14159265359;

const int ConvolutionSamplesOffset = 1; 
const int ConvolutionSampleCount = 256;
const int ConvolutionMaxSamples = 512;
const float ConvolutionRoughness = 0.2;
const float ConvolutionMip = 3.0;
const float EnvironmentScale = 3.0;
const float IblMaxValue[3] = { 102.0, 85.0, 79.0 };
const vec4 IBLCorrection = vec4(1.0);


vec3 rescaleHDR(vec3 hdrPixel) {
    hdrPixel = hdrPixel / (hdrPixel + vec3(1.0));

    if (hdrPixel.x < 0.0)
        hdrPixel.x = 0.0;
    if (hdrPixel.y < 0.0)
        hdrPixel.y = 0.0;
    if (hdrPixel.z < 0.0)
        hdrPixel.z = 0.0;

    float intensity = dot(hdrPixel, vec3(0.299, 0.587, 0.114));

    // Saturation adjustment
    hdrPixel = mix(intensity.xxx, hdrPixel, IBLCorrection.y);

    // Hue adjustment
    vec3 root3 = vec3(0.57735, 0.57735, 0.57735);
    float half_angle = 0.5 * radians(IBLCorrection.z);
    vec4 rot_quat = vec4(root3 * sin(half_angle), cos(half_angle));
    mat3 rot_Matrix = mat3(
        vec3(1.0 - 2.0 * (rot_quat.y * rot_quat.y + rot_quat.z * rot_quat.z), 2.0 * (rot_quat.x * rot_quat.y - rot_quat.w * rot_quat.z), 2.0 * (rot_quat.x * rot_quat.z + rot_quat.w * rot_quat.y)),
        vec3(2.0 * (rot_quat.x * rot_quat.y + rot_quat.w * rot_quat.z), 1.0 - 2.0 * (rot_quat.x * rot_quat.x + rot_quat.z * rot_quat.z), 2.0 * (rot_quat.y * rot_quat.z - rot_quat.w * rot_quat.x)),
        vec3(2.0 * (rot_quat.x * rot_quat.z - rot_quat.w * rot_quat.y), 2.0 * (rot_quat.y * rot_quat.z + rot_quat.w * rot_quat.x), 1.0 - 2.0 * (rot_quat.x * rot_quat.x + rot_quat.y * rot_quat.y))
    );
    hdrPixel = mat3(rot_Matrix) * hdrPixel;

    hdrPixel *= EnvironmentScale;
    return hdrPixel;
}

vec2 Hammersley(uint i, uint N) {
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) / float(N), rdi);
}

vec3 importanceSampleGGX(vec2 Xi, float roughness, vec3 N) {
    float a = roughness * roughness;
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
    vec3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 TangentX = normalize(cross(UpVector, N));
    vec3 TangentY = cross(N, TangentX);
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

float specularD(float roughness, float NoH) {
    float a2 = roughness * roughness;
    float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * d * d);
}

vec4 sumSpecular(vec3 spec, float NoL, vec4 result) {
    result.xyz += spec;
    result.w += NoL;
    return result;
}

vec3 ImportanceSample(vec3 R) {
    vec3 N = normalize(R);
    vec3 V = normalize(R);
    vec4 result = vec4(0.0);

    float sampleStep = float(ConvolutionMaxSamples) / float(ConvolutionSampleCount);
    uint sampleId = uint(ConvolutionSamplesOffset);

    int cubeWidth = textureSize(envCubemap, 0).x;

    for (int i = 0; i < ConvolutionSampleCount; i++) {
        vec2 Xi = Hammersley(sampleId, uint(ConvolutionMaxSamples));

        vec3 H = importanceSampleGGX(Xi, ConvolutionRoughness, N);
        vec3 L = 2.0 * dot(V, H) * H - V;
        float NoL = max(dot(N, L), 0.0);
        float VoL = max(dot(V, L), 0.0);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);
        if (NoL > 0.0) {
            float Dh = specularD(ConvolutionRoughness, NoH);
            float pdf = Dh * NoH / (4.0 * VoH);
            float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);
            float solidAngleSample = 1.0 / (ConvolutionSampleCount * pdf);
            float lod = ConvolutionRoughness == 0.0 ? 0.0 : 0.5 * log2(solidAngleSample / solidAngleTexel);

            vec3 hdrPixel = rescaleHDR(textureLod(envCubemap, L, lod).rgb);
            // vec3 hdrPixel = textureLod(envCubemap, L, lod).rgb;
            result = sumSpecular(hdrPixel, NoL, result);
        }
        sampleId += uint(sampleStep);
    }

    if (result.w == 0.0)
        return result.xyz;
    else
        return result.xyz / result.w;
}

void main() {
    vec3 N = normalize(WorldPos);
    vec3 color;

    vec3 importanceSampled = ImportanceSample(N);

    if (ConvolutionSamplesOffset >= 1) {
        vec3 lastResult = textureLod(envCubemap, N, ConvolutionMip).rgb;
        color = mix(lastResult, importanceSampled, 1.0 / float(ConvolutionSamplesOffset));
    } else {
        color = importanceSampled;
    }

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // prevent clipping
    color = min(color, vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}