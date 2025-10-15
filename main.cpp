#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <emscripten/html5.h>
#endif

#ifdef __EMSCRIPTEN__
EM_JS(void, EnsureHeapViewsExported, (), {
    Module.HEAP8 = HEAP8;
    Module.HEAP16 = HEAP16;
    Module.HEAP32 = HEAP32;
    Module.HEAPU8 = HEAPU8;
    Module.HEAPU16 = HEAPU16;
    Module.HEAPU32 = HEAPU32;
    Module.HEAPF32 = HEAPF32;
    Module.HEAPF64 = HEAPF64;
});

EM_JS(void, InitializeHeapSynchronization, (), {
    Module.HEAP8 = HEAP8;
    Module.HEAP16 = HEAP16;
    Module.HEAP32 = HEAP32;
    Module.HEAPU8 = HEAPU8;
    Module.HEAPU16 = HEAPU16;
    Module.HEAPU32 = HEAPU32;
    Module.HEAPF32 = HEAPF32;
    Module.HEAPF64 = HEAPF64;
    Module.onMemoryGrowth = function() {
        Module.HEAP8 = HEAP8;
        Module.HEAP16 = HEAP16;
        Module.HEAP32 = HEAP32;
        Module.HEAPU8 = HEAPU8;
        Module.HEAPU16 = HEAPU16;
        Module.HEAPU32 = HEAPU32;
        Module.HEAPF32 = HEAPF32;
        Module.HEAPF64 = HEAPF64;
    };
});

#endif

struct GameConfig {
    float powerUpIntervalMin = 8.f;
    float powerUpIntervalMax = 14.f;
    float enemyCountMultiplier = 1.f;
    float enemyDropChance = 0.22f;
    int startingWave = 1;
};

static std::string TrimString(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) start++;
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(start, end - start);
}

static bool ExtractFloatValue(const std::string& text, const std::string& key, float& out) {
    const std::string pattern = "\"" + key + "\"";
    size_t pos = text.find(pattern);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;
    size_t end = text.find_first_of(",}", pos + 1);
    if (end == std::string::npos) end = text.size();
    std::string raw = TrimString(text.substr(pos + 1, end - pos - 1));
    if (raw.empty()) return false;
    char* endPtr = nullptr;
    float value = std::strtof(raw.c_str(), &endPtr);
    if (endPtr == raw.c_str()) return false;
    out = value;
    return true;
}

static bool ExtractIntValue(const std::string& text, const std::string& key, int& out) {
    const std::string pattern = "\"" + key + "\"";
    size_t pos = text.find(pattern);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;
    size_t end = text.find_first_of(",}", pos + 1);
    if (end == std::string::npos) end = text.size();
    std::string raw = TrimString(text.substr(pos + 1, end - pos - 1));
    if (raw.empty()) return false;
    char* endPtr = nullptr;
    long value = std::strtol(raw.c_str(), &endPtr, 10);
    if (endPtr == raw.c_str()) return false;
    out = static_cast<int>(value);
    return true;
}

static bool ParseGameConfig(const std::string& text, GameConfig& outConfig) {
    bool updated = false;
    float fValue = 0.f;
    int iValue = 0;

    if (ExtractFloatValue(text, "powerUpIntervalMin", fValue)) {
        outConfig.powerUpIntervalMin = fValue;
        updated = true;
    }
    if (ExtractFloatValue(text, "powerUpIntervalMax", fValue)) {
        outConfig.powerUpIntervalMax = fValue;
        updated = true;
    }
    if (ExtractFloatValue(text, "enemyCountMultiplier", fValue)) {
        outConfig.enemyCountMultiplier = fValue;
        updated = true;
    }
    if (ExtractFloatValue(text, "enemyDropChance", fValue)) {
        outConfig.enemyDropChance = fValue;
        updated = true;
    }
    if (ExtractIntValue(text, "startingWave", iValue)) {
        outConfig.startingWave = iValue;
        updated = true;
    }
    return updated;
}

static void SanitizeGameConfig(GameConfig& config) {
    if (config.powerUpIntervalMin < 0.5f) config.powerUpIntervalMin = 0.5f;
    if (config.powerUpIntervalMax < config.powerUpIntervalMin) config.powerUpIntervalMax = config.powerUpIntervalMin;
    if (config.enemyCountMultiplier < 0.25f) config.enemyCountMultiplier = 0.25f;
    if (config.enemyCountMultiplier > 3.f) config.enemyCountMultiplier = 3.f;
    if (config.enemyDropChance < 0.f) config.enemyDropChance = 0.f;
    if (config.enemyDropChance > 1.f) config.enemyDropChance = 1.f;
    if (config.startingWave < 1) config.startingWave = 1;
}

#ifdef __EMSCRIPTEN__
static GameConfig* gGameConfigPtr = nullptr;
static bool* gConfigLoadedPtr = nullptr;
static std::string* gConfigStatusPtr = nullptr;

extern "C" void OnConfigFetchSucceeded(emscripten_fetch_t* fetch) {
    std::string response(fetch->data, fetch->numBytes);
    GameConfig parsed = gGameConfigPtr ? *gGameConfigPtr : GameConfig{};
    if (ParseGameConfig(response, parsed)) {
        SanitizeGameConfig(parsed);
        if (gGameConfigPtr) *gGameConfigPtr = parsed;
        if (gConfigStatusPtr) *gConfigStatusPtr = "Configuration loaded.";
    } else {
        if (gConfigStatusPtr) *gConfigStatusPtr = "Config file missing data; using defaults.";
    }
    if (gConfigLoadedPtr) *gConfigLoadedPtr = true;
    emscripten_fetch_close(fetch);
}

extern "C" void OnConfigFetchFailed(emscripten_fetch_t* fetch) {
    if (gConfigStatusPtr) *gConfigStatusPtr = "Config fetch failed; using defaults.";
    if (gConfigLoadedPtr) *gConfigLoadedPtr = true;
    emscripten_fetch_close(fetch);
}
#endif

struct VirtualJoystick {
    Vector2 anchor = {0.f, 0.f};
    Vector2 position = {0.f, 0.f};
    float baseRadius = 80.f;
    float knobRadius = 30.f;
    int pointerId = -1;
    bool active = false;
    Vector2 direction = {0.f, 0.f};
};

enum class EnemyType {
    GRUNT,
    RUNNER,
    TANK
};

enum class PowerUpType {
    RAPID_FIRE,
    SPREAD_SHOT,
    DAMAGE_BOOST,
    SPEED_BOOST,
    SHIELD,
    ROCKET_LAUNCHER
};

enum class ProjectileType {
    BULLET,
    ROCKET
};

struct PowerUp {
    PowerUpType type;
    Vector2 position;
    float radius;
    float duration;
    Color color;
    PowerUp(PowerUpType t, Vector2 pos)
        : type(t), position(pos), radius(18.f), duration(8.f), color(WHITE) {}
};

struct ActivePowerUp {
    PowerUpType type;
    float remaining;
};

static Color GetPowerUpColor(PowerUpType type) {
    switch (type) {
        case PowerUpType::RAPID_FIRE: return ORANGE;
        case PowerUpType::SPREAD_SHOT: return (Color){120, 220, 120, 255};
        case PowerUpType::DAMAGE_BOOST: return (Color){255, 80, 110, 255};
        case PowerUpType::SPEED_BOOST: return (Color){90, 200, 255, 255};
        case PowerUpType::SHIELD: return (Color){150, 240, 255, 255};
        case PowerUpType::ROCKET_LAUNCHER: return (Color){255, 150, 60, 255};
        default: return WHITE;
    }
}

static const char* GetPowerUpLabel(PowerUpType type) {
    switch (type) {
        case PowerUpType::RAPID_FIRE: return "Rapid";
        case PowerUpType::SPREAD_SHOT: return "Spread";
        case PowerUpType::DAMAGE_BOOST: return "Damage";
        case PowerUpType::SPEED_BOOST: return "Speed";
        case PowerUpType::SHIELD: return "Shield";
        case PowerUpType::ROCKET_LAUNCHER: return "Rocket";
        default: return "??";
    }
}

static float GetPowerUpDuration(PowerUpType type) {
    switch (type) {
        case PowerUpType::RAPID_FIRE: return 8.f;
        case PowerUpType::SPREAD_SHOT: return 10.f;
        case PowerUpType::DAMAGE_BOOST: return 8.f;
        case PowerUpType::SPEED_BOOST: return 6.f;
        case PowerUpType::SHIELD: return 12.f;
        case PowerUpType::ROCKET_LAUNCHER: return 12.f;
        default: return 8.f;
    }
}

// ---------------- Player ----------------
class Player {
public:
    Vector2 position;
    int health;
    float speed;
    float baseSpeed;
    float radius;
    const int maxHealth = 100;
    int shieldCharges;
    float shieldTimer;

    Player() {
        position = {500.f, 500.f};
        health = maxHealth;
        baseSpeed = 300.f;
        speed = baseSpeed;
        radius = 20.f;
        shieldCharges = 0;
        shieldTimer = 0.f;
    }

    void ResetHealth() { health = maxHealth; }
    void ResetPosition() { position = {500.f, 500.f}; }
    void ResetStatus() {
        speed = baseSpeed;
        shieldCharges = 0;
        shieldTimer = 0.f;
    }
    void UpdateShield(float delta) {
        if (shieldTimer > 0.f) {
            shieldTimer -= delta;
            if (shieldTimer <= 0.f) {
                shieldTimer = 0.f;
                shieldCharges = 0;
            }
        }
    }

    void Update(float delta, Vector2 inputDir) {
        if (Vector2Length(inputDir) > 1.f) inputDir = Vector2Normalize(inputDir);
        position = Vector2Add(position, Vector2Scale(inputDir, speed * delta));

        if (position.x < radius) position.x = radius;
        if (position.x > GetScreenWidth() - radius) position.x = GetScreenWidth() - radius;
        if (position.y < radius) position.y = radius;
        if (position.y > GetScreenHeight() - radius) position.y = GetScreenHeight() - radius;
        UpdateShield(delta);
    }

    void Draw() const {
        Color bodyColor = GREEN;
        if (shieldCharges > 0) {
            float pulse = 0.5f + 0.5f * sinf(GetTime() * 6.f);
            Color shieldColor = {static_cast<unsigned char>(100 + 80 * pulse),
                                 static_cast<unsigned char>(230),
                                 static_cast<unsigned char>(255),
                                 180};
            DrawCircleV(position, radius + 8.f, Fade(shieldColor, 0.5f));
            DrawRing(position, radius + 2.f, radius + 10.f, 0.f, 360.f, 32,
                     {120, 240, 255, static_cast<unsigned char>(120 + 60 * pulse)});
        }
        DrawCircleV(position, radius, bodyColor);
    }
};

// ---------------- Gun ----------------
class Gun {
public:
    float distanceFromPlayer = 40.f;
    float size = 20.f;

    Vector2 GetPosition(Vector2 playerPos, Vector2 aimPos) const {
        float angle = atan2f(aimPos.y - playerPos.y, aimPos.x - playerPos.x);
        return {playerPos.x + cosf(angle) * distanceFromPlayer,
                playerPos.y + sinf(angle) * distanceFromPlayer};
    }

    float GetAngle(Vector2 playerPos, Vector2 aimPos) const {
        return atan2f(aimPos.y - playerPos.y, aimPos.x - playerPos.x);
    }

    void Draw(Vector2 playerPos, Vector2 aimPos) const {
        Vector2 gunPos = GetPosition(playerPos, aimPos);
        float angle = GetAngle(playerPos, aimPos) * RAD2DEG;
        Rectangle barrel = {playerPos.x, playerPos.y - 5.f, distanceFromPlayer + size, 10.f};
        DrawRectanglePro(barrel, {0.f, 5.f}, angle, DARKGRAY);
        DrawCircleV(gunPos, size * 0.6f, GRAY);
    }
};

struct Explosion {
    Vector2 position;
    float radius;
    float lifetime;
    float elapsed;
    int damage;
    bool applied;
};

// ---------------- Bullet ----------------
class Bullet {
public:
    ProjectileType type;
    Vector2 position;
    Vector2 velocity;
    float radius;
    int damage;
    Color color;
    float explosionRadius;

    Bullet(Vector2 start, Vector2 target, int dmg, Color tint,
           float speedValue = 500.f, ProjectileType projType = ProjectileType::BULLET,
           float explosion = 0.f) {
        type = projType;
        position = start;
        float angle = atan2f(target.y - start.y, target.x - start.x);
        velocity = {cosf(angle) * speedValue, sinf(angle) * speedValue};
        radius = 5.f;
        damage = dmg;
        color = tint;
        explosionRadius = explosion;
        if (type == ProjectileType::ROCKET) radius = 8.f;
    }

    void Update(float delta) {
        position.x += velocity.x * delta;
        position.y += velocity.y * delta;
    }

    void Draw() { DrawCircleV(position, radius, color); }

    bool IsOffScreen() {
        return position.x < 0.f || position.x > GetScreenWidth() ||
               position.y < 0.f || position.y > GetScreenHeight();
    }
};

// ---------------- Enemy ----------------
class Enemy {
public:
    EnemyType type;
    Vector2 position;
    Vector2 facing;
    int health;
    float speed;
    float radius;
    float flashTimer;
    int contactDamage;
    float knockbackResistance;
    Color baseColor;
    Color flashColor;
    float behaviorTimer;

    Enemy(Vector2 spawnPos, EnemyType enemyType, int wave);
    void Update(float delta, Vector2 playerPos);
    void ApplyHit(int damage, const Vector2& knockbackDir, float knockbackStrength);
    void Draw() const;
};

Enemy::Enemy(Vector2 spawnPos, EnemyType enemyType, int wave)
    : type(enemyType), position(spawnPos), facing({1.f, 0.f}), flashTimer(0.f),
      contactDamage(10), knockbackResistance(0.1f), baseColor(RED), flashColor(ORANGE),
      behaviorTimer(static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD) {
    float healthScale = 1.f + (wave - 1) * 0.18f;
    float speedScale = 1.f + (wave - 1) * 0.05f;
    float damageScale = 1.f + (wave - 1) * 0.1f;
    switch (type) {
        case EnemyType::GRUNT: {
            int baseHealth = 45;
            float baseSpeed = 90.f;
            radius = 16.f;
            contactDamage = static_cast<int>(std::round(12 * damageScale));
            health = static_cast<int>(std::round(baseHealth * healthScale));
            speed = baseSpeed * speedScale;
            knockbackResistance = 0.25f;
            baseColor = {200, 60, 60, 255};
            flashColor = {255, 200, 120, 255};
        } break;
        case EnemyType::RUNNER: {
            int baseHealth = 28;
            float baseSpeed = 140.f;
            radius = 12.f;
            contactDamage = static_cast<int>(std::round(9 * damageScale));
            health = static_cast<int>(std::round(baseHealth * healthScale));
            speed = baseSpeed * speedScale;
            knockbackResistance = 0.05f;
            baseColor = {80, 200, 255, 255};
            flashColor = {240, 255, 255, 255};
        } break;
        case EnemyType::TANK: {
            int baseHealth = 110;
            float baseSpeed = 60.f;
            radius = 22.f;
            contactDamage = static_cast<int>(std::round(20 * damageScale));
            health = static_cast<int>(std::round(baseHealth * healthScale));
            speed = baseSpeed * speedScale * 0.85f;
            knockbackResistance = 0.7f;
            baseColor = {90, 70, 150, 255};
            flashColor = {190, 160, 255, 255};
        } break;
    }
    if (health < 1) health = 1;
    if (contactDamage < 1) contactDamage = 1;
}

void Enemy::Update(float delta, Vector2 playerPos) {
    behaviorTimer += delta;
    Vector2 toPlayer = Vector2Subtract(playerPos, position);
    float distance = Vector2Length(toPlayer);
    Vector2 dir = distance > 0.001f ? Vector2Scale(toPlayer, 1.f / distance) : Vector2Zero();
    Vector2 moveDir = dir;

    if (type == EnemyType::RUNNER && distance > 0.001f) {
        Vector2 perp = {-dir.y, dir.x};
        float sway = sinf(behaviorTimer * 6.f) * 0.55f;
        moveDir = Vector2Add(dir, Vector2Scale(perp, sway));
        if (Vector2Length(moveDir) > 0.001f) moveDir = Vector2Normalize(moveDir);
    } else if (type == EnemyType::TANK) {
        float pulse = 1.f + sinf(behaviorTimer * 1.5f) * 0.12f;
        moveDir = Vector2Scale(dir, pulse);
    }

    if (Vector2Length(moveDir) > 0.001f) facing = Vector2Normalize(moveDir);
    position = Vector2Add(position, Vector2Scale(moveDir, speed * delta));

    if (flashTimer > 0.f) {
        flashTimer -= delta;
        if (flashTimer < 0.f) flashTimer = 0.f;
    }
}

void Enemy::ApplyHit(int damage, const Vector2& knockbackDir, float knockbackStrength) {
    health -= damage;
    if (health < 0) health = 0;
    flashTimer = 0.12f;
    float resistance = knockbackResistance;
    if (resistance < 0.f) resistance = 0.f;
    if (resistance > 0.95f) resistance = 0.95f;
    if (knockbackStrength > 0.f && (knockbackDir.x != 0.f || knockbackDir.y != 0.f)) {
        Vector2 dir = Vector2Normalize(knockbackDir);
        float scaled = knockbackStrength * (1.f - resistance);
        position = Vector2Add(position, Vector2Scale(dir, scaled));
    }
}

void Enemy::Draw() const {
    Color color = flashTimer > 0.f ? flashColor : baseColor;
    switch (type) {
        case EnemyType::GRUNT: {
            DrawCircleV(position, radius, color);
            DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius, Fade(BLACK, 0.5f));
            DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius * 0.55f, Fade(flashColor, 0.4f));
        } break;
        case EnemyType::RUNNER: {
            float angle = atan2f(facing.y, facing.x) * RAD2DEG;
            DrawPoly(position, 4, radius * 1.2f, angle, color);
            Vector2 head = Vector2Add(position, Vector2Scale(facing, radius * 1.2f));
            Vector2 tailLeft = Vector2Add(position, Vector2Rotate(Vector2Scale(facing, -radius * 1.6f), 0.6f));
            Vector2 tailRight = Vector2Add(position, Vector2Rotate(Vector2Scale(facing, -radius * 1.6f), -0.6f));
            DrawTriangle(head, tailLeft, tailRight, Fade(color, 0.5f));
        } break;
        case EnemyType::TANK: {
            DrawCircleV(position, radius, color);
            DrawRing(position, radius * 0.6f, radius * 0.95f, 0.f, 360.f, 24, Fade(flashColor, 0.65f));
            DrawCircleV(position, radius * 0.4f, Fade(BLACK, 0.5f));
        } break;
    }
}
// ---------------- Game States ----------------
enum class GameState {
    SPLASH,
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER
};

// ---------------- Main ----------------
int main() {
#ifdef __EMSCRIPTEN__
    InitializeHeapSynchronization();
#endif
    InitWindow(1000, 1000, "WaveBreaker");
    SetTargetFPS(60);

    Player player;
    Gun gun;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<PowerUp> powerUps;
    std::vector<ActivePowerUp> activePowerUps;
    std::vector<Explosion> explosions;
    VirtualJoystick moveStick;
    moveStick.baseRadius = 95.f;
    moveStick.knobRadius = 36.f;
    moveStick.anchor = {130.f, static_cast<float>(GetScreenHeight()) - 140.f};
    moveStick.position = moveStick.anchor;

    int currentWave = 1;
    int enemiesRemaining = 0;
    bool gameOver = false;

    GameConfig gameConfig;
    bool configLoaded = false;
    bool configApplied = false;
    std::string configStatus = "Loading configuration...";

    float powerUpSpawnIntervalMin = gameConfig.powerUpIntervalMin;
    float powerUpSpawnIntervalMax = gameConfig.powerUpIntervalMax;
    float enemyDropChance = gameConfig.enemyDropChance;

    Texture2D splashLogo = LoadTexture("Graphics/bora0devlogo1.png");
    const float splashDuration = 10.f;
    float splashTimer = 0.f;

    GameState state = GameState::SPLASH;
    int prevTouchCount = 0;

    // Audio
    bool audioAvailable = true;
    Sound shootSound{};
    Sound enemyHitSound{};
    Sound playerHitSound{};
    Sound buttonPressSound{};
    Sound explosionSound{};
    Sound gameOverSound{};
#ifdef __EMSCRIPTEN__
    audioAvailable = false;
#else
    InitAudioDevice();
    shootSound = LoadSound("Sounds/wall.mp3");
    enemyHitSound = LoadSound("Sounds/eat.mp3");
    playerHitSound = LoadSound("Sounds/wall.mp3");
    buttonPressSound = LoadSound("Sounds/ButtonPress.wav");
    explosionSound = LoadSound("Sounds/Explosion.wav");
    gameOverSound = LoadSound("Sounds/GameOver.wav");
    SetSoundVolume(shootSound, 0.5f);
    SetSoundVolume(enemyHitSound, 0.7f);
    SetSoundVolume(playerHitSound, 0.8f);
    SetSoundVolume(buttonPressSound, 0.6f);
    SetSoundVolume(explosionSound, 0.7f);
    SetSoundVolume(gameOverSound, 0.9f);
#endif
    auto PlaySoundSafe = [&](Sound& sound) {
        if (audioAvailable) PlaySound(sound);
    };

#ifdef __EMSCRIPTEN__
    gGameConfigPtr = &gameConfig;
    gConfigLoadedPtr = &configLoaded;
    gConfigStatusPtr = &configStatus;

    auto RequestConfig = [&]() {
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        std::strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.onsuccess = OnConfigFetchSucceeded;
        attr.onerror = OnConfigFetchFailed;
        emscripten_fetch(&attr, "game_config.json");
    };
    RequestConfig();
#else
    auto LoadConfigFromFile = [&]() {
        char* data = LoadFileText("game_config.json");
        if (data) {
            std::string content(data);
            UnloadFileText(data);
            if (ParseGameConfig(content, gameConfig)) {
                SanitizeGameConfig(gameConfig);
                configStatus = "Configuration loaded.";
            } else {
                configStatus = "Config missing data; using defaults.";
            }
        } else {
            configStatus = "Config file not found; using defaults.";
        }
        configLoaded = true;
    };
    LoadConfigFromFile();
#endif

    const float baseFireCooldown = 0.22f;
    float fireTimer = 0.f;
    const int baseBulletDamage = 20;
    const float baseBulletSpeed = 520.f;
    const float baseRocketCooldown = 0.65f;
    const int baseRocketDamage = 70;
    const float baseRocketSpeed = 360.f;
    const float rocketExplosionRadius = 110.f;

    float powerUpSpawnTimer = 6.f;
    const int maxFieldPowerUps = 3;

    auto DrawGameplay = [&](Vector2 cursor) {
        const Color background = {10, 12, 16, 255};
        ClearBackground(background);

        bool drawStick = moveStick.active || GetTouchPointCount() > 0;
        if (drawStick) {
            Color baseShade = {60, 90, 140, 120};
            DrawCircleV(moveStick.anchor, moveStick.baseRadius, Fade(baseShade, 0.4f));
            DrawCircleLines(static_cast<int>(moveStick.anchor.x), static_cast<int>(moveStick.anchor.y),
                            moveStick.baseRadius, Fade(LIGHTGRAY, 0.4f));
            Vector2 knobPos = moveStick.active ? moveStick.position : moveStick.anchor;
            DrawCircleV(knobPos, moveStick.knobRadius, Fade(SKYBLUE, 0.7f));
        }

        for (auto &powerUp : powerUps) {
            float pulse = 0.85f + 0.15f * sinf(GetTime() * 6.f + powerUp.position.x * 0.02f);
            float radius = powerUp.radius * pulse;
            DrawRing(powerUp.position, radius * 0.5f, radius, 0.f, 360.f, 24, Fade(powerUp.color, 0.5f));
            DrawPoly(powerUp.position, 5, radius * 0.65f, GetTime() * 90.f, powerUp.color);
            DrawPolyLines(powerUp.position, 5, radius * 0.8f, -GetTime() * 60.f, Fade(powerUp.color, 0.8f));
            const char* label = GetPowerUpLabel(powerUp.type);
            int textWidth = MeasureText(label, 14);
            DrawText(label, static_cast<int>(powerUp.position.x - textWidth / 2),
                     static_cast<int>(powerUp.position.y - 7), 14, WHITE);
        }

        player.Draw();
        gun.Draw(player.position, cursor);
        for (auto &enemy : enemies) enemy.Draw();
        for (auto &bullet : bullets) bullet.Draw();
        for (auto &explosion : explosions) {
            float t = explosion.elapsed / explosion.lifetime;
            if (t > 1.f) t = 1.f;
            Color ringColor = {255, 200, 80, static_cast<unsigned char>(220 * (1.f - t))};
            DrawRing(explosion.position, explosion.radius * 0.2f, explosion.radius, 0.f, 360.f, 36,
                     Fade(ringColor, 0.8f));
            DrawCircleV(explosion.position, explosion.radius * (0.3f + 0.3f * (1.f - t)),
                        Fade((Color){255, 150, 70, 120}, 0.6f * (1.f - t)));
        }

        float healthPercent = static_cast<float>(player.health) / static_cast<float>(player.maxHealth);
        if (healthPercent < 0.f) healthPercent = 0.f;
        const float barWidth = 220.f;
        const float barHeight = 22.f;
        DrawRectangle(20, 20, static_cast<int>(barWidth), static_cast<int>(barHeight), Fade(DARKGRAY, 0.8f));
        DrawRectangle(20, 20, static_cast<int>(barWidth * healthPercent), static_cast<int>(barHeight), healthPercent > 0.35f ? GREEN : MAROON);
        DrawRectangleLines(20, 20, static_cast<int>(barWidth), static_cast<int>(barHeight), BLACK);
        DrawText(TextFormat("%d / %d", player.health, player.maxHealth), 30, 24, 16, WHITE);
        if (player.shieldCharges > 0) {
            DrawText(TextFormat("Shield: %d", player.shieldCharges), 20, 110, 18, SKYBLUE);
        }

        DrawText(TextFormat("Wave %d", currentWave), 20, 60, 22, YELLOW);
        DrawText(TextFormat("Remaining: %d", enemiesRemaining), 20, 90, 20, LIGHTGRAY);

        int index = 0;
        for (auto &effect : activePowerUps) {
            Rectangle box = {
                static_cast<float>(GetScreenWidth() - 160),
                20.f + index * 40.f,
                140.f,
                32.f
            };
            Color fill = Fade(GetPowerUpColor(effect.type), 0.75f);
            DrawRectangleRounded(box, 0.25f, 8, fill);
            DrawRectangleRoundedLines(box, 0.25f, 8, Fade(BLACK, 0.5f));
            DrawText(GetPowerUpLabel(effect.type), static_cast<int>(box.x + 12.f),
                     static_cast<int>(box.y + 8.f), 18, WHITE);
            DrawText(TextFormat("%.1fs", effect.remaining), static_cast<int>(box.x + 12.f),
                     static_cast<int>(box.y + 20.f), 14, LIGHTGRAY);
            index++;
        }

        DrawCircleLines(cursor.x, cursor.y, 10.f, YELLOW);
        DrawLine(cursor.x - 15.f, cursor.y, cursor.x + 15.f, cursor.y, Fade(YELLOW, 0.4f));
        DrawLine(cursor.x, cursor.y - 15.f, cursor.x, cursor.y + 15.f, Fade(YELLOW, 0.4f));
    };

    // Spawn wave
    auto SpawnWave = [&](int wave) {
        enemies.clear();
        bullets.clear();
        powerUps.clear();
        activePowerUps.clear();
        explosions.clear();
        player.ResetHealth();
        player.ResetPosition();
        player.ResetStatus();
        fireTimer = 0.f;
        powerUpSpawnTimer = static_cast<float>(GetRandomValue(
                                  static_cast<int>(powerUpSpawnIntervalMin * 10.f),
                                  static_cast<int>(powerUpSpawnIntervalMax * 10.f))) /
                            10.f;
        moveStick.anchor = {130.f, static_cast<float>(GetScreenHeight()) - 140.f};
        moveStick.position = moveStick.anchor;
        moveStick.pointerId = -1;
        moveStick.active = false;
        moveStick.direction = {0.f, 0.f};

        int baseCount = 8 + (wave - 1) * 3;
        int count = static_cast<int>(std::lround(baseCount * gameConfig.enemyCountMultiplier));
        if (count < 1) count = 1;
        if (count > 45) count = 45;

        float safeRadius = 180.f;
        auto pickType = [&](int waveNum) {
            std::vector<EnemyType> bag = {EnemyType::GRUNT, EnemyType::GRUNT, EnemyType::GRUNT};
            if (waveNum >= 2) {
                bag.push_back(EnemyType::RUNNER);
                bag.push_back(EnemyType::RUNNER);
            }
            if (waveNum >= 4) {
                bag.push_back(EnemyType::TANK);
            }
            int idx = GetRandomValue(0, static_cast<int>(bag.size()) - 1);
            return bag[idx];
        };

        for (int i = 0; i < count; i++) {
            Vector2 spawn = {0.f, 0.f};
            int side = GetRandomValue(0, 3);
            switch (side) {
                case 0: // Left
                    spawn = {-60.f, static_cast<float>(GetRandomValue(0, GetScreenHeight()))};
                    break;
                case 1: // Right
                    spawn = {static_cast<float>(GetScreenWidth()) + 60.f,
                             static_cast<float>(GetRandomValue(0, GetScreenHeight()))};
                    break;
                case 2: // Top
                    spawn = {static_cast<float>(GetRandomValue(0, GetScreenWidth())),
                             -60.f};
                    break;
                case 3: // Bottom
                default:
                    spawn = {static_cast<float>(GetRandomValue(0, GetScreenWidth())),
                             static_cast<float>(GetScreenHeight()) + 60.f};
                    break;
            }

            Vector2 toPlayer = {player.position.x - spawn.x, player.position.y - spawn.y};
            float distance = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
            if (distance < safeRadius) {
                Vector2 dir;
                if (distance == 0.f) {
                    dir = {1.f, 0.f};
                } else {
                    dir = {toPlayer.x / distance, toPlayer.y / distance};
                }
                spawn.x -= dir.x * (safeRadius - distance);
                spawn.y -= dir.y * (safeRadius - distance);
            }
            EnemyType type = pickType(wave);
            enemies.emplace_back(spawn, type, wave);
        }
        enemiesRemaining = count;
    };

    auto ActivatePowerUp = [&](PowerUpType type) {
        float duration = GetPowerUpDuration(type);
        bool found = false;
        for (auto &effect : activePowerUps) {
            if (effect.type == type) {
                effect.remaining = duration;
                found = true;
                break;
            }
        }
        if (!found) {
            activePowerUps.push_back({type, duration});
        }

        switch (type) {
            case PowerUpType::RAPID_FIRE:
            case PowerUpType::SPREAD_SHOT:
            case PowerUpType::DAMAGE_BOOST:
            case PowerUpType::SPEED_BOOST:
                break;
            case PowerUpType::SHIELD:
                player.shieldCharges = std::min(player.shieldCharges + 2, 4);
                player.shieldTimer = duration;
                break;
            case PowerUpType::ROCKET_LAUNCHER:
                fireTimer = 0.f;
                break;
        }
        PlaySoundSafe(enemyHitSound);
    };

    struct PowerStats {
        float speedMultiplier = 1.f;
    float fireRateMultiplier = 1.f;
    float damageMultiplier = 1.f;
    int spreadLevel = 0;
    float shieldRemaining = 0.f;
    bool rocketLauncher = false;
};

    auto ComputePowerStats = [&]() {
        PowerStats stats;
        for (auto &effect : activePowerUps) {
            switch (effect.type) {
                case PowerUpType::RAPID_FIRE:
                    stats.fireRateMultiplier *= 1.75f;
                    break;
                case PowerUpType::SPREAD_SHOT:
                    stats.spreadLevel = std::max(stats.spreadLevel, 1);
                    break;
                case PowerUpType::DAMAGE_BOOST:
                    stats.damageMultiplier *= 1.6f;
                    break;
                case PowerUpType::SPEED_BOOST:
                    stats.speedMultiplier *= 1.35f;
                    break;
                case PowerUpType::SHIELD:
                    stats.shieldRemaining = std::max(stats.shieldRemaining, effect.remaining);
                    break;
                case PowerUpType::ROCKET_LAUNCHER:
                    stats.rocketLauncher = true;
                    break;
            }
        }
        return stats;
    };

    auto CreatePowerUpInstance = [&](PowerUpType type, Vector2 position) {
        PowerUp drop(type, position);
        drop.duration = GetPowerUpDuration(type);
        drop.color = GetPowerUpColor(type);
        if (type == PowerUpType::SHIELD || type == PowerUpType::ROCKET_LAUNCHER) drop.radius = 20.f;
        float screenW = static_cast<float>(GetScreenWidth());
        float screenH = static_cast<float>(GetScreenHeight());
        drop.position.x = std::max(drop.radius, std::min(drop.position.x, screenW - drop.radius));
        drop.position.y = std::max(drop.radius, std::min(drop.position.y, screenH - drop.radius));
        powerUps.push_back(drop);
    };

    auto SpawnRandomPowerUp = [&](Vector2 position) {
        if ((int)powerUps.size() >= maxFieldPowerUps) return;
        std::vector<PowerUpType> bag = {
            PowerUpType::RAPID_FIRE,
            PowerUpType::SPREAD_SHOT,
            PowerUpType::DAMAGE_BOOST,
            PowerUpType::SPEED_BOOST
        };
        if (currentWave >= 2) bag.push_back(PowerUpType::SHIELD);
        if (currentWave >= 3) bag.push_back(PowerUpType::ROCKET_LAUNCHER);
        PowerUpType type = bag[GetRandomValue(0, static_cast<int>(bag.size()) - 1)];
        CreatePowerUpInstance(type, position);
    };

    auto TryDropPowerUp = [&](Vector2 position) {
        if ((int)powerUps.size() >= maxFieldPowerUps) return;
        int roll = GetRandomValue(0, 999);
        if (roll < static_cast<int>(enemyDropChance * 1000.f)) {
            SpawnRandomPowerUp(position);
        }
    };

    auto SpawnExplosion = [&](Vector2 position, float radius, int damage) {
        Explosion ex{position, radius, 0.35f, 0.f, damage, false};
        explosions.push_back(ex);
        PlaySoundSafe(explosionSound);
    };

    auto UpdateJoystick = [&](VirtualJoystick &stick) {
        Vector2 direction = {0.f, 0.f};
        int touchCount = GetTouchPointCount();
        bool pointerFound = false;

        for (int i = 0; i < touchCount; i++) {
            int id = GetTouchPointId(i);
            Vector2 pos = GetTouchPosition(i);
            if (stick.pointerId == id) {
                pointerFound = true;
                stick.position = pos;
                break;
            }
        }

        if (!pointerFound) {
            stick.pointerId = -1;
            stick.active = false;
            stick.position = stick.anchor;
            for (int i = 0; i < touchCount; i++) {
                Vector2 pos = GetTouchPosition(i);
                if (pos.x <= GetScreenWidth() * 0.5f) {
                    stick.pointerId = GetTouchPointId(i);
                    stick.anchor = pos;
                    stick.position = pos;
                    stick.active = true;
                    pointerFound = true;
                    break;
                }
            }
        } else {
            stick.active = true;
        }

        if (pointerFound) {
            Vector2 delta = Vector2Subtract(stick.position, stick.anchor);
            float len = Vector2Length(delta);
            if (len > stick.baseRadius) {
                delta = Vector2Scale(delta, stick.baseRadius / (len > 0.f ? len : 1.f));
                stick.position = Vector2Add(stick.anchor, delta);
            }
            if (stick.baseRadius > 0.f) direction = Vector2Scale(delta, 1.f / stick.baseRadius);
        }

        stick.direction = direction;
        return direction;
    };

#ifdef __EMSCRIPTEN__
    auto main_loop = [&]() -> void {
#endif

    while (!WindowShouldClose()) {
        float delta = GetFrameTime();
#ifdef __EMSCRIPTEN__
        EnsureHeapViewsExported();
#endif
        Vector2 mouse = GetMousePosition();
        bool touchFire = false;
        int touchCount = GetTouchPointCount();
        bool touchPressedThisFrame = touchCount > 0 && prevTouchCount == 0;
        Vector2 uiPointer = mouse;
        if (touchCount > 0) {
            uiPointer = GetTouchPosition(0);
        }
        prevTouchCount = touchCount;

        if (configLoaded && !configApplied) {
            SanitizeGameConfig(gameConfig);
            powerUpSpawnIntervalMin = gameConfig.powerUpIntervalMin;
            powerUpSpawnIntervalMax = gameConfig.powerUpIntervalMax;
            enemyDropChance = gameConfig.enemyDropChance;
            if (state == GameState::SPLASH || state == GameState::MENU) {
                currentWave = std::max(1, gameConfig.startingWave);
            }
            configApplied = true;
            if (configStatus == "Configuration loaded.") {
                configStatus = "Configuration active.";
            }
        }

        for (int t = 0; t < touchCount; t++) {
            int id = GetTouchPointId(t);
            if (id == moveStick.pointerId) continue;
            Vector2 touchPos = GetTouchPosition(t);
            if (touchPos.x >= GetScreenWidth() * 0.55f) {
                mouse = touchPos;
                touchFire = true;
                break;
            }
        }
        bool fireInput = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || touchFire || IsKeyDown(KEY_SPACE);
        if (fireTimer > 0.f) {
            fireTimer -= delta;
            if (fireTimer < 0.f) fireTimer = 0.f;
        }

        // ------------- SPLASH -------------
        if (state == GameState::SPLASH) {
            splashTimer += delta;
            bool skip = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) ||
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || touchPressedThisFrame;
            if (splashTimer >= splashDuration || skip) {
                state = GameState::MENU;
                BeginDrawing();
                ClearBackground(BLACK);
                EndDrawing();
                continue;
            }

            BeginDrawing();
            ClearBackground(BLACK);

            float screenW = static_cast<float>(GetScreenWidth());
            float screenH = static_cast<float>(GetScreenHeight());
            float scale = std::min(screenW / splashLogo.width, screenH / splashLogo.height) * 0.7f;
            if (scale <= 0.f) scale = 1.f;
            float logoWidth = splashLogo.width * scale;
            float logoHeight = splashLogo.height * scale;
            Rectangle src = {0.f, 0.f, static_cast<float>(splashLogo.width), static_cast<float>(splashLogo.height)};
            Rectangle dst = {screenW * 0.5f, screenH * 0.5f, logoWidth, logoHeight};
            Vector2 origin = {logoWidth * 0.5f, logoHeight * 0.5f};
            DrawTexturePro(splashLogo, src, dst, origin, 0.f, Fade(WHITE, 0.95f));

            DrawText("Loading...", static_cast<int>(screenW * 0.5f - 80), static_cast<int>(screenH * 0.75f), 24, LIGHTGRAY);

            EndDrawing();
            continue;
        }

        // ------------- MENU -------------
        if (state == GameState::MENU) {
            Rectangle playBtn = {
                static_cast<float>(GetScreenWidth()/2 - 100),
                static_cast<float>(GetScreenHeight()/2 - 60),
                200.0f,
                60.0f
            };
            Rectangle quitBtn = {
                static_cast<float>(GetScreenWidth()/2 - 100),
                static_cast<float>(GetScreenHeight()/2 + 20),
                200.0f,
                60.0f
            };

            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("WaveBreaker", GetScreenWidth()/2 - 160, 200, 40, YELLOW);

            Color configColor = configLoaded ? (configApplied ? GREEN : SKYBLUE) : LIGHTGRAY;
            int statusFontSize = 18;
            int statusWidth = MeasureText(configStatus.c_str(), statusFontSize);
            int statusX = GetScreenWidth()/2 - statusWidth / 2;
            DrawText(configStatus.c_str(), statusX, 250, statusFontSize, configColor);

            Color playColor = CheckCollisionPointRec(uiPointer, playBtn) ? GRAY : DARKGRAY;
            DrawRectangleRec(playBtn, playColor);
            DrawText("PLAY", playBtn.x + 65, playBtn.y + 15, 30, WHITE);

            Color quitColor = CheckCollisionPointRec(uiPointer, quitBtn) ? GRAY : DARKGRAY;
            DrawRectangleRec(quitBtn, quitColor);
            DrawText("QUIT", quitBtn.x + 65, quitBtn.y + 15, 30, WHITE);

            bool selectPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || touchPressedThisFrame;
            if (CheckCollisionPointRec(uiPointer, playBtn) && selectPressed) {
                currentWave = 1;
                SpawnWave(currentWave);
                gameOver = false;
                PlaySoundSafe(buttonPressSound);
                state = GameState::PLAYING;
            }
            if (CheckCollisionPointRec(uiPointer, quitBtn) && selectPressed) {
                PlaySoundSafe(buttonPressSound);
                CloseWindow();
#ifdef __EMSCRIPTEN__
                return;
#else
                return 0;
#endif
            }

            EndDrawing();
            continue;
        }

        // ------------- PAUSED -------------
        if (state == GameState::PAUSED) {
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::PLAYING;

            Rectangle resumeBtn = {
                static_cast<float>(GetScreenWidth()/2 - 100),
                static_cast<float>(GetScreenHeight()/2 - 100),
                200.0f,
                60.0f
            };
            Rectangle restartBtn = {
                static_cast<float>(GetScreenWidth()/2 - 100),
                static_cast<float>(GetScreenHeight()/2 - 20),
                200.0f,
                60.0f
            };
            Rectangle quitBtn = {
                static_cast<float>(GetScreenWidth()/2 - 100),
                static_cast<float>(GetScreenHeight()/2 + 60),
                200.0f,
                60.0f
            };

            BeginDrawing();
            DrawGameplay(mouse);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));

            Rectangle panel = {
                static_cast<float>(GetScreenWidth()/2 - 180),
                static_cast<float>(GetScreenHeight()/2 - 160),
                360.f,
                320.f
            };
            DrawRectangleRounded(panel, 0.15f, 12, Fade(DARKGRAY, 0.9f));
            DrawText("PAUSED", panel.x + 120, panel.y + 30, 36, YELLOW);

            bool tapPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || touchPressedThisFrame;

            Color resumeColor = CheckCollisionPointRec(uiPointer, resumeBtn) ? LIGHTGRAY : GRAY;
            DrawRectangleRec(resumeBtn, resumeColor);
            DrawText("RESUME", resumeBtn.x + 35, resumeBtn.y + 15, 30, WHITE);

            Color restartColor = CheckCollisionPointRec(uiPointer, restartBtn) ? LIGHTGRAY : GRAY;
            DrawRectangleRec(restartBtn, restartColor);
            DrawText("RESTART", restartBtn.x + 30, restartBtn.y + 15, 30, WHITE);

            Color quitColor = CheckCollisionPointRec(uiPointer, quitBtn) ? LIGHTGRAY : GRAY;
            DrawRectangleRec(quitBtn, quitColor);
            DrawText("MENU", quitBtn.x + 60, quitBtn.y + 15, 30, WHITE);

            if (CheckCollisionPointRec(uiPointer, resumeBtn) && tapPressed) {
                PlaySoundSafe(buttonPressSound);
                state = GameState::PLAYING;
            }
            if (CheckCollisionPointRec(uiPointer, restartBtn) && tapPressed) {
                currentWave = 1;
                SpawnWave(currentWave);
                gameOver = false;
                PlaySoundSafe(buttonPressSound);
                state = GameState::PLAYING;
            }
            if (CheckCollisionPointRec(uiPointer, quitBtn) && tapPressed) {
                PlaySoundSafe(buttonPressSound);
                state = GameState::MENU;
            }

            EndDrawing();
            continue;
        }

        // ------------- PLAYING -------------
        if (state == GameState::PLAYING) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                state = GameState::PAUSED;
                continue;
            }

            if (powerUpSpawnTimer > 0.f) {
                powerUpSpawnTimer -= delta;
            }
            if (powerUpSpawnTimer <= 0.f && (int)powerUps.size() < maxFieldPowerUps) {
                Vector2 spawnPos = {0.f, 0.f};
                bool foundSpot = false;
                int attempts = 0;
                do {
                    spawnPos = {
                        static_cast<float>(GetRandomValue(80, GetScreenWidth() - 80)),
                        static_cast<float>(GetRandomValue(80, GetScreenHeight() - 80))
                    };
                    bool nearPlayer = Vector2Distance(spawnPos, player.position) < 140.f;
                    bool overlaps = false;
                    for (auto &existing : powerUps) {
                        if (Vector2Distance(spawnPos, existing.position) < existing.radius + 50.f) {
                            overlaps = true;
                            break;
                        }
                    }
                    foundSpot = !nearPlayer && !overlaps;
                    attempts++;
                } while (!foundSpot && attempts < 12);
                if (!foundSpot) {
                    spawnPos = {
                        static_cast<float>(GetRandomValue(80, GetScreenWidth() - 80)),
                        static_cast<float>(GetRandomValue(80, GetScreenHeight() - 80))
                    };
                }
                SpawnRandomPowerUp(spawnPos);
                powerUpSpawnTimer = static_cast<float>(GetRandomValue(
                                          static_cast<int>(powerUpSpawnIntervalMin * 10.f),
                                          static_cast<int>(powerUpSpawnIntervalMax * 10.f))) / 10.f;
            }

            for (int i = 0; i < (int)activePowerUps.size();) {
                activePowerUps[i].remaining -= delta;
                if (activePowerUps[i].remaining <= 0.f) {
                    if (activePowerUps[i].type == PowerUpType::SHIELD) {
                        player.shieldCharges = 0;
                        player.shieldTimer = 0.f;
                    }
                    activePowerUps.erase(activePowerUps.begin() + i);
                } else {
                    i++;
                }
            }

            PowerStats stats = ComputePowerStats();
            player.speed = player.baseSpeed * stats.speedMultiplier;
            if (player.speed < player.baseSpeed * 0.6f) player.speed = player.baseSpeed * 0.6f;
            if (player.speed > player.baseSpeed * 2.2f) player.speed = player.baseSpeed * 2.2f;
            if (stats.shieldRemaining > 0.f) player.shieldTimer = stats.shieldRemaining;

            Vector2 moveInput = UpdateJoystick(moveStick);
            Vector2 keyboardDir = {0.f, 0.f};
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) keyboardDir.y -= 1.f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) keyboardDir.y += 1.f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) keyboardDir.x -= 1.f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) keyboardDir.x += 1.f;
            moveInput = Vector2Add(moveInput, keyboardDir);
            if (Vector2Length(moveInput) > 1.f) moveInput = Vector2Normalize(moveInput);
            player.Update(delta, moveInput);

            bool pickedPowerUp = false;
            for (int i = 0; i < (int)powerUps.size();) {
                if (CheckCollisionCircles(player.position, player.radius + 6.f,
                                          powerUps[i].position, powerUps[i].radius)) {
                    ActivatePowerUp(powerUps[i].type);
                    powerUps.erase(powerUps.begin() + i);
                    stats = ComputePowerStats();
                    pickedPowerUp = true;
                } else {
                    ++i;
                }
            }
            if (pickedPowerUp) {
                player.speed = player.baseSpeed * stats.speedMultiplier;
                if (player.speed < player.baseSpeed * 0.6f) player.speed = player.baseSpeed * 0.6f;
                if (player.speed > player.baseSpeed * 2.2f) player.speed = player.baseSpeed * 2.2f;
                if (stats.shieldRemaining > 0.f) player.shieldTimer = stats.shieldRemaining;
            }

            if (stats.shieldRemaining > 0.f) player.shieldTimer = stats.shieldRemaining;
            else if (player.shieldCharges <= 0) player.shieldTimer = 0.f;

            float effectiveCooldown = (stats.rocketLauncher ? baseRocketCooldown : baseFireCooldown) / stats.fireRateMultiplier;
            if (effectiveCooldown < 0.05f) effectiveCooldown = 0.05f;

            if (fireInput && fireTimer <= 0.f) {
                Vector2 origin = gun.GetPosition(player.position, mouse);
                Vector2 target = mouse;
                Vector2 direction = Vector2Normalize(Vector2Subtract(target, origin));
                if (Vector2Length(direction) <= 0.001f) direction = {1.f, 0.f};
                bool rocket = stats.rocketLauncher;
                int projectileDamage = rocket
                    ? std::max(1, static_cast<int>(std::round(baseRocketDamage * stats.damageMultiplier)))
                    : std::max(1, static_cast<int>(std::round(baseBulletDamage * stats.damageMultiplier)));
                float projectileSpeed = rocket
                    ? baseRocketSpeed * (stats.fireRateMultiplier > 1.f ? 1.f + (stats.fireRateMultiplier - 1.f) * 0.2f : 1.f)
                    : baseBulletSpeed * (stats.fireRateMultiplier > 1.f ? 1.f + (stats.fireRateMultiplier - 1.f) * 0.25f : 1.f);
                Color bulletColor;
                if (rocket) {
                    bulletColor = (Color){255, 130, 60, 255};
                } else if (stats.spreadLevel > 0) {
                    bulletColor = (Color){255, 220, 140, 255};
                } else {
                    bulletColor = stats.damageMultiplier > 1.01f ? ORANGE : YELLOW;
                }

                std::vector<float> offsets = {0.f};
                if (!rocket && stats.spreadLevel > 0) {
                    offsets.push_back(0.18f);
                    offsets.push_back(-0.18f);
                }

                for (float offset : offsets) {
                    float angle = atan2f(direction.y, direction.x) + offset;
                    Vector2 aim = {origin.x + cosf(angle) * 1000.f,
                                   origin.y + sinf(angle) * 1000.f};
                    bullets.emplace_back(origin, aim, projectileDamage, bulletColor, projectileSpeed,
                                         rocket ? ProjectileType::ROCKET : ProjectileType::BULLET,
                                         rocket ? rocketExplosionRadius : 0.f);
                }
                PlaySoundSafe(shootSound);
                fireTimer = effectiveCooldown;
            }

            for (int i = 0; i < (int)bullets.size(); i++) {
                bullets[i].Update(delta);
                if (bullets[i].IsOffScreen()) {
                    if (bullets[i].type == ProjectileType::ROCKET) {
                        float radius = bullets[i].explosionRadius > 0.f ? bullets[i].explosionRadius : rocketExplosionRadius;
                        SpawnExplosion(bullets[i].position, radius, bullets[i].damage);
                    }
                    bullets.erase(bullets.begin() + i);
                    i--;
                }
            }

            for (int i = 0; i < (int)enemies.size(); i++) {
                enemies[i].Update(delta, player.position);

                if (CheckCollisionCircles(player.position, player.radius,
                                          enemies[i].position, enemies[i].radius)) {
                    bool blocked = false;
                    if (player.shieldCharges > 0) {
                        player.shieldCharges--;
                        blocked = true;
                        for (auto &effect : activePowerUps) {
                            if (effect.type == PowerUpType::SHIELD && player.shieldCharges <= 0) {
                                effect.remaining = 0.f;
                            }
                        }
                    } else {
                        player.health -= enemies[i].contactDamage;
                        if (player.health < 0) player.health = 0;
                    }
                    PlaySoundSafe(playerHitSound);
                    TryDropPowerUp(enemies[i].position);
                    enemies.erase(enemies.begin() + i);
                    enemiesRemaining--;
                    i--;
                    if (!blocked && player.health <= 0) {
                        gameOver = true;
                        PlaySoundSafe(gameOverSound);
                        state = GameState::GAME_OVER;
                    }
                    continue;
                }

                for (int j = 0; j < (int)bullets.size(); j++) {
                    if (CheckCollisionCircles(bullets[j].position, bullets[j].radius,
                                              enemies[i].position, enemies[i].radius)) {
                        Bullet projectile = bullets[j];
                        Vector2 knockbackDir = Vector2Subtract(enemies[i].position, projectile.position);
                        if (Vector2Length(knockbackDir) > 0.f) knockbackDir = Vector2Normalize(knockbackDir);
                        float knockbackStrength = projectile.type == ProjectileType::ROCKET ? 70.f : 40.f;
                        enemies[i].ApplyHit(projectile.damage, knockbackDir, knockbackStrength);
                        PlaySoundSafe(enemyHitSound);
                        Vector2 deathPos = enemies[i].position;
                        bullets.erase(bullets.begin() + j);
                        j--;
                        if (projectile.type == ProjectileType::ROCKET) {
                            float radius = projectile.explosionRadius > 0.f ? projectile.explosionRadius : rocketExplosionRadius;
                            SpawnExplosion(deathPos, radius, projectile.damage);
                        }
                        if (enemies[i].health <= 0) {
                            TryDropPowerUp(deathPos);
                            enemies.erase(enemies.begin() + i);
                            enemiesRemaining--;
                            i--;
                        }
                        break;
                    }
                }
            }

            for (auto &explosion : explosions) {
                if (explosion.applied) continue;
                for (int idx = 0; idx < (int)enemies.size();) {
                    float dist = Vector2Distance(explosion.position, enemies[idx].position);
                    if (dist <= explosion.radius + enemies[idx].radius) {
                        Vector2 knockDir = Vector2Subtract(enemies[idx].position, explosion.position);
                        if (Vector2Length(knockDir) > 0.f) knockDir = Vector2Normalize(knockDir);
                        enemies[idx].ApplyHit(explosion.damage, knockDir, 90.f);
                        if (enemies[idx].health <= 0) {
                            TryDropPowerUp(enemies[idx].position);
                            enemies.erase(enemies.begin() + idx);
                            enemiesRemaining--;
                            continue;
                        }
                    }
                    idx++;
                }
                explosion.applied = true;
            }

            for (int e = 0; e < (int)explosions.size();) {
                explosions[e].elapsed += delta;
                if (explosions[e].elapsed >= explosions[e].lifetime) {
                    explosions.erase(explosions.begin() + e);
                } else {
                    ++e;
                }
            }

            if (enemiesRemaining < 0) enemiesRemaining = 0;

            if (enemiesRemaining <= 0 && !gameOver) {
                currentWave++;
                SpawnWave(currentWave);
            }

            BeginDrawing();
            DrawGameplay(mouse);
            EndDrawing();
            continue;
        }

        // ------------- GAME OVER -------------
        if (state == GameState::GAME_OVER) {
            BeginDrawing();
            DrawGameplay(mouse);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.75f));

            const char *msg = TextFormat("You survived %d wave%s!", currentWave,
                                         currentWave == 1 ? "" : "s");
            int msgWidth = MeasureText(msg, 32);
            DrawText("GAME OVER", GetScreenWidth()/2 - 140, GetScreenHeight()/2 - 200, 40, RED);
            DrawText(msg, GetScreenWidth()/2 - msgWidth/2, GetScreenHeight()/2 - 140, 32, WHITE);

            Rectangle replayBtn = {
                static_cast<float>(GetScreenWidth()/2 - 150),
                static_cast<float>(GetScreenHeight()/2 - 40),
                120.0f,
                70.0f
            };
            Rectangle menuBtn = {
                static_cast<float>(GetScreenWidth()/2 + 30),
                static_cast<float>(GetScreenHeight()/2 - 40),
                120.0f,
                70.0f
            };

            bool tapPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || touchPressedThisFrame;
            Color replayColor = CheckCollisionPointRec(uiPointer, replayBtn) ? LIGHTGRAY : GRAY;
            Color menuColor = CheckCollisionPointRec(uiPointer, menuBtn) ? LIGHTGRAY : GRAY;
            DrawRectangleRounded(replayBtn, 0.2f, 16, replayColor);
            DrawRectangleRounded(menuBtn, 0.2f, 16, menuColor);
            DrawText("REPLAY", replayBtn.x + 14, replayBtn.y + 24, 24, WHITE);
            DrawText("MENU", menuBtn.x + 28, menuBtn.y + 24, 24, WHITE);

            if (CheckCollisionPointRec(uiPointer, replayBtn) && tapPressed) {
                currentWave = 1;
                SpawnWave(currentWave);
                gameOver = false;
                PlaySoundSafe(buttonPressSound);
                state = GameState::PLAYING;
            }

            if (CheckCollisionPointRec(uiPointer, menuBtn) && tapPressed) {
                PlaySoundSafe(buttonPressSound);
                state = GameState::MENU;
            }

            EndDrawing();
            continue;
        }
    }

#ifdef __EMSCRIPTEN__
    };
    emscripten_set_main_loop_arg([](void* arg){ (*reinterpret_cast<decltype(main_loop)*>(arg))(); }, &main_loop, 0, 1);
#endif

    if (audioAvailable) {
        UnloadSound(shootSound);
        UnloadSound(enemyHitSound);
        UnloadSound(playerHitSound);
        UnloadSound(buttonPressSound);
        UnloadSound(explosionSound);
        UnloadSound(gameOverSound);
        CloseAudioDevice();
    }
    UnloadTexture(splashLogo);
    CloseWindow();
    return 0;
}
