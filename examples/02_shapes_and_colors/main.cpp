// =============================================================================
//  Example 02 - Shapes and Colors
//
//  What you'll learn:
//    - how to draw rectangles, circles, lines, triangles and polygons
//    - filled shapes vs. outlines
//    - drawing text (normal and centered)
//    - making your own colors: Color{r, g, b}, Color::fromHex, see-through
//      colors with withAlpha(), lighter() and darker()
//    - animating things with timeSinceStart() and a rainbow with Color::fromHSV
//
//  Controls: none - just watch! (Escape quits)
//
//  Remember: (0, 0) is the TOP-LEFT corner of the window.
//  x gets bigger to the right, y gets bigger going DOWN.
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::sin, for making things bob up and down

using namespace sprout;

// You can make your own colors and give them names.
// Color{red, green, blue} - each part goes from 0 (none) to 255 (full).
const Color Tangerine = Color{255, 140, 60};
// Or write them like on websites: 0xRRGGBB (red, green, blue in hexadecimal).
const Color Grape = Color::fromHex(0x8E44AD);
const Color Mint = Color::fromHex(0x2ECC71);
const Color Night = Color::fromHex(0x1E1E2E);

struct ShapesScene : Scene {
    void start() override {
        background = Night; // the color the screen is cleared to every frame
    }

    // A small helper so every shape gets a little name tag underneath it.
    void drawLabel(const std::string& text, float centerX, float y) {
        drawTextCentered(text, Vec2{centerX, y}, 15, Color::LightGray);
    }

    void draw() override {
        // timeSinceStart() is how many seconds the game has been running.
        // It keeps growing, so it's perfect for animations.
        float time = timeSinceStart();

        // ---- Title -----------------------------------------------------------
        drawTextCentered("Shapes and Colors", Vec2{400, 30}, 40, Color::White);

        // ---- Row 1: rectangles and circles ---------------------------------
        // drawRect(x, y, width, height, color) - x and y are the TOP-LEFT corner.
        drawRect(50, 75, 100, 60, Tangerine);
        drawLabel("drawRect", 100, 150);

        // A Rect is {x, y, width, height}. Rounded corners look friendly!
        drawRoundedRect(Rect{200, 75, 100, 60}, 15, Grape);
        drawLabel("drawRoundedRect", 250, 150);

        // Just the border. The last number is how thick the line is.
        drawRectOutline(Rect{350, 75, 100, 60}, Mint, 4);
        drawLabel("drawRectOutline", 400, 150);

        // Circles are different: the position is the MIDDLE of the circle.
        drawCircle(Vec2{550, 105}, 32, Color::SkyBlue);
        drawLabel("drawCircle", 550, 150);

        drawCircleOutline(Vec2{700, 105}, 32, Color::Pink, 5);
        drawLabel("drawCircleOutline", 700, 150);

        // ---- Row 2: lines, triangles, polygons -----------------------------
        // A line from one point to another, 6 pixels thick.
        drawLine(Vec2{50, 245}, Vec2{150, 185}, Color::Yellow, 6);
        drawLabel("drawLine", 100, 265);

        // A triangle needs three corners.
        drawTriangle(Vec2{250, 180}, Vec2{200, 245}, Vec2{300, 245}, Color::Red);
        drawLabel("drawTriangle", 250, 265);

        // A polygon is a shape with equal sides. 6 sides = a hexagon.
        // The last value is the rotation in degrees - here it keeps growing,
        // so the hexagon spins (45 degrees every second).
        drawPolygon(Vec2{400, 215}, 6, 35, Color::Gold, time * 45);
        drawLabel("drawPolygon", 400, 265);

        // See-through colors: withAlpha(0.5f) means "half see-through".
        // Where the circles overlap, the colors mix together.
        drawCircle(Vec2{535, 205}, 25, Color::Red.withAlpha(0.6f));
        drawCircle(Vec2{565, 205}, 25, Color::Blue.withAlpha(0.6f));
        drawCircle(Vec2{550, 230}, 25, Color::Green.withAlpha(0.6f));
        drawLabel("withAlpha", 550, 265);

        // lighter() and darker() make shades of any color.
        drawRect(650, 185, 33, 60, Grape.lighter(0.5f));
        drawRect(683, 185, 34, 60, Grape);
        drawRect(717, 185, 33, 60, Grape.darker(0.5f));
        drawLabel("lighter / darker", 700, 265);

        // ---- Row 3: an animated rainbow --------------------------------------
        // Color::fromHSV(hue) picks a color of the rainbow:
        // hue 0 = red, 120 = green, 240 = blue, 360 = red again.
        // Each ball gets a slightly different hue, and adding `time` makes the
        // colors slide along. std::sin makes the balls bob up and down in a wave.
        for (int i = 0; i < 15; i++) {
            float x = 120 + i * 40;
            float y = 330 + std::sin(time * 3 + i * 0.5f) * 15;
            Color rainbow = Color::fromHSV(time * 100 + i * 24);
            drawCircle(Vec2{x, y}, 16, rainbow);
        }
        drawLabel("Color::fromHSV  +  timeSinceStart()", 400, 375);
    }

    // drawUI() is drawn on top of everything. Great for text like this.
    void drawUI() override {
        // drawText(text, x, y, size, color) - x and y are the TOP-LEFT of the text.
        drawText("Try changing the numbers in main.cpp and run it again!", 20, 415, 20, Color::Gray);
    }
};

int main() {
    Game game("02 - Shapes and Colors", 800, 450);
    game.run<ShapesScene>();
}
