// Tests for sprout/scene.hpp and sprout/entity.hpp - WITHOUT a window.
//
// Normally Game runs the scene for you. Here the tests play the part of the
// game loop by calling scene.runStart() once and then scene.runUpdate(dt) for
// every frame. (runDraw() is never called: drawing needs a window.)
#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sprout/entity.hpp>
#include <sprout/scene.hpp>
#include <sprout/tilemap.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;
using sprout_test::ApproxVec2;

namespace {

constexpr float kFrame = 1.0f / 60.0f;

/// A scene whose start() and update() can be filled in by each test.
struct TestScene : Scene {
    int starts = 0;
    int updates = 0;
    std::function<void(TestScene&)> onStart;
    std::function<void(TestScene&, float)> onUpdate;

    void start() override {
        ++starts;
        if (onStart) onStart(*this);
    }
    void update(float dt) override {
        ++updates;
        if (onUpdate) onUpdate(*this, dt);
    }
    void runFrames(int frames, float dt = kFrame) {
        for (int i = 0; i < frames; ++i) runUpdate(dt);
    }
};

/// Counts how often its start() and update() run.
struct Counter : Entity {
    int starts = 0;
    int updates = 0;
    float totalTime = 0.0f;
    void start() override { ++starts; }
    void update(float dt) override {
        ++updates;
        totalTime += dt;
    }
};

struct Enemy : Entity {
    int health;
    explicit Enemy(Vec2 where = {}, int hp = 3) : health(hp) { position = where; }
};
struct Boss : Enemy {
    explicit Boss(Vec2 where = {}) : Enemy(where, 100) { size = {64, 64}; }
};
struct Coin : Entity {
    explicit Coin(Vec2 where) {
        position = where;
        size = {16, 16};
    }
};

/// Runs whatever code a test gives it, every frame.
struct Scripted : Entity {
    std::function<void(Scripted&, float)> onUpdate;
    explicit Scripted(std::function<void(Scripted&, float)> code = {}) : onUpdate(std::move(code)) {}
    void update(float dt) override {
        if (onUpdate) onUpdate(*this, dt);
    }
};

/// Writes its name into a shared list when it updates (to check the order).
struct Logger : Entity {
    std::vector<std::string>* log;
    std::string name;
    Logger(std::vector<std::string>* sharedLog, std::string myName, int myLayer) : log(sharedLog), name(std::move(myName)) {
        layer = myLayer;
    }
    void update(float) override { log->push_back(name); }
};

} // namespace

// =============================================================================
//  Spawning
// =============================================================================

TEST_CASE("spawn<T>(...) gives the values to the entity's constructor") {
    TestScene scene;
    scene.runStart();

    Enemy& enemy = scene.spawn<Enemy>(Vec2{10, 20}, 7);
    CHECK(enemy.health == 7);
    CHECK(enemy.position == Vec2{10, 20});

    Enemy& defaults = scene.spawn<Enemy>();
    CHECK(defaults.health == 3);

    SUBCASE("move-only values (like unique_ptr) are moved in") {
        struct Holder : Entity {
            std::unique_ptr<int> value;
            explicit Holder(std::unique_ptr<int> v) : value(std::move(v)) {}
        };
        Holder& holder = scene.spawn<Holder>(std::make_unique<int>(42));
        REQUIRE(holder.value);
        CHECK(*holder.value == 42);
    }
    SUBCASE("references stay references") {
        struct Bumper : Entity {
            explicit Bumper(int& counter) { ++counter; }
        };
        int counter = 0;
        scene.spawn<Bumper>(counter);
        CHECK(counter == 1);
    }
}

TEST_CASE("start() runs once, right when the entity is spawned, and scene() works there") {
    struct NeedsScene : Entity {
        Scene* seenInStart = nullptr;
        int enemiesInStart = -1;
        void start() override {
            seenInStart = &scene();
            enemiesInStart = scene().count<Enemy>();
        }
    };

    TestScene scene;
    scene.runStart();
    scene.spawn<Enemy>();
    NeedsScene& entity = scene.spawn<NeedsScene>();
    CHECK(entity.seenInStart == &scene);
    CHECK(entity.enemiesInStart == 1);
    CHECK(&entity.scene() == &scene);

    Counter& counter = scene.spawn<Counter>();
    CHECK(counter.starts == 1);
    scene.runFrames(5);
    CHECK(counter.starts == 1); // never again
}

TEST_CASE("scene() throws std::logic_error outside a scene") {
    SUBCASE("an entity that was never spawned") {
        Counter loose;
        CHECK_THROWS_AS(loose.scene(), std::logic_error);
    }
    SUBCASE("inside the constructor (the entity isn't in the scene yet)") {
        struct TooEarly : Entity {
            bool threw = false;
            std::string message;
            TooEarly() {
                try {
                    scene();
                } catch (const std::logic_error& error) {
                    threw = true;
                    message = error.what();
                }
            }
        };
        TestScene scene;
        TooEarly& entity = scene.spawn<TooEarly>();
        CHECK(entity.threw);
        CHECK(entity.message.find("start()") != std::string::npos); // the message tells you what to do
        CHECK_NOTHROW(entity.scene());                               // ...and after spawning it works
    }
    SUBCASE("findOverlapping needs a scene too") {
        Coin loose{Vec2{0, 0}};
        CHECK_THROWS_AS(loose.findOverlapping<Coin>(), std::logic_error);
    }
}

TEST_CASE("runStart runs the scene's start() and keeps what it spawned") {
    TestScene scene;
    scene.onStart = [](TestScene& s) {
        s.spawn<Enemy>(Vec2{1, 1});
        s.spawn<Enemy>(Vec2{2, 2});
    };
    scene.runStart();
    CHECK(scene.starts == 1);
    CHECK(scene.count<Enemy>() == 2);
    scene.runFrames(1);
    CHECK(scene.count<Enemy>() == 2);
}

TEST_CASE("entities spawned in the scene's constructor are kept") {
    struct BuildsInConstructor : Scene {
        BuildsInConstructor() { spawn<Enemy>(); }
    };
    BuildsInConstructor scene;
    scene.runStart();
    scene.runUpdate(kFrame);
    CHECK(scene.count<Enemy>() == 1);
}

// =============================================================================
//  Updating
// =============================================================================

TEST_CASE("every entity's update() runs once per frame with the frame's dt") {
    TestScene scene;
    scene.runStart();
    Counter& a = scene.spawn<Counter>();
    Counter& b = scene.spawn<Counter>();

    scene.runUpdate(0.5f);
    CHECK(a.updates == 1);
    CHECK(b.updates == 1);
    CHECK(a.totalTime == Approx(0.5f));

    scene.runFrames(9, 0.25f);
    CHECK(a.updates == 10);
    CHECK(b.updates == 10);
    CHECK(a.totalTime == Approx(2.75f));
    CHECK(scene.updates == 10); // the scene's own update() too
}

TEST_CASE("layers only change drawing order: every layer is updated, in spawn order") {
    std::vector<std::string> log;
    TestScene scene;
    scene.runStart();
    scene.spawn<Logger>(&log, "front", 10);
    scene.spawn<Logger>(&log, "back", -5);
    scene.spawn<Logger>(&log, "middle", 0);
    scene.runUpdate(kFrame);
    CHECK(log == std::vector<std::string>{"front", "back", "middle"});
}

TEST_CASE("the scene's update() runs before the entities' update()") {
    std::vector<std::string> log;
    TestScene scene;
    scene.onUpdate = [&](TestScene&, float) { log.push_back("scene"); };
    scene.runStart();
    scene.spawn<Logger>(&log, "entity", 0);
    scene.runUpdate(kFrame);
    CHECK(log == std::vector<std::string>{"scene", "entity"});
}

// =============================================================================
//  Destroying
// =============================================================================

TEST_CASE("destroy() removes the entity at the end of the frame") {
    TestScene scene;
    scene.runStart();
    Enemy& doomed = scene.spawn<Enemy>();
    Enemy& survivor = scene.spawn<Enemy>();
    scene.runUpdate(kFrame);
    REQUIRE(scene.count<Enemy>() == 2);

    doomed.destroy();
    CHECK(doomed.isDestroyed());
    CHECK_FALSE(survivor.isDestroyed());
    // Searches ignore destroyed entities straight away...
    CHECK(scene.count<Enemy>() == 1);
    CHECK(scene.findFirst<Enemy>() == &survivor);
    const std::vector<Enemy*> all = scene.findAll<Enemy>();
    REQUIRE(all.size() == 1);
    CHECK(all[0] == &survivor);

    // ...and the entity is really gone after the frame.
    scene.runUpdate(kFrame);
    CHECK(scene.count<Enemy>() == 1);
    CHECK(scene.count() == 1);
}

TEST_CASE("an entity destroyed earlier in the frame doesn't update any more") {
    TestScene scene;
    scene.runStart();
    // The victim is deleted at the end of the frame, so count its updates outside of it.
    int victimUpdates = 0;
    Scripted* victim = nullptr;
    scene.spawn<Scripted>([&](Scripted&, float) {
        if (victim) victim->destroy();
        victim = nullptr; // don't touch it again after this frame
    });
    victim = &scene.spawn<Scripted>([&](Scripted&, float) { ++victimUpdates; });
    scene.runUpdate(kFrame);
    CHECK(victimUpdates == 0); // destroyed by the entity before it, so it was skipped
    CHECK(scene.count() == 1);
    scene.runUpdate(kFrame);
    CHECK(victimUpdates == 0);
}

TEST_CASE("an entity can destroy itself in update() or start()") {
    struct Kamikaze : Entity {
        void start() override { destroy(); }
    };
    TestScene scene;
    scene.runStart();
    scene.spawn<Kamikaze>();
    CHECK(scene.count<Kamikaze>() == 0);
    scene.spawn<Scripted>([](Scripted& self, float) { self.destroy(); });
    CHECK(scene.count() == 1);
    scene.runUpdate(kFrame);
    CHECK(scene.count() == 0);
    scene.runFrames(3);
    CHECK(scene.count() == 0);
}

TEST_CASE("destroyAll<T>() destroys only that type (and types that inherit from it)") {
    TestScene scene;
    scene.runStart();
    scene.spawn<Enemy>();
    scene.spawn<Boss>();
    scene.spawn<Coin>(Vec2{0, 0});
    scene.spawn<Coin>(Vec2{0, 0});

    SUBCASE("Enemy also destroys Boss, because a Boss is an Enemy") {
        scene.destroyAll<Enemy>();
        CHECK(scene.count<Enemy>() == 0);
        CHECK(scene.count<Boss>() == 0);
        CHECK(scene.count<Coin>() == 2);
    }
    SUBCASE("Boss leaves plain enemies alone") {
        scene.destroyAll<Boss>();
        CHECK(scene.count<Enemy>() == 1);
        CHECK(scene.count<Coin>() == 2);
    }
    SUBCASE("no type = everything") {
        scene.destroyAll();
        CHECK(scene.count() == 0);
    }
}

// =============================================================================
//  Spawning during a frame
// =============================================================================

TEST_CASE("entities spawned during update() are not lost") {
    TestScene scene;
    scene.runStart();

    SUBCASE("spawned by another entity: findAll sees them in the same frame") {
        int foundInSameFrame = -1;
        bool spawned = false;
        scene.spawn<Scripted>([&](Scripted& self, float) {
            if (!spawned) {
                spawned = true;
                for (int i = 0; i < 3; ++i) self.scene().spawn<Enemy>(Vec2{i, 0});
                foundInSameFrame = static_cast<int>(self.scene().findAll<Enemy>().size());
            }
        });
        scene.runUpdate(kFrame);
        CHECK(foundInSameFrame == 3);
        CHECK(scene.count<Enemy>() == 3);
        scene.runFrames(2);
        CHECK(scene.count<Enemy>() == 3);
    }
    SUBCASE("spawned by the scene's update()") {
        scene.onUpdate = [](TestScene& s, float) {
            if (s.updates <= 3) s.spawn<Enemy>();
        };
        scene.runFrames(10);
        CHECK(scene.count<Enemy>() == 3);
    }
    SUBCASE("they start right away and update from the next frame on") {
        Counter* child = nullptr;
        scene.spawn<Scripted>([&](Scripted& self, float) {
            if (!child) child = &self.scene().spawn<Counter>();
        });
        scene.runUpdate(kFrame);
        REQUIRE(child != nullptr);
        CHECK(child->starts == 1);
        CHECK(child->updates == 0);
        scene.runUpdate(kFrame);
        CHECK(child->updates == 1);
    }
    SUBCASE("a chain reaction: entities that spawn entities that spawn entities") {
        struct Splitter : Entity {
            int generation;
            explicit Splitter(int gen) : generation(gen) {}
            void start() override {
                if (generation > 0) {
                    scene().spawn<Splitter>(generation - 1);
                    scene().spawn<Splitter>(generation - 1);
                }
            }
        };
        scene.spawn<Splitter>(4); // 1 + 2 + 4 + 8 + 16 entities
        CHECK(scene.count<Splitter>() == 31);
        scene.runUpdate(kFrame);
        CHECK(scene.count<Splitter>() == 31);
    }
    SUBCASE("spawned and destroyed in the same frame") {
        scene.spawn<Scripted>([](Scripted& self, float) {
            if (self.scene().count<Enemy>() == 0) self.scene().spawn<Enemy>().destroy();
        });
        scene.runUpdate(kFrame);
        CHECK(scene.count<Enemy>() == 0);
    }
}

// =============================================================================
//  Finding entities
// =============================================================================

TEST_CASE("findAll, findFirst, count and findClosest understand inheritance") {
    TestScene scene;
    scene.runStart();
    Enemy& grunt = scene.spawn<Enemy>(Vec2{100, 0});
    Boss& boss = scene.spawn<Boss>(Vec2{300, 0});
    Coin& coin = scene.spawn<Coin>(Vec2{10, 0});

    CHECK(scene.count() == 3);
    CHECK(scene.count<Entity>() == 3);
    CHECK(scene.count<Enemy>() == 2); // a Boss is also an Enemy
    CHECK(scene.count<Boss>() == 1);
    CHECK(scene.count<Coin>() == 1);
    CHECK(scene.count<Counter>() == 0);

    const std::vector<Enemy*> enemies = scene.findAll<Enemy>();
    REQUIRE(enemies.size() == 2);
    CHECK(enemies[0] == &grunt); // in spawn order
    CHECK(enemies[1] == &boss);
    CHECK(scene.findAll<Counter>().empty());
    CHECK(scene.findAll().size() == 3);

    CHECK(scene.findFirst<Enemy>() == &grunt);
    CHECK(scene.findFirst<Boss>() == &boss);
    CHECK(scene.findFirst<Coin>() == &coin);
    CHECK(scene.findFirst<Counter>() == nullptr);

    SUBCASE("findClosest") {
        CHECK(scene.findClosest<Enemy>(Vec2{0, 0}) == &grunt);
        CHECK(scene.findClosest<Enemy>(Vec2{250, 0}) == &boss);
        CHECK(scene.findClosest<Boss>(Vec2{0, 0}) == &boss);
        CHECK(scene.findClosest(Vec2{0, 0}) == &coin); // any entity
        CHECK(scene.findClosest<Counter>(Vec2{0, 0}) == nullptr);
        boss.destroy();
        CHECK(scene.findClosest<Enemy>(Vec2{1000, 0}) == &grunt);
    }
    SUBCASE("an empty scene") {
        TestScene empty;
        CHECK(empty.count() == 0);
        CHECK(empty.findAll().empty());
        CHECK(empty.findFirst() == nullptr);
        CHECK(empty.findClosest(Vec2{0, 0}) == nullptr);
    }
}

TEST_CASE("Entity bounds and overlaps") {
    Coin a{Vec2{100, 100}}; // 16x16, centered
    CHECK(a.bounds() == Rect{92, 92, 16, 16});
    Coin b{Vec2{110, 100}};
    Coin touching{Vec2{116, 100}}; // edges touch: not overlapping
    CHECK(a.overlaps(b));
    CHECK(b.overlaps(a));
    CHECK_FALSE(a.overlaps(touching));
    CHECK(a.overlaps(Rect{0, 0, 93, 93}));
    CHECK_FALSE(a.overlaps(Rect{0, 0, 92, 92}));
}

TEST_CASE("findOverlapping and findAllOverlapping") {
    struct Player : Entity {
        Player() { size = {32, 32}; }
    };
    TestScene scene;
    scene.runStart();
    Player& player = scene.spawn<Player>(); // at 0,0: covers -16..16
    Coin& near1 = scene.spawn<Coin>(Vec2{10, 0});
    Coin& near2 = scene.spawn<Coin>(Vec2{-10, 10});
    scene.spawn<Coin>(Vec2{100, 100}); // far away
    scene.spawn<Coin>(Vec2{24, 0});    // exactly touching (16 + 8): doesn't count
    Enemy& enemy = scene.spawn<Enemy>(Vec2{0, 5});

    CHECK(player.findOverlapping<Coin>() == &near1);
    const std::vector<Coin*> coins = player.findAllOverlapping<Coin>();
    REQUIRE(coins.size() == 2);
    CHECK(coins[0] == &near1);
    CHECK(coins[1] == &near2);

    CHECK(player.findOverlapping<Enemy>() == &enemy);
    CHECK(player.findOverlapping<Boss>() == nullptr);

    SUBCASE("an entity never finds itself") {
        CHECK(player.findOverlapping<Player>() == nullptr);
        const std::vector<Entity*> everything = player.findAllOverlapping<Entity>();
        CHECK(everything.size() == 3); // near1, near2 and the enemy, but not the player
        for (Entity* e : everything) CHECK(e != &player);
    }
    SUBCASE("destroyed entities are not found") {
        near1.destroy();
        CHECK(player.findOverlapping<Coin>() == &near2);
        near2.destroy();
        CHECK(player.findOverlapping<Coin>() == nullptr);
        CHECK(player.findAllOverlapping<Coin>().empty());
    }
}

// =============================================================================
//  Pausing
// =============================================================================

TEST_CASE("paused stops entities, timers and particles, but the scene's update() still runs") {
    TestScene scene;
    scene.runStart();
    Counter& counter = scene.spawn<Counter>();
    int timerCalls = 0;
    scene.every(0.1f, [&] { ++timerCalls; });
    scene.particles.add(Particle{{0, 0}, {10, 0}, Color::White, 4.0f, 1.0f, 1.0f});

    scene.paused = true;
    scene.runFrames(60);
    CHECK(scene.updates == 60);
    CHECK(counter.updates == 0);
    CHECK(timerCalls == 0);
    CHECK(scene.particles.count() == 1); // frozen, not dying

    SUBCASE("the scene can un-pause itself from its own update()") {
        scene.onUpdate = [](TestScene& s, float) { s.paused = false; };
        scene.runUpdate(0.1f);
        CHECK_FALSE(scene.paused);
        CHECK(counter.updates == 1);
        CHECK(timerCalls == 1);
    }
    SUBCASE("spawning and destroying still work while paused") {
        scene.spawn<Enemy>();
        counter.destroy();
        scene.runUpdate(kFrame);
        CHECK(scene.count<Enemy>() == 1);
        CHECK(scene.count<Counter>() == 0);
    }
}

// =============================================================================
//  after() and every()
// =============================================================================

TEST_CASE("after() runs code once, after a delay") {
    TestScene scene;
    scene.runStart();
    int calls = 0;
    scene.after(1.0f, [&] { ++calls; });

    scene.runUpdate(0.5f);
    CHECK(calls == 0);
    scene.runUpdate(0.25f);
    CHECK(calls == 0);
    scene.runUpdate(0.25f);
    CHECK(calls == 1); // exactly 1 second
    scene.runFrames(100, 0.5f);
    CHECK(calls == 1); // never again

    SUBCASE("after(0) runs on the next frame") {
        int soon = 0;
        scene.after(0.0f, [&] { ++soon; });
        CHECK(soon == 0);
        scene.runUpdate(kFrame);
        CHECK(soon == 1);
    }
}

TEST_CASE("after() can spawn entities and change the scene") {
    TestScene scene;
    scene.runStart();
    scene.after(0.5f, [&] { scene.spawn<Enemy>(); });
    scene.runFrames(29);
    CHECK(scene.count<Enemy>() == 0);
    scene.runFrames(2);
    CHECK(scene.count<Enemy>() == 1);
}

TEST_CASE("every() runs code again and again") {
    TestScene scene;
    scene.runStart();
    int calls = 0;
    scene.every(1.0f, [&] { ++calls; });

    scene.runUpdate(0.5f);
    CHECK(calls == 0);
    scene.runUpdate(0.5f);
    CHECK(calls == 1);
    scene.runUpdate(1.0f);
    CHECK(calls == 2);

    SUBCASE("a big dt runs it as many times as it should have run") {
        scene.runUpdate(3.5f);
        CHECK(calls == 5);
        scene.runUpdate(0.5f); // the half second left over is not forgotten
        CHECK(calls == 6);
    }
    SUBCASE("60 fps for 10 seconds") {
        scene.runFrames(600);
        CHECK(calls >= 11);
        CHECK(calls <= 12);
    }
}

TEST_CASE("every() with zero seconds doesn't hang") {
    TestScene scene;
    scene.runStart();
    int calls = 0;
    scene.every(0.0f, [&] { ++calls; });
    scene.runUpdate(kFrame);
    CHECK(calls > 0);
    CHECK(calls < 100000);
}

TEST_CASE("cancelTimer stops a timer, cancelAllTimers stops them all") {
    TestScene scene;
    scene.runStart();
    int a = 0, b = 0, c = 0;
    const int idA = scene.every(0.1f, [&] { ++a; });
    const int idB = scene.after(0.5f, [&] { ++b; });
    scene.every(0.1f, [&] { ++c; });
    CHECK(idA != idB);

    scene.runUpdate(0.1f);
    CHECK(a == 1);
    scene.cancelTimer(idA);
    scene.cancelTimer(idB);
    scene.cancelTimer(12345); // unknown ids are ignored
    scene.runFrames(10, 0.1f);
    CHECK(a == 1);
    CHECK(b == 0);
    CHECK(c == 11);

    scene.cancelAllTimers();
    scene.runFrames(10, 0.1f);
    CHECK(c == 11);

    SUBCASE("new timers work after cancelAllTimers") {
        int d = 0;
        scene.after(0.1f, [&] { ++d; });
        scene.runUpdate(0.1f);
        CHECK(d == 1);
    }
}

TEST_CASE("a repeating timer can cancel itself from its own callback") {
    TestScene scene;
    scene.runStart();
    int calls = 0;
    int id = 0;
    id = scene.every(0.1f, [&] {
        ++calls;
        if (calls == 3) scene.cancelTimer(id);
    });
    scene.runUpdate(1.0f); // would fire 10 times if it didn't stop
    CHECK(calls == 3);
    scene.runUpdate(1.0f);
    CHECK(calls == 3);
}

TEST_CASE("a timer callback can add another timer") {
    TestScene scene;
    scene.runStart();
    std::vector<std::string> log;
    scene.after(0.5f, [&] {
        log.push_back("first");
        scene.after(0.5f, [&] {
            log.push_back("second");
            scene.after(0.5f, [&] { log.push_back("third"); });
        });
    });
    scene.runFrames(4, 0.125f);
    CHECK(log == std::vector<std::string>{"first"});
    scene.runFrames(4, 0.125f);
    CHECK(log == std::vector<std::string>{"first", "second"});
    scene.runFrames(3, 0.125f);
    CHECK(log == std::vector<std::string>{"first", "second"});
    scene.runFrames(1, 0.125f);
    CHECK(log == std::vector<std::string>{"first", "second", "third"});

    SUBCASE("adding lots of timers from a callback (the list grows while it is being run)") {
        int fired = 0;
        scene.every(0.1f, [&] {
            for (int i = 0; i < 50; ++i) scene.after(0.1f, [&] { ++fired; });
        });
        scene.runFrames(10, 0.1f);
        CHECK(fired == 450); // 9 frames' worth have fired, the last 50 are still waiting
    }
}

TEST_CASE("a timer callback can call cancelAllTimers") {
    TestScene scene;
    scene.runStart();
    int ticks = 0;
    int other = 0;
    scene.every(0.1f, [&] {
        ++ticks;
        if (ticks == 2) scene.cancelAllTimers();
    });
    scene.every(0.1f, [&] { ++other; });

    scene.runUpdate(0.1f);
    CHECK(ticks == 1);
    CHECK(other == 1);
    scene.runUpdate(0.1f);
    CHECK(ticks == 2);
    CHECK(other == 1); // cancelled before its turn this frame
    scene.runFrames(10, 0.1f);
    CHECK(ticks == 2);
    CHECK(other == 1);

    SUBCASE("...and then start a fresh timer in the same callback") {
        int fresh = 0;
        scene.after(0.1f, [&] {
            scene.cancelAllTimers();
            scene.every(0.1f, [&] { ++fresh; });
        });
        scene.runUpdate(0.1f);
        CHECK(fresh == 0);
        scene.runFrames(3, 0.1f);
        CHECK(fresh == 3);
    }
}

TEST_CASE("timers are owned by the scene: a new scene has none") {
    int calls = 0;
    {
        TestScene first;
        first.after(0.1f, [&] { ++calls; });
    }
    TestScene second;
    second.runStart();
    second.runFrames(10, 0.1f);
    CHECK(calls == 0);
}

// =============================================================================
//  moveAndSlide
// =============================================================================

namespace {
/// A platformer-style box: gravity pulls it down, it walks at `walkSpeed`.
struct Walker : Entity {
    float gravity = 1200.0f;
    float walkSpeed = 0.0f;
    MoveResult last;
    Walker(Vec2 where, Vec2 hitbox) {
        position = where;
        size = hitbox;
    }
    void update(float dt) override {
        velocity.x = walkSpeed;
        velocity.y += gravity * dt;
        last = moveAndSlide(dt);
    }
};
} // namespace

TEST_CASE("moveAndSlide with addSolid(Rect): floors, walls and ceilings") {
    TestScene scene;
    scene.runStart();
    scene.addSolid(Rect{0, 300, 800, 20});  // floor
    scene.addSolid(Rect{500, 0, 20, 300});  // wall on the right
    scene.addSolid(Rect{0, 0, 20, 300});    // wall on the left
    scene.addSolid(Rect{0, -20, 800, 20});  // ceiling
    CHECK(scene.solidRects().size() == 4);

    SUBCASE("falling onto the floor") {
        Walker& walker = scene.spawn<Walker>(Vec2{100, 100}, Vec2{20, 20});
        scene.runFrames(120);
        CHECK(walker.isOnFloor());
        CHECK_FALSE(walker.isOnWall());
        CHECK_FALSE(walker.isOnCeiling());
        CHECK(walker.bounds().bottom() == Approx(300.0f));
        CHECK(walker.velocity.y == 0.0f); // landing stops the fall
        CHECK(walker.last.hitBottom);
    }
    SUBCASE("walking into a wall stops the sideways speed") {
        Walker& walker = scene.spawn<Walker>(Vec2{400, 290}, Vec2{20, 20});
        walker.walkSpeed = 300.0f;
        scene.runFrames(60);
        CHECK(walker.isOnWall());
        CHECK(walker.isOnFloor());
        CHECK(walker.bounds().right() == Approx(500.0f));
        CHECK(walker.velocity.x == 0.0f);
    }
    SUBCASE("the left wall") {
        Walker& walker = scene.spawn<Walker>(Vec2{100, 290}, Vec2{20, 20});
        walker.walkSpeed = -300.0f;
        scene.runFrames(60);
        CHECK(walker.isOnWall());
        CHECK(walker.bounds().left() == Approx(20.0f));
    }
    SUBCASE("jumping into the ceiling") {
        Walker& walker = scene.spawn<Walker>(Vec2{100, 100}, Vec2{20, 20});
        walker.velocity = {0, -2000};
        walker.gravity = 0.0f;
        scene.runUpdate(kFrame * 5);
        CHECK(walker.isOnCeiling());
        CHECK(walker.bounds().top() == Approx(0.0f));
        CHECK(walker.velocity.y == 0.0f);
    }
    SUBCASE("landing keeps the sideways speed (sliding)") {
        struct Slider : Entity {
            void update(float dt) override { moveAndSlide(dt); }
        };
        Slider& slider = scene.spawn<Slider>();
        slider.position = {100, 280};
        slider.size = {20, 20};
        slider.velocity = {120, 600};
        scene.runUpdate(0.1f);
        CHECK(slider.isOnFloor());
        CHECK(slider.velocity.x == 120.0f);
        CHECK(slider.velocity.y == 0.0f);
        CHECK(slider.position.x == Approx(112.0f));
    }
    SUBCASE("clearSolids removes everything") {
        scene.clearSolids();
        CHECK(scene.solidRects().empty());
        Walker& walker = scene.spawn<Walker>(Vec2{100, 100}, Vec2{20, 20});
        scene.runFrames(60);
        CHECK_FALSE(walker.isOnFloor());
        CHECK(walker.position.y > 400.0f);
    }
}

TEST_CASE("moveAndSlide flags are only true after touching something") {
    TestScene scene;
    scene.runStart();
    struct Floater : Entity {
        void update(float dt) override { moveAndSlide(dt); }
    };
    Floater& floater = scene.spawn<Floater>();
    floater.velocity = {50, 50};
    scene.runFrames(10);
    CHECK_FALSE(floater.isOnFloor());
    CHECK_FALSE(floater.isOnWall());
    CHECK_FALSE(floater.isOnCeiling());
    CHECK(floater.position == ApproxVec2(50.0 * 10 / 60, 50.0 * 10 / 60));
    CHECK(floater.velocity == Vec2{50, 50}); // nothing hit: velocity unchanged
}

TEST_CASE("moveAndSlide outside a scene just moves") {
    Counter loose;
    loose.velocity = {100, -50};
    const MoveResult result = loose.moveAndSlide(0.5f);
    CHECK_FALSE(result.hitAnything());
    CHECK(loose.position == ApproxVec2(50, -25));
}

TEST_CASE("moveAndSlide with addSolid(TileMap)") {
    struct Level : TestScene {
        TileMap map{32, {
            "##########################################",
            "#........................................#",
            "#........................................#",
            "#........................................#",
            "#..........................###...........#",
            "##########################################",
        }};
        void start() override {
            map.setSolid("#");
            addSolid(map);
            addSolid(map); // adding the same map twice is harmless
        }
    };
    Level scene;
    scene.runStart();
    CHECK(scene.solidMaps().size() == 1);

    SUBCASE("walking across the tile floor without getting stuck on the seams") {
        // 28.7 pixels tall: an "awkward" size that causes rounding errors.
        Walker& walker = scene.spawn<Walker>(Vec2{50, 100}, Vec2{20, 28.7f});
        walker.walkSpeed = 200.0f;
        scene.runFrames(30); // fall onto the floor first
        REQUIRE(walker.isOnFloor());
        float lastX = walker.position.x;
        for (int frame = 0; frame < 150; ++frame) {
            CAPTURE(frame);
            scene.runUpdate(kFrame);
            REQUIRE(walker.isOnFloor());
            REQUIRE_FALSE(walker.isOnWall());
            REQUIRE(walker.position.x - lastX == Approx(200.0f * kFrame).epsilon(0.01));
            REQUIRE(walker.bounds().bottom() == Approx(160.0f));
            lastX = walker.position.x;
        }
    }
    SUBCASE("walking into the raised block") {
        Walker& walker = scene.spawn<Walker>(Vec2{700, 140}, Vec2{20, 20});
        walker.walkSpeed = 300.0f;
        scene.runFrames(120);
        CHECK(walker.isOnWall());
        CHECK(walker.bounds().right() == Approx(27 * 32.0f));
    }
    SUBCASE("landing on the raised block") {
        Walker& walker = scene.spawn<Walker>(Vec2{28 * 32 + 16, 60}, Vec2{20, 20});
        scene.runFrames(120);
        CHECK(walker.isOnFloor());
        CHECK(walker.bounds().bottom() == Approx(128.0f));
    }
    SUBCASE("rects and tile maps together") {
        scene.addSolid(Rect{100, 100, 50, 10}); // a floating platform
        Walker& walker = scene.spawn<Walker>(Vec2{125, 60}, Vec2{20, 20});
        scene.runFrames(120);
        CHECK(walker.isOnFloor());
        CHECK(walker.bounds().bottom() == Approx(100.0f));
    }
}

TEST_CASE("a player taller than a tile, spawned on its 'P' marker, stands on the floor") {
    // The usual way to place the player: at the middle of the 'P' tile. A hitbox
    // taller than one tile then starts a few pixels inside the floor below it.
    struct Level : TestScene {
        TileMap map{32, {
            "##########",
            "#........#",
            "#...P....#",
            "##########",
        }};
        void start() override {
            map.setSolid("#");
            addSolid(map);
            for (Vec2 spot : map.findTiles('P')) spawn<Walker>(spot, Vec2{24, 40});
        }
    };
    Level scene;
    scene.runStart();
    Walker* player = scene.findFirst<Walker>();
    REQUIRE(player != nullptr);
    CHECK(player->bounds().bottom() == Approx(100.0f)); // starts 4 pixels inside the floor (y = 96)
    scene.runFrames(60);
    CHECK(player->isOnFloor());
    CHECK(player->bounds().bottom() == Approx(96.0f));
}

TEST_CASE("an entity whose hitbox grows while standing stays on the floor") {
    TestScene scene;
    scene.runStart();
    scene.addSolid(Rect{0, 300, 800, 20});
    Walker& walker = scene.spawn<Walker>(Vec2{100, 290}, Vec2{20, 20});
    scene.runFrames(10);
    REQUIRE(walker.isOnFloor());
    walker.size = {20, 40}; // stands up / grows - the middle stays put, so the feet go 10 px into the floor
    scene.runFrames(30);
    CHECK(walker.isOnFloor());
    CHECK(walker.bounds().bottom() == Approx(300.0f));
}

TEST_CASE("an entity spawned a little inside the ceiling falls out of it and lands") {
    TestScene scene;
    scene.runStart();
    scene.addSolid(Rect{0, 0, 400, 32});   // ceiling
    scene.addSolid(Rect{0, 200, 400, 32}); // floor
    Walker& walker = scene.spawn<Walker>(Vec2{100, 40}, Vec2{20, 20}); // top at y = 30: 2 px inside the ceiling
    scene.runFrames(60);
    CHECK(walker.isOnFloor());
    CHECK(walker.bounds().bottom() == Approx(200.0f));
}

TEST_CASE("which frame newly spawned entities first update in") {
    TestScene scene;
    scene.runStart();

    SUBCASE("spawned between frames: updated in the next frame") {
        Counter& counter = scene.spawn<Counter>();
        scene.runUpdate(kFrame);
        CHECK(counter.updates == 1);
    }
    SUBCASE("spawned by the scene's update(): updated in that same frame") {
        Counter* counter = nullptr;
        scene.onUpdate = [&](TestScene& s, float) {
            if (!counter) counter = &s.spawn<Counter>();
        };
        scene.runUpdate(kFrame);
        REQUIRE(counter != nullptr);
        CHECK(counter->updates == 1);
    }
    SUBCASE("spawned by after(): updated in that same frame") {
        Counter* counter = nullptr;
        scene.after(0.0f, [&] { counter = &scene.spawn<Counter>(); });
        scene.runUpdate(kFrame);
        REQUIRE(counter != nullptr);
        CHECK(counter->updates == 1);
    }
    SUBCASE("spawned by another entity's update(): updated from the next frame") {
        Counter* counter = nullptr;
        scene.spawn<Scripted>([&](Scripted& self, float) {
            if (!counter) counter = &self.scene().spawn<Counter>();
        });
        scene.runUpdate(kFrame);
        REQUIRE(counter != nullptr);
        CHECK(counter->updates == 0);
        CHECK(scene.findFirst<Counter>() == counter); // ...but it can already be found
        scene.runUpdate(kFrame);
        CHECK(counter->updates == 1);
    }
}
