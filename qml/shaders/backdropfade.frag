#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 background;
    vec4 target;
    float dim;
    float fadeStart;
    float smoothFade;
    float dither;
};

// Dave Hoskins' sine-free hash: sin()-based ones lose precision on some GPUs and show patterns.
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    float t = clamp((qt_TexCoord0.y - fadeStart) / max(1.0 - fadeStart, 1e-5), 0.0, 1.0);
    // Smoothstep has zero slope where it meets the background; a linear ramp's kink there
    // reads as a line over bright art.
    float k = mix(t, t * t * (3.0 - 2.0 * t), smoothFade);
    float a = mix(dim, 1.0, k);
    vec4 color = vec4(mix(background.rgb, target.rgb, k) * a, a);
    // Triangular noise of about one 8-bit step, added before the framebuffer rounds: near
    // black the fade has only a few levels left, which high-contrast panels show as bands.
    float noise = hash(gl_FragCoord.xy) + hash(gl_FragCoord.yx + 17.0) - 1.0;
    color.rgb += noise * dither / 255.0;
    fragColor = clamp(color, 0.0, 1.0) * qt_Opacity;
}
