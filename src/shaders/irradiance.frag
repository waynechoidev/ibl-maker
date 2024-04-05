#version 420 core
out vec4 FragColor;
in vec3 WorldPos;
uniform samplerCube envCubemap;

const float PI = 3.14159265359;
const int minSampleCount = 32;
const int maxSampleCount = 512;
const float sampleCountScaleFactor = 0.5;

vec3 calculateIrradiance(vec3 N, int sampleCount) {
    vec3 irradiance = vec3(0.0);

    for (int i = 0; i < sampleCount; ++i) {
        // Sampling direction for importance sampling
        vec3 sampleDir = normalize(texture(envCubemap, N).rgb * 2.0 - 1.0);
        float cosTheta = max(dot(sampleDir, N), 0.0);
        irradiance += texture(envCubemap, sampleDir).rgb * cosTheta;
    }

    irradiance = PI * irradiance / float(sampleCount);
    return irradiance;
}

void main() {
    vec3 N = normalize(WorldPos);
    int sampleCount = minSampleCount;
    vec3 irradiance = calculateIrradiance(N, sampleCount);

    // Adaptive sample count
    float irradianceVariance = length(irradiance - irradiance.rrr);
    while (irradianceVariance > 0.01 && sampleCount < maxSampleCount) {
        sampleCount = int(sampleCount * (1.0 + sampleCountScaleFactor));
        irradiance = calculateIrradiance(N, sampleCount);
        irradianceVariance = length(irradiance - irradiance.rrr);
    }

    FragColor = vec4(irradiance, 1.0);
}