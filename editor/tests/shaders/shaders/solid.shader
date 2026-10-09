// The shaders test (scripts/test-shaders.sh): one colour, from a parameter
// (the scene sets it to blue).
uniform vec4 fill : source_color = vec4(1.0, 0.0, 0.0, 1.0);

void fragment() {
    COLOR = fill;
}
