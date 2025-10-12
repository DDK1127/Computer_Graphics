#version 330 core
in vec3 vN;
in vec3 vWPos;
in vec2 vUV;

out vec4 oColor;

uniform sampler2D uTex;
uniform vec3 uCam;

void main(){
    vec3 N = normalize(vN);
    vec3 L = normalize(vec3(0.6,0.7,0.5));
    vec3 V = normalize(uCam - vWPos);
    vec3 H = normalize(L+V);

    float diff = max(dot(N,L),0.0);
    float spec = pow(max(dot(N,H),0.0), 32.0);

    // 半球環境光，上方偏藍，下方偏暖
    vec3 sky = vec3(0.5,0.6,0.8);
    vec3 ground = vec3(0.8,0.7,0.6);
    float hemi = 0.5 * (N.y + 1.0);
    vec3 ambient = mix(ground, sky, hemi);

    vec3 base = texture(uTex, vUV).rgb;
    vec3 color = base * (ambient + diff) + 0.3 * spec;

    oColor = vec4(color, 1.0);
}
