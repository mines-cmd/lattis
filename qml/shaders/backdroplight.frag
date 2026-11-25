#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float limit;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 color = texture(source, qt_TexCoord0);
    float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    // Below the knee the art is left exactly as it is, so dark games don't change.
    float knee = limit * 0.8;
    if (luma > knee) {
        // The slope is 1 at the knee: no visible edge where the compression starts.
        float room = max(limit - knee, 1e-4);
        float held = knee + room * (1.0 - exp((knee - luma) / room));
        color.rgb *= held / luma;
    }
    fragColor = color * qt_Opacity;
}
