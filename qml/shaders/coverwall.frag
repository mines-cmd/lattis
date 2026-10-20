#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 rot0;      // R00, R01, R20, R21 of the wall's rotation
    vec4 rot1;      // R10, R11
    vec2 viewSize;  // the window, in pixels
    vec2 origin;    // the flat wall's top left, as a distance from the plane's center
    vec2 planeSize; // the flat wall's size
    float viewDistance; // from the viewer to the plane
};

layout(binding = 1) uniform sampler2D source;

// A plane point (u, v) rotated by R lands at the screen offset (x', y') / (1 - z' / viewDistance)
// from the window's center. Solving that for (u, v) at one screen pixel is two linear equations.
void main()
{
    vec2 p = (qt_TexCoord0 - 0.5) * viewSize;
    float a11 = rot0.x + p.x * rot0.z / viewDistance;
    float a12 = rot0.y + p.x * rot0.w / viewDistance;
    float a21 = rot1.x + p.y * rot0.z / viewDistance;
    float a22 = rot1.y + p.y * rot0.w / viewDistance;
    float det = a11 * a22 - a12 * a21;
    vec2 uv = vec2(p.x * a22 - p.y * a12, a11 * p.y - a21 * p.x) / det;
    vec2 t = (uv - origin) / planeSize;
    if (any(lessThan(t, vec2(0.0))) || any(greaterThan(t, vec2(1.0)))) {
        fragColor = vec4(0.0);
        return;
    }
    fragColor = texture(source, t) * qt_Opacity;
}
