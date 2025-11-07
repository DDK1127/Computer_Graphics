#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 ViewDir;
} vs_out;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoord = aTex;

    // 計算觀察方向（相機位置固定在原點假設）
    vec3 camPos = vec3(inverse(view)[3]);
    vs_out.ViewDir = normalize(camPos - worldPos.xyz);

    gl_Position = projection * view * worldPos;
}
