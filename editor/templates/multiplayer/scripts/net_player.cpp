// One player. Every game has a copy of every player, but only one game moves
// each: the player's multiplayer authority (the peer it belongs to). Its
// MultiplayerSynchronizer sends position, velocity and which way it faces to
// the other games, so their copies follow along.
#include <cmath>

#include "script.hpp"

using namespace s2d;

class NetPlayer : public Script {
public:
    EXPORT(float, speed, 220);
    EXPORT(float, jumpSpeed, 520);
    EXPORT(float, gravity, 1300);

    void ready() override {
        int id = node()->multiplayerAuthority();
        // Each player gets a color from its id: the same in every game
        static const Color colors[] = {WHITE, {255, 140, 140, 255}, {140, 200, 255, 255}, {150, 240, 150, 255},
                                       {255, 220, 120, 255}, {220, 160, 255, 255}};
        if (auto* sprite = getNode("Sprite")->as<Sprite2D>()) sprite->modulate = colors[id % 6];
        name = "P" + std::to_string(id) + (isMultiplayerAuthority() ? " (you)" : "");
        setLabel(name);
    }

    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        auto* sprite = getNode("Sprite")->as<AnimatedSprite2D>();
        if (!body || !sprite) return;

        if (waveTime > 0 && (waveTime -= delta) <= 0) setLabel(name);

        if (isMultiplayerAuthority()) {
            // Our own player: read the keyboard and move
            float direction = 0;
            if (IsKeyDown(KEY_LEFT)) direction -= 1;
            if (IsKeyDown(KEY_RIGHT)) direction += 1;
            body->velocity.x = direction * speed;
            body->velocity.y += gravity * delta;
            if (body->isOnFloor() && IsKeyPressed(KEY_SPACE)) body->velocity.y = -jumpSpeed;
            body->moveAndSlide();
            if (direction != 0) sprite->flipH = direction < 0;
            if (body->position.y > 900) body->position = Vector2{400, 100};  // fell off: back to the top

            if (IsKeyPressed(KEY_E)) {
                rpc("wave");  // on everyone else's copy of this player...
                wave();       // ...and on ours
            }
        }
        // Everyone works out the animation from the velocity the synchronizer sends
        float vx = body->velocity.x, vy = body->velocity.y;
        if (vy < -1) sprite->play("jump");
        else if (vy > 60) sprite->play("fall");
        else if (std::fabs(vx) > 1) sprite->play("run");
        else sprite->play("idle");
    }

    // Other players call this through rpc("wave")
    RPC(wave);
    void wave() {
        setLabel(name + ": hi!");
        waveTime = 1.5f;
    }

private:
    void setLabel(const std::string& text) {
        if (auto* label = getNode("NameLabel")->as<Label>()) label->text = text;
    }

    std::string name;
    float waveTime = 0;
};

REGISTER_SCRIPT(NetPlayer)
