// A mistake on line 4: the test checks the error says so
void fragment() {
    vec2 uv = UV;
    COLOR = vec4(uv.x, nope, 0.0, 1.0);
}
