// The picture times a colour that a script sets (green)
uniform vec4 tint : source_color = vec4(1.0, 0.0, 1.0, 1.0);
uniform float strength : hint_range(0.0, 1.0) = 1.0;

void fragment() {
    COLOR = texture(TEXTURE, UV) * MODULATE * mix(vec4(1.0), tint, strength);
}
