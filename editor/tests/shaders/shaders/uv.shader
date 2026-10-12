// UV goes from (0, 0) at the top left to (1, 1) at the bottom right
void fragment() {
    COLOR = vec4(UV.x, UV.y, 0.0, 1.0);
}
