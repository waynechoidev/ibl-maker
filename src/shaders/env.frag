#version 420 core

out vec4 FragColor;

in vec3 WorldPos;

uniform samplerCube envCubemap;

void main() {
    vec3 N = normalize(WorldPos);

    vec3 color = texture(envCubemap,N).xyz;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // prevent clipping
    color = min(color, vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}