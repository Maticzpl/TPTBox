#version 430

// TODO
layout(shared, binding = 4) uniform Settings {
    int MAX_RAY_STEPS;
    uint DEBUG_MODE;      // 0 = regular rendering, see Renderer.h for flags
    float AO_STRENGTH;    // 0 = No AO effect, 1 = max AO effect
};

uniform vec2 resolution;  // Viewport res
out vec4 FragColor;

uniform sampler2D baseTexture;
uniform sampler2D glowTexture;
uniform sampler2D blurTexture;
uniform sampler2D depthTexture;

void main() {
    gl_FragDepth = texture2D(depthTexture, gl_FragCoord.xy / resolution).r;

    vec4 orgColor = texture2D(baseTexture, gl_FragCoord.xy / resolution);
    vec4 glowColor = texture2D(glowTexture, gl_FragCoord.xy / resolution);

    for (int dx = -3; dx <= 3; dx++)
    for (int dy = -3; dy <= 3; dy++) {
        glowColor += texture(glowTexture, (gl_FragCoord.xy + vec2(dx, dy)) / resolution, 0.0);
    }
    glowColor /= 100;

    vec4 blurColor = vec4(0.0);
    for (int dx = -2; dx <= 2; dx++)
    for (int dy = -2; dy <= 2; dy++) {
        if (dx != 0 || dy != 0)
            blurColor += texture(blurTexture, (gl_FragCoord.xy + vec2(dx, dy)) / resolution, 0.0);
    }
    blurColor /= 25;

    orgColor.rgb *= 1.0 - blurColor.a;

    vec4 outColor = orgColor + blurColor + 2 * glowColor;
    FragColor = outColor;
}