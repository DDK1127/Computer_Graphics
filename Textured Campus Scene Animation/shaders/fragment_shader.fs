#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 ViewDir;
} fs_in;

out vec4 FragColor;

uniform sampler2D uDiffuse;

// 白天設定
uniform vec3 lightDir = normalize(vec3(-0.3, -1.0, -0.3));
uniform vec3 lightColor = vec3(1.0, 1.0, 1.0);
uniform vec3 ambientColor = vec3(0.3, 0.3, 0.3);

// 天空顏色漸層
vec3 getSkyColor(vec3 pos)
{
    float t = clamp(pos.y / 50.0, 0.0, 1.0);
    vec3 horizon = vec3(0.7, 0.85, 1.0);
    vec3 zenith  = vec3(0.3, 0.6, 1.0);
    return mix(horizon, zenith, t);
}

void main()
{
    vec3 N = normalize(fs_in.Normal);
    vec3 V = normalize(fs_in.ViewDir);
    vec3 L = normalize(-lightDir);

    // Diffuse
    float diff = max(dot(N, L), 0.0);

    // Specular（模擬太陽反光）
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 64.0);

    // Base color from texture
    vec3 texColor = texture(uDiffuse, fs_in.TexCoord).rgb;

    // Combine lighting
    vec3 lighting = texColor * (ambientColor + lightColor * diff) + spec * lightColor * 0.5;

    // 混入天空顏色（視覺自然化）
    vec3 sky = getSkyColor(fs_in.FragPos);
    lighting = mix(sky, lighting, 0.85);

    FragColor = vec4(lighting, 1.0);
}
