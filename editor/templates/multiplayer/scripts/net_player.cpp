// One player. Every game has a copy of every player; the server decides where
// each one really is.
//
// Prediction: the player's own game reads the keyboard into the INPUT
// variables (gatherInput) and moves at once (simulate), without waiting for
// the server. The inputs go to the server, which runs the same simulate() to
// make the real move, and sends the result back to everyone through the
// MultiplayerSynchronizer. If the server's result differs (another player was
// in the way...), the player's game corrects itself without a stutter.
//
// Lag compensation: F shoots. The server checks the hit with everyone moved
// back to where the shooter saw them (lagCompensated), so aiming at what you
// see works even with lag.
#include <cmath>

#include "script.hpp"

using namespace s2d;

class NetPlayer : public Script {
public:
    EXPORT(float, speed, 220);
    EXPORT(float, jumpSpeed, 520);
    EXPORT(float, gravity, 1300);
    EXPORT(int, hits, 0);  // shots that hit someone (counted by the server, synced to everyone)

    // What the player presses: sent to the server 60 times a second
    INPUT(float, moveX, 0);
    INPUT(bool, jump, false);

    void ready() override {
        int id = node()->multiplayerAuthority();
        // Each player gets a color from its id: the same in every game
        static const Color colors[] = {WHITE, {255, 140, 140, 255}, {140, 200, 255, 255}, {150, 240, 150, 255},
                                       {255, 220, 120, 255}, {220, 160, 255, 255}};
        color = colors[id % 6];
        if (auto* sprite = getNode("Sprite")->as<Sprite2D>()) sprite->modulate = color;
        name = "P" + std::to_string(id) + (isMultiplayerAuthority() ? " (you)" : "");
    }

    // On this player's own game, 60 times a second: read the keyboard
    void gatherInput() override {
        moveX = (IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
        jump = IsKeyDown(KEY_SPACE);
    }

    // On the server (the real move) and on this player's own game (the
    // prediction): the same code, so they agree
    void simulate(float delta) override {
        auto* body = self<CharacterBody2D>();
        auto* sprite = getNode("Sprite")->as<Sprite2D>();
        body->velocity.x = moveX * speed;
        body->velocity.y += gravity * delta;
        if (jump && body->isOnFloor()) body->velocity.y = -jumpSpeed;
        body->moveAndSlide();
        if (moveX != 0) sprite->flipH = moveX < 0;
        if (body->position.y > 900) {  // fell off: back to the top
            body->position = Vector2{400, 100};
            body->velocity = Vector2{0, 0};
        }
    }

    // Every frame, on every game
    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        auto* sprite = getNode("Sprite")->as<AnimatedSprite2D>();
        if (!body || !sprite) return;

        if (isMultiplayerAuthority()) {
            if (IsKeyPressed(KEY_E)) {
                rpc("wave");  // on everyone else's copy of this player...
                wave();       // ...and on ours
            }
            if (IsKeyPressed(KEY_F)) {
                float dir = sprite->flipH ? -1.0f : 1.0f;
                rpc("showShot", body->position, dir);  // everyone sees the shot...
                showShot(body->position, dir);
                rpcId(1, "shoot", body->position, dir);  // ...and the server decides what it hit
            }
        }

        // The label: name, hits, and a word now and then
        if (waveTime > 0) waveTime -= delta;
        if (hitTime > 0) hitTime -= delta;
        std::string text = name + (hits > 0 ? "  " + std::to_string(hits) + " hits" : "");
        if (waveTime > 0) text = name + ": hi!";
        if (auto* label = getNode("NameLabel")->as<Label>()) label->text = text;
        sprite->modulate = hitTime > 0 ? Color{255, 60, 60, 255} : color;  // red for a moment when hit

        // The shot's line fades away
        if (beam && (beamTime -= delta) <= 0) {
            beam->queueFree();
            beam = nullptr;
        }

        // Everyone works out the animation from the velocity
        float vx = body->velocity.x, vy = body->velocity.y;
        if (vy < -1) sprite->play("jump");
        else if (vy > 60) sprite->play("fall");
        else if (std::fabs(vx) > 1) sprite->play("run");
        else sprite->play("idle");
    }

    // --- RPCs ---------------------------------------------------------------
    RPC(wave);
    void wave() { waveTime = 1.5f; }

    // A line where the shot went (just for show, in every game)
    RPC(showShot);
    void showShot(Vector2 from, float dir) {
        if (beam) beam->queueFree();
        auto line = createNode("ColorRect");
        auto* rect = static_cast<ColorRect*>(line.get());
        rect->size = Vector2{kRange, 3};
        rect->position = Vector2{dir > 0 ? from.x : from.x - kRange, from.y - 6};
        rect->color = Color{255, 230, 120, 200};
        beam = node()->root()->addChild(std::move(line));
        beamTime = 0.12f;
    }

    // The server: what did the shot hit? Checked where the shooter saw everyone.
    RPC(shoot);
    void shoot(Vector2 from, float dir) {
        if (!multiplayer().isServer() || rpcSender() != node()->multiplayerAuthority()) return;
        Node* victim = nullptr;
        lagCompensated([&] {
            float nearest = kRange;
            for (auto& child : node()->parent()->children()) {
                if (child.get() == node()) continue;
                auto* shape = child->getChild<CollisionShape2D>();
                if (!shape) continue;
                Rectangle r = shape->worldAABB();
                float distance = dir > 0 ? r.x - from.x : from.x - (r.x + r.width);
                if (from.y >= r.y && from.y <= r.y + r.height && distance >= -r.width && distance < nearest) {
                    nearest = distance;
                    victim = child.get();
                }
            }
        });
        if (!victim || !victim->script) return;
        ++hits;                          // synced to everyone (":hits" in the synchronizer)
        victim->script->rpc("wasHit");   // the victim flashes red in every game...
        victim->script->callRpc("wasHit", {});  // ...and here
    }

    RPC(wasHit);
    void wasHit() { hitTime = 0.3f; }

private:
    static constexpr float kRange = 500;
    std::string name;
    Color color = WHITE;
    float waveTime = 0, hitTime = 0;
    Node* beam = nullptr;
    float beamTime = 0;
};

REGISTER_SCRIPT(NetPlayer)
