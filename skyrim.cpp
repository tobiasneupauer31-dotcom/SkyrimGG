#include "raylib.h"
#include "raymath.h"
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// --- FARBY ---
#define LIGHTGREEN CLITERAL(Color){144,238,144,255}
#define MY_DARKGREEN CLITERAL(Color){0,100,0,255}
#define SAND CLITERAL(Color){238,214,175,255}
#define WATER_COLOR CLITERAL(Color){0,121,241,100}
#define MY_GOLD CLITERAL(Color){255,215,0,255}
const Color SKYRIM_BG_TOP = {18,18,22,255};
const Color SKYRIM_BG_BOT = {6,6,8,255};
const Color SKY_DAY_TOP = {82,140,200,255};
const Color SKY_DAY_BOT = {150,190,230,255};
const Color SKY_NIGHT_TOP = {12,14,28,255};
const Color SKY_NIGHT_BOT = {5,6,16,255};
const Color SKY_SUN = {255,214,120,255};
const Color SKY_MOON = {200,210,230,255};
const Color SKYRIM_PANEL = {30,28,26,230};
const Color SKYRIM_PANEL_EDGE = {90,84,72,255};
const Color SKYRIM_GOLD = {210,178,96,255};
const Color SKYRIM_GLOW = {210,178,96,90};
const Color SKYRIM_TEXT = {220,216,200,255};
const Color SKYRIM_MUTED = {140,136,120,255};
const Color BIOME_FOREST = {46,104,54,255};
const Color BIOME_ROCK = {120,120,120,255};
const Color BIOME_SNOW = {215,215,230,255};
const Color ROAD_COLOR = {90,72,52,255};
const Color STONE_LIGHT = {150,150,155,255};
const Color STONE_DARK = {90,90,95,255};
const Color BANNER_BLUE = {40,80,140,255};

const int MAP_SIZE = 320;
const float WORLD_SIZE = 520.0f;
const float HALF_WORLD = WORLD_SIZE * 0.5f;

// --- ŠTRUKTÚRY ---
struct Item { 
    std::string name; std::string category; int damage; int heal; int price; float weight; 
    bool isEquipped; std::string description; 
};

struct Enemy { 
    Vector3 position; Vector3 startPos; float health; float maxHealth; 
    bool active; float speed; float detectionRange; bool isAggressive; float attackTimer;
    int type; // 0 melee bandit, 1 archer, 2 wolf
    float rangedCooldown;
    float attackAnimTimer;
    float burnTimer = 0.0f;
    float slowTimer = 0.0f;
    float poisonTimer = 0.0f;
};

struct Tree { Vector3 position; };
// type: 0 fire, 1 frost, 2 poison
struct Fireball { Vector3 position; Vector3 direction; bool active; float timer; float speed; float damage; int type; };
struct Projectile { Vector3 position; Vector3 direction; bool active; float timer; float speed; float damage; Color color; };
struct Chest { Vector3 position; bool active; bool locked; int lockDifficulty; Item content; };
struct NPC { Vector3 position; std::string name; std::string message; bool isTalking; bool hasQuest; int questProgress; };
struct House { Vector3 position; Vector3 size; float roofHeight; Color wall; Color roof; Vector3 doorDir; bool chimney; bool canEnter; int shopIndex; };
enum SkillSection {HEALTH=0, DAMAGE, STAMINA, MANA};

enum class WeaponKind { SWORD=0, AXE, DAGGER, BOW, STAFF };

enum class Skill {
    ONE_HANDED = 0,
    ARCHERY,
    DESTRUCTION,
    LOCKPICKING,
    SMITHING,
    ALCHEMY,
    SPEECH,
    SNEAK,
    COUNT
};

struct SkillState {
    int level = 15;
    float xp = 0.0f;
};

enum class Faction {
    GUARD = 0,
    SMITHS,
    ALCHEMISTS,
    HUNTERS,
    TOWNSFOLK,
    COUNT
};

struct Perk {
    std::string name;
    bool unlocked;
    int levelReq;
    SkillSection section;
    int cost;
    std::string info;
    Vector2 pos;
    std::vector<int> children;
};

enum class MenuView {
    HOME = 0,
    INVENTORY,
    SKILL_TREE,
    QUESTS
};

enum InteriorScene {
    INT_HOUSE = 0,
    INT_BLACKSMITH,
    INT_ALCHEMIST,
    INT_GENERAL,
    INT_INN,
    INT_CASTLE,
    INT_HUNTER,
    INT_TEMPLE
};

struct Quest {
    std::string title;
    std::string objective;
    std::string reward;
    bool completed;
    bool active;
    bool rewardGiven;
    bool notified;
    int rewardGold;
    int rewardXp;
    int rewardPerkPoints;
    std::string id;
    int step = 0;
    int target = 0;
};

struct LootDrop {
    Vector3 position;
    bool active;
    Item item;
    int gold;
    float timer;
};

struct Notification {
    std::string text;
    float timer;
};

struct POI {
    std::string name;
    Vector3 position;
    bool discovered;
    bool canFastTravel;
    Color color;
};

struct WeatherParticle {
    Vector2 pos;
    float speed;
    float drift;
    float size;
};

enum class LockTargetType { NONE = 0, CHEST, HOUSE };

struct LockpickState {
    bool active = false;
    LockTargetType targetType = LockTargetType::NONE;
    int index = -1;
    int difficulty = 0; // 0 easy, 1 medium, 2 hard

    float sweetSpotDeg = 90.0f;
    float toleranceDeg = 18.0f;
    float pickDeg = 30.0f;
    float lockTurn = 0.0f;        // 0..1
    float durability = 1.0f;      // 0..1
    float breakCooldown = 0.0f;
};

struct KillcamState {
    bool active = false;
    float timer = 0.0f;
    float duration = 0.0f;
    Vector3 target = {0,0,0};
};

struct IntroCinematicState {
    bool played = false;
    bool active = false;
    float t = 0.0f;
    float duration = 17.0f;

    Vector3 cartPos = {0,0,0};
    float cartYaw = 0.0f;
    float cartSpeed = 7.0f;

    bool cartDepart = false;
    float cartDepartTimer = 0.0f;
    int cueIdx = -1;
};

static float Rand01(){
    return (float)GetRandomValue(0, 10000)/10000.0f;
}

static int ClampInt(int v, int lo, int hi){
    return (v < lo) ? lo : (v > hi ? hi : v);
}
 
static void EnsureParticles(std::vector<WeatherParticle>& parts, int count, int w, int h, float minSpeed, float maxSpeed, float minSize, float maxSize, float driftScale){
    if((int)parts.size() == count) return;
    parts.clear();
    parts.reserve(count);
    for(int i=0;i<count;i++){
        WeatherParticle p;
        p.pos = {(float)GetRandomValue(0,w), (float)GetRandomValue(0,h)};
        p.speed = minSpeed + (maxSpeed-minSpeed)*Rand01();
        p.size  = minSize + (maxSize-minSize)*Rand01();
        p.drift = (Rand01()*2.0f - 1.0f)*driftScale;
        parts.push_back(p);
    }
}

static void UpdateAndDrawRain(std::vector<WeatherParticle>& drops, int w, int h, float dt){
    Color col = Fade(SKYBLUE, 0.45f);
    for(auto& d : drops){
        d.pos.x += d.drift*dt;
        d.pos.y += d.speed*dt;
        if(d.pos.y > (float)h + 20.0f){
            d.pos.y = (float)GetRandomValue(-60, -10);
            d.pos.x = (float)GetRandomValue(0, w);
        }
        if(d.pos.x < -20.0f) d.pos.x = (float)w + 10.0f;
        if(d.pos.x > (float)w + 20.0f) d.pos.x = -10.0f;
        DrawLine((int)d.pos.x, (int)d.pos.y, (int)(d.pos.x + 4), (int)(d.pos.y + 12), col);
    }
}

static void UpdateAndDrawSnow(std::vector<WeatherParticle>& flakes, int w, int h, float dt, float t){
    for(int i=0;i<(int)flakes.size();i++){
        auto& f = flakes[i];
        f.pos.x += (f.drift + sinf(t*0.8f + (float)i)*10.0f)*dt;
        f.pos.y += f.speed*dt;
        if(f.pos.y > (float)h + 10.0f){
            f.pos.y = (float)GetRandomValue(-40, -10);
            f.pos.x = (float)GetRandomValue(0, w);
        }
        if(f.pos.x < -20.0f) f.pos.x = (float)w + 10.0f;
        if(f.pos.x > (float)w + 20.0f) f.pos.x = -10.0f;
        DrawCircle((int)f.pos.x, (int)f.pos.y, f.size, Fade(WHITE, 0.65f));
    }
}

static void DrawCrosshair(int w, int h, Color col){
    int cx = w/2;
    int cy = h/2;
    DrawLine(cx-7, cy, cx-2, cy, col);
    DrawLine(cx+2, cy, cx+7, cy, col);
    DrawLine(cx, cy-7, cx, cy-2, col);
    DrawLine(cx, cy+2, cx, cy+7, col);
    DrawCircleLines(cx, cy, 2, Fade(col, 0.6f));
}

static void DrawSkyrimBar(Rectangle r, float value, float maxValue, Color fill, Color edge, const char* label){
    float pct = (maxValue > 0.0f) ? Clamp(value/maxValue, 0.0f, 1.0f) : 0.0f;
    DrawRectangleRec(r, Fade(BLACK, 0.55f));
    DrawRectangleLinesEx(r, 2, Fade(edge, 0.85f));
    Rectangle f = {r.x+2, r.y+2, (r.width-4)*pct, r.height-4};
    DrawRectangleRec(f, Fade(fill, 0.95f));
    DrawText(label, (int)r.x, (int)(r.y - 18), 16, SKYRIM_TEXT);
}

static void DrawTextWrapped(const std::string& text, Rectangle rec, int fontSize, Color col){
    std::string line;
    std::string word;
    float y = rec.y;
    auto flushLine = [&](){
        if(!line.empty()){
            DrawText(line.c_str(), (int)rec.x, (int)y, fontSize, col);
            y += (float)fontSize + 4.0f;
            line.clear();
        }
    };
    auto appendWord = [&](const std::string& w){
        std::string test = line.empty() ? w : (line + " " + w);
        if(MeasureText(test.c_str(), fontSize) > (int)rec.width && !line.empty()){
            flushLine();
            test = w;
        }
        line = test;
    };

    for(size_t i=0;i<=text.size();i++){
        char c = (i < text.size()) ? text[i] : '\n';
        if(c=='\n'){
            if(!word.empty()){ appendWord(word); word.clear(); }
            flushLine();
            if(y > rec.y + rec.height - fontSize) return;
            continue;
        }
        if(c==' ' || c=='\t'){
            if(!word.empty()){
                appendWord(word);
                word.clear();
            }
        } else {
            word.push_back(c);
        }
        if(y > rec.y + rec.height - fontSize) return;
    }
    if(!word.empty()) appendWord(word);
    flushLine();
}

static void DrawSubtitlePanel(int screenWidth, int screenHeight, const std::string& line){
    Rectangle panel = {screenWidth/2.0f - 520.0f, (float)screenHeight - 150.0f, 1040.0f, 96.0f};
    DrawRectangleRec(panel, Fade(BLACK, 0.70f));
    DrawRectangleLinesEx(panel, 2, Fade(SKYRIM_PANEL_EDGE, 0.85f));
    Rectangle textRec = {panel.x + 18, panel.y + 18, panel.width - 36, panel.height - 36};
    DrawTextWrapped(line, textRec, 20, SKYRIM_TEXT);
}

static void DrawCartAndHorses(Vector3 cartPos, float yawRad){
    // very simple stylized cart + two horses
    Vector3 fwd = {sinf(yawRad), 0.0f, cosf(yawRad)};
    Vector3 right = {cosf(yawRad), 0.0f, -sinf(yawRad)};

    auto at = [&](float r, float u, float f)->Vector3{
        Vector3 p = cartPos;
        p.x += right.x*r + fwd.x*f;
        p.y += u;
        p.z += right.z*r + fwd.z*f;
        return p;
    };

    // cart bed
    DrawCube(at(0.0f, 0.65f, 0.0f), 2.2f, 0.6f, 1.4f, BROWN);
    DrawCubeWires(at(0.0f, 0.65f, 0.0f), 2.2f, 0.6f, 1.4f, Fade(BLACK,0.35f));
    // side rails
    DrawCube(at(0.0f, 1.05f, 0.0f), 2.2f, 0.15f, 1.4f, DARKBROWN);
    DrawCube(at(0.0f, 0.95f, 0.65f), 2.2f, 0.35f, 0.1f, DARKBROWN);
    DrawCube(at(0.0f, 0.95f,-0.65f), 2.2f, 0.35f, 0.1f, DARKBROWN);

    // wheels
    DrawCylinder(at(0.95f, 0.25f, 0.55f), 0.28f, 0.28f, 0.2f, 10, GRAY);
    DrawCylinder(at(-0.95f,0.25f, 0.55f), 0.28f, 0.28f, 0.2f, 10, GRAY);
    DrawCylinder(at(0.95f, 0.25f,-0.55f), 0.28f, 0.28f, 0.2f, 10, GRAY);
    DrawCylinder(at(-0.95f,0.25f,-0.55f), 0.28f, 0.28f, 0.2f, 10, GRAY);

    // shaft
    DrawCube(at(0.0f, 0.55f, 1.45f), 0.25f, 0.12f, 2.0f, DARKBROWN);

    // horses
    for(int i=0;i<2;i++){
        float side = (i==0) ? -0.65f : 0.65f;
        Vector3 hp = at(side, 0.0f, 2.9f);
        hp.y = cartPos.y;
        DrawCube({hp.x, hp.y+0.75f, hp.z}, 0.6f, 0.9f, 1.5f, DARKBROWN);
        DrawCube({hp.x, hp.y+1.1f, hp.z+0.95f}, 0.35f, 0.35f, 0.45f, DARKBROWN); // head
        DrawCylinder({hp.x-0.2f, hp.y+0.25f, hp.z-0.5f}, 0.07f, 0.07f, 0.5f, 8, DARKBROWN);
        DrawCylinder({hp.x+0.2f, hp.y+0.25f, hp.z-0.5f}, 0.07f, 0.07f, 0.5f, 8, DARKBROWN);
        DrawCylinder({hp.x-0.2f, hp.y+0.25f, hp.z+0.5f}, 0.07f, 0.07f, 0.5f, 8, DARKBROWN);
        DrawCylinder({hp.x+0.2f, hp.y+0.25f, hp.z+0.5f}, 0.07f, 0.07f, 0.5f, 8, DARKBROWN);
    }
}

struct IntroCue {
    float start;
    float end;
    const char* text;
    int voiceIndex; // maps to sound/intro_{index}.wav if available
};

static const IntroCue INTRO_CUES[] = {
    {0.00f, 5.50f, "Vozkar: Hej. Nezaspavaj. Este chvilu... Brana mesta je dnes zatvorena a to nikdy nie je dobre znamenie.", 0},
    {5.50f, 11.00f, "Spoluvaznen: Zavreli ju po tom, co sa v noci nieco hybalo v ruinach. Hovoria, ze kamene spievaju... a straze sa boja pocuvat.", 1},
    {11.00f, 13.60f, "Strazca (v dialke): Halt! Zastavte voz! Kto ide k brane?", 2},
    {13.60f, 17.00f, "Vozkar: Tu vystup. Ak chces dnu, budes potrebovat povolenie. A ak chces prezit... drz sa pri svetle.", 3}
};

static int GetIntroCueIndex(float t){
    for(int i=0;i<(int)(sizeof(INTRO_CUES)/sizeof(INTRO_CUES[0]));i++){
        if(t >= INTRO_CUES[i].start && t < INTRO_CUES[i].end) return i;
    }
    return -1;
}

static bool InventoryHasItem(const std::vector<Item>& inv, const std::string& name){
    for(const auto& it : inv){
        if(it.name == name) return true;
    }
    return false;
}

static bool InventoryRemoveItemOnce(std::vector<Item>& inv, const std::string& name){
    for(int i=0;i<(int)inv.size();i++){
        if(inv[i].name == name){
            inv.erase(inv.begin()+i);
            return true;
        }
    }
    return false;
}

static int InventoryCountItem(const std::vector<Item>& inv, const std::string& name){
    int n = 0;
    for(const auto& it : inv) if(it.name == name) n++;
    return n;
}

static bool InventoryRemoveItemN(std::vector<Item>& inv, const std::string& name, int count){
    for(int i=0;i<count;i++){
        if(!InventoryRemoveItemOnce(inv, name)) return false;
    }
    return true;
}

struct WeaponUpgrade {
    std::string weaponName;
    int bonusDamage;
};

static int GetWeaponUpgradeBonus(const std::vector<WeaponUpgrade>& ups, const std::string& weaponName){
    for(const auto& u : ups) if(u.weaponName == weaponName) return u.bonusDamage;
    return 0;
}

static void AddWeaponUpgradeBonus(std::vector<WeaponUpgrade>& ups, const std::string& weaponName, int delta, int cap){
    for(auto& u : ups){
        if(u.weaponName == weaponName){
            u.bonusDamage = std::min(cap, u.bonusDamage + delta);
            return;
        }
    }
    ups.push_back({weaponName, std::min(cap, delta)});
}

static Vector3 RotateYVec(Vector3 v, float yawRad){
    float s = sinf(yawRad);
    float c = cosf(yawRad);
    return {v.x*c + v.z*s, v.y, -v.x*s + v.z*c};
}

static Vector3 AtYaw(Vector3 origin, Vector3 localOffset, float yawRad){
    return Vector3Add(origin, RotateYVec(localOffset, yawRad));
}

static float DistPointSegmentXZ(Vector3 p, Vector3 a, Vector3 b){
    Vector2 pp = {p.x, p.z};
    Vector2 aa = {a.x, a.z};
    Vector2 bb = {b.x, b.z};
    Vector2 ab = Vector2Subtract(bb, aa);
    float abLen2 = ab.x*ab.x + ab.y*ab.y;
    if(abLen2 < 0.000001f){
        Vector2 d = Vector2Subtract(pp, aa);
        return sqrtf(d.x*d.x + d.y*d.y);
    }
    Vector2 ap = Vector2Subtract(pp, aa);
    float t = (ap.x*ab.x + ap.y*ab.y) / abLen2;
    t = Clamp(t, 0.0f, 1.0f);
    Vector2 closest = {aa.x + ab.x*t, aa.y + ab.y*t};
    Vector2 d = Vector2Subtract(pp, closest);
    return sqrtf(d.x*d.x + d.y*d.y);
}

static void DrawPlayerAvatar(Vector3 pos, float yawRad, float walkPhase, bool moving, bool sprinting, float attack01){
    float stride = moving ? (sprinting ? 0.55f : 0.35f) : 0.0f;
    float swing = sinf(walkPhase) * stride;
    float swingOpp = sinf(walkPhase + PI) * stride;

    Color armor = Fade(GRAY, 0.92f);
    Color cloth = {92, 60, 130, 255};
    Color leather = DARKBROWN;
    Color skin = BEIGE;
    Color steel = LIGHTGRAY;
    bool attacking = (attack01 > 0.001f);

    // shadow
    DrawCircle3D({pos.x, pos.y+0.02f, pos.z}, 0.55f, {1,0,0}, 90.0f, Fade(BLACK, 0.25f));

    // torso
    Vector3 torsoA = AtYaw(pos, {0.0f, 0.95f, 0.0f}, yawRad);
    Vector3 torsoB = AtYaw(pos, {0.0f, 1.75f, 0.0f}, yawRad);
    DrawCapsule(torsoA, torsoB, 0.28f, 10, 6, cloth);
    DrawCapsule(AtYaw(pos, {0.0f, 1.05f, 0.02f}, yawRad), AtYaw(pos, {0.0f, 1.72f, 0.02f}, yawRad), 0.31f, 10, 6, Fade(armor, 0.9f));

    // head + hood/helmet
    DrawSphere(AtYaw(pos, {0.0f, 2.05f, 0.0f}, yawRad), 0.22f, skin);
    DrawSphere(AtYaw(pos, {0.0f, 2.12f, 0.0f}, yawRad), 0.24f, Fade(armor, 0.85f));

    // legs
    Vector3 lHip = AtYaw(pos, {-0.18f, 0.90f, 0.0f}, yawRad);
    Vector3 lFoot = AtYaw(pos, {-0.18f, 0.12f, 0.12f*swing}, yawRad);
    Vector3 rHip = AtYaw(pos, { 0.18f, 0.90f, 0.0f}, yawRad);
    Vector3 rFoot = AtYaw(pos, { 0.18f, 0.12f, 0.12f*swingOpp}, yawRad);
    DrawCapsule(lHip, lFoot, 0.12f, 10, 6, leather);
    DrawCapsule(rHip, rFoot, 0.12f, 10, 6, leather);
    DrawSphere(lFoot, 0.13f, Fade(armor,0.9f));
    DrawSphere(rFoot, 0.13f, Fade(armor,0.9f));

    // arms
    Vector3 lShoulder = AtYaw(pos, {-0.38f, 1.60f, 0.0f}, yawRad);
    Vector3 lHand = AtYaw(pos, {-0.55f, 0.98f, -0.10f*swingOpp}, yawRad);
    Vector3 rShoulder = AtYaw(pos, { 0.38f, 1.60f, 0.0f}, yawRad);
    Vector3 rHand = AtYaw(pos, { 0.55f, 0.98f, -0.10f*swing}, yawRad);
    if(attacking){
        float arc = sinf(attack01 * PI);       // 0..1..0
        float sweep = cosf(attack01 * PI);     // 1..-1
        rShoulder = AtYaw(pos, {0.34f, 1.64f, 0.06f}, yawRad);
        rHand = AtYaw(pos, {0.65f*sweep, 1.10f + 0.32f*arc, 0.46f}, yawRad);
    }
    DrawCapsule(lShoulder, lHand, 0.10f, 10, 6, cloth);
    DrawCapsule(rShoulder, rHand, 0.10f, 10, 6, cloth);
    DrawSphere(lHand, 0.10f, skin);
    DrawSphere(rHand, 0.10f, skin);

    // belt
    DrawCube(AtYaw(pos, {0.0f, 1.08f, 0.02f}, yawRad), 0.85f, 0.14f, 0.55f, DARKBROWN);
    if(attacking){
        float arc = sinf(attack01 * PI);
        float sweep = cosf(attack01 * PI);
        Vector3 swordTip = Vector3Add(rHand, RotateYVec({-0.35f*sweep, 0.25f + 0.15f*arc, 1.15f}, yawRad));
        Vector3 swordGuard = Vector3Add(rHand, RotateYVec({0.0f, 0.05f, 0.12f}, yawRad));
        DrawCylinderEx(rHand, swordTip, 0.035f, 0.02f, 10, steel);
        DrawCube(swordGuard, 0.24f, 0.04f, 0.10f, DARKGRAY);
        DrawSphere(swordTip, 0.04f, steel);
    } else {
        // sword (back scabbard)
        Vector3 swordBase = AtYaw(pos, {0.28f, 1.35f, -0.18f}, yawRad);
        Vector3 swordTip = AtYaw(pos, {0.28f, 1.75f, -0.55f}, yawRad);
        DrawCylinderEx(swordBase, swordTip, 0.03f, 0.03f, 8, steel);
        DrawSphere(swordTip, 0.045f, steel);
    }
}

static void DrawPlayerAvatar(Vector3 pos, float yawRad, float walkPhase, bool moving, bool sprinting, float attack01, WeaponKind wKind){
    // draw base body
    float stride = moving ? (sprinting ? 0.55f : 0.35f) : 0.0f;
    float swing = sinf(walkPhase) * stride;
    float swingOpp = sinf(walkPhase + PI) * stride;

    Color armor = Fade(GRAY, 0.92f);
    Color cloth = {92, 60, 130, 255};
    Color leather = DARKBROWN;
    Color skin = BEIGE;
    Color steel = LIGHTGRAY;
    bool attacking = (attack01 > 0.001f);

    DrawCircle3D({pos.x, pos.y+0.02f, pos.z}, 0.55f, {1,0,0}, 90.0f, Fade(BLACK, 0.25f));

    Vector3 torsoA = AtYaw(pos, {0.0f, 0.95f, 0.0f}, yawRad);
    Vector3 torsoB = AtYaw(pos, {0.0f, 1.75f, 0.0f}, yawRad);
    DrawCapsule(torsoA, torsoB, 0.28f, 10, 6, cloth);
    DrawCapsule(AtYaw(pos, {0.0f, 1.05f, 0.02f}, yawRad), AtYaw(pos, {0.0f, 1.72f, 0.02f}, yawRad), 0.31f, 10, 6, Fade(armor, 0.9f));

    DrawSphere(AtYaw(pos, {0.0f, 2.05f, 0.0f}, yawRad), 0.22f, skin);
    DrawSphere(AtYaw(pos, {0.0f, 2.12f, 0.0f}, yawRad), 0.24f, Fade(armor, 0.85f));

    Vector3 lHip = AtYaw(pos, {-0.18f, 0.90f, 0.0f}, yawRad);
    Vector3 lFoot = AtYaw(pos, {-0.18f, 0.12f, 0.12f*swing}, yawRad);
    Vector3 rHip = AtYaw(pos, { 0.18f, 0.90f, 0.0f}, yawRad);
    Vector3 rFoot = AtYaw(pos, { 0.18f, 0.12f, 0.12f*swingOpp}, yawRad);
    DrawCapsule(lHip, lFoot, 0.12f, 10, 6, leather);
    DrawCapsule(rHip, rFoot, 0.12f, 10, 6, leather);
    DrawSphere(lFoot, 0.13f, Fade(armor,0.9f));
    DrawSphere(rFoot, 0.13f, Fade(armor,0.9f));

    Vector3 lShoulder = AtYaw(pos, {-0.38f, 1.60f, 0.0f}, yawRad);
    Vector3 lHand = AtYaw(pos, {-0.55f, 0.98f, -0.10f*swingOpp}, yawRad);
    Vector3 rShoulder = AtYaw(pos, { 0.38f, 1.60f, 0.0f}, yawRad);
    Vector3 rHand = AtYaw(pos, { 0.55f, 0.98f, -0.10f*swing}, yawRad);
    if(attacking){
        float arc = sinf(attack01 * PI);
        float sweep = cosf(attack01 * PI);
        rShoulder = AtYaw(pos, {0.34f, 1.64f, 0.06f}, yawRad);
        rHand = AtYaw(pos, {0.65f*sweep, 1.10f + 0.32f*arc, 0.46f}, yawRad);
    }
    DrawCapsule(lShoulder, lHand, 0.10f, 10, 6, cloth);
    DrawCapsule(rShoulder, rHand, 0.10f, 10, 6, cloth);
    DrawSphere(lHand, 0.10f, skin);
    DrawSphere(rHand, 0.10f, skin);

    DrawCube(AtYaw(pos, {0.0f, 1.08f, 0.02f}, yawRad), 0.85f, 0.14f, 0.55f, DARKBROWN);

    auto DrawSword = [&](bool inHand){
        if(inHand){
            float arc = sinf(attack01 * PI);
            float sweep = cosf(attack01 * PI);
            Vector3 tip = Vector3Add(rHand, RotateYVec({-0.35f*sweep, 0.25f + 0.15f*arc, 1.15f}, yawRad));
            Vector3 guard = Vector3Add(rHand, RotateYVec({0.0f, 0.05f, 0.12f}, yawRad));
            DrawCylinderEx(rHand, tip, 0.035f, 0.02f, 10, steel);
            DrawCube(guard, 0.24f, 0.04f, 0.10f, DARKGRAY);
            DrawSphere(tip, 0.04f, steel);
        } else {
            Vector3 base = AtYaw(pos, {0.28f, 1.35f, -0.18f}, yawRad);
            Vector3 tip  = AtYaw(pos, {0.28f, 1.75f, -0.55f}, yawRad);
            DrawCylinderEx(base, tip, 0.03f, 0.03f, 8, steel);
            DrawSphere(tip, 0.045f, steel);
        }
    };
    auto DrawDagger = [&](bool inHand){
        if(inHand){
            float arc = sinf(attack01 * PI);
            Vector3 tip = Vector3Add(rHand, RotateYVec({0.0f, 0.18f + 0.06f*arc, 0.55f}, yawRad));
            DrawCylinderEx(rHand, tip, 0.03f, 0.015f, 10, steel);
            DrawCube(Vector3Add(rHand, RotateYVec({0.0f,0.06f,0.08f}, yawRad)), 0.16f, 0.03f, 0.08f, DARKGRAY);
            DrawSphere(tip, 0.03f, steel);
        } else {
            Vector3 base = AtYaw(pos, {0.26f, 1.18f, -0.10f}, yawRad);
            Vector3 tip  = AtYaw(pos, {0.26f, 1.38f, -0.36f}, yawRad);
            DrawCylinderEx(base, tip, 0.025f, 0.02f, 8, steel);
        }
    };
    auto DrawAxe = [&](bool inHand){
        if(inHand){
            float arc = sinf(attack01 * PI);
            float sweep = cosf(attack01 * PI);
            Vector3 tip = Vector3Add(rHand, RotateYVec({-0.20f*sweep, 0.22f + 0.18f*arc, 0.95f}, yawRad));
            DrawCylinderEx(rHand, tip, 0.04f, 0.03f, 10, DARKBROWN);
            DrawCube(Vector3Add(tip, RotateYVec({0.0f,0.02f,0.10f}, yawRad)), 0.34f, 0.26f, 0.08f, Fade(DARKGRAY,0.95f));
        } else {
            Vector3 base = AtYaw(pos, {0.30f, 1.35f, -0.18f}, yawRad);
            Vector3 tip  = AtYaw(pos, {0.30f, 1.70f, -0.48f}, yawRad);
            DrawCylinderEx(base, tip, 0.035f, 0.035f, 8, DARKBROWN);
        }
    };
    auto DrawBow = [&](bool inHand){
        if(inHand){
            Vector3 a = Vector3Add(lHand, RotateYVec({0.0f, 0.20f, 0.35f}, yawRad));
            Vector3 b = Vector3Add(lHand, RotateYVec({0.0f, 0.55f, -0.05f}, yawRad));
            DrawCylinderEx(a, b, 0.02f, 0.02f, 10, DARKBROWN);
            DrawLine3D(a, b, Fade(WHITE, 0.55f));
        } else {
            Vector3 a = AtYaw(pos, {-0.35f, 1.25f, -0.25f}, yawRad);
            Vector3 b = AtYaw(pos, {-0.35f, 1.55f, -0.45f}, yawRad);
            DrawCylinderEx(a, b, 0.02f, 0.02f, 8, DARKBROWN);
        }
    };

    if(attacking){
        if(wKind == WeaponKind::DAGGER) DrawDagger(true);
        else if(wKind == WeaponKind::AXE) DrawAxe(true);
        else if(wKind == WeaponKind::BOW) DrawBow(true);
        else DrawSword(true);
    } else {
        if(wKind == WeaponKind::DAGGER) DrawDagger(false);
        else if(wKind == WeaponKind::AXE) DrawAxe(false);
        else if(wKind == WeaponKind::BOW) DrawBow(false);
        else DrawSword(false);
    }
}

static float YawToFace(Vector3 from, Vector3 to){
    Vector3 d = Vector3Subtract(to, from);
    return atan2f(d.x, d.z);
}

static void DrawHumanoidBasic(Vector3 pos, float yawRad, float scale, Color cloth, Color armor, Color skin, bool helmet){
    auto at = [&](Vector3 o)->Vector3{ return Vector3Add(pos, RotateYVec(Vector3Scale(o, scale), yawRad)); };

    DrawCircle3D({pos.x, pos.y+0.02f, pos.z}, 0.55f*scale, {1,0,0}, 90.0f, Fade(BLACK, 0.22f));

    Vector3 torsoA = at({0.0f, 0.90f, 0.0f});
    Vector3 torsoB = at({0.0f, 1.70f, 0.0f});
    DrawCapsule(torsoA, torsoB, 0.28f*scale, 10, 6, cloth);
    DrawCapsule(at({0.0f, 1.02f, 0.02f}), at({0.0f, 1.68f, 0.02f}), 0.31f*scale, 10, 6, Fade(armor, 0.88f));

    DrawSphere(at({0.0f, 2.02f, 0.0f}), 0.22f*scale, skin);
    DrawSphere(at({0.0f, 2.08f, 0.0f}), 0.24f*scale, helmet ? Fade(armor,0.92f) : Fade(cloth,0.55f));

    Vector3 lHip = at({-0.18f, 0.86f, 0.0f});
    Vector3 lFoot = at({-0.18f, 0.12f, 0.05f});
    Vector3 rHip = at({ 0.18f, 0.86f, 0.0f});
    Vector3 rFoot = at({ 0.18f, 0.12f, 0.05f});
    DrawCapsule(lHip, lFoot, 0.12f*scale, 10, 6, DARKBROWN);
    DrawCapsule(rHip, rFoot, 0.12f*scale, 10, 6, DARKBROWN);
    DrawSphere(lFoot, 0.13f*scale, Fade(armor,0.85f));
    DrawSphere(rFoot, 0.13f*scale, Fade(armor,0.85f));

    Vector3 lShoulder = at({-0.38f, 1.55f, 0.0f});
    Vector3 lHand = at({-0.55f, 0.98f, 0.06f});
    Vector3 rShoulder = at({ 0.38f, 1.55f, 0.0f});
    Vector3 rHand = at({ 0.55f, 0.98f, 0.06f});
    DrawCapsule(lShoulder, lHand, 0.10f*scale, 10, 6, cloth);
    DrawCapsule(rShoulder, rHand, 0.10f*scale, 10, 6, cloth);
    DrawSphere(lHand, 0.10f*scale, skin);
    DrawSphere(rHand, 0.10f*scale, skin);

    DrawCube(at({0.0f, 1.02f, 0.02f}), 0.85f*scale, 0.14f*scale, 0.55f*scale, DARKBROWN);
}

enum NpcLook {
    LOOK_OLDMAN = 0,
    LOOK_BLACKSMITH,
    LOOK_GUARD,
    LOOK_INNKEEPER,
    LOOK_ALCHEMIST,
    LOOK_HUNTER,
    LOOK_PATROL,
    LOOK_SHOPKEEP
};

static void DrawNpcAvatar(Vector3 pos, float yawRad, int look){
    Color skin = BEIGE;
    Color cloth = {92, 60, 130, 255};
    Color armor = Fade(GRAY, 0.92f);
    bool helmet = false;

    if(look == LOOK_OLDMAN){ cloth = {70, 62, 55, 255}; armor = {60,60,60,255}; helmet = false; }
    if(look == LOOK_BLACKSMITH){ cloth = {78, 52, 24, 255}; armor = Fade(LIGHTGRAY,0.9f); helmet = true; }
    if(look == LOOK_GUARD){ cloth = BANNER_BLUE; armor = Fade(LIGHTGRAY,0.95f); helmet = true; }
    if(look == LOOK_INNKEEPER){ cloth = {100, 72, 28, 255}; armor = {55,55,55,255}; helmet = false; }
    if(look == LOOK_ALCHEMIST){ cloth = {30, 92, 55, 255}; armor = {55,55,55,255}; helmet = false; }
    if(look == LOOK_HUNTER){ cloth = {62, 82, 38, 255}; armor = {55,55,55,255}; helmet = false; }
    if(look == LOOK_PATROL){ cloth = {90, 80, 60, 255}; armor = Fade(GRAY,0.9f); helmet = true; }
    if(look == LOOK_SHOPKEEP){ cloth = {86, 52, 18, 255}; armor = {55,55,55,255}; helmet = false; }

    DrawHumanoidBasic(pos, yawRad, 1.0f, cloth, armor, skin, helmet);

    auto at = [&](Vector3 o)->Vector3{ return Vector3Add(pos, RotateYVec(o, yawRad)); };
    Vector3 rHand = at({0.55f, 0.98f, 0.06f});
    Vector3 lHand = at({-0.55f, 0.98f, 0.06f});

    if(look == LOOK_OLDMAN){
        Vector3 staffTop = Vector3Add(rHand, RotateYVec({0.05f, 0.95f, 0.15f}, yawRad));
        DrawCylinderEx(rHand, staffTop, 0.05f, 0.04f, 10, DARKBROWN);
        DrawSphere(staffTop, 0.12f, SKYRIM_GOLD);
    } else if(look == LOOK_BLACKSMITH){
        Vector3 hammerHead = Vector3Add(rHand, RotateYVec({0.0f, 0.15f, 0.20f}, yawRad));
        DrawCylinderEx(rHand, hammerHead, 0.035f, 0.03f, 8, DARKBROWN);
        DrawCube(hammerHead, 0.25f, 0.12f, 0.12f, DARKGRAY);
        DrawCube(at({0.0f, 1.25f, 0.25f}), 0.75f, 0.85f, 0.25f, Fade(BLACK,0.15f)); // apron
    } else if(look == LOOK_GUARD){
        Vector3 shield = at({-0.75f, 1.10f, 0.15f});
        DrawCube(shield, 0.10f, 0.75f, 0.55f, BANNER_BLUE);
        DrawCubeWires(shield, 0.10f, 0.75f, 0.55f, Fade(SKYRIM_PANEL_EDGE,0.7f));
        Vector3 spearTip = Vector3Add(rHand, RotateYVec({0.0f, 0.65f, 1.15f}, yawRad));
        DrawCylinderEx(rHand, spearTip, 0.03f, 0.02f, 10, DARKBROWN);
        DrawSphere(spearTip, 0.06f, LIGHTGRAY);
    } else if(look == LOOK_INNKEEPER){
        DrawCube(at({0.0f, 1.10f, 0.28f}), 0.60f, 0.75f, 0.20f, Fade(WHITE,0.25f)); // apron
        DrawSphere(Vector3Add(rHand, RotateYVec({0.0f, 0.05f, 0.20f}, yawRad)), 0.10f, SKYRIM_GOLD); // mug
    } else if(look == LOOK_ALCHEMIST){
        DrawSphere(Vector3Add(rHand, RotateYVec({0.0f, 0.05f, 0.20f}, yawRad)), 0.10f, SKYBLUE);
        DrawSphere(Vector3Add(rHand, RotateYVec({0.0f, 0.18f, 0.20f}, yawRad)), 0.06f, GREEN);
    } else if(look == LOOK_HUNTER){
        Vector3 bowTop = Vector3Add(lHand, RotateYVec({0.0f, 0.55f, 0.75f}, yawRad));
        Vector3 bowBot = Vector3Add(lHand, RotateYVec({0.0f,-0.35f, 0.85f}, yawRad));
        DrawCylinderEx(bowBot, bowTop, 0.025f, 0.025f, 10, DARKBROWN);
        DrawLine3D(bowBot, bowTop, Fade(WHITE,0.55f));
    }
}

static void DrawBanditAvatar(Vector3 pos, float yawRad, bool aggressive, float health01, float attack01){
    Color skin = {120, 110, 100, 255};
    Color passiveCloth = {70,70,70,255};
    Color cloth = aggressive ? MAROON : passiveCloth;
    Color armor = aggressive ? DARKGRAY : GRAY;
    DrawHumanoidBasic(pos, yawRad, 1.05f, cloth, armor, skin, true);

    auto at = [&](Vector3 o)->Vector3{ return Vector3Add(pos, RotateYVec(Vector3Scale(o, 1.05f), yawRad)); };
    Vector3 rHand = at({0.55f, 0.98f, 0.06f});

    float arc = sinf(attack01 * PI);
    float sweep = cosf(attack01 * PI);
    Vector3 tip = Vector3Add(rHand, RotateYVec({-0.25f*sweep, 0.25f + 0.20f*arc, 1.10f}, yawRad));
    DrawCylinderEx(rHand, tip, 0.035f, 0.03f, 10, DARKBROWN);
    DrawCube(Vector3Add(rHand, RotateYVec({-0.12f*sweep, 0.30f + 0.20f*arc, 0.80f}, yawRad)), 0.30f, 0.20f, 0.10f, Fade(LIGHTGRAY, 0.9f));
    DrawSphere(tip, 0.045f, Fade(LIGHTGRAY,0.9f));

    if(health01 < 0.35f){
        DrawSphere(at({0.0f, 2.05f, 0.0f}), 0.26f, Fade(RED, 0.45f));
    }
}

static void DrawArcherAvatar(Vector3 pos, float yawRad, bool aggressive, float health01, float attack01){
    Color skin = {130, 120, 105, 255};
    Color cloth = aggressive ? Color{60, 70, 55, 255} : Color{75, 75, 75, 255};
    Color armor = Fade({120, 100, 70, 255}, 0.95f);
    DrawHumanoidBasic(pos, yawRad, 1.0f, cloth, armor, skin, false);

    auto at = [&](Vector3 o)->Vector3{ return Vector3Add(pos, RotateYVec(o, yawRad)); };
    Vector3 lHand = at({-0.55f, 0.98f, 0.06f});
    Vector3 rHand = at({0.55f, 0.98f, 0.06f});

    float raise = sinf(attack01 * PI);
    Vector3 bowTop = Vector3Add(lHand, RotateYVec({0.0f, 0.35f + 0.20f*raise, 0.75f}, yawRad));
    Vector3 bowBot = Vector3Add(lHand, RotateYVec({0.0f,-0.45f + 0.10f*raise, 0.85f}, yawRad));
    DrawCylinderEx(bowBot, bowTop, 0.025f, 0.025f, 10, DARKBROWN);
    DrawLine3D(bowBot, bowTop, Fade(WHITE,0.55f));

    if(raise > 0.1f){
        Vector3 arrowA = Vector3Add(rHand, RotateYVec({0.0f, 0.10f, 0.55f}, yawRad));
        Vector3 arrowB = Vector3Add(arrowA, RotateYVec({0.0f, 0.05f, 0.90f}, yawRad));
        DrawCylinderEx(arrowA, arrowB, 0.012f, 0.010f, 8, {200,170,60,255});
        DrawSphere(arrowB, 0.02f, LIGHTGRAY);
    }

    if(health01 < 0.35f){
        DrawSphere(at({0.0f, 2.02f, 0.0f}), 0.24f, Fade(RED, 0.30f));
    }
}

static void DrawWolfAvatar(Vector3 pos, float yawRad, bool aggressive, float health01, float attack01){
    (void)aggressive;
    float t = attack01;
    float snap = sinf(t*PI);
    Color fur = {85, 85, 90, 255};
    Color fur2 = {60, 60, 65, 255};

    DrawCircle3D({pos.x, pos.y+0.02f, pos.z}, 0.65f, {1,0,0}, 90.0f, Fade(BLACK, 0.22f));

    Vector3 bodyA = Vector3Add(pos, RotateYVec({0.0f, 0.55f, 0.10f}, yawRad));
    Vector3 bodyB = Vector3Add(pos, RotateYVec({0.0f, 0.75f, -0.75f}, yawRad));
    DrawCapsule(bodyA, bodyB, 0.32f, 10, 6, fur);
    DrawSphere(Vector3Add(pos, RotateYVec({0.0f, 0.78f, -0.95f}, yawRad)), 0.26f, fur2); // rump

    Vector3 head = Vector3Add(pos, RotateYVec({0.0f, 0.80f + 0.10f*snap, 0.65f}, yawRad));
    DrawSphere(head, 0.22f, fur);
    DrawSphere(Vector3Add(head, RotateYVec({0.0f, -0.04f, 0.18f}, yawRad)), 0.10f, DARKGRAY); // snout
    DrawSphere(Vector3Add(head, RotateYVec({0.0f, -0.02f, 0.26f}, yawRad)), 0.05f, BLACK);    // nose

    // legs
    for(int i=-1;i<=1;i+=2){
        DrawCylinder(Vector3Add(pos, RotateYVec({0.22f*(float)i, 0.0f, 0.25f}, yawRad)), 0.06f, 0.05f, 0.55f, 8, fur2);
        DrawCylinder(Vector3Add(pos, RotateYVec({0.22f*(float)i, 0.0f,-0.55f}, yawRad)), 0.06f, 0.05f, 0.55f, 8, fur2);
    }
    // tail
    Vector3 tailA = Vector3Add(pos, RotateYVec({0.0f, 0.75f, -1.00f}, yawRad));
    Vector3 tailB = Vector3Add(pos, RotateYVec({0.0f, 0.55f, -1.45f}, yawRad));
    DrawCylinderEx(tailA, tailB, 0.06f, 0.02f, 8, fur2);

    if(health01 < 0.35f){
        DrawSphere(Vector3Add(pos, RotateYVec({0.0f, 0.95f, 0.45f}, yawRad)), 0.18f, Fade(RED, 0.28f));
    }
}

static void DrawEnemyAvatar(Vector3 pos, float yawRad, int type, bool aggressive, float health01, float attack01){
    if(type == 2){
        DrawWolfAvatar(pos, yawRad, aggressive, health01, attack01);
        return;
    }
    if(type == 1){
        DrawArcherAvatar(pos, yawRad, aggressive, health01, attack01);
        return;
    }
    DrawBanditAvatar(pos, yawRad, aggressive, health01, attack01);
}

static void DrawBossAvatar(Vector3 pos, float yawRad, float health01){
    Color skin = {95, 92, 88, 255};
    Color cloth = {60, 40, 30, 255};
    Color armor = Fade(DARKGRAY, 0.95f);
    float s = 1.75f;
    DrawHumanoidBasic(pos, yawRad, s, cloth, armor, skin, true);

    auto at = [&](Vector3 o)->Vector3{ return Vector3Add(pos, RotateYVec(Vector3Scale(o, s), yawRad)); };
    Vector3 head = at({0.0f, 2.02f, 0.0f});
    DrawSphere(head, 0.30f*s, Fade(armor,0.95f));
    DrawSphere(Vector3Add(head, RotateYVec({-0.10f*s, 0.02f*s, 0.22f*s}, yawRad)), 0.06f*s, ORANGE);
    DrawSphere(Vector3Add(head, RotateYVec({ 0.10f*s, 0.02f*s, 0.22f*s}, yawRad)), 0.06f*s, ORANGE);

    Vector3 hornL = Vector3Add(head, RotateYVec({-0.18f*s, 0.15f*s, 0.05f*s}, yawRad));
    Vector3 hornR = Vector3Add(head, RotateYVec({ 0.18f*s, 0.15f*s, 0.05f*s}, yawRad));
    DrawCylinderEx(hornL, Vector3Add(hornL, RotateYVec({-0.25f*s, 0.25f*s, -0.05f*s}, yawRad)), 0.07f*s, 0.02f*s, 10, LIGHTGRAY);
    DrawCylinderEx(hornR, Vector3Add(hornR, RotateYVec({ 0.25f*s, 0.25f*s, -0.05f*s}, yawRad)), 0.07f*s, 0.02f*s, 10, LIGHTGRAY);

    Vector3 rHand = at({0.55f, 0.98f, 0.06f});
    Vector3 maceTip = Vector3Add(rHand, RotateYVec({0.0f, 0.25f, 1.45f*s/1.75f}, yawRad));
    DrawCylinderEx(rHand, maceTip, 0.06f*s, 0.05f*s, 10, DARKBROWN);
    DrawSphere(maceTip, 0.18f*s, Fade(MAROON,0.95f));

    if(health01 < 0.55f){
        DrawSphere(at({0.0f, 1.55f, 0.0f}), 0.55f*s, Fade(ORANGE, 0.12f));
    }
}

static bool NameHas(const std::string& s, const char* sub){
    return s.find(sub) != std::string::npos;
}

static Color LerpColor(Color a, Color b, float t);

static const char* SkillName(Skill s){
    switch(s){
        case Skill::ONE_HANDED: return "ONE-HANDED";
        case Skill::ARCHERY: return "ARCHERY";
        case Skill::DESTRUCTION: return "DESTRUCTION";
        case Skill::LOCKPICKING: return "LOCKPICKING";
        case Skill::SMITHING: return "SMITHING";
        case Skill::ALCHEMY: return "ALCHEMY";
        case Skill::SPEECH: return "SPEECH";
        case Skill::SNEAK: return "SNEAK";
        default: return "SKILL";
    }
}

static const char* FactionName(Faction f){
    switch(f){
        case Faction::GUARD: return "GUARD";
        case Faction::SMITHS: return "SMITHS";
        case Faction::ALCHEMISTS: return "ALCHEMISTS";
        case Faction::HUNTERS: return "HUNTERS";
        case Faction::TOWNSFOLK: return "TOWNSFOLK";
        default: return "FACTION";
    }
}

static WeaponKind WeaponKindFromItemName(const std::string& n){
    if(NameHas(n, "LUK")) return WeaponKind::BOW;
    if(NameHas(n, "NOZ") || NameHas(n, "DAGGER")) return WeaponKind::DAGGER;
    if(NameHas(n, "SEKAC") || NameHas(n, "AXE")) return WeaponKind::AXE;
    if(NameHas(n, "STAFF") || NameHas(n, "PALICA")) return WeaponKind::STAFF;
    return WeaponKind::SWORD;
}

static float SkillDamageMult(int level){
    // 15 -> 1.00, 100 -> ~1.45
    float t = Clamp(((float)level - 15.0f)/85.0f, 0.0f, 1.0f);
    return 1.0f + 0.45f*t;
}

static float SkillCostMult(int level){
    // 15 -> 1.00, 100 -> 0.75
    float t = Clamp(((float)level - 15.0f)/85.0f, 0.0f, 1.0f);
    return 1.0f - 0.25f*t;
}

static float SkillLockTolBonus(int level){
    // 15 -> 0, 100 -> +12 degrees
    float t = Clamp(((float)level - 15.0f)/85.0f, 0.0f, 1.0f);
    return 12.0f*t;
}

static float RepPriceMult(int rep){
    // rep -100..100 => 1.12..0.88
    float r = Clamp((float)rep/100.0f, -1.0f, 1.0f);
    return 1.0f - 0.12f*r;
}

static void AddSkillXP(SkillState& s, float amount){
    if(amount <= 0.0f) return;
    if(s.level < 15) s.level = 15;
    if(s.level > 100) s.level = 100;
    if(s.level >= 100) return;
    s.xp += amount;
    while(s.level < 100){
        float need = 25.0f + (float)(s.level - 15) * 7.5f;
        if(s.xp < need) break;
        s.xp -= need;
        s.level++;
    }
}

static Texture2D LoadTextureFromProceduralImage(Image img){
    Texture2D t = LoadTextureFromImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    UnloadImage(img);
    return t;
}

static Texture2D MakeWoodTexture(int w, int h){
    Image img = GenImageColor(w, h, {118, 82, 42, 255});
    const int plankW = 10;

    // planks + seams
    for(int x=0;x<w;x+=plankW){
        int plank = x/plankW;
        int dv = (plank*17) % 18; // deterministic variation
        Color c = {(unsigned char)(112+dv), (unsigned char)(76+dv/2), (unsigned char)(38+dv/3), 255};
        ImageDrawRectangle(&img, x, 0, plankW-1, h, c);
        ImageDrawRectangle(&img, x+plankW-1, 0, 1, h, {70, 45, 24, 255});
    }

    // subtle grain + wear
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            Color px = GetImageColor(img, x, y);
            float g = 0.5f + 0.5f*sinf((float)y*0.42f + (float)x*0.18f);
            float g2 = 0.5f + 0.5f*sinf((float)y*0.11f + (float)(x*x)*0.004f);
            float t = Clamp(0.18f*g + 0.10f*g2, 0.0f, 0.35f);
            Color dark = {60, 38, 20, 255};
            Color out = LerpColor(px, dark, t);
            if((x+y) % 97 == 0) out = LerpColor(out, {170,120,70,255}, 0.35f);
            ImageDrawPixel(&img, x, y, out);
        }
    }

    // knots (small dark ovals)
    for(int i=0;i<10;i++){
        int cx = GetRandomValue(2, w-3);
        int cy = GetRandomValue(2, h-3);
        ImageDrawRectangle(&img, cx-2, cy-1, 5, 3, {70, 45, 24, 255});
        ImageDrawRectangle(&img, cx-1, cy, 3, 1, {45, 30, 18, 255});
        ImageDrawRectangle(&img, cx, cy-1, 1, 3, {45, 30, 18, 255});
    }

    return LoadTextureFromProceduralImage(img);
}

static Texture2D MakeStoneTexture(int w, int h){
    Image img = GenImageColor(w, h, {172,178,186,255});
    // simple block pattern
    for(int y=0;y<h;y+=10){
        for(int x=0;x<w;x+=14){
            int ox = (y/10)%2 ? 7 : 0;
            Rectangle r = {(float)((x+ox)%w), (float)y, 13.0f, 9.0f};
            int dv = ((x*17 + y*23) % 26) - 13;
            Color c = {(unsigned char)Clamp(165+dv, 120, 210),
                       (unsigned char)Clamp(170+dv, 120, 215),
                       (unsigned char)Clamp(178+dv, 125, 225), 255};
            ImageDrawRectangleRec(&img, r, c);
        }
    }
    // mortar lines
    for(int y=0;y<h;y+=10) ImageDrawRectangle(&img, 0, y, w, 1, {120,120,120,255});
    for(int x=0;x<w;x+=14) ImageDrawRectangle(&img, x, 0, 1, h, {120,120,120,255});

    // speckles
    for(int i=0;i<w*h/2;i++){
        int x = GetRandomValue(0, w-1);
        int y = GetRandomValue(0, h-1);
        Color px = GetImageColor(img, x, y);
        int dv = GetRandomValue(-18, 18);
        Color out = {(unsigned char)Clamp((int)px.r + dv, 80, 240),
                     (unsigned char)Clamp((int)px.g + dv, 80, 240),
                     (unsigned char)Clamp((int)px.b + dv, 80, 240), 255};
        if(GetRandomValue(0, 100) < 8) out = {90, 95, 105, 255};
        ImageDrawPixel(&img, x, y, out);
    }

    // cracks
    for(int i=0;i<10;i++){
        int x0 = GetRandomValue(0, w-1);
        int y0 = GetRandomValue(0, h-1);
        int len = GetRandomValue(10, 28);
        int dx = GetRandomValue(-1, 1);
        int dy = GetRandomValue(-1, 1);
        if(dx==0 && dy==0) dx=1;
        int x = x0, y = y0;
        for(int k=0;k<len;k++){
            x = (x + dx + w) % w;
            y = (y + dy + h) % h;
            ImageDrawPixel(&img, x, y, {80, 82, 90, 255});
            if(k % 4 == 0) ImageDrawPixel(&img, (x+1)%w, y, {100, 102, 112, 255});
        }
    }
    return LoadTextureFromProceduralImage(img);
}

static Texture2D MakeRoofTexture(int w, int h){
    // thatch-like roof (better when tinted brown)
    Image img = GenImageColor(w, h, {190,170,120,255});
    for(int y=0;y<h;y++){
        int band = y/4;
        int wob = (band*7) % 3;
        for(int x=0;x<w;x++){
            Color base = GetImageColor(img, x, y);
            float stripe = 0.5f + 0.5f*sinf(((float)x + wob)*0.55f + (float)y*0.12f);
            float t = Clamp(0.25f*stripe, 0.0f, 0.25f);
            Color dark = {120, 98, 60, 255};
            Color out = LerpColor(base, dark, t);
            if((x + band*3) % 23 == 0) out = LerpColor(out, {220,200,145,255}, 0.35f);
            ImageDrawPixel(&img, x, y, out);
        }
        if(y % 4 == 0){
            ImageDrawRectangle(&img, 0, y, w, 1, {140,115,75,255});
        }
    }
    ImageDrawRectangle(&img, 0, 0, w, 2, {230,215,165,255}); // ridge highlight
    return LoadTextureFromProceduralImage(img);
}

static void DrawTexturedBox(const Model& mdl, Vector3 pos, float sx, float sy, float sz, Color tint){
    DrawModelEx(mdl, pos, {0,1,0}, 0.0f, {sx, sy, sz}, tint);
}

static void DrawChestModel(Vector3 pos, bool locked, float t){
    float bob = sinf(t*1.8f + pos.x*0.3f + pos.z*0.2f) * 0.02f;
    Vector3 p = {pos.x, pos.y + bob, pos.z};
    Color wood = {118, 76, 38, 255};
    Color metal = locked ? SKYRIM_GOLD : Fade(GRAY, 0.9f);

    DrawCube({p.x, p.y+0.35f, p.z}, 1.5f, 0.7f, 1.0f, wood);
    DrawCube({p.x, p.y+0.78f, p.z}, 1.5f, 0.18f, 1.0f, Fade(wood,0.95f));
    DrawCube({p.x, p.y+0.86f, p.z}, 1.45f, 0.12f, 0.95f, Fade(wood,0.90f));

    // bands + lock
    DrawCube({p.x, p.y+0.40f, p.z}, 1.55f, 0.08f, 0.08f, metal);
    DrawCube({p.x, p.y+0.62f, p.z}, 1.55f, 0.08f, 0.08f, metal);
    DrawCube({p.x, p.y+0.55f, p.z+0.51f}, 0.22f, 0.22f, 0.06f, metal);
}

static void DrawLootModel(const LootDrop& l, float t){
    float bob = 0.18f + sinf(t*2.2f + l.position.x*0.2f + l.position.z*0.25f) * 0.06f;
    float yaw = t*1.4f;
    Vector3 base = {l.position.x, l.position.y + bob, l.position.z};

    // gold-only drop
    if((l.item.category=="NONE" || l.item.name.empty()) && l.gold > 0){
        DrawSphere(base, 0.22f, SKYRIM_GOLD);
        DrawSphere(Vector3Add(base,{0.18f,0.02f,0.10f}), 0.12f, SKYRIM_GOLD);
        DrawSphere(Vector3Add(base,{-0.16f,-0.01f,-0.10f}), 0.10f, SKYRIM_GOLD);
        DrawCube(Vector3Add(base,{0.0f,-0.10f,0.0f}), 0.35f, 0.16f, 0.25f, DARKBROWN); // pouch
        return;
    }

    auto at = [&](Vector3 off)->Vector3{ return Vector3Add(base, RotateYVec(off, yaw)); };

    const std::string& n = l.item.name;
    const std::string& c = l.item.category;

    // --- WEAPONS ---
    if(c == "WEAPON"){
        if(NameHas(n, "MEC")){
            Vector3 h = at({0.0f,-0.05f,0.0f});
            Vector3 tip = Vector3Add(h, RotateYVec({0.0f, 0.55f, 1.0f}, yaw));
            DrawCylinderEx(h, tip, 0.03f, 0.02f, 10, LIGHTGRAY);
            DrawCube(Vector3Add(h, RotateYVec({0.0f,0.08f,0.10f}, yaw)), 0.22f, 0.04f, 0.12f, DARKGRAY);
            DrawCylinderEx(Vector3Add(h, RotateYVec({0.0f,0.0f,-0.05f}, yaw)), Vector3Add(h, RotateYVec({0.0f,-0.15f,-0.18f}, yaw)), 0.03f, 0.03f, 8, DARKBROWN);
            DrawSphere(tip, 0.04f, LIGHTGRAY);
            return;
        }
        if(NameHas(n, "LUK")){
            Vector3 top = at({0.0f, 0.55f, 0.45f});
            Vector3 bot = at({0.0f,-0.25f, 0.55f});
            DrawCylinderEx(bot, top, 0.025f, 0.025f, 10, DARKBROWN);
            DrawLine3D(bot, top, Fade(WHITE,0.55f));
            DrawSphere(at({0.0f,0.10f,0.52f}), 0.06f, SKYRIM_GOLD);
            return;
        }
        if(NameHas(n, "NOZ") || NameHas(n, "NOZIK")){
            Vector3 h = at({0.0f, 0.0f, 0.0f});
            Vector3 tip = Vector3Add(h, RotateYVec({0.0f, 0.25f, 0.65f}, yaw));
            DrawCylinderEx(h, tip, 0.02f, 0.01f, 10, LIGHTGRAY);
            DrawCylinderEx(Vector3Add(h, RotateYVec({0.0f,-0.05f,-0.12f}, yaw)), Vector3Add(h, RotateYVec({0.0f,-0.12f,-0.25f}, yaw)), 0.02f, 0.02f, 8, DARKBROWN);
            DrawSphere(tip, 0.03f, LIGHTGRAY);
            return;
        }
        // generic weapon: axe/cleaver
        Vector3 h = at({0.0f,-0.05f,0.0f});
        Vector3 tip = Vector3Add(h, RotateYVec({0.0f, 0.45f, 0.9f}, yaw));
        DrawCylinderEx(h, tip, 0.035f, 0.03f, 10, DARKBROWN);
        DrawCube(Vector3Add(h, RotateYVec({0.0f,0.30f,0.70f}, yaw)), 0.32f, 0.20f, 0.10f, Fade(LIGHTGRAY,0.9f));
        return;
    }

    // --- POTIONS / FOOD ---
    if(c == "POTION"){
        // bottle / bowl color
        Color liquid = RED;
        if(NameHas(n, "MANY")) liquid = SKYBLUE;
        if(NameHas(n, "STAM")) liquid = GREEN;
        if(NameHas(n, "CHLEB")) liquid = {210, 170, 120, 255};
        if(NameHas(n, "POLIEV")) liquid = ORANGE;

        if(NameHas(n, "CHLEB")){
            DrawCube(base, 0.55f, 0.28f, 0.40f, {210,170,120,255});
            DrawCubeWires(base, 0.55f, 0.28f, 0.40f, Fade(BROWN,0.45f));
            return;
        }
        if(NameHas(n, "POLIEV")){
            DrawCylinder(base, 0.38f, 0.38f, 0.18f, 14, GRAY);
            DrawCylinder(Vector3Add(base,{0,0.10f,0}), 0.32f, 0.32f, 0.05f, 14, liquid);
            DrawSphere(Vector3Add(base,{0.0f,0.28f,0.0f}), 0.08f, Fade(WHITE,0.35f)); // steam puff
            return;
        }

        // potion bottle
        DrawCylinder(base, 0.18f, 0.22f, 0.40f, 12, Fade(GRAY,0.35f));
        DrawCylinder(Vector3Add(base,{0,0.30f,0}), 0.08f, 0.10f, 0.22f, 12, Fade(GRAY,0.35f));
        DrawSphere(Vector3Add(base,{0,0.44f,0}), 0.09f, DARKBROWN); // cork
        DrawSphere(Vector3Add(base,{0,0.12f,0}), 0.16f, Fade(liquid,0.85f));
        return;
    }

    // --- OTHER / MATERIALS ---
    if(NameHas(n, "MODRY") && (NameHas(n, "KVET") || NameHas(n, "LIST"))){
        // blue flower/leaf
        DrawCylinder(base, 0.03f, 0.02f, 0.30f, 10, DARKGREEN);
        Vector3 top = Vector3Add(base,{0,0.22f,0});
        for(int i=0;i<5;i++){
            float a = (float)i/5.0f * 2.0f*PI;
            DrawSphere(Vector3Add(top,{cosf(a)*0.10f, 0.03f, sinf(a)*0.10f}), 0.06f, BLUE);
        }
        DrawSphere(Vector3Add(top,{0,0.04f,0}), 0.04f, SKYRIM_GOLD);
        return;
    }
    if(NameHas(n, "PSEN")){
        // wheat bundle
        for(int i=0;i<7;i++){
            float rx = ((i%3)-1)*0.04f;
            float rz = ((i/3)-1)*0.04f;
            DrawCylinder(Vector3Add(base,{rx,0.0f,rz}), 0.012f, 0.010f, 0.45f, 8, {200,170,60,255});
            DrawSphere(Vector3Add(base,{rx,0.45f,rz}), 0.03f, {200,170,60,255});
        }
        return;
    }
    if(NameHas(n, "RUDA")){
        // ore rock
        DrawSphere(base, 0.28f, DARKGRAY);
        DrawSphere(Vector3Add(base,{0.18f,0.05f,0.08f}), 0.16f, GRAY);
        DrawSphere(Vector3Add(base,{-0.16f,-0.03f,-0.10f}), 0.14f, STONE_DARK);
        DrawSphere(Vector3Add(base,{0.05f,0.10f,-0.18f}), 0.08f, Fade(LIGHTGRAY,0.85f));
        return;
    }
    if(NameHas(n, "KOZA")){
        // leather/pelt roll
        DrawCube(base, 0.62f, 0.12f, 0.48f, {120, 78, 46, 255});
        DrawCube(Vector3Add(base,{0.0f,0.12f,0.0f}), 0.58f, 0.06f, 0.44f, Fade({150, 110, 70, 255},0.9f));
        return;
    }
    if(NameHas(n, "SIPY")){
        // arrows bundle
        for(int i=0;i<4;i++){
            Vector3 a = at({-0.10f + i*0.06f, -0.05f, 0.0f});
            Vector3 b = Vector3Add(a, RotateYVec({0.0f,0.35f,0.75f}, yaw));
            DrawCylinderEx(a, b, 0.015f, 0.012f, 8, DARKBROWN);
            DrawSphere(b, 0.02f, LIGHTGRAY);
        }
        return;
    }
    if(NameHas(n, "TORCH")){
        DrawCylinder(Vector3Add(base,{0,0.0f,0}), 0.04f, 0.03f, 0.55f, 10, DARKBROWN);
        DrawSphere(Vector3Add(base,{0,0.55f,0}), 0.16f, ORANGE);
        DrawSphere(Vector3Add(base,{0.06f,0.60f,0.02f}), 0.10f, Fade(ORANGE,0.75f));
        return;
    }
    if(NameHas(n, "LOCKPICK")){
        Vector3 a = at({0.0f,-0.10f,0.0f});
        Vector3 b = Vector3Add(a, RotateYVec({0.0f,0.10f,0.80f}, yaw));
        DrawCylinderEx(a, b, 0.01f, 0.01f, 8, LIGHTGRAY);
        DrawCylinderEx(b, Vector3Add(b, RotateYVec({0.10f,0.0f,0.10f}, yaw)), 0.01f, 0.01f, 8, LIGHTGRAY);
        return;
    }
    if(NameHas(n, "MAPA")){
        // scroll / map
        DrawCylinder(base, 0.22f, 0.22f, 0.10f, 14, {210,200,170,255});
        DrawCube(Vector3Add(base,{0.0f,0.10f,0.0f}), 0.65f, 0.02f, 0.45f, {210,200,170,255});
        DrawCube(Vector3Add(base,{0.0f,0.11f,0.0f}), 0.60f, 0.01f, 0.40f, Fade(BROWN,0.25f));
        return;
    }

    // fallback
    DrawCube(base, 0.45f, 0.45f, 0.45f, SKYRIM_GOLD);
    DrawCubeWires(base, 0.45f, 0.45f, 0.45f, Fade(BLACK,0.35f));
}

static void DrawDialoguePanel(int screenWidth, int screenHeight, const std::string& npcName, const std::string& npcLine,
                             const std::vector<std::string>& options, const std::vector<bool>& enabled){
    Rectangle panel = {screenWidth/2.0f - 390.0f, (float)screenHeight - 220.0f, 780.0f, 175.0f};
    DrawRectangleRec(panel, Fade(BLACK, 0.86f));
    DrawRectangleLinesEx(panel, 2, Fade(SKYRIM_PANEL_EDGE, 0.9f));
    DrawRectangleGradientV((int)panel.x, (int)panel.y, (int)panel.width, 40, Fade(SKYRIM_GLOW,0.16f), Fade(BLANK,0.0f));

    DrawText(npcName.c_str(), (int)panel.x + 18, (int)panel.y + 12, 20, SKYRIM_TEXT);
    DrawLine((int)panel.x + 16, (int)panel.y + 38, (int)(panel.x + panel.width - 16), (int)panel.y + 38, Fade(SKYRIM_PANEL_EDGE, 0.6f));

    Rectangle lineRec = {panel.x + 18, panel.y + 46, panel.width - 36, 56};
    DrawTextWrapped(npcLine, lineRec, 18, SKYRIM_TEXT);

    int y = (int)panel.y + 110;
    for(int i=0;i<(int)options.size();i++){
        std::string s = TextFormat("%i) %s", i+1, options[i].c_str());
        Color col = (i < (int)enabled.size() && !enabled[i]) ? SKYRIM_MUTED : SKYRIM_TEXT;
        DrawText(s.c_str(), (int)panel.x + 18, y, 18, col);
        y += 22;
        if(y > (int)(panel.y + panel.height - 18)) break;
    }

    DrawText("ESC/F - zavriet", (int)panel.x + (int)panel.width - 150, (int)panel.y + (int)panel.height - 22, 14, SKYRIM_MUTED);
}

static void DrawVignette(int w, int h);
static void DrawLockpickUI(int screenWidth, int screenHeight, const LockpickState& s, int lockpicks, int difficulty){
    DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK,0.72f));
    DrawVignette(screenWidth, screenHeight);

    Rectangle panel = {screenWidth/2.0f - 260.0f, screenHeight/2.0f - 210.0f, 520.0f, 420.0f};
    DrawRectangleRec(panel, Fade(BLACK,0.62f));
    DrawRectangleLinesEx(panel, 2, Fade(SKYRIM_PANEL_EDGE,0.9f));
    DrawText("LOCKPICKING", (int)panel.x + 18, (int)panel.y + 16, 24, SKYRIM_TEXT);
    DrawText(TextFormat("LOCKPICKS: %i", lockpicks), (int)panel.x + 18, (int)panel.y + 46, 18, SKYRIM_GOLD);
    const char* diff = (difficulty<=0) ? "EASY" : (difficulty==1 ? "MEDIUM" : "HARD");
    DrawText(TextFormat("DIFFICULTY: %s", diff), (int)panel.x + 260, (int)panel.y + 46, 18, SKYRIM_MUTED);

    int cx = screenWidth/2;
    int cy = (int)(panel.y + 210);
    float r = 92.0f;

    // lock ring
    DrawCircle(cx, cy, r+18, Fade(SKYRIM_PANEL,0.7f));
    DrawCircleLines(cx, cy, r+18, Fade(SKYRIM_PANEL_EDGE,0.8f));
    DrawCircle(cx, cy, r, Fade(BLACK,0.6f));
    DrawCircleLines(cx, cy, r, Fade(SKYRIM_PANEL_EDGE,0.7f));

    // lock turn indicator
    float lockAngle = -70.0f + s.lockTurn*140.0f;
    Vector2 a = {(float)cx + cosf(lockAngle*DEG2RAD)*70.0f, (float)cy + sinf(lockAngle*DEG2RAD)*70.0f};
    Vector2 b = {(float)cx + cosf(lockAngle*DEG2RAD)*25.0f, (float)cy + sinf(lockAngle*DEG2RAD)*25.0f};
    DrawLineEx(b, a, 6, Fade(SKYRIM_GOLD,0.85f));
    DrawCircle(cx, cy, 10, Fade(SKYRIM_GOLD,0.65f));

    // pick
    float pickRad = (s.pickDeg - 90.0f) * DEG2RAD;
    Vector2 p0 = {(float)cx, (float)cy};
    Vector2 p1 = {(float)cx + cosf(pickRad)*120.0f, (float)cy + sinf(pickRad)*120.0f};
    DrawLineEx(p0, p1, 4, Fade(SKYRIM_TEXT,0.85f));
    DrawCircleV(p1, 6, Fade(SKYRIM_TEXT,0.85f));

    // durability bar
    Rectangle dur = {panel.x + 18, panel.y + panel.height - 82, panel.width - 36, 14};
    DrawRectangleRec(dur, Fade(BLACK,0.55f));
    DrawRectangleLinesEx(dur, 1, Fade(SKYRIM_PANEL_EDGE,0.75f));
    DrawRectangle((int)dur.x+2, (int)dur.y+2, (int)((dur.width-4)*Clamp(s.durability,0.0f,1.0f)), (int)dur.height-4, Fade(RED,0.75f));
    DrawText("PICK DURABILITY", (int)dur.x, (int)dur.y-18, 14, SKYRIM_MUTED);

    DrawText("A/D - move pick    Hold F - turn lock    ESC - cancel", (int)panel.x + 18, (int)panel.y + (int)panel.height - 42, 16, SKYRIM_MUTED);
}

static void DrawVignette(int w, int h){
    Color edge = {0,0,0,120};
    DrawRectangleGradientV(0,0,w,120,edge,BLANK);
    DrawRectangleGradientV(0,h-120,w,120,BLANK,edge);
    DrawRectangleGradientH(0,0,120,h,edge,BLANK);
    DrawRectangleGradientH(w-120,0,120,h,BLANK,edge);
}

static void DrawFogOverlay(int w, int h, float t){
    Color fog = {200,200,200,20};
    for(int i=0;i<6;i++){
        float x = (w*0.15f) + (float)i* (w*0.15f) + sinf(t*0.4f + i)*20.0f;
        float y = (h*0.20f) + (float)i* (h*0.08f) + cosf(t*0.5f + i)*15.0f;
        float r = 120.0f + i*18.0f;
        DrawCircle((int)x, (int)y, r, fog);
    }
}

static void DrawPerkIcon(SkillSection section, Vector2 pos, Color col){
    switch(section){
        case HEALTH: {
            DrawLineEx({pos.x-6,pos.y},{pos.x+6,pos.y},2,col);
            DrawLineEx({pos.x,pos.y-6},{pos.x,pos.y+6},2,col);
        } break;
        case DAMAGE: {
            DrawLineEx({pos.x-6,pos.y-6},{pos.x+6,pos.y+6},2,col);
            DrawLineEx({pos.x-3,pos.y-6},{pos.x+6,pos.y+3},1,col);
        } break;
        case STAMINA: {
            DrawLineEx({pos.x-5,pos.y-6},{pos.x+2,pos.y-2},2,col);
            DrawLineEx({pos.x+2,pos.y-2},{pos.x-1,pos.y+2},2,col);
            DrawLineEx({pos.x-1,pos.y+2},{pos.x+6,pos.y+6},2,col);
        } break;
        case MANA: {
            DrawCircleLines((int)pos.x, (int)pos.y, 6, col);
            DrawCircleLines((int)pos.x, (int)pos.y, 3, col);
        } break;
    }
}

// --- Pomocné funkcie ---
static Color LerpColor(Color a, Color b, float t){
    t = Clamp(t, 0.0f, 1.0f);
    return {
        (unsigned char)(a.r + (b.r - a.r)*t),
        (unsigned char)(a.g + (b.g - a.g)*t),
        (unsigned char)(a.b + (b.b - a.b)*t),
        (unsigned char)(a.a + (b.a - a.a)*t)
    };
}

static Vector3 ChooseDoorDirToStreet(Vector3 pos){
    // hlavna ulica ide po osi Z (x = 0), domy po stranach sa otacaju k nej
    if(fabsf(pos.x) > 6.0f){
        return {(pos.x > 0 ? -1.0f : 1.0f), 0, 0};
    }
    // domy blizko osi Z sa otacaju smerom po ulici
    return {0, 0, (pos.z > 0 ? -1.0f : 1.0f)};
}

static Vector3 GetHouseDoorPos(const House& h){
    float off = (fabsf(h.doorDir.x) > 0.5f) ? (h.size.x*0.5f + 0.05f) : (h.size.z*0.5f + 0.05f);
    return {h.position.x + h.doorDir.x*off, h.position.y, h.position.z + h.doorDir.z*off};
}

struct CompassMarker {
    Vector3 position;
    Color color;
};

static float WrapAnglePi(float a){
    while(a > PI) a -= 2.0f*PI;
    while(a < -PI) a += 2.0f*PI;
    return a;
}

static float BearingTo(Vector3 from, Vector3 to){
    Vector3 d = Vector3Subtract(to, from);
    return atan2f(d.x, d.z);
}

static void DrawItemIcon2D(const Item& it, int x, int y, int s){
    Color bg = Fade(BLACK, 0.35f);
    Color edge = Fade(SKYRIM_PANEL_EDGE, 0.75f);
    Color fill = SKYRIM_PANEL_EDGE;
    const char* label = "?";
    if(it.category=="WEAPON"){ fill = Fade(RED,0.55f); label = "W"; }
    else if(it.category=="MAGIC"){ fill = Fade(SKYBLUE,0.55f); label = "M"; }
    else if(it.category=="POTION"){ fill = Fade(GREEN,0.55f); label = "P"; }
    else if(it.category=="BOOK"){ fill = Fade(SKYRIM_GOLD,0.55f); label = "B"; }
    else if(it.category=="OTHER"){ fill = Fade(SKYRIM_PANEL_EDGE,0.55f); label = "O"; }
    if(it.name.find("MODRY")!=std::string::npos) fill = Fade(BLUE,0.55f), label = "F";
    if(it.name.find("PSEN")!=std::string::npos) fill = Fade({200,170,60,255},0.55f), label = "H";
    if(it.name.find("MAPA")!=std::string::npos) fill = Fade({210,200,170,255},0.55f), label = "M";
    if(it.name.find("LOCKPICK")!=std::string::npos) fill = Fade(LIGHTGRAY,0.55f), label = "L";

    DrawRectangle(x, y, s, s, bg);
    DrawRectangleLines(x, y, s, s, edge);
    DrawRectangle(x+2, y+2, s-4, s-4, fill);
    int fs = s - 10;
    if(fs < 10) fs = 10;
    int tw = MeasureText(label, fs);
    DrawText(label, x + s/2 - tw/2, y + s/2 - fs/2, fs, SKYRIM_TEXT);
}

static void DrawItemPreview3D(const Item& it, float t){
    LootDrop ld;
    ld.position = {0,0,0};
    ld.active = true;
    ld.item = it;
    ld.gold = 0;
    ld.timer = 99999.0f;
    DrawLootModel(ld, t);
}

static void RenderInventoryPreview(RenderTexture2D& rt, const Item& it, float t){
    Camera3D cam{};
    cam.position = {2.2f, 2.0f, 2.2f};
    cam.target = {0.0f, 0.45f, 0.0f};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(rt);
    ClearBackground({0,0,0,0});
    BeginMode3D(cam);
        DrawCube({0.0f, -0.2f, 0.0f}, 3.8f, 0.08f, 3.8f, Fade(BLACK, 0.35f));
        DrawCubeWires({0.0f, -0.2f, 0.0f}, 3.8f, 0.08f, 3.8f, Fade(SKYRIM_PANEL_EDGE, 0.35f));
        DrawItemPreview3D(it, t);
    EndMode3D();
    EndTextureMode();
}

static float Approach(float v, float target, float delta){
    if(v < target) return fminf(target, v + delta);
    if(v > target) return fmaxf(target, v - delta);
    return v;
}

static void DrawCompass(float playerAngle, Vector3 playerPos, int screenWidth, const std::vector<CompassMarker>& markers){
    const float fovDeg = 130.0f;
    const float halfFov = (fovDeg*DEG2RAD)*0.5f;

    float headingDeg = fmodf((playerAngle * RAD2DEG) + 360.0f, 360.0f);
    int w = 520;
    int h = 34;
    int x0 = screenWidth/2 - w/2;
    int y0 = 10;

    DrawRectangle(x0, y0, w, h, Fade(BLACK, 0.55f));
    DrawRectangleLinesEx({(float)x0,(float)y0,(float)w,(float)h}, 2, Fade(SKYRIM_PANEL_EDGE, 0.9f));
    DrawRectangleGradientV(x0, y0, w, h, Fade(SKYRIM_GLOW, 0.12f), Fade(BLANK, 0.0f));

    int cx = x0 + w/2;
    DrawLine(cx, y0+6, cx, y0+h-6, Fade(SKYRIM_GOLD, 0.55f));
    DrawTriangle({(float)cx, (float)(y0+h+2)}, {(float)cx-7, (float)(y0+h-6)}, {(float)cx+7, (float)(y0+h-6)}, Fade(SKYRIM_GOLD, 0.9f));
    DrawText(TextFormat("%03i", (int)roundf(headingDeg)), cx - 18, y0+h+6, 16, SKYRIM_MUTED);

    auto xForDelta = [&](float delta){
        float t = delta/halfFov;
        return (int)roundf((float)cx + t*(w*0.5f));
    };

    // ticks
    for(int deg=-90; deg<=90; deg+=15){
        float d = deg*DEG2RAD;
        int x = xForDelta(d);
        if(x < x0+4 || x > x0+w-4) continue;
        int th = (deg % 30 == 0) ? 10 : 6;
        DrawLine(x, y0+h-4, x, y0+h-4-th, Fade(SKYRIM_PANEL_EDGE, 0.75f));
    }

    // cardinals: N=0, E=90, S=180, W=270
    struct Card { float bearing; const char* name; };
    Card cards[4] = {
        {0.0f, "N"},
        {PI/2.0f, "E"},
        {PI, "S"},
        {-PI/2.0f, "W"}
    };
    for(const auto& c : cards){
        float delta = WrapAnglePi(c.bearing - playerAngle);
        if(fabsf(delta) > halfFov) continue;
        int x = xForDelta(delta);
        int tw = MeasureText(c.name, 18);
        DrawText(c.name, x - tw/2, y0+7, 18, SKYRIM_TEXT);
    }

    // markers
    for(const auto& m : markers){
        float b = BearingTo(playerPos, m.position);
        float delta = WrapAnglePi(b - playerAngle);
        if(fabsf(delta) > halfFov) continue;
        int x = xForDelta(delta);
        if(x < x0+8 || x > x0+w-8) continue;
        DrawTriangle({(float)x, (float)(y0+6)}, {(float)x-6, (float)(y0+14)}, {(float)x+6, (float)(y0+14)}, Fade(m.color, 0.95f));
        DrawLine(x, y0+14, x, y0+h-6, Fade(m.color, 0.45f));
    }
}

float GetGroundHeight(Image *noise, Vector3 pos){
    float imgX = (pos.x + HALF_WORLD)/WORLD_SIZE*(float)MAP_SIZE;
    float imgZ = (pos.z + HALF_WORLD)/WORLD_SIZE*(float)MAP_SIZE;
    if(imgX >=0 && imgX<MAP_SIZE && imgZ>=0 && imgZ<MAP_SIZE){
        Color pixel = GetImageColor(*noise, (int)imgX,(int)imgZ);
        return -2.0f + (pixel.r/255.0f)*10.0f;
    }
    return 0.0f;
}

// --- Hlavný program ---
int main(){
    const int screenWidth=1200, screenHeight=800;
    InitWindow(screenWidth,screenHeight,"Echoes of the Forgotten");
    SetRandomSeed(12345);
    InitAudioDevice();
    bool audioReady = IsAudioDeviceReady();
    Music musicDay{};
    Music musicNight{};
    bool musicDayLoaded = false;
    bool musicNightLoaded = false;
    Sound sfxKillcam{};
    bool killcamSfxLoaded = false;
	    std::array<Sound, 4> sfxIntro{};
	    std::array<bool, 4> introSfxLoaded{};
    if(audioReady && FileExists("sound/ambient_day.ogg")){
        musicDay = LoadMusicStream("sound/ambient_day.ogg");
        musicDay.looping = true;
        PlayMusicStream(musicDay);
        musicDayLoaded = true;
    }
    if(audioReady && FileExists("sound/ambient_night.ogg")){
        musicNight = LoadMusicStream("sound/ambient_night.ogg");
        musicNight.looping = true;
        musicNightLoaded = true;
    }
    if(audioReady && FileExists("sound/killcam.wav")){
        sfxKillcam = LoadSound("sound/killcam.wav");
        killcamSfxLoaded = true;
    }
	    if(audioReady){
	        for(int i=0;i<(int)sfxIntro.size();i++){
	            const char* fn = TextFormat("sound/intro_%i.wav", i);
	            if(FileExists(fn)){
	                sfxIntro[i] = LoadSound(fn);
	                introSfxLoaded[i] = true;
	            }
	        }
	    }

	    // --- Procedural textures (budovy, strechy) ---
	    Texture2D texWoodWall = MakeWoodTexture(64, 64);
	    Texture2D texStoneWall = MakeStoneTexture(64, 64);
	    Texture2D texRoof = MakeRoofTexture(64, 64);
	    Model mdlWoodBox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
	    Model mdlStoneBox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
	    Model mdlRoofBox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
	    SetMaterialTexture(&mdlWoodBox.materials[0], MATERIAL_MAP_DIFFUSE, texWoodWall);
	    SetMaterialTexture(&mdlStoneBox.materials[0], MATERIAL_MAP_DIFFUSE, texStoneWall);
	    SetMaterialTexture(&mdlRoofBox.materials[0], MATERIAL_MAP_DIFFUSE, texRoof);

	    RenderTexture2D invPreviewRT = LoadRenderTexture(256, 256);
	    SetTextureFilter(invPreviewRT.texture, TEXTURE_FILTER_BILINEAR);

	    // --- Hráč ---
	    float baseMaxHealth=100.0f, playerMaxHealth=100.0f, playerHealth=100.0f;
    float baseMaxMana=100.0f, playerMaxMana=100.0f, playerMana=100.0f;
    float baseMaxStamina=100.0f, playerMaxStamina=100.0f, playerStamina=100.0f;
	    float manaRegenPerSec=12.0f;
	    float fireballManaCost=25.0f;
	    float frostboltManaCost=22.0f;
	    float poisonboltManaCost=20.0f;
	    float hpRegenPerSec=6.0f;
	    float staminaRegenPerSec=22.0f;
	    float baseFireballDamage=40.0f;
	    float baseFrostDamage=32.0f;
	    float basePoisonDamage=26.0f;
    int gold=120, xp=0, xpToNextLevel=100, level=1;
	    int perkPoints=2;
	    int lockpicks = 8;
	    std::array<SkillState, (int)Skill::COUNT> skills;
	    std::array<int, (int)Faction::COUNT> factionRep{};
	    for(auto& s : skills){ s.level = 15; s.xp = 0.0f; }
	    factionRep[(int)Faction::GUARD] = 0;
	    factionRep[(int)Faction::SMITHS] = 0;
	    factionRep[(int)Faction::ALCHEMISTS] = 0;
	    factionRep[(int)Faction::HUNTERS] = 0;
	    factionRep[(int)Faction::TOWNSFOLK] = 0;
	    bool showMainMenu=true;
	    bool settingsOpen=false;
	    bool mouseCaptured=true;
    int graphicsQuality=2; // 0 low, 1 med, 2 high
    bool weatherEnabled=true;
    bool fullscreenEnabled=false;
    int mainMenuIndex=0; // 0 Start, 1 Settings, 2 Quit
    [[maybe_unused]] int settingsIndex=0; // 0 Graphics, 1 Weather, 2 Fullscreen, 3 Close
    bool showLoading=false;
    float loadingTimer=0.0f;
    float loadingTotal=1.5f;
    bool pendingTeleport=false;
    Vector3 pendingTeleportPos={0,0,0};
    float timeOfDay=0.22f; // 0..1 (0 = midnight)
    float dayLengthSec=240.0f;
    bool inInterior=false;
    int interiorKind=0; // 0 house, 1 dungeon
    int interiorScene = INT_HOUSE;
    int interiorLevel = 0;
    Vector3 interiorReturn={0,0,0};
    [[maybe_unused]] int activeInterior=-1;
	Enemy dungeonBoss = {{0,0,0},{0,0,0}, 260, 260, false, 0.06f, 30.0f, true, 0, 0, 0.0f, 0.0f};
	bool dungeonBossDefeated = false;
	bool dungeonRewardTaken = false;
	bool dungeonGateOpen = false;
	bool dungeonLeverPulled = false;
	float dungeonTrapCooldown = 0.0f;
    float interactLock=0.0f;
    float waterLevel=-1.2f; bool isUnderwater=false;
    [[maybe_unused]] bool fixedWorld=true;
    bool fixedWeather=true;
	    bool enableEncounters=true;
    const float cityRadius = 36.0f;
    bool gateUnlocked = false;
    Vector3 gatePos = {0,0,0};
    bool insideCity = false;
    bool minimapEnabled = false;
	    bool torchActive = false;
	    float manaRegenDelay = 0.0f;
	    float hpRegenDelay = 0.0f;
	    float staminaRegenDelay = 0.0f;
	    Vector3 playerPos={0,10,0}; float playerAngle=0.0f; float swordTimer=0.0f;
	    float playerWalkPhase = 0.0f;
	    bool playerIsMoving = false;
	    bool playerIsSprinting = false;
		    float playerAttackTimer = 0.0f;
		    float playerAttackDuration = 0.34f;
		    int activeSpell = 0; // 0 fire, 1 frost, 2 poison
		    float playerBurnTimer = 0.0f;
		    float playerPoisonTimer = 0.0f;
		    float playerSlowTimer = 0.0f;

	    // --- Terén ---
	    Image noiseImage=GenImagePerlinNoise(MAP_SIZE,MAP_SIZE,0,0,2.0f);
	    Image biomeNoise=GenImagePerlinNoise(MAP_SIZE,MAP_SIZE,100,100,4.0f);

	    // --- Hora + cesticka + chrám na vrchu (uprava heightmapy) ---
	    Vector3 mountainCenter = {-170.0f, 0.0f, -170.0f};
	    float mountainRadius = 78.0f;
	    float mountainPeakAdd = 9.0f;     // (saturuje na max vysku heightmapy)
	    float mountainPlateauR = 10.0f;
	    float mountainPlateauH = 7.4f;    // plosinka pre chrám
	    float trailWidth = 3.2f;
	    float trailCarveDepth = 1.4f;
	    std::vector<Vector3> mountainTrail = {
	        {mountainCenter.x + mountainRadius + 18.0f, 0.0f, mountainCenter.z + 0.0f},
	        {mountainCenter.x + 34.0f, 0.0f, mountainCenter.z + 34.0f},
	        {mountainCenter.x - 4.0f,  0.0f, mountainCenter.z + 38.0f},
	        {mountainCenter.x - 34.0f, 0.0f, mountainCenter.z + 8.0f},
	        {mountainCenter.x - 22.0f, 0.0f, mountainCenter.z - 28.0f},
	        {mountainCenter.x + 12.0f, 0.0f, mountainCenter.z - 34.0f},
	        {mountainCenter.x + 30.0f, 0.0f, mountainCenter.z - 10.0f},
	        {mountainCenter.x + 0.0f,  0.0f, mountainCenter.z + 8.0f} // pred dverami chrámu
	    };

	    for(int y=0;y<MAP_SIZE;y++) for(int x=0;x<MAP_SIZE;x++){
	        float worldX = ((float)x/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        float worldZ = ((float)y/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        Vector3 wp = {worldX, 0.0f, worldZ};
	        float dist = Vector3Distance(wp, mountainCenter);
	        if(dist > mountainRadius) continue;

	        Color px = GetImageColor(noiseImage, x, y);
	        float h = -2.0f + (px.r/255.0f)*10.0f;

	        float t = 1.0f - (dist/mountainRadius);
	        float add = mountainPeakAdd * (t*t);

	        float dTrail = 9999.0f;
	        for(int i=0;i+1<(int)mountainTrail.size();i++){
	            dTrail = fminf(dTrail, DistPointSegmentXZ(wp, mountainTrail[i], mountainTrail[i+1]));
	        }
	        float carve = 0.0f;
	        if(dTrail < trailWidth){
	            float k = 1.0f - (dTrail/trailWidth);
	            carve = trailCarveDepth * (k*k);
	        }

	        h = h + add - carve;
	        if(dist < mountainPlateauR) h = fmaxf(h, mountainPlateauH);
	        h = Clamp(h, -2.0f, 8.0f);
	        unsigned char nr = (unsigned char)Clamp(((h + 2.0f)/10.0f)*255.0f, 0.0f, 255.0f);
	        ImageDrawPixel(&noiseImage, x, y, {nr,nr,nr,255});
	    }

	    Vector3 cityCenter = {0,0,0};
	    cityCenter.y = GetGroundHeight(&noiseImage, cityCenter);
	    gatePos = {cityCenter.x, 0, cityCenter.z + cityRadius};
	    gatePos.y = GetGroundHeight(&noiseImage, gatePos);
	    playerPos = {gatePos.x, gatePos.y, gatePos.z + 18.0f};

	    Image terrainColors=GenImageColor(MAP_SIZE,MAP_SIZE,LIGHTGREEN);
	    for(int y=0;y<MAP_SIZE;y++) for(int x=0;x<MAP_SIZE;x++){
	        Color pixel=GetImageColor(noiseImage,x,y);
	        float h=-2.0f + (pixel.r/255.0f)*10.0f;
	        Color bc = GetImageColor(biomeNoise,x,y);
	        float biome = bc.r/255.0f;
	        Color base = BIOME_FOREST;
        if(biome>0.66f) base = BIOME_SNOW;
        else if(biome>0.33f) base = BIOME_ROCK;
        if(h<waterLevel) base = SAND;

        bool isRoad = false;
        // jednoduchá cesta (kríž)
        if(abs(x - MAP_SIZE/2) < 2 || abs(y - MAP_SIZE/2) < 2) isRoad = true;

        // mesto: kruhova cesta + spojky
        float worldX = ((float)x/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
        float worldZ = ((float)y/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
        float dx = worldX - cityCenter.x;
        float dz = worldZ - cityCenter.z;
        float dist = sqrtf(dx*dx + dz*dz);
        if(dist > cityRadius-5.0f && dist < cityRadius-3.0f) isRoad = true;
        // hlavna ulica
        if(fabsf(dx) < 1.6f && dist < cityRadius-6.0f) isRoad = true;
	        // bocne ulicky
	        if(fabsf(dz - 20.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz - 12.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz - 4.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 4.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 12.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 20.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        // hora: serpentína k chrámu
	        {
	            Vector3 wp = {worldX, 0.0f, worldZ};
	            float dTrail = 9999.0f;
	            for(int i=0;i+1<(int)mountainTrail.size();i++){
	                dTrail = fminf(dTrail, DistPointSegmentXZ(wp, mountainTrail[i], mountainTrail[i+1]));
	            }
	            if(dTrail < trailWidth) isRoad = true;
	        }

	        if(isRoad){
	            // kamienková cesta (bez šachovnice)
	            unsigned hx = (unsigned)x*73856093u;
	            unsigned hy = (unsigned)y*19349663u;
	            unsigned hsh = (hx ^ hy) + 0x9e3779b9u;
	            hsh ^= (hsh >> 15);
	            hsh *= 2246822519u;
	            hsh ^= (hsh >> 13);
	            float rnd = (float)(hsh & 0xFFFFu) / 65535.0f;
	            float rnd2 = (float)((hsh >> 16) & 0xFFFFu) / 65535.0f;

	            float pebble = Clamp(0.50f + (rnd-0.5f)*0.35f + ((bc.g/255.0f)-0.5f)*0.18f, 0.22f, 0.88f);
	            Color stone = LerpColor(STONE_DARK, STONE_LIGHT, pebble);
	            Color dirt = LerpColor(ROAD_COLOR, STONE_DARK, 0.55f);
	            base = LerpColor(dirt, stone, 0.75f + 0.20f*(rnd2-0.5f));

	            // malé kamienky
	            if((hsh % 29u) == 0u) base = LerpColor(base, STONE_LIGHT, 0.65f);
	            if((hsh % 41u) == 0u) base = LerpColor(base, {190,190,195,255}, 0.75f);
	            if((hsh % 37u) == 0u) base = LerpColor(base, STONE_DARK, 0.55f);
	        }

	        ImageDrawPixel(&terrainColors,x,y,base);
	    }
    // --- Stromy (fixne pozicie) ---
    std::vector<Tree> forest;
    std::vector<Vector3> treeSpots = {
        {-40,0,-30},{-35,0,-20},{-30,0,-10},{-25,0,-5},{-20,0,0},
        {-45,0,10},{-38,0,18},{-28,0,22},{-18,0,26},{-10,0,30},
        {20,0,-35},{28,0,-28},{34,0,-20},{42,0,-10},{50,0,0},
        {36,0,12},{28,0,18},{20,0,24},{10,0,28},{0,0,32},
        {-60,0,40},{-52,0,46},{-44,0,52},{-36,0,58},{-28,0,64},
        {60,0,42},{54,0,48},{48,0,54},{42,0,60},{36,0,66}
    };
    for(auto& t: treeSpots){
        float ty = GetGroundHeight(&noiseImage, t);
        if(ty > waterLevel+0.8f && Vector3Distance(t, cityCenter) > cityRadius + 6.0f){
            forest.push_back({{t.x, ty, t.z}});
        }
    }

	    // --- Body zaujmu (POI) ---
	    std::vector<Vector3> poiRuins = {{-120,0,80},{130,0,-60}};
	    std::vector<Vector3> poiCamps = {{-40,0,-140},{90,0,120}};
	    std::vector<Vector3> poiCaves = {{-150,0,-20},{160,0,40}};
	    for(auto& p: poiRuins) p.y = GetGroundHeight(&noiseImage,p);
	    for(auto& p: poiCamps) p.y = GetGroundHeight(&noiseImage,p);
	    for(auto& p: poiCaves) p.y = GetGroundHeight(&noiseImage,p);

	    Vector3 templePos = {mountainCenter.x, 0.0f, mountainCenter.z};
	    templePos.y = GetGroundHeight(&noiseImage, templePos);
	    Vector3 templeDoorPos = {templePos.x, templePos.y, templePos.z + 6.2f};

	    // --- Detail props + voda (svet) ---
	    auto IsRoadWorld = [&](float wx, float wz)->bool{
	        // cross road
	        if(fabsf(wx) < 3.5f || fabsf(wz) < 3.5f) return true;

	        float dx = wx - cityCenter.x;
	        float dz = wz - cityCenter.z;
	        float dist = sqrtf(dx*dx + dz*dz);
	        if(dist > cityRadius-5.0f && dist < cityRadius-3.0f) return true;            // ring
	        if(fabsf(dx) < 1.6f && dist < cityRadius-6.0f) return true;                   // main
	        if(fabsf(dz - 20.0f) < 1.2f && fabsf(dx) < 14.0f) return true;
	        if(fabsf(dz - 12.0f) < 1.2f && fabsf(dx) < 14.0f) return true;
	        if(fabsf(dz - 4.0f) < 1.2f && fabsf(dx) < 14.0f) return true;
	        if(fabsf(dz + 4.0f) < 1.2f && fabsf(dx) < 14.0f) return true;
	        if(fabsf(dz + 12.0f) < 1.2f && fabsf(dx) < 14.0f) return true;
	        if(fabsf(dz + 20.0f) < 1.2f && fabsf(dx) < 14.0f) return true;

	        Vector3 p = {wx, 0.0f, wz};
	        float dTrail = 9999.0f;
	        for(int i=0;i+1<(int)mountainTrail.size();i++){
	            dTrail = fminf(dTrail, DistPointSegmentXZ(p, mountainTrail[i], mountainTrail[i+1]));
	        }
	        if(dTrail < trailWidth) return true;
	        return false;
	    };

	    std::vector<Vector3> worldRocks;
	    std::vector<float> worldRockScale;
	    std::vector<Vector3> worldBushes;
	    std::vector<float> worldBushScale;
	    std::vector<Vector3> worldGrass;
	    std::vector<float> worldGrassScale;

	    for(int i=0;i<260;i++){
	        float x = (float)GetRandomValue((int)-HALF_WORLD+10, (int)HALF_WORLD-10);
	        float z = (float)GetRandomValue((int)-HALF_WORLD+10, (int)HALF_WORLD-10);
	        if(IsRoadWorld(x,z)) continue;
	        Vector3 p = {x,0,z};
	        p.y = GetGroundHeight(&noiseImage, p);
	        if(p.y < waterLevel + 0.4f) continue;
	        if(Vector3Distance(p, cityCenter) < cityRadius + 8.0f) continue;

	        float mDist = Vector3Distance({p.x,0,p.z}, {mountainCenter.x,0,mountainCenter.z});
	        bool nearMountain = (mDist < mountainRadius + 22.0f);

	        if(nearMountain || GetRandomValue(0,100) < 40){
	            worldRocks.push_back(p);
	            worldRockScale.push_back(0.35f + Rand01()*0.65f);
	        } else if(GetRandomValue(0,100) < 55){
	            worldBushes.push_back(p);
	            worldBushScale.push_back(0.55f + Rand01()*0.75f);
	        } else {
	            worldGrass.push_back(p);
	            worldGrassScale.push_back(0.35f + Rand01()*0.60f);
	        }
	    }

	    std::vector<Vector3> waterFoam;
	    std::vector<float> waterFoamPhase;
	    for(int y=0;y<MAP_SIZE;y+=8) for(int x=0;x<MAP_SIZE;x+=8){
	        float worldX = ((float)x/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        float worldZ = ((float)y/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        float dx = worldX - cityCenter.x;
	        float dz = worldZ - cityCenter.z;
	        if(sqrtf(dx*dx + dz*dz) < cityRadius + 7.0f) continue;
	        if(IsRoadWorld(worldX, worldZ)) continue;
	        Color px = GetImageColor(noiseImage, x, y);
	        float h = -2.0f + (px.r/255.0f)*10.0f;
	        if(fabsf(h - waterLevel) < 0.22f){
	            Vector3 p = {worldX, waterLevel+0.03f, worldZ};
	            waterFoam.push_back(p);
	            waterFoamPhase.push_back(Rand01()*6.28f);
	            if((int)waterFoam.size() > 800) break;
	        }
	    }

	    std::vector<Vector3> waterRipples;
	    std::vector<float> waterRipplePhase;
	    for(int i=0;i<140;i++){
	        float x = (float)GetRandomValue((int)-HALF_WORLD+20, (int)HALF_WORLD-20);
	        float z = (float)GetRandomValue((int)-HALF_WORLD+20, (int)HALF_WORLD-20);
	        if(IsRoadWorld(x,z)) continue;
	        Vector3 p = {x,0,z};
	        float gy = GetGroundHeight(&noiseImage, p);
	        if(gy > waterLevel - 1.0f) continue;
	        waterRipples.push_back({x, waterLevel+0.02f, z});
	        waterRipplePhase.push_back(Rand01()*6.28f);
	    }

	    std::vector<POI> worldPOIs;
	    worldPOIs.push_back({"BRANA", gatePos, true, true, SKYRIM_GOLD});
	    worldPOIs.push_back({"MESTO", cityCenter, false, true, SKYRIM_GOLD});
	    worldPOIs.push_back({"CHRAM NA HORE", templePos, false, true, SKYRIM_GOLD});
	    for(int i=0;i<(int)poiRuins.size();i++) worldPOIs.push_back({TextFormat("RUINY %i", i+1), poiRuins[i], false, true, ORANGE});
	    for(int i=0;i<(int)poiCamps.size();i++) worldPOIs.push_back({TextFormat("TABOR %i", i+1), poiCamps[i], false, true, SKYRIM_PANEL_EDGE});
	    for(int i=0;i<(int)poiCaves.size();i++) worldPOIs.push_back({TextFormat("JASKYNA %i", i+1), poiCaves[i], false, true, GRAY});

    Vector3 dungeonEntrance = poiCaves.size() > 0 ? poiCaves[0] : Vector3{0,0,0};
    std::vector<Vector3> dungeonTraps = {
        {0.0f, 0.0f, 0.8f},
        {0.0f, 0.0f, -1.6f},
        {0.0f, 0.0f, -18.0f}
    };
    Vector3 dungeonRewardPos = {2.6f, 0.6f, -29.0f};

    // --- Mesto ---
    std::vector<House> cityHouses;
    std::vector<std::string> shopNames = {"Kovac", "Alchymista", "Lovec", "General", "Hostinec"};
    std::vector<std::vector<Item>> shopCatalogs = {
        {
            {"ZELEZNY MEC","WEAPON",25,0,120,10.0f,false,"Pevny mec."},
            {"STRIEBORNY MEC","WEAPON",45,0,220,8.5f,false,"Ostry mec proti priseram."},
            {"STARY SEKAC","WEAPON",35,0,160,9.5f,false,"Tazsia zbran."},
            {"ZELEZNA RUDA","OTHER",0,0,10,0.4f,false,"Material na zbroj/zbrane."},
            {"KOZA","OTHER",0,0,8,0.3f,false,"Koza na craft."}
        },
	        {
	            {"SILNY ELIXIR","POTION",0,80,60,0.5f,false,"Lieci o 80 HP."},
	            {"ELIXIR MANY","POTION",0,0,55,0.5f,false,"Obnovi manu."},
	            {"ELIXIR HP","POTION",0,50,40,0.5f,false,"Obnovuje zdravie."},
	            {"PRSTEN MAGIE","MAGIC",0,0,180,0.2f,false,"Katalyzator pre magiu (E)."},
	            {"KNIHA MRAZU","BOOK",0,0,160,2.0f,false,"Uci mrazivy projektil. Z = prepnut spell."},
	            {"KNIHA JEDU","BOOK",0,0,150,2.0f,false,"Uci jedovy projektil. Z = prepnut spell."},
	            {"MODRY KVET","OTHER",0,0,6,0.1f,false,"Surovina do alchymie."},
	            {"PSENICA","OTHER",0,0,6,0.1f,false,"Surovina do alchymie."}
	        },
        {
            {"LUK","WEAPON",30,0,140,4.0f,false,"Rychly luk."},
            {"SIPY x20","OTHER",0,0,25,1.0f,false,"Zakladne sipy."},
            {"NOZIK","WEAPON",18,0,60,2.0f,false,"Lahka zbran."}
        },
        {
            {"CHLEB","POTION",0,15,8,0.2f,false,"Male liecenie."},
            {"TORCH","OTHER",0,0,15,1.0f,false,"Svetlo do noci."},
            {"LOCKPICK","OTHER",0,0,12,0.0f,false,"Odomykanie truhlic a dveri."},
            {"MAPA","OTHER",0,0,20,0.1f,false,"Jednoducha mapa."}
        },
        {
            {"POLIEVKA","POTION",0,30,20,0.4f,false,"Zahreje a lieci."},
            {"ELIXIR STAMINY","POTION",0,0,35,0.4f,false,"Obnovi vytrvalost."},
            {"MAPA","OTHER",0,0,20,0.1f,false,"Zobrazi mini mapu."}
        }
    };
	    std::vector<Item> activeShopItems;
	    std::string activeShopName = "OBCHOD";
	    int activeShopIndex = 3;
    // Vacsie budovy (Solitude styl)
    {
        std::vector<Vector3> bigPos = {
            {cityCenter.x + 10.0f, 0, cityCenter.z + 6.0f},   // kovac
            {cityCenter.x - 14.0f, 0, cityCenter.z - 2.0f},   // chrám
            {cityCenter.x + 14.0f, 0, cityCenter.z + 12.0f},  // hostinec
            {cityCenter.x + 0.0f, 0, cityCenter.z - 18.0f}    // hrad
        };
        std::vector<Vector3> bigSize = {
            {6.8f, 4.6f, 6.0f},
            {8.5f, 5.0f, 7.0f},
            {7.5f, 4.8f, 6.5f},
            {9.5f, 5.6f, 8.0f}
        };
        for(int i=0;i<(int)bigPos.size(); i++){
            Vector3 p = bigPos[i];
            p.y = GetGroundHeight(&noiseImage, p);
            Vector3 size = bigSize[i];
            float roofH = 1.8f;
            Color wall = (i==1) ? SAND : BEIGE;
            Color roof = (i==3) ? DARKBROWN : BROWN;
            Vector3 doorDir = ChooseDoorDirToStreet(p);
            int shopIndex = (i==0 ? 0 : (i==1 ? 3 : (i==2 ? 4 : 3)));
            cityHouses.push_back({p, size, roofH, wall, roof, doorDir, true, true, shopIndex});
        }
    }
    // fixne domy (hlavna ulica + bocne ulicky)
    std::vector<Vector3> housePos = {
        {cityCenter.x-12,0,cityCenter.z+20},{cityCenter.x+12,0,cityCenter.z+20},
        {cityCenter.x-12,0,cityCenter.z+12},{cityCenter.x+12,0,cityCenter.z+12},
        {cityCenter.x-12,0,cityCenter.z+4},{cityCenter.x+12,0,cityCenter.z+4},
        {cityCenter.x-12,0,cityCenter.z-4},{cityCenter.x+12,0,cityCenter.z-4},
        {cityCenter.x-12,0,cityCenter.z-12},{cityCenter.x+12,0,cityCenter.z-12},
        {cityCenter.x-16,0,cityCenter.z-18},{cityCenter.x+16,0,cityCenter.z-18},
        {cityCenter.x-6,0,cityCenter.z+26},{cityCenter.x+6,0,cityCenter.z+26},
        {cityCenter.x-6,0,cityCenter.z-24},{cityCenter.x+6,0,cityCenter.z-24}
    };
    std::vector<Vector3> houseSize = {
        {3.8f,2.8f,4.2f},{3.8f,2.8f,4.2f},
        {4.0f,3.0f,4.5f},{4.0f,3.0f,4.5f},
        {3.6f,2.6f,4.0f},{3.6f,2.6f,4.0f},
        {3.6f,2.6f,4.0f},{3.6f,2.6f,4.0f},
        {4.2f,3.2f,4.6f},{4.2f,3.2f,4.6f},
        {4.6f,3.2f,4.2f},{4.6f,3.2f,4.2f},
        {3.4f,2.6f,3.8f},{3.4f,2.6f,3.8f},
        {3.8f,2.8f,4.2f},{3.8f,2.8f,4.2f}
    };
    for(int i=0;i<(int)housePos.size(); i++){
        Vector3 p = housePos[i];
        p.y = GetGroundHeight(&noiseImage, p);
        Vector3 size = houseSize[i];
        float roofH = 1.4f;
        Color wall = (i%2==0) ? BEIGE : SAND;
        Color roof = (i%3==0) ? DARKBROWN : BROWN;
        bool chimney = (i%4==0);
        Vector3 doorDir = ChooseDoorDirToStreet(p);
        bool canEnter = (i%3==0);
        int shopIndex = i % (int)shopCatalogs.size();
        cityHouses.push_back({p, size, roofH, wall, roof, doorDir, chimney, canEnter, shopIndex});
    }

	    // --- Oprava uliciek: cesta nezasahuje do domov ---
	    for(int y=0;y<MAP_SIZE;y++) for(int x=0;x<MAP_SIZE;x++){
	        float worldX = ((float)x/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        float worldZ = ((float)y/(float)MAP_SIZE)*WORLD_SIZE - HALF_WORLD;
	        float dx = worldX - cityCenter.x;
	        float dz = worldZ - cityCenter.z;
	        float dist = sqrtf(dx*dx + dz*dz);

	        bool isRoad = false;
	        if(abs(x - MAP_SIZE/2) < 2 || abs(y - MAP_SIZE/2) < 2) isRoad = true;
	        if(dist > cityRadius-5.0f && dist < cityRadius-3.0f) isRoad = true;
	        if(fabsf(dx) < 1.6f && dist < cityRadius-6.0f) isRoad = true;
	        if(fabsf(dz - 20.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz - 12.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz - 4.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 4.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 12.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        if(fabsf(dz + 20.0f) < 1.2f && fabsf(dx) < 14.0f) isRoad = true;
	        {
	            Vector3 wp = {worldX, 0.0f, worldZ};
	            float dTrail = 9999.0f;
	            for(int i=0;i+1<(int)mountainTrail.size();i++){
	                dTrail = fminf(dTrail, DistPointSegmentXZ(wp, mountainTrail[i], mountainTrail[i+1]));
	            }
	            if(dTrail < trailWidth) isRoad = true;
	        }

	        if(!isRoad) continue;

	        bool inHouse = false;
	        for(const auto& h: cityHouses){
	            float hx = h.position.x;
	            float hz = h.position.z;
	            float halfX = h.size.x*0.5f + 0.4f;
	            float halfZ = h.size.z*0.5f + 0.4f;
	            if(fabsf(worldX - hx) < halfX && fabsf(worldZ - hz) < halfZ){
	                inHouse = true; break;
	            }
	        }
	        if(inHouse){
	            Color pixel=GetImageColor(noiseImage,x,y);
	            float hgt=-2.0f + (pixel.r/255.0f)*10.0f;
	            Color bc = GetImageColor(biomeNoise,x,y);
	            float biome = bc.r/255.0f;
	            Color base = BIOME_FOREST;
	            if(biome>0.66f) base = BIOME_SNOW;
	            else if(biome>0.33f) base = BIOME_ROCK;
	            if(hgt<waterLevel) base = SAND;
	            ImageDrawPixel(&terrainColors,x,y,base);
	        }
	    }

    Mesh terrainMesh=GenMeshHeightmap(noiseImage,{WORLD_SIZE,10,WORLD_SIZE});
    Model terrainModel=LoadModelFromMesh(terrainMesh);
    Texture2D terrainTex=LoadTextureFromImage(terrainColors);
    terrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=terrainTex;

    // --- Inventár a shop ---
    std::vector<Item> inventory = {{"DREVENY MEC","WEAPON",12,0,0,6.0f,true,"Slabsia zbran na zaciatok."}};
    std::vector<WeaponUpgrade> weaponUpgrades;
    std::vector<Item> shopItems = {
        {"SILNY ELIXIR","POTION",0,80,50,0.5f,false,"Lieci o 80 HP."},
        {"STRIEBORNY MEC","WEAPON",45,0,200,8.5f,false,"Velmi ostry proti orkom."},
        {"DRACI PRSTEN","MAGIC",60,0,450,0.2f,false,"Zvysuje silu magie."}
    };

    // --- Questy ---
	    std::vector<Quest> quests = {
        {"Zabij Orkov", "Zabij 3 orkov v okoli lesa.", "100 gold + 80 XP + 1 perk point", false, false, false, false, 100, 80, 1},
        {"Kniha Ohna", "Zabij 1 orka pre knihu ohna.", "KNIHA OHNA + 60 XP + 1 perk point", false, false, false, false, 0, 60, 1},
        {"Zlodejska Skuska", "Nazbieraj 200 gold.", "50 XP + 1 perk point", false, false, false, false, 0, 50, 1},
        {"Cistka Lesa", "Zabij 5 orkov.", "150 gold + 100 XP", false, false, false, false, 150, 100, 0},
        {"Lov Truhlic", "Otvori 2 truhlice.", "40 XP + 1 perk point", false, false, false, false, 0, 40, 1},
        {"Bylinkarov dlh", "Prines Alchymistovi: 3x MODRY KVET a 3x PSENICA.", "70 gold + 60 XP + 1 perk point", false, false, false, false, 70, 60, 1},
        {"Kovacova zakazka", "Vylepsi si zbran u kovaca aspon raz.", "90 gold + 80 XP", false, false, false, false, 90, 80, 0},
        {"Lovec: Taborove cesty", "Objav aspon 2 tabory (TABOR) vo svete.", "80 gold + 70 XP", false, false, false, false, 80, 70, 0},
	        {"Straz: Zamky", "Odomkni 1 zamok (truhlica alebo dvere).", "Lockpicky x2 + 50 XP + 1 perk point", false, false, false, false, 0, 50, 1},
	        {"Chrám na hore", "Najdi cestu na horu a pomodli sa pri oltari v chrame.", "Pozehnanie (+25 mana + 1 perk point)", false, false, false, false, 0, 0, 0},
	        {"Srdce Jaskyne", "V meste najdi Hostinskeho. Nieco sa deje v jaskyni.", "200 gold + 160 XP + 2 perk point", false, false, false, false, 200, 160, 2},
	        {"Echoes of the Forgotten", "Porozpravaj sa so Starym Pustovnikom pred branou.", "Pristup do mesta + 80 XP + 1 perk point", false, false, false, false, 0, 80, 1}
	    };
    auto FindQuestIdx = [&](const std::string& title)->int{
        for(int i=0;i<(int)quests.size();i++){
            if(quests[i].title == title) return i;
        }
        return -1;
    };
    int storyQuestIdx = FindQuestIdx("Echoes of the Forgotten");
    int herbQuestIdx = FindQuestIdx("Bylinkarov dlh");
    int smithQuestIdx = FindQuestIdx("Kovacova zakazka");
    int campsQuestIdx = FindQuestIdx("Lovec: Taborove cesty");
	    int locksQuestIdx = FindQuestIdx("Straz: Zamky");
	    int templeQuestIdx = FindQuestIdx("Chrám na hore");
	    int caveQuestIdx = FindQuestIdx("Srdce Jaskyne");
    int selectedQuestIdx = -1;
	    int openedChests = 0;
	    int enemiesKilled = 0;
    [[maybe_unused]] int lootPicked = 0;
    int locksPicked = 0;
    int weaponUpgradesDone = 0;
    [[maybe_unused]] int potionsBrewed = 0;
	    int storyStage = 0; // 0=inactive, 1=find sigil, 2=return to hermit, 3=tell guard, 4=done
	    int caveStage = 0;  // 0 not started, 1 get to cave, 2 return to innkeeper
		    bool storyIntroShown = false;
		    const std::string storySigilName = "SIGIL STRAZE";
		    bool templeBlessingTaken = false;
	    Vector3 storyRuin = poiRuins[0];

	    // --- Quest metadata (id/target) ---
	    for(auto& q : quests){
	        if(q.title == "Zabij Orkov"){ q.id = "kill_orcs_3"; q.target = 3; }
	        else if(q.title == "Kniha Ohna"){ q.id = "kill_orcs_1_book"; q.target = 1; }
	        else if(q.title == "Zlodejska Skuska"){ q.id = "gold_200"; q.target = 200; }
	        else if(q.title == "Cistka Lesa"){ q.id = "kill_orcs_5"; q.target = 5; }
	        else if(q.title == "Lov Truhlic"){ q.id = "open_chests_2"; q.target = 2; }
	        else if(q.title == "Bylinkarov dlh"){ q.id = "deliver_herbs"; q.target = 1; }
	        else if(q.title == "Kovacova zakazka"){ q.id = "smith_upgrade"; q.target = 1; }
	        else if(q.title == "Lovec: Taborove cesty"){ q.id = "discover_camps_2"; q.target = 2; }
	        else if(q.title == "Straz: Zamky"){ q.id = "pick_locks_1"; q.target = 1; }
	        else if(q.title == "Chrám na hore"){ q.id = "temple_blessing"; q.target = 1; }
	        else if(q.title == "Srdce Jaskyne"){ q.id = "cave_heart"; q.target = 1; }
	        else if(q.title == "Echoes of the Forgotten"){ q.id = "main_story"; q.target = 4; }
	    }

    // --- Skill tree ---
    SkillSection activeSkillSection = HEALTH;
    std::vector<std::string> skillSections = {"HEALTH","DAMAGE","STAMINA","MANA"};
    std::vector<Perk> perks;
    // HEALTH (dole zaklad, hore lepsie)
    perks.push_back({"HP Boost", false, 1, HEALTH, 1, "+30 max HP", {screenWidth/2.0f, screenHeight - 160.0f}, {1,2}});
    perks.push_back({"HP Regeneration", false, 3, HEALTH, 1, "Regeneruje zivoty", {screenWidth/2.0f - 180.0f, screenHeight - 280.0f}, {3}});
    perks.push_back({"Vitality", false, 3, HEALTH, 1, "+10 max HP", {screenWidth/2.0f + 180.0f, screenHeight - 280.0f}, {3}});
    perks.push_back({"Iron Skin", false, 6, HEALTH, 2, "Menej poskodenia", {screenWidth/2.0f, screenHeight - 420.0f}, {}});

    // DAMAGE
    perks.push_back({"Sword Mastery", false, 1, DAMAGE, 1, "+20% dmg meca", {screenWidth/2.0f, screenHeight - 160.0f}, {5,6}});
    perks.push_back({"Double Strike", false, 4, DAMAGE, 1, "15% sanca na 2. uder", {screenWidth/2.0f - 180.0f, screenHeight - 280.0f}, {7}});
    perks.push_back({"Rage", false, 4, DAMAGE, 1, "+10% dmg", {screenWidth/2.0f + 180.0f, screenHeight - 280.0f}, {7}});
    perks.push_back({"Executioner", false, 7, DAMAGE, 2, "+20% dmg na low HP", {screenWidth/2.0f, screenHeight - 420.0f}, {}});

    // STAMINA
    perks.push_back({"Endurance", false, 1, STAMINA, 1, "+10 stamina", {screenWidth/2.0f, screenHeight - 160.0f}, {9,10}});
    perks.push_back({"Sprinter", false, 3, STAMINA, 1, "Rychlejsi pohyb", {screenWidth/2.0f - 180.0f, screenHeight - 280.0f}, {11}});
    perks.push_back({"Evasion", false, 3, STAMINA, 1, "Menej dmg pri pohybe", {screenWidth/2.0f + 180.0f, screenHeight - 280.0f}, {11}});
    perks.push_back({"Athlete", false, 6, STAMINA, 2, "Rychlejsia akcia", {screenWidth/2.0f, screenHeight - 420.0f}, {}});

    // MANA
    perks.push_back({"Mana Boost", false, 1, MANA, 1, "+50 max mana", {screenWidth/2.0f, screenHeight - 160.0f}, {13,14}});
    perks.push_back({"Spell Power", false, 4, MANA, 1, "+20% dmg fireball", {screenWidth/2.0f - 180.0f, screenHeight - 280.0f}, {15}});
    perks.push_back({"Arcane Flow", false, 4, MANA, 1, "Rychlejsia mana regen", {screenWidth/2.0f + 180.0f, screenHeight - 280.0f}, {15}});
    perks.push_back({"Channeling", false, 7, MANA, 2, "Lacnejsie caro", {screenWidth/2.0f, screenHeight - 420.0f}, {}});

    // LOCKPICKING (napojene na STAMINA strom)
    int perkLockpickingIdx = (int)perks.size();
    perks.push_back({"Lockpicking", false, 2, STAMINA, 1, "Lahsie zamky (+tolerancia).", {screenWidth/2.0f - 180.0f, screenHeight - 520.0f}, {}});
    int perkLocksmithIdx = (int)perks.size();
    perks.push_back({"Locksmith", false, 6, STAMINA, 2, "Este lahsi zamok + menej lomenia.", {screenWidth/2.0f + 180.0f, screenHeight - 520.0f}, {}});
    if((int)perks.size() > 11){
        perks[11].children.push_back(perkLockpickingIdx);               // Athlete -> Lockpicking
        perks[perkLockpickingIdx].children.push_back(perkLocksmithIdx); // Lockpicking -> Locksmith
    }

    // --- Truhlice ---
    std::vector<Chest> chests;
    chests.push_back({{6,0,6}, true, false, 0, {"ZELEZNY MEC","WEAPON",25,0,0,10.0f,false,"Kvalitny mec. Da sa kupit u kovaca."}});
    chests.push_back({{-18,0,14}, true, true, 2, {"DAEDRICKY MEC","WEAPON",55,0,0,18.0f,false,"Zbran z inej dimenzie."}});
    chests.push_back({{12,0,-8}, true, true, 0, {"ELIXIR HP","POTION",0,50,0,0.5f,false,"Obnovuje zdravie."}});
    // story chest: sigil v ruinach
    chests.push_back({Vector3Add(storyRuin, {2.5f,0,1.5f}), true, true, 1, {storySigilName,"OTHER",0,0,0,1.0f,false,"Stary kovovy symbol. Na povrchu su runy straze."}});
    for(auto& c:chests) c.position.y=GetGroundHeight(&noiseImage,c.position)+0.5f;

	// --- NPC ---
	Vector3 castleCenter = {cityCenter.x, 0, cityCenter.z-18.0f};
	castleCenter.y = GetGroundHeight(&noiseImage, castleCenter);
	NPC oldMan={{gatePos.x + 4.0f, 0, gatePos.z + 10.0f}, "Stary Pustovnik",
	            "Zastav sa, kym je cas.", false, true, 0};
	oldMan.position.y = GetGroundHeight(&noiseImage, oldMan.position);
	NPC blacksmith={{cityCenter.x+10.0f,0,cityCenter.z+6.0f}, "Kovac",
	                "Vitaj, dobrodruh. Mam tovar aj opravy.", false, false, 0};
	blacksmith.position.y = GetGroundHeight(&noiseImage, blacksmith.position);
	blacksmith.position.y = GetGroundHeight(&noiseImage, blacksmith.position);
	NPC guard={{gatePos.x,0,gatePos.z+6.0f}, "Strazca",
	           "Brana je zatvorena. Chces vstup?", false, false, 0};
	guard.position.y = GetGroundHeight(&noiseImage, guard.position);
	NPC patrolGuard={{cityCenter.x-10.0f,0,cityCenter.z-6.0f}, "Strazca",
	                 "Bezpecnost mesta je prvorada.", false, false, 0};
	patrolGuard.position.y = GetGroundHeight(&noiseImage, patrolGuard.position);
	NPC innkeeper={{cityCenter.x+14.0f,0,cityCenter.z+12.0f}, "Hostinsky",
	               "Vitaj v hostinci. Novinky stoja viac ako pivo.", false, false, 0};
	innkeeper.position.y = GetGroundHeight(&noiseImage, innkeeper.position);
	NPC alchemist={{cityCenter.x-14.0f,0,cityCenter.z-2.0f}, "Alchymista",
	               "Vonia to tu bylinami... a problemami.", false, false, 0};
	alchemist.position.y = GetGroundHeight(&noiseImage, alchemist.position);
	NPC hunter={{cityCenter.x+9.0f,0,cityCenter.z+18.0f}, "Lovec",
	            "Stopy vedu stale niekam. Otazka je kam.", false, false, 0};
	hunter.position.y = GetGroundHeight(&noiseImage, hunter.position);
	std::vector<Vector3> guardPatrol = {
	    {cityCenter.x-10.0f, 0, cityCenter.z-6.0f},
	    {cityCenter.x+12.0f, 0, cityCenter.z-6.0f},
	    {cityCenter.x+12.0f, 0, cityCenter.z+10.0f},
	    {cityCenter.x-10.0f, 0, cityCenter.z+10.0f}
	};
    for(auto& p: guardPatrol) p.y = GetGroundHeight(&noiseImage, p);
    int guardPatrolIdx = 0;
    float guardSpeed = 0.04f;

    // --- Nepriatelia ---
    std::vector<Enemy> enemies;
	    std::vector<Vector3> enemySpots = {
	        {40,0,40},{55,0,60},{70,0,45},{85,0,70},{100,0,55},{120,0,80},
	        {-80,0,-40},{-95,0,-55},{-60,0,-70}
	    };
	    for(auto& epos: enemySpots){
	        float ey=GetGroundHeight(&noiseImage,{epos.x,0,epos.z});
	        int t = 0;
	        if(((int)enemies.size() % 3) == 1) t = 1; // archer
	        if(epos.x < -50.0f && epos.z < -30.0f) t = 2; // wolves in wild
	        float hp = (t==2) ? 70.0f : 100.0f;
	        float spd = (t==2) ? 0.082f : 0.05f;
	        enemies.push_back({{epos.x,ey,epos.z},{epos.x,ey,epos.z},hp,hp,true,spd,20,false,0, t, 0.0f, 0.0f});
	    }

	    // --- Fireball / projektily ---
	    std::vector<Fireball> fireballs;
	    std::vector<Projectile> playerProjectiles;
	    std::vector<Projectile> enemyProjectiles;
	    std::vector<LootDrop> lootDrops;
	    std::vector<Notification> notifications;

    // --- Suroviny do alchymie/craftingu (zbieranie vo svete) ---
    for(int i=0;i<(int)forest.size() && i<16;i+=2){
        Vector3 p = Vector3Add(forest[i].position, {0,0.2f,0});
        lootDrops.push_back({p, true, {"MODRY KVET","OTHER",0,0,0,0.1f,false,"Surovina do alchymie."}, 0, 99999.0f});
    }
    for(int i=0;i<(int)poiCamps.size();i++){
        Vector3 p = Vector3Add(poiCamps[i], {(float)GetRandomValue(-2,2),0.2f,(float)GetRandomValue(-2,2)});
        p.y = GetGroundHeight(&noiseImage, p) + 0.2f;
        lootDrops.push_back({p, true, {"PSENICA","OTHER",0,0,0,0.1f,false,"Surovina do alchymie."}, 0, 99999.0f});
    }
    for(int i=0;i<(int)poiRuins.size();i++){
        Vector3 p = Vector3Add(poiRuins[i], {(float)GetRandomValue(-3,3),0.2f,(float)GetRandomValue(-3,3)});
        p.y = GetGroundHeight(&noiseImage, p) + 0.2f;
        lootDrops.push_back({p, true, {"ZELEZNA RUDA","OTHER",0,0,0,0.4f,false,"Material na zbroj/zbrane."}, 0, 99999.0f});
    }
    for(int i=0;i<6;i++){
        Vector3 p = {cityCenter.x + (float)GetRandomValue(-18,18), 0, cityCenter.z + (float)GetRandomValue(-18,18)};
        p.y = GetGroundHeight(&noiseImage, p) + 0.2f;
        lootDrops.push_back({p, true, {"KOZA","OTHER",0,0,0,0.3f,false,"Koza na craft."}, 0, 99999.0f});
    }

    std::vector<Vector3> townLamps;
    std::vector<Vector3> townStalls;
    std::vector<Vector3> townFences;
    std::vector<Vector3> wallGuards;
    std::vector<float> wallGuardPhase;
    std::vector<Vector3> dockPlanks;
    std::vector<Vector3> dockBoats;
    townLamps = {
        {cityCenter.x-1.5f, 0, cityCenter.z+8.0f},
        {cityCenter.x+1.5f, 0, cityCenter.z+2.0f},
        {cityCenter.x-1.5f, 0, cityCenter.z-6.0f},
        {cityCenter.x+1.5f, 0, cityCenter.z-14.0f}
    };
    townStalls = {
        {cityCenter.x+5.0f, 0, cityCenter.z+10.0f},
        {cityCenter.x-5.0f, 0, cityCenter.z+10.0f},
        {cityCenter.x+5.0f, 0, cityCenter.z+4.0f},
        {cityCenter.x-5.0f, 0, cityCenter.z+4.0f}
    };
    townFences = {
        {cityCenter.x+10.0f, 0, cityCenter.z+16.0f},
        {cityCenter.x+10.8f, 0, cityCenter.z+16.0f},
        {cityCenter.x+11.6f, 0, cityCenter.z+16.0f},
        {cityCenter.x-10.0f, 0, cityCenter.z+16.0f},
        {cityCenter.x-10.8f, 0, cityCenter.z+16.0f},
        {cityCenter.x-11.6f, 0, cityCenter.z+16.0f}
    };
    dockPlanks = {
        {cityCenter.x+18.0f, 0, cityCenter.z+44.0f},
        {cityCenter.x+22.0f, 0, cityCenter.z+44.0f},
        {cityCenter.x+26.0f, 0, cityCenter.z+44.0f},
        {cityCenter.x+18.0f, 0, cityCenter.z+40.0f},
        {cityCenter.x+22.0f, 0, cityCenter.z+40.0f},
        {cityCenter.x+26.0f, 0, cityCenter.z+40.0f}
    };
    dockBoats = {
        {cityCenter.x+20.0f, 0, cityCenter.z+50.0f},
        {cityCenter.x+26.0f, 0, cityCenter.z+52.0f}
    };
    for(auto& p: townLamps) p.y = GetGroundHeight(&noiseImage, p);
    for(auto& p: townStalls) p.y = GetGroundHeight(&noiseImage, p);
    for(auto& p: townFences) p.y = GetGroundHeight(&noiseImage, p);
    for(auto& p: dockPlanks) p.y = GetGroundHeight(&noiseImage, p);
    for(auto& p: dockBoats) p.y = GetGroundHeight(&noiseImage, p);
    for(int i=0;i<6;i++){
        float ang = (float)i/6.0f * 2.0f * PI;
        Vector3 wp = {cityCenter.x + cosf(ang)*cityRadius, 0, cityCenter.z + sinf(ang)*cityRadius};
        wp.y = GetGroundHeight(&noiseImage, wp) + 2.4f;
        wallGuards.push_back(wp);
        wallGuardPhase.push_back((float)i*0.3f);
    }

	    bool showMenu=false, showShop=false;
	    bool showWorldMap=false;
	    bool showCrafting=false;
	    float shopAnim = 0.0f;
	    float craftingAnim = 0.0f;
	    float worldMapAnim = 0.0f;
	    int craftingMode=0; // 0 smithing, 1 alchemy
	    int selectedRecipeIdx=-1;
    MenuView menuView = MenuView::HOME;
    float talkCooldown=0.0f;
    float encounterTimer=10.0f;
    int weatherType=0; // 0 clear, 1 fog, 2 rain, 3 snow
    float weatherTimer=20.0f;
    int selectedItemIdx=-1, selectedCategory=0, selectedShopIdx=-1;
    int invScroll=0;
    std::vector<std::string> catNames={"VSETKO","ZBRANE","ABILITY","OSTATNE"};
    Camera3D camera={{0,10,10},{0,0,0},{0,1,0},60,CAMERA_PERSPECTIVE};

    int oldManNode = 0;
    int guardNode = 0;
    int smithNode = 0;
    int innNode = 0;
    int alchNode = 0;
    int hunterNode = 0;
    std::string dlgNpcName, dlgNpcLine;
    std::vector<std::string> dlgOptions;
    std::vector<bool> dlgEnabled;

    std::vector<WeatherParticle> rainDrops;
    std::vector<WeatherParticle> snowFlakes;
    int lastGraphicsQuality = graphicsQuality;
	    LockpickState lockpick;
	    KillcamState killcam;
	    IntroCinematicState intro;

	    SetTargetFPS(60);

	    auto SaveGameToFile = [&](const char* path)->bool{
	        std::ofstream out(path, std::ios::binary);
	        if(!out) return false;

	        out << "EOTF_SAVE 2\n";
	        out << "PLAYER " << playerPos.x << " " << playerPos.y << " " << playerPos.z << " " << playerAngle << "\n";
	        out << "TIME " << timeOfDay << " " << dayLengthSec << "\n";
	        out << "FLAGS " << (int)inInterior << " " << interiorKind << " " << interiorScene << " " << interiorLevel << " "
	            << interiorReturn.x << " " << interiorReturn.y << " " << interiorReturn.z << " "
	            << gateUnlocked << " " << templeBlessingTaken << " " << dungeonBossDefeated << " " << dungeonRewardTaken << "\n";
	        out << "STATS " << baseMaxHealth << " " << playerMaxHealth << " " << playerHealth << " "
	            << baseMaxMana << " " << playerMaxMana << " " << playerMana << " "
	            << baseMaxStamina << " " << playerMaxStamina << " " << playerStamina << "\n";
	        out << "PROG " << gold << " " << xp << " " << xpToNextLevel << " " << level << " " << perkPoints << " " << lockpicks << "\n";
	        out << "COUNTS " << enemiesKilled << " " << openedChests << " " << locksPicked << " " << weaponUpgradesDone << " " << lootPicked << "\n";
	        out << "STORY " << storyStage << " " << caveStage << " " << (storyIntroShown ? 1 : 0) << "\n";
	        out << "SPELL " << activeSpell << "\n";
	        out << "WEATHER " << weatherType << " " << weatherTimer << " " << (int)weatherEnabled << "\n";

	        out << "SKILLS " << (int)skills.size() << "\n";
	        for(int i=0;i<(int)skills.size();i++){
	            out << "SK " << i << " " << skills[i].level << " " << skills[i].xp << "\n";
	        }

	        out << "REPS " << (int)factionRep.size() << "\n";
	        for(int i=0;i<(int)factionRep.size();i++){
	            out << "REP " << i << " " << factionRep[i] << "\n";
	        }

	        out << "INVENTORY " << inventory.size() << "\n";
	        for(const auto& it : inventory){
	            out << "IT " << std::quoted(it.name) << " " << std::quoted(it.category) << " "
	                << it.damage << " " << it.heal << " " << it.price << " " << it.weight << " "
	                << (it.isEquipped ? 1 : 0) << " " << std::quoted(it.description) << "\n";
	        }

	        out << "UPGRADES " << weaponUpgrades.size() << "\n";
	        for(const auto& u : weaponUpgrades){
	            out << "UP " << std::quoted(u.weaponName) << " " << u.bonusDamage << "\n";
	        }

	        out << "CHESTS " << chests.size() << "\n";
	        for(const auto& c : chests){
	            out << "CH " << (c.active ? 1 : 0) << " " << (c.locked ? 1 : 0) << "\n";
	        }

	        out << "HOUSES " << cityHouses.size() << "\n";
	        for(const auto& h : cityHouses){
	            out << "H " << (h.canEnter ? 1 : 0) << "\n";
	        }

	        out << "POIS " << worldPOIs.size() << "\n";
	        for(const auto& p : worldPOIs){
	            out << "POI " << std::quoted(p.name) << " " << (p.discovered ? 1 : 0) << "\n";
	        }

	        out << "QUESTS " << quests.size() << "\n";
	        for(const auto& q : quests){
	            out << "Q "
	                << std::quoted(q.title) << " "
	                << std::quoted(q.objective) << " "
	                << std::quoted(q.reward) << " "
	                << (q.completed ? 1 : 0) << " "
	                << (q.active ? 1 : 0) << " "
	                << (q.rewardGiven ? 1 : 0) << " "
	                << (q.notified ? 1 : 0) << " "
	                << q.rewardGold << " " << q.rewardXp << " " << q.rewardPerkPoints << " "
	                << std::quoted(q.id) << " " << q.step << " " << q.target << "\n";
	        }

	        out << "PERKS " << perks.size() << "\n";
	        for(const auto& p : perks){
	            out << "P " << (p.unlocked ? 1 : 0) << "\n";
	        }

	        return true;
	    };

	    auto LoadGameFromFile = [&](const char* path)->bool{
	        std::ifstream in(path, std::ios::binary);
	        if(!in) return false;
	        std::string tag;
	        int version = 0;
	        in >> tag >> version;
	        if(tag != "EOTF_SAVE") return false;
	        if(version < 2) return false;

	        size_t invCount = 0, upCount = 0, chestCount = 0, poiCount = 0, qCount = 0, perkCount = 0;
	        while(in >> tag){
	            if(tag == "PLAYER"){
	                in >> playerPos.x >> playerPos.y >> playerPos.z >> playerAngle;
	            } else if(tag == "TIME"){
	                in >> timeOfDay >> dayLengthSec;
	            } else if(tag == "FLAGS"){
	                int ii = 0; int sc = 0;
	                in >> ii >> interiorKind >> sc >> interiorLevel
	                   >> interiorReturn.x >> interiorReturn.y >> interiorReturn.z
	                   >> gateUnlocked >> templeBlessingTaken >> dungeonBossDefeated >> dungeonRewardTaken;
	                inInterior = (ii != 0);
	                interiorScene = (InteriorScene)sc;
	            } else if(tag == "STATS"){
	                in >> baseMaxHealth >> playerMaxHealth >> playerHealth
	                   >> baseMaxMana >> playerMaxMana >> playerMana
	                   >> baseMaxStamina >> playerMaxStamina >> playerStamina;
	            } else if(tag == "PROG"){
	                in >> gold >> xp >> xpToNextLevel >> level >> perkPoints >> lockpicks;
		            } else if(tag == "COUNTS"){
		                in >> enemiesKilled >> openedChests >> locksPicked >> weaponUpgradesDone >> lootPicked;
		            } else if(tag == "STORY"){
		                int introShown = 0;
		                in >> storyStage >> caveStage >> introShown;
		                storyIntroShown = (introShown != 0);
		            } else if(tag == "SPELL"){
		                in >> activeSpell;
		                if(activeSpell < 0) activeSpell = 0;
		                if(activeSpell > 2) activeSpell = 0;
		            } else if(tag == "WEATHER"){
		                int we = 0;
		                in >> weatherType >> weatherTimer >> we;
		                weatherEnabled = (we != 0);
	            } else if(tag == "SKILLS"){
	                int n = 0; in >> n;
	                // read SK lines later
	            } else if(tag == "SK"){
	                int idx = 0;
	                in >> idx;
	                if(idx >= 0 && idx < (int)skills.size()){
	                    in >> skills[idx].level >> skills[idx].xp;
	                    skills[idx].level = (int)Clamp((float)skills[idx].level, 15.0f, 100.0f);
	                    if(skills[idx].xp < 0.0f) skills[idx].xp = 0.0f;
	                } else {
	                    int dummyL=0; float dummyXp=0.0f;
	                    in >> dummyL >> dummyXp;
	                }
	            } else if(tag == "REPS"){
	                int n = 0; in >> n;
	            } else if(tag == "REP"){
	                int idx = 0, val = 0;
	                in >> idx >> val;
	                if(idx >= 0 && idx < (int)factionRep.size()){
	                    factionRep[idx] = (int)Clamp((float)val, -100.0f, 100.0f);
	                }
	            } else if(tag == "INVENTORY"){
	                in >> invCount;
	                inventory.clear();
	                for(size_t i=0;i<invCount;i++){
	                    std::string itTag;
	                    in >> itTag;
	                    if(itTag != "IT") return false;
	                    Item it;
	                    int eq = 0;
	                    in >> std::quoted(it.name) >> std::quoted(it.category)
	                       >> it.damage >> it.heal >> it.price >> it.weight >> eq >> std::quoted(it.description);
	                    it.isEquipped = (eq != 0);
	                    inventory.push_back(it);
	                }
	            } else if(tag == "UPGRADES"){
	                in >> upCount;
	                weaponUpgrades.clear();
	                for(size_t i=0;i<upCount;i++){
	                    std::string upTag;
	                    in >> upTag;
	                    if(upTag != "UP") return false;
	                    WeaponUpgrade u;
	                    in >> std::quoted(u.weaponName) >> u.bonusDamage;
	                    weaponUpgrades.push_back(u);
	                }
		            } else if(tag == "CHESTS"){
		                in >> chestCount;
		                for(size_t i=0;i<chestCount;i++){
		                    std::string chTag; in >> chTag;
		                    if(chTag != "CH") return false;
		                    int a=0,l=0; in >> a >> l;
		                    if(i < chests.size()){
		                        chests[i].active = (a != 0);
		                        chests[i].locked = (l != 0);
		                    }
		                }
		            } else if(tag == "HOUSES"){
		                size_t houseCount = 0;
		                in >> houseCount;
		                for(size_t i=0;i<houseCount;i++){
		                    std::string hTag; in >> hTag;
		                    if(hTag != "H") return false;
		                    int ce = 0; in >> ce;
		                    if(i < cityHouses.size()){
		                        cityHouses[i].canEnter = (ce != 0);
		                    }
		                }
		            } else if(tag == "POIS"){
	                in >> poiCount;
	                for(size_t i=0;i<poiCount;i++){
	                    std::string pTag; in >> pTag;
	                    if(pTag != "POI") return false;
	                    std::string name;
	                    int disc = 0;
	                    in >> std::quoted(name) >> disc;
	                    for(auto& poi : worldPOIs){
	                        if(poi.name == name){
	                            poi.discovered = (disc != 0);
	                            break;
	                        }
	                    }
	                }
	            } else if(tag == "QUESTS"){
	                in >> qCount;
	                for(size_t i=0;i<qCount;i++){
	                    std::string qTag; in >> qTag;
	                    if(qTag != "Q") return false;
	                    Quest q;
	                    int c=0,a=0,rg=0,nt=0;
	                    in >> std::quoted(q.title) >> std::quoted(q.objective) >> std::quoted(q.reward)
	                       >> c >> a >> rg >> nt
	                       >> q.rewardGold >> q.rewardXp >> q.rewardPerkPoints
	                       >> std::quoted(q.id) >> q.step >> q.target;
	                    q.completed = (c!=0);
	                    q.active = (a!=0);
	                    q.rewardGiven = (rg!=0);
	                    q.notified = (nt!=0);
	                    if(i < quests.size()){
	                        quests[i].objective = q.objective;
	                        quests[i].reward = q.reward;
	                        quests[i].completed = q.completed;
	                        quests[i].active = q.active;
	                        quests[i].rewardGiven = q.rewardGiven;
	                        quests[i].notified = q.notified;
	                        quests[i].id = q.id;
	                        quests[i].step = q.step;
	                        quests[i].target = q.target;
	                    }
	                }
	            } else if(tag == "PERKS"){
	                in >> perkCount;
	                for(size_t i=0;i<perkCount;i++){
	                    std::string pTag; in >> pTag;
	                    if(pTag != "P") return false;
	                    int u=0; in >> u;
	                    if(i < perks.size()) perks[i].unlocked = (u != 0);
	                }
	            } else {
	                // unknown tag: ignore line
	                std::string rest;
	                std::getline(in, rest);
	            }
	        }

	        // close UI/locks on load
	        showMenu = false;
	        showShop = false;
	        showWorldMap = false;
	        showCrafting = false;
	        oldMan.isTalking = guard.isTalking = blacksmith.isTalking = innkeeper.isTalking = alchemist.isTalking = hunter.isTalking = false;
	        lockpick.active = false;
	        intro.active = false;
	        showLoading = false;
	        pendingTeleport = false;
	        killcam.active = false;
		        interactLock = 0.6f;
		        if(storyQuestIdx >= 0){
		            storyStage = quests[storyQuestIdx].step ? quests[storyQuestIdx].step : storyStage;
		            if(quests[storyQuestIdx].active || quests[storyQuestIdx].completed) storyIntroShown = true;
		        }
		        if(caveQuestIdx >= 0){
		            caveStage = quests[caveQuestIdx].step ? quests[caveQuestIdx].step : caveStage;
		        }
		        return true;
		    };

	    // --- Herná slučka ---
		    while(!WindowShouldClose()){
		        Vector2 mousePos=GetMousePosition();
	        float dtRaw = GetFrameTime();
        if(killcam.active){
            killcam.timer -= dtRaw;
            if(killcam.timer <= 0.0f) killcam.active = false;
        }
	        float timeScale = killcam.active ? 0.25f : 1.0f;
	        float dt = dtRaw * timeScale;
	        bool actionPressed = IsKeyPressed(KEY_F);
	        if(interactLock > 0.0f) interactLock -= dt;
        if(manaRegenDelay > 0.0f) manaRegenDelay -= dt;
        if(hpRegenDelay > 0.0f) hpRegenDelay -= dt;
        if(staminaRegenDelay > 0.0f) staminaRegenDelay -= dt;
        dlgNpcName.clear();
        dlgNpcLine.clear();
        dlgOptions.clear();
	        dlgEnabled.clear();
		        auto pushNote = [&](const std::string& t){
		            notifications.push_back({t, 3.2f});
		        };
		        if(IsKeyPressed(KEY_F5)){
		            bool ok = SaveGameToFile("savegame.txt");
		            pushNote(ok ? "Ulozene: savegame.txt" : "Ulozenie zlyhalo.");
		        }
		        if(IsKeyPressed(KEY_F9)){
		            bool ok = LoadGameFromFile("savegame.txt");
		            pushNote(ok ? "Nacitane: savegame.txt" : "Nacitanie zlyhalo.");
		        }
		        shopAnim = Approach(shopAnim, showShop ? 1.0f : 0.0f, dtRaw*8.0f);
		        craftingAnim = Approach(craftingAnim, showCrafting ? 1.0f : 0.0f, dtRaw*8.0f);
		        worldMapAnim = Approach(worldMapAnim, showWorldMap ? 1.0f : 0.0f, dtRaw*7.0f);
	        timeOfDay = fmodf(timeOfDay + (dt/dayLengthSec), 1.0f);
        if(showLoading){
            loadingTimer -= dt;
            if(loadingTimer <= 0.0f){
                showLoading = false;
                showMainMenu = false;
                settingsOpen = false;
                showMenu = false;
                showShop = false;
                showWorldMap = false;
                oldMan.isTalking = false;
                guard.isTalking = false;
                blacksmith.isTalking = false;
                innkeeper.isTalking = false;
                alchemist.isTalking = false;
                hunter.isTalking = false;
                if(pendingTeleport){
                    inInterior = false;
                    playerPos = pendingTeleportPos;
                    playerPos.y = GetGroundHeight(&noiseImage, playerPos);
                    pendingTeleport = false;
                    pushNote("Fast travel.");
                }
                if(!intro.played){
                    intro.played = true;
                    intro.active = true;
                    intro.t = 0.0f;
                    intro.cartYaw = PI; // coming towards the gate from outside
                    intro.cartDepart = false;
                    intro.cartDepartTimer = 0.0f;
                    intro.cartPos = {gatePos.x, 0.0f, gatePos.z + 95.0f};
                    intro.cartPos.y = GetGroundHeight(&noiseImage, intro.cartPos);
                    // keep player "in cart" during the ride
                    playerPos = intro.cartPos;
                    playerAngle = intro.cartYaw;
                }
            }
        }
        float daylight = Clamp(sinf(timeOfDay*2.0f*PI - PI/2.0f)*0.5f + 0.5f, 0.0f, 1.0f);
        bool isNight = daylight < 0.25f;
        for(auto& l: lootDrops) if(l.active){
            l.timer -= dt;
            if(l.timer <= 0.0f) l.active = false;
        }
        for(auto& n: notifications) if(n.timer > 0.0f) n.timer -= dt;
        notifications.erase(
            std::remove_if(notifications.begin(), notifications.end(),
                [](const Notification& n){ return n.timer <= 0.0f; }),
            notifications.end()
        );
        if(audioReady){
            if(musicDayLoaded) UpdateMusicStream(musicDay);
            if(musicNightLoaded) UpdateMusicStream(musicNight);
            if(musicDayLoaded) SetMusicVolume(musicDay, 0.35f * daylight);
            if(musicNightLoaded) SetMusicVolume(musicNight, 0.35f * (1.0f - daylight));
            if(musicDayLoaded && !IsMusicStreamPlaying(musicDay)) PlayMusicStream(musicDay);
            if(musicNightLoaded && (1.0f - daylight) > 0.1f && !IsMusicStreamPlaying(musicNight)) PlayMusicStream(musicNight);
        }
        // --- Cursor handling ---
        bool inGameplay = !showMenu && !showShop && !showWorldMap && !showCrafting &&
            !oldMan.isTalking && !guard.isTalking && !blacksmith.isTalking && !innkeeper.isTalking && !alchemist.isTalking && !hunter.isTalking &&
            !settingsOpen && !showMainMenu && !showLoading && !lockpick.active && !intro.active;
        if(inGameplay && mouseCaptured){
            HideCursor();
        } else {
            ShowCursor();
        }
        if(talkCooldown>0.0f) talkCooldown-=dt;
        if(showMainMenu || showLoading){
            BeginDrawing();
            ClearBackground(SKYRIM_BG_BOT);
            DrawRectangleGradientV(0,0,screenWidth,screenHeight,SKYRIM_BG_TOP,SKYRIM_BG_BOT);
            DrawVignette(screenWidth, screenHeight);
            // Jemné "panning" pozadie
            {
                float t = (float)GetTime();
                int bx = (int)(screenWidth/2 + sinf(t*0.2f)*40.0f);
                int by = (int)(screenHeight/2 + cosf(t*0.2f)*30.0f);
                DrawCircle(bx-140, by+40, 120, Fade(SKYRIM_GLOW,0.2f));
                DrawCircle(bx+120, by-20, 80, Fade(SKYRIM_GLOW,0.15f));
                DrawLine(bx-200, by+80, bx+200, by-80, Fade(SKYRIM_PANEL_EDGE,0.3f));
            }
            DrawText("ECHOES OF THE FORGOTTEN", screenWidth/2 - 220, 120, 28, SKYRIM_TEXT);
            DrawText("A Skyrim-like Demo", screenWidth/2 - 120, 155, 16, SKYRIM_MUTED);

            if(showLoading){
                DrawText("LOADING...", screenWidth/2 - 80, 300, 24, SKYRIM_TEXT);
                DrawRectangle(screenWidth/2 - 150, 340, 300, 12, Fade(BLACK,0.4f));
                float denom = (loadingTotal > 0.001f) ? loadingTotal : 1.0f;
                float prog = 1.0f - (loadingTimer/denom);
                if(prog < 0) prog = 0; if(prog > 1) prog = 1;
                DrawRectangle(screenWidth/2 - 150, 340, (int)(300*prog), 12, SKYRIM_GOLD);
                EndDrawing();
                continue;
            }

            if(settingsOpen){
                if(IsKeyPressed(KEY_ESCAPE)) settingsOpen=false;
            }

            Rectangle startBtn = {(float)screenWidth/2 - 120, 260, 240, 50};
            Rectangle settingsBtn  = {(float)screenWidth/2 - 120, 330, 240, 50};
            Rectangle quitBtn  = {(float)screenWidth/2 - 120, 400, 240, 50};
            if(!settingsOpen){
                if(CheckCollisionPointRec(mousePos, startBtn)) mainMenuIndex = 0;
                else if(CheckCollisionPointRec(mousePos, settingsBtn)) mainMenuIndex = 1;
                else if(CheckCollisionPointRec(mousePos, quitBtn)) mainMenuIndex = 2;
            }
            bool hoverStart = (!settingsOpen && mainMenuIndex==0);
            bool hoverSettings = (!settingsOpen && mainMenuIndex==1);
            bool hoverQuit = (!settingsOpen && mainMenuIndex==2);
            DrawRectangleRec(startBtn, hoverStart ? SKYRIM_GOLD : SKYRIM_PANEL);
            DrawRectangleLinesEx(startBtn, 2, SKYRIM_PANEL_EDGE);
            DrawText("START", startBtn.x + 80, startBtn.y + 12, 24, SKYRIM_TEXT);
            DrawRectangleRec(settingsBtn, hoverSettings ? SKYRIM_GOLD : SKYRIM_PANEL);
            DrawRectangleLinesEx(settingsBtn, 2, SKYRIM_PANEL_EDGE);
            DrawText("SETTINGS", settingsBtn.x + 60, settingsBtn.y + 12, 24, SKYRIM_TEXT);
            DrawRectangleRec(quitBtn, hoverQuit ? SKYRIM_GOLD : SKYRIM_PANEL);
            DrawRectangleLinesEx(quitBtn, 2, SKYRIM_PANEL_EDGE);
            DrawText("QUIT", quitBtn.x + 88, quitBtn.y + 12, 24, SKYRIM_TEXT);

            if((CheckCollisionPointRec(mousePos,startBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && !settingsOpen){
                showLoading = true;
                loadingTotal = 1.5f;
                loadingTimer = loadingTotal;
                settingsOpen = false;
            }
            if((CheckCollisionPointRec(mousePos,settingsBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && !settingsOpen){
                settingsOpen = true;
            }
            if((CheckCollisionPointRec(mousePos,quitBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && !settingsOpen){
                CloseWindow();
            }

            // Settings panel
            if(settingsOpen){
                Rectangle panel = {screenWidth/2.0f - 220.0f, 220, 440, 240};
                DrawRectangleRec(panel, Fade(BLACK,0.85f));
                DrawRectangleLinesEx(panel, 2, SKYRIM_PANEL_EDGE);
                DrawText("SETTINGS", panel.x+20, panel.y+15, 22, SKYRIM_TEXT);

                // Graphics quality
                DrawText("GRAPHICS:", panel.x+20, panel.y+60, 18, SKYRIM_TEXT);
                Rectangle gLow = {panel.x+140, panel.y+55, 70, 28};
                Rectangle gMed = {panel.x+220, panel.y+55, 70, 28};
                Rectangle gHigh= {panel.x+300, panel.y+55, 70, 28};
                DrawRectangleRec(gLow, graphicsQuality==0?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawRectangleRec(gMed, graphicsQuality==1?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawRectangleRec(gHigh,graphicsQuality==2?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawText("LOW", gLow.x+15, gLow.y+6, 16, SKYRIM_TEXT);
                DrawText("MED", gMed.x+15, gMed.y+6, 16, SKYRIM_TEXT);
                DrawText("HIGH",gHigh.x+10, gHigh.y+6,16, SKYRIM_TEXT);
                if(CheckCollisionPointRec(mousePos,gLow) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) graphicsQuality=0;
                if(CheckCollisionPointRec(mousePos,gMed) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) graphicsQuality=1;
                if(CheckCollisionPointRec(mousePos,gHigh)&& IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) graphicsQuality=2;

                // Weather
                DrawText("WEATHER:", panel.x+20, panel.y+100, 18, SKYRIM_TEXT);
                Rectangle wOn = {panel.x+140, panel.y+95, 70, 28};
                Rectangle wOff= {panel.x+220, panel.y+95, 70, 28};
                DrawRectangleRec(wOn, weatherEnabled?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawRectangleRec(wOff,!weatherEnabled?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawText("ON", wOn.x+20, wOn.y+6, 16, SKYRIM_TEXT);
                DrawText("OFF", wOff.x+15, wOff.y+6, 16, SKYRIM_TEXT);
                if(CheckCollisionPointRec(mousePos,wOn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) weatherEnabled=true;
                if(CheckCollisionPointRec(mousePos,wOff) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) weatherEnabled=false;

                // Fullscreen
                DrawText("FULLSCREEN:", panel.x+20, panel.y+140, 18, SKYRIM_TEXT);
                Rectangle fOn = {panel.x+180, panel.y+135, 70, 28};
                Rectangle fOff= {panel.x+260, panel.y+135, 70, 28};
                DrawRectangleRec(fOn, fullscreenEnabled?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawRectangleRec(fOff,!fullscreenEnabled?SKYRIM_GOLD:SKYRIM_PANEL);
                DrawText("ON", fOn.x+20, fOn.y+6, 16, SKYRIM_TEXT);
                DrawText("OFF", fOff.x+15, fOff.y+6, 16, SKYRIM_TEXT);
                if(CheckCollisionPointRec(mousePos,fOn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    if(!IsWindowFullscreen()) ToggleFullscreen();
                    fullscreenEnabled=true;
                }
                if(CheckCollisionPointRec(mousePos,fOff) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    if(IsWindowFullscreen()) ToggleFullscreen();
                    fullscreenEnabled=false;
                }

                Rectangle closeBtn = {panel.x+150, panel.y+185, 140, 32};
                DrawRectangleRec(closeBtn, SKYRIM_PANEL_EDGE);
                DrawText("CLOSE", closeBtn.x+40, closeBtn.y+6, 18, SKYRIM_BG_BOT);
                if(CheckCollisionPointRec(mousePos, closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    settingsOpen=false;
                }
            }
            EndDrawing();
            continue;
        }
        if(encounterTimer>0.0f) encounterTimer-=dt;
        if(weatherTimer>0.0f) weatherTimer-=dt;
        if(!fixedWeather && weatherTimer<=0.0f){
            weatherType = GetRandomValue(0,3);
            weatherTimer = (float)GetRandomValue(20,45);
        }

        // --- Perk efekty (pasívne) ---
        bool hasHpRegen=false;
        bool hasHpBoost=false;
        bool hasVitality=false;
        bool hasIronSkin=false;
        bool hasManaBoost=false;
        bool hasSwordMastery=false;
        bool hasDoubleStrike=false;
        bool hasRage=false;
        bool hasExecutioner=false;
        bool hasEndurance=false;
        bool hasSprinter=false;
        bool hasEvasion=false;
        bool hasAthlete=false;
        bool hasLockpicking=false;
        bool hasLocksmith=false;
        bool hasSpellPower=false;
        bool hasArcaneFlow=false;
        bool hasChanneling=false;
        for(const auto& p: perks){
            if(!p.unlocked) continue;
            if(p.name.find("Regeneration")!=std::string::npos) hasHpRegen=true;
            if(p.name.find("HP Boost")!=std::string::npos) hasHpBoost=true;
            if(p.name.find("Vitality")!=std::string::npos) hasVitality=true;
            if(p.name.find("Iron Skin")!=std::string::npos) hasIronSkin=true;
            if(p.name.find("Mana Boost")!=std::string::npos) hasManaBoost=true;
            if(p.name.find("Sword Mastery")!=std::string::npos) hasSwordMastery=true;
            if(p.name.find("Double Strike")!=std::string::npos) hasDoubleStrike=true;
            if(p.name.find("Rage")!=std::string::npos) hasRage=true;
            if(p.name.find("Executioner")!=std::string::npos) hasExecutioner=true;
            if(p.name.find("Endurance")!=std::string::npos) hasEndurance=true;
            if(p.name.find("Sprinter")!=std::string::npos) hasSprinter=true;
            if(p.name.find("Evasion")!=std::string::npos) hasEvasion=true;
            if(p.name.find("Athlete")!=std::string::npos) hasAthlete=true;
            if(p.name.find("Lockpicking")!=std::string::npos) hasLockpicking=true;
            if(p.name.find("Locksmith")!=std::string::npos) hasLocksmith=true;
            if(p.name.find("Spell Power")!=std::string::npos) hasSpellPower=true;
            if(p.name.find("Arcane Flow")!=std::string::npos) hasArcaneFlow=true;
            if(p.name.find("Channeling")!=std::string::npos) hasChanneling=true;
        }

        playerMaxHealth = baseMaxHealth + (hasHpBoost ? 30.0f : 0.0f) + (hasVitality ? 10.0f : 0.0f);
        playerMaxMana = baseMaxMana + (hasManaBoost ? 50.0f : 0.0f);
        playerMaxStamina = baseMaxStamina + (hasEndurance ? 10.0f : 0.0f);
        if(playerHealth > playerMaxHealth) playerHealth = playerMaxHealth;
        if(playerMana > playerMaxMana) playerMana = playerMaxMana;
        if(playerStamina > playerMaxStamina) playerStamina = playerMaxStamina;

	        float swordBonus = (hasSwordMastery ? 0.20f : 0.0f) + (hasRage ? 0.10f : 0.0f);
	        float fireballBonus = hasSpellPower ? 0.20f : 0.0f;
	        float doubleStrikeChance = hasDoubleStrike ? 0.15f : 0.0f;
	        float manaRegen = manaRegenPerSec + (hasArcaneFlow ? 6.0f : 0.0f);
	        float fireballCost = fireballManaCost * (hasChanneling ? 0.80f : 1.0f);
	        float swordCooldown = hasAthlete ? 0.35f : 0.5f;
	        float lockpickTolBonus = (hasLockpicking ? 6.0f : 0.0f) + (hasLocksmith ? 10.0f : 0.0f);
	        float lockpickDurMult = hasLocksmith ? 0.55f : (hasLockpicking ? 0.75f : 1.0f);
	        float oneHandedMult = SkillDamageMult(skills[(int)Skill::ONE_HANDED].level);
	        float destructionMult = SkillDamageMult(skills[(int)Skill::DESTRUCTION].level);
	        swordBonus += (oneHandedMult - 1.0f);
	        fireballBonus += (destructionMult - 1.0f);
	        fireballCost *= SkillCostMult(skills[(int)Skill::DESTRUCTION].level);
	        lockpickTolBonus += SkillLockTolBonus(skills[(int)Skill::LOCKPICKING].level);
	        {
	            float t = Clamp(((float)skills[(int)Skill::LOCKPICKING].level - 15.0f)/85.0f, 0.0f, 1.0f);
	            lockpickDurMult *= (1.0f - 0.18f*t);
	            lockpickDurMult = Clamp(lockpickDurMult, 0.45f, 1.10f);
	        }

        auto StartLockpick = [&](LockTargetType type, int index, int difficulty){
            if(lockpicks <= 0){
                pushNote("Nemas lockpicky.");
                return;
            }
            lockpick.active = true;
            lockpick.targetType = type;
            lockpick.index = index;
            lockpick.difficulty = ClampInt(difficulty, 0, 2);

            float baseTol = (lockpick.difficulty==0) ? 18.0f : (lockpick.difficulty==1 ? 12.0f : 8.0f);
            lockpick.toleranceDeg = baseTol + lockpickTolBonus;
            lockpick.sweetSpotDeg = 20.0f + Rand01()*140.0f;
            lockpick.pickDeg = (float)GetRandomValue(0, 180);
            lockpick.lockTurn = 0.0f;
            lockpick.durability = 1.0f;
            lockpick.breakCooldown = 0.0f;
        };

        if(lockpick.active){
            float dtUI = GetFrameTime(); // necitit slow-mo, UI ma byt citlive
            if(lockpick.breakCooldown > 0.0f) lockpick.breakCooldown -= dtUI;

            if(IsKeyPressed(KEY_ESCAPE)){
                lockpick.active = false;
            } else {
                const float pickSpeed = 110.0f;
                if(IsKeyDown(KEY_A)) lockpick.pickDeg -= pickSpeed*dtUI;
                if(IsKeyDown(KEY_D)) lockpick.pickDeg += pickSpeed*dtUI;
                lockpick.pickDeg = Clamp(lockpick.pickDeg, 0.0f, 180.0f);

                if(IsKeyDown(KEY_F) && lockpick.breakCooldown <= 0.0f){
                    float diff = fabsf(lockpick.pickDeg - lockpick.sweetSpotDeg);
                    float tol = fmaxf(1.0f, lockpick.toleranceDeg);
                    float maxTurn = 0.0f;
                    if(diff <= tol){
                        maxTurn = 1.0f;
                    } else {
                        // cim dalej od sweet spotu, tym menej sa zamok otoci
                        maxTurn = Clamp(1.0f - (diff - tol)/90.0f, 0.0f, 1.0f);
                    }

                    lockpick.lockTurn += 0.85f*dtUI;
                    if(lockpick.lockTurn > maxTurn){
                        lockpick.lockTurn = maxTurn;
                        float dmg = (0.45f + 0.75f*(diff/180.0f)) * lockpickDurMult;
                        lockpick.durability -= dmg*dtUI;
                        if(lockpick.durability <= 0.0f){
                            lockpicks = std::max(0, lockpicks - 1);
                            lockpick.durability = 1.0f;
                            lockpick.lockTurn = 0.0f;
                            lockpick.breakCooldown = 0.55f;
                            pushNote("Lockpick sa zlomil.");
                            if(lockpicks <= 0){
                                lockpick.active = false;
                            }
                        }
                    }

	                    if(lockpick.lockTurn >= 0.995f && fabsf(lockpick.pickDeg - lockpick.sweetSpotDeg) <= tol){
	                        // success
	                        if(lockpick.targetType == LockTargetType::CHEST){
	                            int ci = lockpick.index;
	                            if(ci >= 0 && ci < (int)chests.size() && chests[ci].active){
	                                chests[ci].locked = false;
	                                inventory.push_back(chests[ci].content);
	                                chests[ci].active = false;
	                                openedChests++;
	                                locksPicked++;
	                                AddSkillXP(skills[(int)Skill::LOCKPICKING], 12.0f + (float)lockpick.difficulty*8.0f);
	                                factionRep[(int)Faction::GUARD] = std::min(100, factionRep[(int)Faction::GUARD] + 1);
	                                pushNote("Odomkol si truhlicu.");
	                            }
	                        } else if(lockpick.targetType == LockTargetType::HOUSE){
	                            int hi = lockpick.index;
	                            if(hi >= 0 && hi < (int)cityHouses.size()){
	                                cityHouses[hi].canEnter = true;
	                                locksPicked++;
	                                AddSkillXP(skills[(int)Skill::LOCKPICKING], 10.0f + (float)lockpick.difficulty*7.0f);
	                                pushNote("Dvere odomknute.");
	                            }
	                        }
	                        interactLock = 0.25f;
	                        lockpick.active = false;
	                    }
                } else {
                    lockpick.lockTurn -= 1.2f*dtUI;
                    if(lockpick.lockTurn < 0.0f) lockpick.lockTurn = 0.0f;
                }
            }
        }

	        if(hasHpRegen && hpRegenDelay <= 0.0f){
	            playerHealth = fminf(playerMaxHealth, playerHealth + hpRegenPerSec*dt);
	        }
	        if(staminaRegenDelay <= 0.0f){
	            playerStamina = fminf(playerMaxStamina, playerStamina + staminaRegenPerSec*dt);
	        }
	        if(playerBurnTimer > 0.0f){
	            playerBurnTimer = fmaxf(0.0f, playerBurnTimer - dt);
	            playerHealth -= 4.0f*dt;
	            hpRegenDelay = fmaxf(hpRegenDelay, 1.0f);
	        }
	        if(playerPoisonTimer > 0.0f){
	            playerPoisonTimer = fmaxf(0.0f, playerPoisonTimer - dt);
	            playerHealth -= 2.5f*dt;
	            hpRegenDelay = fmaxf(hpRegenDelay, 1.0f);
	        }
	        if(playerSlowTimer > 0.0f){
	            playerSlowTimer = fmaxf(0.0f, playerSlowTimer - dt);
	        }

        // --- Intro cinematic (cart ride) ---
        if(intro.active){
            intro.t += dtRaw;
            float approachEndZ = gatePos.z + 24.0f;
            float stopTime = 11.0f;
            float dismountTime = 14.5f;

            if(intro.t < stopTime){
                intro.cartPos.z = fmaxf(approachEndZ, intro.cartPos.z - intro.cartSpeed*dtRaw);
                intro.cartPos.y = GetGroundHeight(&noiseImage, intro.cartPos);
                playerPos = intro.cartPos;
                playerAngle = intro.cartYaw;
            } else if(intro.t < dismountTime){
                // stop in front of gate
                intro.cartPos.z = approachEndZ;
                intro.cartPos.y = GetGroundHeight(&noiseImage, intro.cartPos);
                playerPos = intro.cartPos;
                playerAngle = intro.cartYaw;
            } else {
                // hand over control to player; cart leaves back the way it came
                intro.active = false;
                intro.cartDepart = true;
                intro.cartDepartTimer = 5.0f;
                playerPos = {gatePos.x, gatePos.y, gatePos.z + 18.0f};
                playerAngle = PI;
                interactLock = 0.6f;
            }

            int ci = GetIntroCueIndex(intro.t);
            if(ci != intro.cueIdx){
                intro.cueIdx = ci;
                int vi = (ci >= 0) ? INTRO_CUES[ci].voiceIndex : -1;
                if(audioReady && vi >= 0 && vi < (int)sfxIntro.size() && introSfxLoaded[vi]){
                    PlaySound(sfxIntro[vi]);
                }
            }
        }
        if(intro.cartDepart){
            intro.cartDepartTimer -= dtRaw;
            intro.cartPos.z += intro.cartSpeed*dtRaw;
            intro.cartPos.y = GetGroundHeight(&noiseImage, intro.cartPos);
            if(intro.cartDepartTimer <= 0.0f) intro.cartDepart = false;
        }

        // --- TAB: inventár ---
        if(IsKeyPressed(KEY_TAB)){
            showMenu=!showMenu; showShop=false;
            if(showMenu) menuView = MenuView::HOME;
        }
        if(IsKeyPressed(KEY_M) && !showMenu && !showShop && !showCrafting &&
            !oldMan.isTalking && !guard.isTalking && !blacksmith.isTalking && !innkeeper.isTalking && !alchemist.isTalking && !hunter.isTalking &&
            !settingsOpen && !showMainMenu && !showLoading && !lockpick.active){
            if(InventoryHasItem(inventory, "MAPA")){
                showWorldMap = !showWorldMap;
                if(showWorldMap){
                    showMenu = false;
                    showShop = false;
                }
            } else {
                pushNote("Nemas mapu (MAPA).");
            }
        }
        if(showWorldMap && IsKeyPressed(KEY_ESCAPE)){
            showWorldMap = false;
        }

	        // --- Hráč pohyb ---
	        if(!showMenu && !showShop && !showWorldMap && !showCrafting &&
	            !oldMan.isTalking && !guard.isTalking && !blacksmith.isTalking && !innkeeper.isTalking && !alchemist.isTalking && !hunter.isTalking &&
	            !lockpick.active){
		            bool actionPressed = IsKeyPressed(KEY_F);
		            bool knowsFrost = InventoryHasItem(inventory, "KNIHA MRAZU");
		            bool knowsPoison = InventoryHasItem(inventory, "KNIHA JEDU");
		            if(IsKeyPressed(KEY_Z)){
		                // cyklenie spellov (fire je vzdy dostupny)
		                int next = activeSpell;
		                for(int k=0;k<3;k++){
		                    next = (next + 1) % 3;
		                    if(next == 0) break;
		                    if(next == 1 && knowsFrost) break;
		                    if(next == 2 && knowsPoison) break;
		                }
		                activeSpell = next;
		                const char* nm = (activeSpell==0) ? "OHEŇ" : (activeSpell==1 ? "MRAZ" : "JED");
		                pushNote(std::string("Spell: ") + nm);
		            }
		            isUnderwater=(playerPos.y<waterLevel);
	            float speedBonus = (hasSprinter ? 0.05f : 0.0f) + (hasEvasion ? 0.03f : 0.0f);
	            bool moving = (IsKeyDown(KEY_W)||IsKeyDown(KEY_A)||IsKeyDown(KEY_S)||IsKeyDown(KEY_D));
	            bool sprinting = (!isUnderwater && moving && IsKeyDown(KEY_LEFT_SHIFT) && playerStamina > 0.5f);
	            playerIsMoving = moving;
	            playerIsSprinting = sprinting;
		            if(playerIsMoving){
		                playerWalkPhase += dt * (playerIsSprinting ? 10.0f : 7.0f);
		            }
		            float moveSpeed=(isUnderwater?0.12f:0.25f) + speedBonus;
		            if(playerSlowTimer > 0.0f) moveSpeed *= 0.70f;
		            if(sprinting){
		                moveSpeed *= 1.65f;
		                playerStamina = fmaxf(0.0f, playerStamina - 28.0f*dt);
		                staminaRegenDelay = 0.8f;
		            }
            if(inInterior){
                if(interiorKind == 0){
                    if(IsKeyDown(KEY_W)) {playerPos.x+=sinf(playerAngle)*moveSpeed; playerPos.z+=cosf(playerAngle)*moveSpeed;}
                    if(IsKeyDown(KEY_S)) {playerPos.x-=sinf(playerAngle)*moveSpeed; playerPos.z-=cosf(playerAngle)*moveSpeed;}
                    if(IsKeyDown(KEY_D)) {playerPos.x+=sinf(playerAngle-PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle-PI/2)*moveSpeed;}
                    if(IsKeyDown(KEY_A)) {playerPos.x+=sinf(playerAngle+PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle+PI/2)*moveSpeed;}
                    if(mouseCaptured){
                        Vector2 center = {(float)screenWidth/2.0f, (float)screenHeight/2.0f};
                        Vector2 delta = Vector2Subtract(mousePos, center);
                        playerAngle -= delta.x*0.0025f;
                        SetMousePosition((int)center.x, (int)center.y);
	        }
	        bool anyDialogueNow = (oldMan.isTalking || guard.isTalking || blacksmith.isTalking || innkeeper.isTalking || alchemist.isTalking || hunter.isTalking);
	        if(showMenu || showShop || showWorldMap || showCrafting || anyDialogueNow || lockpick.active){
	            playerIsMoving = false;
	            playerIsSprinting = false;
	        }

                    float baseY = 0.0f;
                    float minX = -5.5f, maxX = 5.5f, minZ = -5.5f, maxZ = 5.5f;
	                    if(interiorScene == INT_BLACKSMITH){ minX=-6.5f; maxX=6.5f; minZ=-5.5f; maxZ=6.8f; }
	                    if(interiorScene == INT_ALCHEMIST){ minX=-6.2f; maxX=6.2f; minZ=-5.5f; maxZ=6.8f; }
	                    if(interiorScene == INT_GENERAL){ minX=-6.5f; maxX=6.5f; minZ=-5.5f; maxZ=6.8f; }
	                    if(interiorScene == INT_HUNTER){ minX=-6.2f; maxX=6.2f; minZ=-5.5f; maxZ=6.8f; }
	                    if(interiorScene == INT_INN){ minX=-8.5f; maxX=8.5f; minZ=-7.0f; maxZ=7.2f; }
	                    if(interiorScene == INT_TEMPLE){ minX=-8.0f; maxX=8.0f; minZ=-9.5f; maxZ=9.5f; }
	                    if(interiorScene == INT_CASTLE){
	                        baseY = (float)interiorLevel * 3.0f;
	                        if(interiorLevel <= 1){
	                            minX=-12.0f; maxX=12.0f; minZ=-12.0f; maxZ=12.4f;
                        } else {
                            minX=-4.0f; maxX=4.0f; minZ=-4.0f; maxZ=4.0f;
                        }
                    }

                    playerPos.y = baseY;
                    playerPos.x = Clamp(playerPos.x, minX+0.5f, maxX-0.5f);
                    playerPos.z = Clamp(playerPos.z, minZ+0.5f, maxZ-0.5f);

                    auto doorPos = [&]()->Vector3{
                        if(interiorScene == INT_CASTLE) return {0, baseY, 12.2f};
                        return {0, baseY, maxZ-0.3f};
                    };
                    Vector3 interiorDoor = doorPos();

                    // castle stairs/tower "rooms"
                    if(interiorScene == INT_CASTLE && (actionPressed || IsKeyPressed(KEY_E))){
                        Vector3 stairs = {9.2f, baseY, 4.2f};
                        Vector3 towerDoor = {-9.5f, baseY, -9.5f};
                        if(interiorLevel==0 && Vector3Distance(playerPos, stairs) < 1.6f){
                            interiorLevel = 1;
                            playerPos = {9.2f, 3.0f, 4.2f};
                            pushNote("Vystupil si na balkon.");
                        } else if(interiorLevel==1 && Vector3Distance(playerPos, stairs) < 1.6f){
                            interiorLevel = 0;
                            playerPos = {9.2f, 0.0f, 4.2f};
                            pushNote("Zostupil si do saly.");
                        } else if(interiorLevel==1 && Vector3Distance(playerPos, towerDoor) < 1.8f){
                            interiorLevel = 2;
                            playerPos = {0.0f, 6.0f, 0.0f};
                            pushNote("Vstupil si do veze.");
                        } else if(interiorLevel==2 && Vector3Distance(playerPos, {0,baseY,0}) < 1.6f){
                            interiorLevel = 1;
                            playerPos = {-9.5f, 3.0f, -9.5f};
                            pushNote("Vratil si sa na poschodie.");
                        }
                    }

                    // exit
                    if((actionPressed || IsKeyPressed(KEY_E)) && Vector3Distance(playerPos, interiorDoor) < 1.4f){
                        inInterior = false;
                        interiorKind = 0;
                        interiorLevel = 0;
                        playerPos = Vector3Add(interiorReturn, {0,0,2.5f});
                        interactLock = 0.6f;
                        pushNote("Opustil si interier.");
                    }

		                    // interior NPC interaction (behind counter)
		                    if(actionPressed && interiorScene != INT_CASTLE){
		                        Vector3 npcPos = {0, baseY, -3.6f};
		                        if(interiorScene == INT_INN) npcPos = {0, baseY, -4.4f};
		                        if(Vector3Distance(playerPos, npcPos) < 2.0f){
		                            if(interiorScene == INT_BLACKSMITH){ blacksmith.isTalking = true; smithNode = 0; talkCooldown = 0.4f; }
		                            if(interiorScene == INT_ALCHEMIST){ alchemist.isTalking = true; alchNode = 0; talkCooldown = 0.4f; }
		                            if(interiorScene == INT_INN){ innkeeper.isTalking = true; innNode = 0; talkCooldown = 0.4f; }
		                            if(interiorScene == INT_HUNTER){ hunter.isTalking = true; hunterNode = 0; talkCooldown = 0.4f; }
		                        }
		                    }

		                    // inn: sleep / rest (stairs to upper rooms)
		                    if(interiorScene == INT_INN && (actionPressed || IsKeyPressed(KEY_E))){
		                        Vector3 sleepSpot = {7.2f, baseY, 5.2f};
		                        float sd = Vector3Distance(playerPos, sleepSpot);
		                        if(sd < 2.0f){
		                            const int cost = 10;
		                            if(gold >= cost){
		                                gold -= cost;
		                                timeOfDay = 0.28f; // rano
		                                playerHealth = playerMaxHealth;
		                                playerMana = playerMaxMana;
		                                playerStamina = playerMaxStamina;
		                                playerBurnTimer = playerPoisonTimer = playerSlowTimer = 0.0f;
		                                hpRegenDelay = manaRegenDelay = staminaRegenDelay = 0.0f;
		                                pushNote("Prespal si noc. Oddych obnovil sily.");
		                                AddSkillXP(skills[(int)Skill::SPEECH], 4.0f);
		                                factionRep[(int)Faction::TOWNSFOLK] = std::min(100, factionRep[(int)Faction::TOWNSFOLK] + 1);
		                            } else {
		                                pushNote("Nemas gold na izbu (10).");
		                            }
		                        }
		                    }

		                    // temple altar blessing
		                    if(interiorScene == INT_TEMPLE && actionPressed){
		                        Vector3 altarPos = {0.0f, baseY, -6.6f};
	                        float ad = Vector3Distance(playerPos, altarPos);
	                        if(ad < 2.2f){
	                            if(!templeBlessingTaken){
	                                templeBlessingTaken = true;
	                                baseMaxMana += 25.0f;
	                                playerMaxMana += 25.0f;
	                                playerMana = playerMaxMana;
	                                perkPoints += 1;
	                                pushNote("Pozehnanie Hory: +25 max mana a +1 perk point.");
	                                if(templeQuestIdx >= 0){
	                                    quests[templeQuestIdx].active = true;
	                                    quests[templeQuestIdx].completed = true;
	                                }
	                            } else {
	                                pushNote("Oltar uz mlci. Pozehnanie si uz dostal.");
	                            }
	                        }
	                    }
	                } else {
	                    Vector3 prev = playerPos;
	                    if(IsKeyDown(KEY_W)) {playerPos.x+=sinf(playerAngle)*moveSpeed; playerPos.z+=cosf(playerAngle)*moveSpeed;}
                    if(IsKeyDown(KEY_S)) {playerPos.x-=sinf(playerAngle)*moveSpeed; playerPos.z-=cosf(playerAngle)*moveSpeed;}
                    if(IsKeyDown(KEY_D)) {playerPos.x+=sinf(playerAngle-PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle-PI/2)*moveSpeed;}
                    if(IsKeyDown(KEY_A)) {playerPos.x+=sinf(playerAngle+PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle+PI/2)*moveSpeed;}
                    if(mouseCaptured){
                        Vector2 center = {(float)screenWidth/2.0f, (float)screenHeight/2.0f};
                        Vector2 delta = Vector2Subtract(mousePos, center);
                        playerAngle -= delta.x*0.0025f;
                        SetMousePosition((int)center.x, (int)center.y);
                    }
                    playerPos.y = 0.0f;
                    auto inRect = [&](float x, float z, float minX, float maxX, float minZ, float maxZ){
                        return (x >= minX && x <= maxX && z >= minZ && z <= maxZ);
                    };
                    auto insideDungeon = [&](float x, float z){
                        if(inRect(x,z,-5.0f, 5.0f,  2.0f, 12.0f)) return true;   // room 1
                        if(inRect(x,z,-1.6f, 1.6f, -4.0f,  2.0f)) return true;   // corridor 1
                        if(inRect(x,z,-5.0f, 5.0f,-14.0f, -4.0f)) return true;   // room 2
                        if(inRect(x,z,-1.6f, 1.6f,-22.0f,-14.0f)) return true;   // corridor 2
                        if(inRect(x,z,-6.0f, 6.0f,-34.0f,-22.0f)) return true;   // boss room
                        return false;
                    };
	                    if(!insideDungeon(playerPos.x, playerPos.z)){
	                        playerPos = prev;
	                    }
	                    // gate before boss room
	                    if(!dungeonGateOpen){
	                        bool enteringBoss = (playerPos.z < -22.0f) && (prev.z >= -22.0f) && (fabsf(playerPos.x) < 2.0f);
	                        bool inBossArea = (playerPos.z < -22.0f) && (fabsf(playerPos.x) < 2.0f);
	                        if(enteringBoss || inBossArea){
	                            playerPos = prev;
	                        }
	                    }

	                    // lever in room 2 opens the gate
	                    Vector3 leverPos = {4.0f, 0.0f, -9.0f};
	                    if(actionPressed && !dungeonLeverPulled && Vector3Distance(playerPos, leverPos) < 1.6f){
	                        dungeonLeverPulled = true;
	                        dungeonGateOpen = true;
	                        pushNote("Paka cvakla. Niekde v hlbine sa otvorili dvere.");
	                    }

	                    if(dungeonTrapCooldown > 0.0f) dungeonTrapCooldown -= dt;
                    for(const auto& tp : dungeonTraps){
                        if(dungeonTrapCooldown <= 0.0f && Vector3Distance(playerPos, tp) < 0.85f){
                            playerHealth -= 15.0f;
                            hpRegenDelay = 2.0f;
                            dungeonTrapCooldown = 0.9f;
                            pushNote("Pasca!");
                            break;
                        }
                    }

                    Vector3 exitDoor = {0,0,12.2f};
                    if((actionPressed || IsKeyPressed(KEY_E)) && Vector3Distance(playerPos, exitDoor) < 1.3f){
                        inInterior = false;
                        interiorKind = 0;
                        playerPos = Vector3Add(interiorReturn, {0,0,2.5f});
                        interactLock = 0.6f;
                        pushNote("Opustil si jaskynu.");
                    }

                    if((actionPressed || IsKeyPressed(KEY_E)) && dungeonBossDefeated && !dungeonRewardTaken && Vector3Distance(playerPos, dungeonRewardPos) < 1.4f){
                        inventory.push_back({"ANCIENT BLADE","WEAPON",70,0,0,14.0f,false,"Unikatna odmena z jaskyne."});
                        gold += 120;
                        dungeonRewardTaken = true;
                        pushNote("Ziskal si unikatnu odmenu.");
                    }
                }
            } else {
                if(IsKeyDown(KEY_W)) {playerPos.x+=sinf(playerAngle)*moveSpeed; playerPos.z+=cosf(playerAngle)*moveSpeed;}
                if(IsKeyDown(KEY_S)) {playerPos.x-=sinf(playerAngle)*moveSpeed; playerPos.z-=cosf(playerAngle)*moveSpeed;}
                if(IsKeyDown(KEY_D)) {playerPos.x+=sinf(playerAngle-PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle-PI/2)*moveSpeed;}
                if(IsKeyDown(KEY_A)) {playerPos.x+=sinf(playerAngle+PI/2)*moveSpeed; playerPos.z+=cosf(playerAngle+PI/2)*moveSpeed;}
                if(mouseCaptured){
                    Vector2 center = {(float)screenWidth/2.0f, (float)screenHeight/2.0f};
                    Vector2 delta = Vector2Subtract(mousePos, center);
                    playerAngle -= delta.x*0.0025f;
                    SetMousePosition((int)center.x, (int)center.y);
                }

                float gY=GetGroundHeight(&noiseImage,playerPos);
                if(isUnderwater){
                    if(IsKeyDown(KEY_SPACE)) playerPos.y+=0.15f;
                    if(IsKeyDown(KEY_LEFT_SHIFT)) playerPos.y-=0.15f;
                    if(playerPos.y<gY) playerPos.y=gY;
                } else playerPos.y=gY;
                float distToCity = Vector3Distance(playerPos, cityCenter);
                float distToGate = Vector3Distance(playerPos, gatePos);
                if(distToCity < cityRadius-2.0f) insideCity = true;
                if(distToCity > cityRadius+2.0f) insideCity = false;
                for(auto& poi : worldPOIs){
                    if(!poi.discovered && Vector3Distance(playerPos, poi.position) < 32.0f){
                        poi.discovered = true;
                        pushNote(std::string("Objavene: ") + poi.name);
                    }
                }

                if(!gateUnlocked && distToGate > 4.5f && distToCity < cityRadius-0.6f){
                    Vector3 pushDir = Vector3Normalize(Vector3Subtract(playerPos, cityCenter));
                    playerPos = Vector3Add(cityCenter, Vector3Scale(pushDir, cityRadius-0.6f));
                    pushNote("Brana je zatvorena.");
                }
                if(insideCity && distToGate > 4.5f && distToCity > cityRadius-0.6f){
                    Vector3 pushDir = Vector3Normalize(Vector3Subtract(playerPos, cityCenter));
                    playerPos = Vector3Add(cityCenter, Vector3Scale(pushDir, cityRadius-0.6f));
                }
                if(!insideCity && distToGate > 4.5f && distToCity < cityRadius-0.6f){
                    Vector3 pushDir = Vector3Normalize(Vector3Subtract(playerPos, cityCenter));
                    playerPos = Vector3Add(cityCenter, Vector3Scale(pushDir, cityRadius-0.6f));
                }
            }

		            // --- Combat: zbrane (melee + luk) ---
		            if(playerAttackTimer > 0.0f) playerAttackTimer = fmaxf(0.0f, playerAttackTimer - dt);
		            if(swordTimer>0) swordTimer-=dt;
		            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && swordTimer<=0){
		                int dmg=0;
		                std::string weaponName;
		                for(auto& it:inventory){
	                    if(it.isEquipped && it.category=="WEAPON"){
	                        dmg = it.damage;
	                        weaponName = it.name;
	                    }
	                }
	                if(!weaponName.empty()){
	                    dmg += GetWeaponUpgradeBonus(weaponUpgrades, weaponName);
	                }
		                if(dmg>0){
		                    WeaponKind wKind = WeaponKindFromItemName(weaponName);
		                    float cd = swordCooldown;
		                    float staminaCost = 10.0f;
		                    float range = 3.5f;
		                    float dmgMult = 1.0f;
		                    float dur = 0.34f;
		                    if(wKind == WeaponKind::DAGGER){ cd *= 0.75f; staminaCost = 8.0f; range = 2.7f; dur = 0.26f; dmgMult = 0.90f; }
		                    if(wKind == WeaponKind::AXE){ cd *= 1.25f; staminaCost = 15.0f; range = 3.9f; dur = 0.42f; dmgMult = 1.15f; }
		                    if(wKind == WeaponKind::BOW){ cd *= 1.05f; staminaCost = 12.0f; dur = 0.22f; }

		                    if(playerStamina < staminaCost){
		                        pushNote("Nemas stamina.");
		                    } else {
		                        swordTimer = cd;
		                        playerAttackDuration = dur;
		                        playerAttackTimer = playerAttackDuration;
		                        playerStamina = fmaxf(0.0f, playerStamina - staminaCost);
		                        staminaRegenDelay = 0.9f;

		                        if(wKind == WeaponKind::BOW){
		                            Projectile pr;
		                            pr.position = Vector3Add(playerPos, {0,1.45f,0});
		                            pr.direction = Vector3Normalize({sinf(playerAngle), 0.02f, cosf(playerAngle)});
		                            pr.active = true;
		                            pr.timer = 3.8f;
		                            pr.speed = 22.0f;
		                            float archMult = SkillDamageMult(skills[(int)Skill::ARCHERY].level);
		                            pr.damage = (float)dmg * archMult;
		                            pr.color = {220, 210, 190, 255};
		                            playerProjectiles.push_back(pr);
		                            AddSkillXP(skills[(int)Skill::ARCHERY], 7.0f);
		                        } else {
		                            bool anyHit = false;
		                            for(auto& e:enemies) if(e.active && Vector3Distance(playerPos,e.position) < range){
		                                float hpBefore = e.health;
		                                int totalDmg = (int)roundf(dmg * (1.0f + swordBonus) * dmgMult);
		                                if(hasExecutioner && e.health < e.maxHealth*0.3f) totalDmg += (int)roundf((float)dmg * 0.20f);
		                                bool doDouble = (doubleStrikeChance > 0.0f) && (GetRandomValue(1,100) <= (int)roundf(doubleStrikeChance*100.0f));
		                                if(doDouble) totalDmg += (int)roundf((float)dmg * (1.0f + swordBonus) * dmgMult);
		                                e.health-=totalDmg; e.isAggressive=true;
		                                e.position=Vector3Add(e.position,Vector3Scale({sinf(playerAngle),0,cosf(playerAngle)},1.5f));
		                                anyHit = true;
		                                if(e.health<=0){
		                                    if(!killcam.active && hpBefore <= e.maxHealth*0.25f){
		                                        killcam.active = true;
		                                        killcam.duration = 1.05f;
		                                        killcam.timer = killcam.duration;
		                                        killcam.target = e.position;
		                                        if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
		                                    }
		                                    e.active=false; xp+=40;
		                                    enemiesKilled++;
		                                    if(oldMan.hasQuest) oldMan.questProgress++;
		                                    LootDrop ld = {e.position, true, {"", "NONE", 0, 0, 0, 0.0f, false, ""}, GetRandomValue(20,60), 20.0f};
		                                    if(GetRandomValue(0,100) < 35) ld.item = {"ELIXIR HP","POTION",0,40,0,0.5f,false,"Obnovuje zdravie."};
		                                    lootDrops.push_back(ld);
		                                    pushNote("Nepriatel porazeny. Loot na zemi.");
		                                }
		                            }
		                            if(anyHit){
		                                AddSkillXP(skills[(int)Skill::ONE_HANDED], 5.0f + (float)dmg*0.25f);
		                            }
		                            if(inInterior && interiorKind==1 && dungeonBoss.active && Vector3Distance(playerPos, dungeonBoss.position) < range+0.3f){
		                                float hpBefore = dungeonBoss.health;
		                                int totalDmg = (int)roundf((float)dmg * (1.0f + swordBonus) * dmgMult);
		                                if(hasExecutioner && dungeonBoss.health < dungeonBoss.maxHealth*0.3f) totalDmg += (int)roundf((float)dmg * 0.20f);
		                                dungeonBoss.health -= totalDmg;
		                                dungeonBoss.position = Vector3Add(dungeonBoss.position, Vector3Scale({sinf(playerAngle),0,cosf(playerAngle)},1.1f));
		                                AddSkillXP(skills[(int)Skill::ONE_HANDED], 7.0f + (float)dmg*0.30f);
		                                if(dungeonBoss.health <= 0.0f){
		                                    if(!killcam.active && hpBefore <= dungeonBoss.maxHealth*0.25f){
		                                        killcam.active = true;
		                                        killcam.duration = 1.15f;
		                                        killcam.timer = killcam.duration;
		                                        killcam.target = dungeonBoss.position;
		                                        if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
		                                    }
		                                    dungeonBoss.active = false;
		                                    dungeonBossDefeated = true;
		                                    xp += 160;
		                                    perkPoints += 1;
		                                    pushNote("Boss porazeny!");
		                                }
		                            }
		                        }
		                    }
		                }
		            }

		            // v interieroch (domy) aspon pohybuj projektily, aby nezamrzli na mieste
		            if(inInterior && interiorKind==0){
		                for(auto& p : playerProjectiles) if(p.active){
		                    p.position = Vector3Add(p.position, Vector3Scale(p.direction, p.speed * dt));
		                    p.timer -= dt;
		                    if(p.timer <= 0.0f) p.active = false;
		                }
		            }

	            if(!inInterior){
	            // --- Mana regen ---
	            if(manaRegenDelay <= 0.0f){
	                playerMana = fminf(playerMaxMana, playerMana + manaRegen*dt);
	            }

	                // --- Magic: projektily + status efekty ---
	                bool magicReady=false;
	                for(auto& it:inventory) if(it.isEquipped && it.category=="MAGIC") magicReady=true;
	                bool knowsFrost = InventoryHasItem(inventory, "KNIHA MRAZU");
	                bool knowsPoison = InventoryHasItem(inventory, "KNIHA JEDU");
	                if(activeSpell == 1 && !knowsFrost) activeSpell = 0;
	                if(activeSpell == 2 && !knowsPoison) activeSpell = 0;

	                float spellCost = (activeSpell==0) ? fireballCost : (activeSpell==1 ? (frostboltManaCost * (hasChanneling ? 0.80f : 1.0f) * SkillCostMult(skills[(int)Skill::DESTRUCTION].level))
	                                                             : (poisonboltManaCost * (hasChanneling ? 0.80f : 1.0f) * SkillCostMult(skills[(int)Skill::DESTRUCTION].level)));
	                float baseDmg = (activeSpell==0) ? baseFireballDamage : (activeSpell==1 ? baseFrostDamage : basePoisonDamage);
	                float spellDmg = baseDmg * (1.0f + fireballBonus);

	                if(IsKeyPressed(KEY_E) && magicReady && playerMana >= spellCost){
	                    Fireball fb;
	                    fb.position = {playerPos.x,playerPos.y+1.5f,playerPos.z};
	                    fb.direction = Vector3Normalize({sinf(playerAngle), 0.02f, cosf(playerAngle)});
	                    fb.active = true;
	                    fb.timer = 5.0f;
	                    fb.speed = 18.0f;
	                    fb.damage = spellDmg;
	                    fb.type = activeSpell;
	                    fireballs.push_back(fb);
	                    playerMana -= spellCost;
	                    manaRegenDelay = 1.2f;
	                    staminaRegenDelay = fmaxf(staminaRegenDelay, 0.4f);
	                }

	                for(auto& f:fireballs) if(f.active){
	                    f.position = Vector3Add(f.position, Vector3Scale(f.direction, f.speed * dt));
	                    f.timer -= dt;
	                    if(f.timer <= 0.0f) { f.active = false; continue; }
	                    for(auto& e:enemies) if(e.active && Vector3Distance(f.position,e.position)<2.5f){
	                        float hpBefore = e.health;
	                        e.health -= f.damage;
	                        e.isAggressive=true;
	                        f.active=false;
	                        if(f.type == 0) e.burnTimer = fmaxf(e.burnTimer, 3.6f);
	                        if(f.type == 1) e.slowTimer = fmaxf(e.slowTimer, 3.0f);
	                        if(f.type == 2) e.poisonTimer = fmaxf(e.poisonTimer, 4.2f);
	                        AddSkillXP(skills[(int)Skill::DESTRUCTION], 6.0f + f.damage*0.12f);
	                        if(e.health<=0){
	                            if(!killcam.active && hpBefore <= e.maxHealth*0.25f){
	                                killcam.active = true;
	                                killcam.duration = 1.05f;
	                                killcam.timer = killcam.duration;
	                                killcam.target = e.position;
	                                if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
	                            }
	                            e.active=false; xp+=40;
	                            enemiesKilled++;
	                            if(oldMan.hasQuest) oldMan.questProgress++;
	                            LootDrop ld = {e.position, true, {"", "NONE", 0, 0, 0, 0.0f, false, ""}, GetRandomValue(20,60), 20.0f};
	                            if(GetRandomValue(0,100) < 35) ld.item = {"ELIXIR HP","POTION",0,40,0,0.5f,false,"Obnovuje zdravie."};
	                            lootDrops.push_back(ld);
	                            pushNote("Nepriatel porazeny. Loot na zemi.");
	                        }
	                        break;
	                    }
	                }

	                // --- Player projectiles (luk) ---
	                for(auto& p : playerProjectiles) if(p.active){
	                    p.position = Vector3Add(p.position, Vector3Scale(p.direction, p.speed * dt));
	                    p.timer -= dt;
	                    if(p.timer <= 0.0f) p.active = false;
	                    if(!p.active) continue;
	                    for(auto& e:enemies) if(e.active && Vector3Distance(p.position, Vector3Add(e.position,{0,1.1f,0})) < 1.0f){
	                        float hpBefore = e.health;
	                        e.health -= p.damage;
	                        e.isAggressive = true;
	                        p.active = false;
	                        AddSkillXP(skills[(int)Skill::ARCHERY], 6.0f + p.damage*0.08f);
	                        if(e.health<=0){
	                            if(!killcam.active && hpBefore <= e.maxHealth*0.25f){
	                                killcam.active = true;
	                                killcam.duration = 1.05f;
	                                killcam.timer = killcam.duration;
	                                killcam.target = e.position;
	                                if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
	                            }
	                            e.active=false; xp+=40;
	                            enemiesKilled++;
	                            LootDrop ld = {e.position, true, {"", "NONE", 0, 0, 0, 0.0f, false, ""}, GetRandomValue(20,60), 20.0f};
	                            if(GetRandomValue(0,100) < 30) ld.item = {"ELIXIR STAMINY","POTION",0,0,0,0.4f,false,"Obnovi vytrvalost."};
	                            lootDrops.push_back(ld);
	                            pushNote("Nepriatel porazeny. Loot na zemi.");
	                        }
	                        break;
	                    }
	                }

	                // --- Enemy projectiles ---
	                for(auto& p : enemyProjectiles) if(p.active){
	                    p.position = Vector3Add(p.position, Vector3Scale(p.direction, p.speed));
	                    p.timer -= dt;
	                    if(p.timer <= 0.0f) p.active = false;
	                    float hit = Vector3Distance(p.position, Vector3Add(playerPos,{0,1.0f,0}));
	                    if(p.active && hit < 0.85f){
	                        float dmg = p.damage;
	                        if(hasIronSkin) dmg*=0.80f;
	                        if(hasEvasion) dmg*=0.90f;
	                        playerHealth -= dmg;
	                        hpRegenDelay = 2.0f;
	                        p.active = false;
	                        pushNote("Zasah!");
	                    }
	                }

		                // --- AI nepriatelia (typy + správanie) ---
		                for(auto& e:enemies) if(e.active){
		                    if(e.rangedCooldown > 0.0f) e.rangedCooldown -= dt;
		                    if(e.attackAnimTimer > 0.0f) e.attackAnimTimer = fmaxf(0.0f, e.attackAnimTimer - dt);
		                    if(e.burnTimer > 0.0f){
		                        e.burnTimer = fmaxf(0.0f, e.burnTimer - dt);
		                        e.health -= 5.0f*dt;
		                    }
		                    if(e.poisonTimer > 0.0f){
		                        e.poisonTimer = fmaxf(0.0f, e.poisonTimer - dt);
		                        e.health -= 3.0f*dt;
		                    }
		                    if(e.slowTimer > 0.0f){
		                        e.slowTimer = fmaxf(0.0f, e.slowTimer - dt);
		                    }
		                    if(e.health <= 0.0f){
		                        e.active = false;
		                        xp += 40;
		                        enemiesKilled++;
		                        LootDrop ld = {e.position, true, {"", "NONE", 0, 0, 0, 0.0f, false, ""}, GetRandomValue(20,60), 20.0f};
		                        if(GetRandomValue(0,100) < 25) ld.item = {"ELIXIR HP","POTION",0,40,0,0.5f,false,"Obnovuje zdravie."};
		                        lootDrops.push_back(ld);
		                        continue;
		                    }

		                    float dist = Vector3Distance(playerPos, e.position);
		                    if(dist > 170.0f && !e.isAggressive){
		                        // streaming/optimalizacia: daleko od hraca, netreba AI
		                        continue;
		                    }
		                    if(dist < e.detectionRange) e.isAggressive = true;

	                    if(!e.isAggressive){
	                        // idle: drobné prešľapovanie okolo startPos
	                        float tt = (float)GetTime() * 0.6f + e.startPos.x*0.1f;
	                        e.position.x = Lerp(e.position.x, e.startPos.x + sinf(tt)*1.2f, 0.02f);
	                        e.position.z = Lerp(e.position.z, e.startPos.z + cosf(tt)*1.2f, 0.02f);
	                        e.position.y = GetGroundHeight(&noiseImage, e.position);
	                        continue;
	                    }

	                    Vector3 toPlayer = Vector3Subtract(playerPos, e.position);
	                    toPlayer.y = 0.0f;
	                    float len = Vector3Length(toPlayer);
	                    if(len < 0.001f) len = 0.001f;
		                    Vector3 dir = Vector3Scale(toPlayer, 1.0f/len);
		                    Vector3 right = {dir.z, 0.0f, -dir.x};
		                    float sp = e.speed * (e.slowTimer > 0.0f ? 0.62f : 1.0f);

		                    if(e.type == 1){
		                        // archer: drž si odstup, strafe + strieľaj
		                        float desired = 9.0f;
		                        float k = Clamp((len - desired)/desired, -1.0f, 1.0f);
		                        Vector3 move = Vector3Scale(dir, k * sp * 1.15f);
		                        float strafe = sinf((float)GetTime()*1.2f + e.startPos.x*0.1f) * 0.75f;
		                        move = Vector3Add(move, Vector3Scale(right, strafe*sp));
		                        if(len < 5.5f) move = Vector3Add(move, Vector3Scale(dir, -sp*2.2f));
		                        e.position = Vector3Add(e.position, move);

	                        if(e.rangedCooldown <= 0.0f && len > 6.0f && len < 18.0f){
	                            Projectile pr;
	                            pr.position = Vector3Add(e.position, {0,1.3f,0});
	                            pr.direction = Vector3Normalize(Vector3Subtract(Vector3Add(playerPos,{0,1.2f,0}), pr.position));
	                            pr.active = true;
	                            pr.timer = 4.0f;
	                            pr.speed = 0.55f;
	                            pr.damage = 7.0f;
	                            pr.color = {200,170,60,255};
	                            enemyProjectiles.push_back(pr);
	                            e.rangedCooldown = 1.55f;
	                            e.attackAnimTimer = 0.32f;
	                        }
	                        // melee panic
	                        if(len < 2.0f){
	                            e.attackTimer += dt;
	                            if(e.attackTimer > 1.35f){
	                                float dmg = 8.0f;
	                                if(hasIronSkin) dmg*=0.80f;
	                                if(hasEvasion) dmg*=0.90f;
	                                playerHealth -= dmg;
	                                hpRegenDelay = 2.0f;
	                                e.attackTimer = 0.0f;
	                                e.attackAnimTimer = 0.30f;
	                            }
	                        } else {
	                            e.attackTimer = 0.0f;
	                        }
		                    } else if(e.type == 2){
		                        // wolf: rýchly melee
		                        float sp2 = sp * 1.7f;
		                        e.position = Vector3Add(e.position, Vector3Scale(dir, sp2));
		                        if(len < 2.1f){
	                            e.attackTimer += dt;
	                            if(e.attackTimer > 0.95f){
	                                float dmg = 9.0f;
	                                if(hasIronSkin) dmg*=0.80f;
	                                if(hasEvasion) dmg*=0.90f;
	                                playerHealth -= dmg;
	                                hpRegenDelay = 2.0f;
	                                e.attackTimer = 0.0f;
	                                e.attackAnimTimer = 0.28f;
	                            }
	                        } else {
	                            e.attackTimer = 0.0f;
	                        }
		                    } else {
		                        // bandit melee
		                        e.position = Vector3Add(e.position, Vector3Scale(dir, sp));
		                        if(len < 2.0f){
	                            e.attackTimer += dt;
	                            if(e.attackTimer > 1.2f){
	                                float dmg = 10.0f;
	                                if(hasIronSkin) dmg*=0.80f;
	                                if(hasEvasion) dmg*=0.90f;
	                                playerHealth -= dmg;
	                                hpRegenDelay = 2.0f;
	                                e.attackTimer = 0.0f;
	                                e.attackAnimTimer = 0.32f;
	                            }
	                        } else {
	                            e.attackTimer = 0.0f;
	                        }
	                    }

	                    e.position.y = GetGroundHeight(&noiseImage, e.position);
	                }
	            }
	            }

		            // --- Dungeon: magia + boss AI ---
		            if(inInterior && interiorKind==1){
		                if(manaRegenDelay <= 0.0f){
		                    playerMana = fminf(playerMaxMana, playerMana + manaRegen*dt);
		                }
		                bool magicReady=false;
		                for(auto& it:inventory) if(it.isEquipped && it.category=="MAGIC") magicReady=true;
		                bool knowsFrost = InventoryHasItem(inventory, "KNIHA MRAZU");
		                bool knowsPoison = InventoryHasItem(inventory, "KNIHA JEDU");
		                if(activeSpell == 1 && !knowsFrost) activeSpell = 0;
		                if(activeSpell == 2 && !knowsPoison) activeSpell = 0;

		                float spellCost = (activeSpell==0) ? fireballCost : (activeSpell==1 ? (frostboltManaCost * (hasChanneling ? 0.80f : 1.0f) * SkillCostMult(skills[(int)Skill::DESTRUCTION].level))
		                                                             : (poisonboltManaCost * (hasChanneling ? 0.80f : 1.0f) * SkillCostMult(skills[(int)Skill::DESTRUCTION].level)));
		                float baseDmg = (activeSpell==0) ? baseFireballDamage : (activeSpell==1 ? baseFrostDamage : basePoisonDamage);
		                float spellDmg = baseDmg * (1.0f + fireballBonus);

		                if(IsKeyPressed(KEY_E) && magicReady && playerMana >= spellCost){
		                    Fireball fb;
		                    fb.position = {playerPos.x,playerPos.y+1.5f,playerPos.z};
		                    fb.direction = Vector3Normalize({sinf(playerAngle), 0.02f, cosf(playerAngle)});
		                    fb.active = true;
		                    fb.timer = 5.0f;
		                    fb.speed = 18.0f;
		                    fb.damage = spellDmg;
		                    fb.type = activeSpell;
		                    fireballs.push_back(fb);
		                    playerMana -= spellCost;
		                    manaRegenDelay = 1.2f;
		                    staminaRegenDelay = fmaxf(staminaRegenDelay, 0.4f);
		                }
		                for(auto& f:fireballs) if(f.active){
		                    f.position = Vector3Add(f.position, Vector3Scale(f.direction, f.speed * dt));
		                    f.timer -= dt;
		                    if(f.timer <= 0.0f) { f.active = false; continue; }
		                    if(dungeonBoss.active && Vector3Distance(f.position,dungeonBoss.position)<2.8f){
		                        float hpBefore = dungeonBoss.health;
		                        dungeonBoss.health -= f.damage;
		                        if(f.type == 0) dungeonBoss.burnTimer = fmaxf(dungeonBoss.burnTimer, 3.8f);
		                        if(f.type == 1) dungeonBoss.slowTimer = fmaxf(dungeonBoss.slowTimer, 3.0f);
		                        if(f.type == 2) dungeonBoss.poisonTimer = fmaxf(dungeonBoss.poisonTimer, 4.2f);
		                        AddSkillXP(skills[(int)Skill::DESTRUCTION], 8.0f + f.damage*0.16f);
		                        f.active = false;
		                        if(dungeonBoss.health <= 0.0f){
		                            if(!killcam.active && hpBefore <= dungeonBoss.maxHealth*0.25f){
		                                killcam.active = true;
		                                killcam.duration = 1.15f;
		                                killcam.timer = killcam.duration;
		                                killcam.target = dungeonBoss.position;
		                                if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
		                            }
		                            dungeonBoss.active = false;
		                            dungeonBossDefeated = true;
		                            xp += 160;
		                            perkPoints += 1;
		                            pushNote("Boss porazeny!");
		                        }
		                    }
		                }

		                for(auto& p : playerProjectiles) if(p.active){
		                    p.position = Vector3Add(p.position, Vector3Scale(p.direction, p.speed * dt));
		                    p.timer -= dt;
		                    if(p.timer <= 0.0f) p.active = false;
		                    if(!p.active) continue;
		                    if(dungeonBoss.active && Vector3Distance(p.position, Vector3Add(dungeonBoss.position,{0,1.3f,0})) < 1.1f){
		                        float hpBefore = dungeonBoss.health;
		                        dungeonBoss.health -= p.damage;
		                        p.active = false;
		                        AddSkillXP(skills[(int)Skill::ARCHERY], 8.0f + p.damage*0.12f);
		                        if(dungeonBoss.health <= 0.0f){
		                            if(!killcam.active && hpBefore <= dungeonBoss.maxHealth*0.25f){
		                                killcam.active = true;
		                                killcam.duration = 1.15f;
		                                killcam.timer = killcam.duration;
		                                killcam.target = dungeonBoss.position;
		                                if(audioReady && killcamSfxLoaded) PlaySound(sfxKillcam);
		                            }
		                            dungeonBoss.active = false;
		                            dungeonBossDefeated = true;
		                            xp += 160;
		                            perkPoints += 1;
		                            pushNote("Boss porazeny!");
		                        }
		                    }
		                }

		                if(dungeonBoss.active){
		                    if(dungeonBoss.burnTimer > 0.0f){
		                        dungeonBoss.burnTimer = fmaxf(0.0f, dungeonBoss.burnTimer - dt);
		                        dungeonBoss.health -= 6.0f*dt;
		                    }
		                    if(dungeonBoss.poisonTimer > 0.0f){
		                        dungeonBoss.poisonTimer = fmaxf(0.0f, dungeonBoss.poisonTimer - dt);
		                        dungeonBoss.health -= 4.0f*dt;
		                    }
		                    if(dungeonBoss.slowTimer > 0.0f){
		                        dungeonBoss.slowTimer = fmaxf(0.0f, dungeonBoss.slowTimer - dt);
		                    }
		                    if(dungeonBoss.health <= 0.0f){
		                        dungeonBoss.active = false;
		                        dungeonBossDefeated = true;
		                        xp += 160;
		                        perkPoints += 1;
		                        pushNote("Boss porazeny!");
		                    }
		                    float dist = Vector3Distance(playerPos, dungeonBoss.position);
		                    Vector3 dir = Vector3Normalize(Vector3Subtract(playerPos, dungeonBoss.position));
		                    float sp = dungeonBoss.speed;
		                    if(dungeonBoss.slowTimer > 0.0f) sp *= 0.62f;
		                    dungeonBoss.position.x += dir.x * sp;
		                    dungeonBoss.position.z += dir.z * sp;
		                    dungeonBoss.position.y = 0.0f;
		                    if(dist < 2.3f){
		                        dungeonBoss.attackTimer += dt;
		                        if(dungeonBoss.attackTimer > 1.05f){
		                            float dmg = 18.0f;
	                            if(hasIronSkin) dmg*=0.80f;
	                            if(hasEvasion) dmg*=0.90f;
	                            playerHealth -= dmg;
	                            hpRegenDelay = 2.0f;
	                            dungeonBoss.attackTimer = 0.0f;
		                        }
		                    } else {
		                        dungeonBoss.attackTimer = 0.0f;
		                    }
		                }
		            }

	            // --- Jedna interakcia naraz (najblizsi ciel) ---
	            if(!inInterior && actionPressed && interactLock <= 0.0f){
	                enum ActionType {ACT_NONE, ACT_DOOR, ACT_TEMPLE, ACT_CHEST, ACT_LOOT, ACT_DUNGEON, ACT_NPC_OLD, ACT_NPC_GUARD, ACT_NPC_SMITH, ACT_NPC_INN, ACT_NPC_ALCH, ACT_NPC_HUNTER};
                ActionType bestType = ACT_NONE;
                int bestIndex = -1;
                float bestDist = 999.0f;

	                for(int hi=0; hi<(int)cityHouses.size(); hi++){
	                    const House& h = cityHouses[hi];
	                    Vector3 door = GetHouseDoorPos(h);
	                    float d = Vector3Distance(playerPos, door);
	                    if(d < bestDist && d < 1.0f){
	                        bestDist = d; bestType = ACT_DOOR; bestIndex = hi;
	                    }
	                }
	                {
	                    float d = Vector3Distance(playerPos, templeDoorPos);
	                    if(d < bestDist && d < 1.2f){
	                        bestDist = d; bestType = ACT_TEMPLE; bestIndex = 0;
	                    }
	                }
                for(int i=0;i<(int)chests.size(); i++){
                    if(!chests[i].active) continue;
                    float d = Vector3Distance(playerPos, chests[i].position);
                    if(d < bestDist && d < 1.0f){
                        bestDist = d; bestType = ACT_CHEST; bestIndex = i;
                    }
                }
                for(int i=0;i<(int)lootDrops.size(); i++){
                    if(!lootDrops[i].active) continue;
                    float d = Vector3Distance(playerPos, lootDrops[i].position);
                    if(d < bestDist && d < 1.0f){
                        bestDist = d; bestType = ACT_LOOT; bestIndex = i;
                    }
                }
                {
                    float d = Vector3Distance(playerPos, dungeonEntrance);
                    if(d < bestDist && d < 2.2f){
                        bestDist = d; bestType = ACT_DUNGEON; bestIndex = 0;
                    }
                }
                if(talkCooldown<=0.0f){
                    float dOld = Vector3Distance(playerPos, oldMan.position);
                    if(dOld < bestDist && dOld < 1.0f){
                        bestDist = dOld; bestType = ACT_NPC_OLD; bestIndex = 0;
                    }
                    float dGuard = Vector3Distance(playerPos, guard.position);
                    if(dGuard < bestDist && dGuard < 1.0f){
                        bestDist = dGuard; bestType = ACT_NPC_GUARD; bestIndex = 0;
                    }
                    float dSmith = Vector3Distance(playerPos, blacksmith.position);
                    if(dSmith < bestDist && dSmith < 1.0f){
                        bestDist = dSmith; bestType = ACT_NPC_SMITH; bestIndex = 0;
                    }
                    float dInn = Vector3Distance(playerPos, innkeeper.position);
                    if(dInn < bestDist && dInn < 1.0f){
                        bestDist = dInn; bestType = ACT_NPC_INN; bestIndex = 0;
                    }
                    float dAlch = Vector3Distance(playerPos, alchemist.position);
                    if(dAlch < bestDist && dAlch < 1.0f){
                        bestDist = dAlch; bestType = ACT_NPC_ALCH; bestIndex = 0;
                    }
                    float dHunter = Vector3Distance(playerPos, hunter.position);
                    if(dHunter < bestDist && dHunter < 1.0f){
                        bestDist = dHunter; bestType = ACT_NPC_HUNTER; bestIndex = 0;
                    }
                }

		                switch(bestType){
		                    case ACT_DOOR: {
		                        if(cityHouses[bestIndex].canEnter){
		                            inInterior = true;
		                            interiorKind = 0;
	                            interiorLevel = 0;
	                            {
	                                const House& hh = cityHouses[bestIndex];
	                                float dCastle = Vector3Distance(hh.position, castleCenter);
	                                if(dCastle < 0.25f){
	                                    interiorScene = INT_CASTLE;
	                                } else if(hh.shopIndex == 0){
	                                    interiorScene = INT_BLACKSMITH;
	                                } else if(hh.shopIndex == 1){
	                                    interiorScene = INT_ALCHEMIST;
	                                } else if(hh.shopIndex == 4){
	                                    interiorScene = INT_INN;
	                                } else if(hh.shopIndex == 2){
	                                    interiorScene = INT_HUNTER;
	                                } else if(hh.shopIndex == 3){
	                                    interiorScene = INT_GENERAL;
	                                } else {
	                                    interiorScene = INT_HOUSE;
	                                }
	                            }
	                            interiorReturn = playerPos;
	                            playerPos = (interiorScene == INT_CASTLE) ? Vector3{0,0,9.0f} : Vector3{0,0,4.0f};
	                            playerAngle = 0.0f;
	                            activeInterior = bestIndex;
		                            pushNote("Vstupil si do interieru.");
		                        } else {
		                            StartLockpick(LockTargetType::HOUSE, bestIndex, 1);
		                        }
		                    } break;
		                    case ACT_TEMPLE: {
		                        inInterior = true;
		                        interiorKind = 0;
		                        interiorScene = INT_TEMPLE;
		                        interiorLevel = 0;
		                        interiorReturn = playerPos;
		                        playerPos = {0.0f, 0.0f, 4.0f};
		                        playerAngle = 0.0f;
		                        interactLock = 0.6f;
		                        pushNote("Vstupil si do chrámu.");
		                        if(templeQuestIdx >= 0 && !quests[templeQuestIdx].active && !quests[templeQuestIdx].completed){
		                            quests[templeQuestIdx].active = true;
		                            pushNote("Quest prijaty: " + quests[templeQuestIdx].title);
		                        }
		                    } break;
		                    case ACT_CHEST: {
		                        if(chests[bestIndex].locked){
		                            StartLockpick(LockTargetType::CHEST, bestIndex, chests[bestIndex].lockDifficulty);
		                        } else {
	                            inventory.push_back(chests[bestIndex].content);
	                            chests[bestIndex].active=false;
	                            openedChests++;
	                            pushNote("Nasiel si predmet v truhlici.");
	                        }
	                    } break;
                    case ACT_LOOT: {
                        lootDrops[bestIndex].active=false;
                        if(lootDrops[bestIndex].gold>0){ gold += lootDrops[bestIndex].gold; }
                        if(!lootDrops[bestIndex].item.name.empty()) inventory.push_back(lootDrops[bestIndex].item);
                        lootPicked++;
                        pushNote("Loot ziskany.");
                    } break;
	                    case ACT_DUNGEON: {
	                        inInterior = true;
	                        interiorKind = 1;
	                        interiorReturn = playerPos;
	                        playerPos = {0,0,10.0f};
	                        playerAngle = PI;
	                        interactLock = 0.6f;
	                        pushNote("Vstupil si do jaskyne.");
	                        dungeonGateOpen = dungeonBossDefeated;
	                        dungeonLeverPulled = false;
	                        if(!dungeonBossDefeated){
	                            dungeonBoss.active = true;
	                            dungeonBoss.position = {0,0,-28.0f};
	                            dungeonBoss.startPos = dungeonBoss.position;
	                            dungeonBoss.health = dungeonBoss.maxHealth;
	                            dungeonRewardTaken = false;
	                        }
	                    } break;
                    case ACT_NPC_OLD: {
                        oldMan.isTalking=true;
                        oldManNode = 0;
                        talkCooldown=0.4f;
                    } break;
                    case ACT_NPC_GUARD: {
                        guard.isTalking=true;
                        guardNode = 0;
                        talkCooldown=0.4f;
                    } break;
                    case ACT_NPC_SMITH: {
                        blacksmith.isTalking=true;
                        smithNode = 0;
                        talkCooldown=0.4f;
                    } break;
                    case ACT_NPC_INN: {
                        innkeeper.isTalking = true;
                        innNode = 0;
                        talkCooldown = 0.4f;
                    } break;
                    case ACT_NPC_ALCH: {
                        alchemist.isTalking = true;
                        alchNode = 0;
                        talkCooldown = 0.4f;
                    } break;
                    case ACT_NPC_HUNTER: {
                        hunter.isTalking = true;
                        hunterNode = 0;
                        talkCooldown = 0.4f;
                    } break;
                    default: break;
                }
            }

	            // --- Obchod ---
	            if(IsKeyPressed(KEY_S)){
	                if(inInterior && interiorKind==0){
	                    if(interiorScene==INT_BLACKSMITH || interiorScene==INT_ALCHEMIST || interiorScene==INT_HUNTER || interiorScene==INT_GENERAL || interiorScene==INT_INN){
	                        int sidx = 3;
	                        if(interiorScene==INT_BLACKSMITH) sidx = 0;
	                        else if(interiorScene==INT_ALCHEMIST) sidx = 1;
	                        else if(interiorScene==INT_HUNTER) sidx = 2;
	                        else if(interiorScene==INT_GENERAL) sidx = 3;
	                        else if(interiorScene==INT_INN) sidx = 4;
		                        activeShopItems = shopCatalogs[sidx];
		                        activeShopName = shopNames[sidx];
		                        activeShopIndex = sidx;
		                        showShop = true;
		                        selectedShopIdx = -1;
		                        pushNote("Otvoril si obchod.");
	                    } else {
	                        pushNote("Tu nie je obchod.");
	                    }
	                } else if(!inInterior){
	                    float smithDist = Vector3Distance(playerPos,blacksmith.position);
	                    if(smithDist < 1.0f){
		                        activeShopItems = shopCatalogs[0];
	                        activeShopName = shopNames[0];
	                        activeShopIndex = 0;
	                        showShop = true;
	                        selectedShopIdx = -1;
	                        pushNote("Otvoril si obchod.");
                    } else {
                        for(int hi=0; hi<(int)cityHouses.size(); hi++){
                            Vector3 door = GetHouseDoorPos(cityHouses[hi]);
                            if(Vector3Distance(playerPos, door) < 1.0f){
	                                int sidx = cityHouses[hi].shopIndex;
	                                activeShopItems = shopCatalogs[sidx];
	                                activeShopName = shopNames[sidx];
	                                activeShopIndex = sidx;
	                                showShop = true;
	                                selectedShopIdx = -1;
	                                pushNote("Otvoril si obchod.");
                                break;
                            }
                        }
                    }
                }
            }

            // --- Crafting/Alchemy ---
            if(IsKeyPressed(KEY_C)){
                if(inInterior && interiorKind==0){
                    if(interiorScene==INT_BLACKSMITH){
                        showCrafting = true;
                        craftingMode = 0;
                        selectedRecipeIdx = -1;
                        pushNote("Kovac: crafting.");
                    } else if(interiorScene==INT_ALCHEMIST){
                        showCrafting = true;
                        craftingMode = 1;
                        selectedRecipeIdx = -1;
                        pushNote("Alchymista: alchemy.");
                    }
                } else if(!inInterior){
                    float smithDist = Vector3Distance(playerPos, blacksmith.position);
                    if(smithDist < 1.2f){
                        showCrafting = true;
                        craftingMode = 0;
                        selectedRecipeIdx = -1;
                        pushNote("Kovac: crafting.");
                    } else {
                        bool opened = false;
                        for(int hi=0; hi<(int)cityHouses.size(); hi++){
                            if(cityHouses[hi].shopIndex != 1) continue; // alchymista
                            Vector3 door = GetHouseDoorPos(cityHouses[hi]);
                            if(Vector3Distance(playerPos, door) < 1.0f){
                                showCrafting = true;
                                craftingMode = 1;
                                selectedRecipeIdx = -1;
                                pushNote("Alchymista: alchemy.");
                                opened = true;
                                break;
                            }
                        }
                        if(!opened){
                            pushNote("Nie si pri kovacovi ani alchymistovi.");
                        }
                    }
                }
            }

            // --- Random encounter ---
	            if(enableEncounters && !inInterior && encounterTimer<=0.0f){
	                if(GetRandomValue(0,100) < 35){
	                    float angle = (float)GetRandomValue(0,360) * DEG2RAD;
	                    float dist = (float)GetRandomValue(20,35);
	                    Vector3 sp = {playerPos.x + cosf(angle)*dist, 0, playerPos.z + sinf(angle)*dist};
	                    sp.y = GetGroundHeight(&noiseImage, sp);
	                    int et = (GetRandomValue(0,100) < 35) ? 1 : 0; // archer sometimes
	                    if(GetRandomValue(0,100) < 30) et = 2; // wolf pack sometimes
	                    float hp = (et==2) ? 70.0f : 100.0f;
	                    float spd = (et==2) ? 0.085f : 0.06f;
	                    enemies.push_back({sp,sp,hp,hp,true,spd,22,false,0, et, 0.0f, 0.0f});
	                    if(GetRandomValue(0,100) < 40){
	                        Vector3 sp2 = {sp.x + 2.5f, sp.y, sp.z + 2.5f};
	                        int et2 = (et==2) ? 2 : ((GetRandomValue(0,100) < 20) ? 1 : 0);
	                        float hp2 = (et2==2) ? 60.0f : 90.0f;
	                        float spd2 = (et2==2) ? 0.090f : 0.07f;
	                        enemies.push_back({sp2,sp2,hp2,hp2,true,spd2,20,false,0, et2, 0.0f, 0.0f});
	                    }
	                    pushNote("Nahodny stret!");
	                }
	                encounterTimer = (float)GetRandomValue(8,16);
	            }

            // --- Patrol NPC (strazca) ---
            if(!inInterior && guardPatrol.size() > 0){
                Vector3 target = guardPatrol[guardPatrolIdx];
                Vector3 toTarget = Vector3Subtract(target, patrolGuard.position);
                float dist = Vector3Length(toTarget);
                if(dist < 0.6f){
                    guardPatrolIdx = (guardPatrolIdx + 1) % guardPatrol.size();
                } else {
                    Vector3 dir = Vector3Normalize(toTarget);
                    patrolGuard.position.x += dir.x * guardSpeed;
                    patrolGuard.position.z += dir.z * guardSpeed;
                    patrolGuard.position.y = GetGroundHeight(&noiseImage, patrolGuard.position);
                }
            }
            // straze na hradbach (jemny pohyb)
            for(int i=0;i<(int)wallGuards.size(); i++){
                float t = (float)GetTime()*0.6f + wallGuardPhase[i]*6.0f;
                wallGuards[i].x += cosf(t)*0.002f;
                wallGuards[i].z += sinf(t)*0.002f;
            }

            // --- Leveling ---
            if(xp>=xpToNextLevel){
                xp-=xpToNextLevel;
                level++;
                xpToNextLevel+=50;
                baseMaxHealth+=20.0f;
                playerMaxHealth = baseMaxHealth + (hasHpBoost ? 30.0f : 0.0f);
                playerHealth = playerMaxHealth;
                perkPoints += 2;
                pushNote("Novy level!");
            }

        // --- Quest progress (central) ---
        int discoveredCamps = 0;
        for(const auto& p : worldPOIs){
            if(p.discovered && p.name.find("TABOR") == 0) discoveredCamps++;
        }

        for(auto& q : quests){
            if(!q.active || q.completed) continue;
            if(q.id == "kill_orcs_3" || q.id == "kill_orcs_5"){
                int cur = enemiesKilled;
                q.objective = TextFormat("Zabij nepriatelov: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "kill_orcs_1_book"){
                int cur = enemiesKilled;
                q.objective = TextFormat("Zabij nepriatela pre knihu: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "gold_200"){
                int cur = gold;
                q.objective = TextFormat("Nazbieraj gold: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "open_chests_2"){
                int cur = openedChests;
                q.objective = TextFormat("Otvor truhlice: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "discover_camps_2"){
                int cur = discoveredCamps;
                q.objective = TextFormat("Objav tabory: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "pick_locks_1"){
                int cur = locksPicked;
                q.objective = TextFormat("Odomkni zamky: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            } else if(q.id == "smith_upgrade"){
                int cur = weaponUpgradesDone;
                q.objective = TextFormat("Vylepsi zbran u kovaca: %i/%i", std::min(cur, q.target), q.target);
                if(cur >= q.target) q.completed = true;
            }
        }

        // --- Story quest (Skyrim-like) ---
        Quest& storyQ = quests[storyQuestIdx >= 0 ? storyQuestIdx : (int)quests.size()-1];
        if(storyQuestIdx >= 0 && !storyIntroShown && !intro.active){
            storyIntroShown = true;
            storyQ.active = true;
            storyStage = 0;
            storyQ.objective = "Porozpravaj sa so strazcom pri brane.";
            pushNote("Prisiel si k mestu. Strazca ta zastavi pri brane.");
        }
        if(storyQ.active && !storyQ.completed){
            bool hasSigil = InventoryHasItem(inventory, storySigilName);
            if(storyStage == 1 && hasSigil){
                storyStage = 2;
                storyQ.objective = "Mas Sigil Straze. Vrat sa za Starym Pustovnikom pred branou.";
                pushNote("Citis, ako Sigil vibruje. Mal by si sa vratit.");
            }
            if(storyStage >= 3 && gateUnlocked){
                storyStage = 4;
                storyQ.completed = true;
            }
        }
        if(storyQuestIdx >= 0) quests[storyQuestIdx].step = storyStage;

        // --- Main quest: Srdce Jaskyne ---
        if(caveQuestIdx >= 0){
            Quest& caveQ = quests[caveQuestIdx];
            if(caveQ.active && !caveQ.completed){
                if(caveStage == 1 && dungeonBossDefeated){
                    caveStage = 2;
                    caveQ.objective = "Boss v jaskyni padol. Vrat sa za Hostinskym v meste.";
                    pushNote("Z jaskyne sa vytraca tlak. Niekto bude chciet pocut spravu.");
                }
            }
        }
        if(caveQuestIdx >= 0) quests[caveQuestIdx].step = caveStage;

	            // --- Quest rewards (raz) ---
		            for(auto& q: quests){
		                if(q.active && q.completed && !q.rewardGiven){
		                    gold += q.rewardGold;
	                    xp += q.rewardXp;
	                    perkPoints += q.rewardPerkPoints;
	                    // reputacia (factions)
	                    if(q.id == "main_story") factionRep[(int)Faction::GUARD] = std::min(100, factionRep[(int)Faction::GUARD] + 12);
	                    if(q.id == "smith_upgrade") factionRep[(int)Faction::SMITHS] = std::min(100, factionRep[(int)Faction::SMITHS] + 8);
	                    if(q.id == "deliver_herbs") factionRep[(int)Faction::ALCHEMISTS] = std::min(100, factionRep[(int)Faction::ALCHEMISTS] + 8);
	                    if(q.id == "discover_camps_2") factionRep[(int)Faction::HUNTERS] = std::min(100, factionRep[(int)Faction::HUNTERS] + 7);
	                    if(q.id == "pick_locks_1") factionRep[(int)Faction::GUARD] = std::min(100, factionRep[(int)Faction::GUARD] + 6);
	                    if(q.id == "cave_heart") factionRep[(int)Faction::TOWNSFOLK] = std::min(100, factionRep[(int)Faction::TOWNSFOLK] + 10);
	                    if(q.id == "temple_blessing") factionRep[(int)Faction::TOWNSFOLK] = std::min(100, factionRep[(int)Faction::TOWNSFOLK] + 5);
	                    if(q.title.find("Kniha Ohna")!=std::string::npos){
	                        inventory.push_back({"KNIHA OHNA","BOOK",0,0,0,2.0f,false,"Po precitani odomkne Fireball (E)."});
	                    }
	                    if(q.title.find("Straz: Zamky")!=std::string::npos){
	                        lockpicks += 2;
	                    }
	                    q.rewardGiven = true;
	                }
                if(q.completed && !q.notified){
                    pushNote("Quest splneny: " + q.title);
                    q.notified = true;
	                }
	            }
	        
	        // Kamera nech vždy sleduje hráča
	        camera.position={playerPos.x-sinf(playerAngle)*8.0f,playerPos.y+4.0f,playerPos.z-cosf(playerAngle)*8.0f};
	        camera.target={playerPos.x,playerPos.y+1.5f,playerPos.z};

        // --- NPC dialog (Skyrim-like story + volby) ---
        auto pickChoice = [&]()->int{
            if(IsKeyPressed(KEY_ONE)) return 1;
            if(IsKeyPressed(KEY_TWO)) return 2;
            if(IsKeyPressed(KEY_THREE)) return 3;
            if(IsKeyPressed(KEY_FOUR)) return 4;
            return 0;
        };
        auto closeTalk = [&](){
            oldMan.isTalking = false;
            guard.isTalking = false;
            blacksmith.isTalking = false;
            innkeeper.isTalking = false;
            alchemist.isTalking = false;
            hunter.isTalking = false;
            talkCooldown = 0.4f;
        };

	        Quest& storyQDlg = quests[storyQuestIdx >= 0 ? storyQuestIdx : (int)quests.size()-1];
	        bool hasSigil = InventoryHasItem(inventory, storySigilName);

        if(oldMan.isTalking){
            dlgNpcName = oldMan.name;
            int choice = pickChoice();
            auto canTake = [&](int qi)->bool{
                if(qi < 0 || qi >= storyQuestIdx) return false;
                if(quests[qi].active || quests[qi].completed) return false;
                // prereq: "Zabij Orkov" az po "Kniha Ohna"
                if(qi==0 && !quests[1].completed) return false;
                return true;
            };
            auto takeQuest = [&](int qi){
                if(!canTake(qi)) return;
                quests[qi].active = true;
                pushNote("Quest prijaty: " + quests[qi].title);
            };

            // main menu
            if(oldManNode == 0){
                if(storyStage==0) dlgNpcLine = "Ticho pri brane nie je nahoda. Ak mas odvahu, vypocuj ma.";
                else if(storyStage==1) dlgNpcLine = "Runy v ruinach este nespia. Sigil Straze na teba caka.";
                else if(storyStage==2) dlgNpcLine = "Vzduch okolo teba znie... nesies nieco stare. Ukaz mi to.";
                else if(storyStage==3) dlgNpcLine = "Dobre. Teraz za strazcom. Brana sa neotvori bez rozkazu.";
                else dlgNpcLine = "Echoes sa ozvu aj v kameni. Mesto uz pozna tvoje meno, dobrodruh.";

                dlgOptions = {"Kto si?", "Co sa deje s branou?", "Mas pre mna pracu?", "Dovidenia."};
                dlgEnabled = {true,true,true,true};

                if(choice==1) oldManNode = 1;
                if(choice==2) oldManNode = 2;
                if(choice==3) oldManNode = 10;
                if(choice==4) closeTalk();
            } else if(oldManNode == 1){
                dlgNpcLine = "Volaju ma Pustovnik. Kedysi som strazil tieto mury. Dnes uz len pocuvam, co sa v noci ozve zo zeme.";
                dlgOptions = {"Spat.", "Nieco o meste?", "Mas pre mna pracu?", "Dovidenia."};
                dlgEnabled = {true,true,true,true};
                if(choice==1) oldManNode = 0;
                if(choice==2) oldManNode = 2;
                if(choice==3) oldManNode = 10;
                if(choice==4) closeTalk();
            } else if(oldManNode == 2){
	                if(storyStage==0){
	                    dlgNpcLine = "Strazca nema rozkaz otvorit. Rozkaz lezi v ruinach: Sigil Straze. Prines ho a ja ti dam slovo, ktore branu pohne.";
	                    dlgOptions = {"Prijimam ulohu.", "Kde su ruiny?", "Nie teraz.", "Spat."};
	                    dlgEnabled = {!storyQDlg.completed, true, true, true};
	                    if(choice==1 && dlgEnabled[0]){
	                        storyQDlg.active = true;
	                        storyStage = 1;
	                        storyQDlg.objective = "Najdi Sigil Straze v starych ruinach za mestom.";
	                        pushNote("Quest prijaty: " + storyQDlg.title);
	                        oldManNode = 0;
	                    }
                    if(choice==2) oldManNode = 20;
                    if(choice==3) oldManNode = 0;
                    if(choice==4) oldManNode = 0;
                } else if(storyStage==1){
                    dlgNpcLine = "Najdi Sigil. Ked ho vezmes do ruk, bude vibrovat ako dych v noci.";
                    dlgOptions = {"Pripomen mi cestu.", "Mas inu pracu?", "Spat.", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1) oldManNode = 20;
                    if(choice==2) oldManNode = 10;
                    if(choice==3) oldManNode = 0;
                    if(choice==4) closeTalk();
                } else if(storyStage==2){
                    dlgNpcLine = "Vidim to v tvojom pohlade. Mas Sigil Straze?";
                    dlgOptions = {"Mam ho.", "Co to vlastne je?", "Spat.", "Dovidenia."};
                    dlgEnabled = {hasSigil, true, true, true};
	                    if(choice==1 && hasSigil){
	                        InventoryRemoveItemOnce(inventory, storySigilName);
	                        storyStage = 3;
	                        storyQDlg.objective = "Zanes spravu strazcovi pri brane. Ukaz mu povolenie Pustovnika.";
	                        pushNote("Pustovnik: \"Teraz za strazcom.\"");
	                        oldManNode = 0;
	                    }
                    if(choice==2) oldManNode = 21;
                    if(choice==3) oldManNode = 0;
                    if(choice==4) closeTalk();
                } else if(storyStage==3){
                    dlgNpcLine = "Nezabudni. Strazca pri brane. Povedz mu, ze Pustovnik drzi svoje slovo.";
                    dlgOptions = {"Rozumiem.", "Mas inu pracu?", "Spat.", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1) closeTalk();
                    if(choice==2) oldManNode = 10;
                    if(choice==3) oldManNode = 0;
                    if(choice==4) closeTalk();
                } else {
                    dlgNpcLine = "Ak chces, mozes sa postarat o veci, na ktore uz mesto nema ruky.";
                    dlgOptions = {"Mas pre mna pracu?", "Spat.", "Dovidenia.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1) oldManNode = 10;
                    if(choice==2) oldManNode = 0;
                    if(choice==3) closeTalk();
                }
            } else if(oldManNode == 20){
                dlgNpcLine = "Severozapadne od mesta. Uvidis kamene popraskane ako zuby. Tam lezi Sigil.";
                dlgOptions = {"Rozumiem.", "Spat.", "Dovidenia.", ""};
                dlgEnabled = {true,true,true,false};
                if(choice==1 || choice==2) oldManNode = 0;
                if(choice==3) closeTalk();
            } else if(oldManNode == 21){
                dlgNpcLine = "Je to znak starej straze. Bez neho sa brana neotvori, ani keby si mal zlato aj mece.";
                dlgOptions = {"Rozumiem.", "Spat.", "Dovidenia.", ""};
                dlgEnabled = {true,true,true,false};
                if(choice==1 || choice==2) oldManNode = 0;
                if(choice==3) closeTalk();
            } else if(oldManNode == 10){
                dlgNpcLine = "Vyber si ulohu. Niektore su zamknute, kym sa neosvedcis.";
                dlgOptions = {
                    quests[1].title + (canTake(1) ? "" : " (TAKEN)"),
                    quests[4].title + (canTake(4) ? "" : " (TAKEN)"),
                    quests[2].title + (canTake(2) ? "" : " (TAKEN)"),
                    "Spat."
                };
                dlgEnabled = {canTake(1), canTake(4), canTake(2), true};
                if(choice==1) takeQuest(1);
                if(choice==2) takeQuest(4);
                if(choice==3) takeQuest(2);
                if(choice==4) oldManNode = 0;
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(guard.isTalking){
            dlgNpcName = guard.name;
            int choice = pickChoice();

            if(guardNode == 0){
                if(gateUnlocked){
                    dlgNpcLine = "Brana je otvorena. Drz sa pravidiel a nebude problem.";
                    dlgOptions = {"Dovidenia.", "", "", ""};
                    dlgEnabled = {true,false,false,false};
                    if(choice==1) closeTalk();
	                } else if(storyStage >= 3 && storyQDlg.active && !storyQDlg.completed){
                    dlgNpcLine = "Halt. Vidim, ze si sa vratil. Mas povolenie?";
                    dlgOptions = {"Mam povolenie. Otvor branu.", "Preco bola zatvorena?", "Kto tu veli?", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1){
                        gateUnlocked = true;
                        pushNote("Strazca otvara branu.");
                        closeTalk();
                    }
                    if(choice==2) guardNode = 2;
                    if(choice==3) guardNode = 3;
                    if(choice==4) closeTalk();
                } else {
                    dlgNpcLine = "Stoj! Brana je zatvorena. Bez rozkazu nikoho nepustime dnu.";
                    dlgOptions = {"Potrebujem vstup.", "Preco je brana zatvorena?", "Kto tu veli?", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1) guardNode = 1;
                    if(choice==2) guardNode = 2;
                    if(choice==3) guardNode = 3;
                    if(choice==4) closeTalk();
                }
            } else if(guardNode == 1){
                dlgNpcLine = "Rozumiem. Ale pravidla su pravidla. Ak chces dovnutra, najdi niekoho, kto ti da slovo.";
                dlgOptions = {"Rozumiem.", "Kto mi da povolenie?", "Spat.", "Dovidenia."};
                dlgEnabled = {true,true,true,true};
                if(choice==1) guardNode = 0;
                if(choice==2){
                    dlgNpcLine = "Ten pustovnik pred branou. Kedysi bol jeden z nas. Ak on povie 'ano', ja otvorim.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
	                    if(storyStage==0 && storyQDlg.active && !storyQDlg.completed){
	                        storyQDlg.objective = "Porozpravaj sa so Starym Pustovnikom pred branou.";
	                        pushNote("Strazca: \"Skus pustovnika pred branou.\"");
	                    }
                    if(choice==1) guardNode = 0;
                    if(choice==2) closeTalk();
                }
                if(choice==3) guardNode = 0;
                if(choice==4) closeTalk();
            } else if(guardNode == 2){
                dlgNpcLine = "Mali sme v noci problemy. Niekto sa snazil otvorit branu zvnutra. Kym sa to nevysetri, nikto neprejde.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) guardNode = 0;
                if(choice==2) closeTalk();
            } else if(guardNode == 3){
                dlgNpcLine = "Ja len drzim branu. Velitel je v meste. A ten, kto drzi rozkaz... ten drzi kluc.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) guardNode = 0;
                if(choice==2) closeTalk();
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(blacksmith.isTalking){
            dlgNpcName = blacksmith.name;
            int choice = pickChoice();

            if(smithNode == 0){
                dlgNpcLine = "Vitaj. Oceľ ta nesklame, ak sa o nu staras. Co potrebujes?";
                dlgOptions = {"Ukaz mi tovar.", "Povedz mi o Sigile.", "Mas pre mna pracu?", "Dovidenia."};
                dlgEnabled = {true,true,true,true};
                if(choice==1){
                    activeShopItems = shopCatalogs[0];
                    activeShopName = shopNames[0];
                    showShop = true;
                    selectedShopIdx = -1;
                    pushNote("Otvoril si obchod.");
                    closeTalk();
                }
                if(choice==2) smithNode = 1;
                if(choice==3) smithNode = 2;
                if(choice==4) closeTalk();
            } else if(smithNode == 1){
                dlgNpcLine = "Sigil? Kedysi ho nosili strazcovia. Vraj vie branu 'pocut'. Ak ho mas, drz ho daleko od vody.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) smithNode = 0;
                if(choice==2) closeTalk();
            } else if(smithNode == 2){
                Quest* smithQ = (smithQuestIdx >= 0) ? &quests[smithQuestIdx] : nullptr;
                if(!smithQ){
                    dlgNpcLine = "Dnes nemam pracu.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) smithNode = 0;
                    if(choice==2) closeTalk();
                } else if(smithQ->completed){
                    dlgNpcLine = "Dobra praca. Tvoja zbran znie inak. Pocut to.";
                    dlgOptions = {"Spat.", "Mas nejaku radu?", "Dovidenia.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1) smithNode = 0;
                    if(choice==2) smithNode = 3;
                    if(choice==3) closeTalk();
                } else if(!smithQ->active){
                    dlgNpcLine = "Mam pre teba zakazku. Vylepsi si zbran. Potom mi ukaz, co si spravil.";
                    dlgOptions = {"Berem to.", "Mas nejaku radu?", "Spat.", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1){
                        smithQ->active = true;
                        pushNote("Quest prijaty: " + smithQ->title);
                        smithNode = 0;
                    }
                    if(choice==2) smithNode = 3;
                    if(choice==3) smithNode = 0;
                    if(choice==4) closeTalk();
                } else {
                    dlgNpcLine = "Chod k vyhni (C) a urob upgrade. Potom mi to ukaz.";
                    dlgOptions = {"Spat.", "Mas nejaku radu?", "Dovidenia.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1) smithNode = 0;
                    if(choice==2) smithNode = 3;
                    if(choice==3) closeTalk();
                }
            } else if(smithNode == 3){
                dlgNpcLine = "Zbieraj zeleznu rudu. A drz kozu v suchu. A ked sa ti zlomi lockpick, neplac. Vymen ho.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) smithNode = 0;
                if(choice==2) closeTalk();
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(innkeeper.isTalking){
            dlgNpcName = innkeeper.name;
            int choice = pickChoice();
            Quest* caveQ = (caveQuestIdx >= 0) ? &quests[caveQuestIdx] : nullptr;

            if(innNode == 0){
                if(caveQ && caveQ->active && caveStage == 2 && dungeonBossDefeated && !caveQ->completed){
                    dlgNpcLine = "Takze je to pravda. Ten tlak... uz to nie je citit. Dobre. Mesto ti dluzi.";
                    dlgOptions = {"Tu je sprava.", "Co bolo v jaskyni?", "Dovidenia.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1){
                        caveQ->completed = true;
                        pushNote("Hostinsky: \"Mas moje slovo.\"");
                        closeTalk();
                    }
                    if(choice==2) innNode = 2;
                    if(choice==3) closeTalk();
                } else {
                    dlgNpcLine = "Hostinec pocuje viac, nez vidi. Ak mas otazky, mam odpovede. Za cenu.";
                    dlgOptions = {"Novinky.", "Mas pre mna pracu?", "Co vies o jaskyni?", "Dovidenia."};
                    dlgEnabled = {true,true,true,true};
                    if(choice==1) innNode = 1;
                    if(choice==2) innNode = 10;
                    if(choice==3) innNode = 2;
                    if(choice==4) closeTalk();
                }
            } else if(innNode == 1){
                dlgNpcLine = "Straze su nervozne. Ludia su nervozni. A ked su nervozni, piju. Takze... ja som spokojny.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) innNode = 0;
                if(choice==2) closeTalk();
            } else if(innNode == 2){
                dlgNpcLine = "Jaskyna na okraji sveta. Kazdy vie, kde je, ale nikto nechce ist prvy. Ak to vycistis, budes mat miesto pri ohni.";
                dlgOptions = {"Rozumiem.", "Spat.", "Dovidenia.", ""};
                dlgEnabled = {true,true,true,false};
                if(choice==1 || choice==2) innNode = 0;
                if(choice==3) closeTalk();
            } else if(innNode == 10){
                if(!caveQ){
                    dlgNpcLine = "Dnes nemam nic. Skus zajtra.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) innNode = 0;
                    if(choice==2) closeTalk();
                } else if(caveQ->completed){
                    dlgNpcLine = "Urobil si viac, nez som cakal. Dnes uz len oddychuj.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) innNode = 0;
                    if(choice==2) closeTalk();
                } else if(!gateUnlocked){
                    dlgNpcLine = "Najprv sa dostan dnu do mesta. Brana je zatvorena.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) innNode = 0;
                    if(choice==2) closeTalk();
                } else if(!caveQ->active){
                    dlgNpcLine = "Dobre. Chcem spravu z jaskyne. Ak je to pravda, niekto tam dolu dycha.";
                    dlgOptions = {"Berem to.", "Neskor.", "Spat.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1){
                        caveQ->active = true;
                        caveStage = 1;
                        caveQ->objective = "Vydaj sa do jaskyne (JASKYNA 1) a poraz to, co je v hlbine.";
                        pushNote("Quest prijaty: " + caveQ->title);
                        innNode = 0;
                    }
                    if(choice==2) innNode = 0;
                    if(choice==3) innNode = 0;
                } else {
                    dlgNpcLine = "Chod. A vrat sa zive.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) innNode = 0;
                    if(choice==2) closeTalk();
                }
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(alchemist.isTalking){
            dlgNpcName = alchemist.name;
            int choice = pickChoice();
            Quest* herbQ = (herbQuestIdx >= 0) ? &quests[herbQuestIdx] : nullptr;
            int haveFlower = InventoryCountItem(inventory, "MODRY KVET");
            int haveWheat = InventoryCountItem(inventory, "PSENICA");

            if(alchNode == 0){
                dlgNpcLine = "Ak chces prezit v tychto koncinach, nauc sa varit. A nauc sa zbierat.";
                dlgOptions = {"Mas pre mna pracu?", "Ako funguje alchymia?", "Spat.", "Dovidenia."};
                dlgEnabled = {true,true,true,true};
                if(choice==1) alchNode = 10;
                if(choice==2) alchNode = 1;
                if(choice==3) alchNode = 0;
                if(choice==4) closeTalk();
            } else if(alchNode == 1){
                dlgNpcLine = "Zbieraj suroviny vo svete. Pri mne stlac C a uvaris elixiry. Nie je to magia. Je to disciplina.";
                dlgOptions = {"Rozumiem.", "Spat.", "Dovidenia.", ""};
                dlgEnabled = {true,true,true,false};
                if(choice==1 || choice==2) alchNode = 0;
                if(choice==3) closeTalk();
            } else if(alchNode == 10){
                if(!herbQ){
                    dlgNpcLine = "Dnes nemam nic.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) alchNode = 0;
                    if(choice==2) closeTalk();
                } else if(herbQ->completed){
                    dlgNpcLine = "Bylinky si priniesol. Teraz uz len ich nezabudni pouzit.";
                    dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                    dlgEnabled = {true,true,false,false};
                    if(choice==1) alchNode = 0;
                    if(choice==2) closeTalk();
                } else if(!herbQ->active){
                    dlgNpcLine = "Potrebujem 3 modre kvety a 3 psenice. Nie pre mna. Pre mesto.";
                    dlgOptions = {"Berem to.", "Neskor.", "Spat.", ""};
                    dlgEnabled = {true,true,true,false};
                    if(choice==1){
                        herbQ->active = true;
                        pushNote("Quest prijaty: " + herbQ->title);
                        alchNode = 0;
                    }
                    if(choice==2 || choice==3) alchNode = 0;
                } else {
                    dlgNpcLine = TextFormat("Mas: MODRY KVET %i/3  |  PSENICA %i/3", haveFlower, haveWheat);
                    dlgOptions = {"Odovzdat suroviny.", "Spat.", "Dovidenia.", ""};
                    dlgEnabled = {haveFlower>=3 && haveWheat>=3, true, true, false};
                    if(choice==1 && dlgEnabled[0]){
                        InventoryRemoveItemN(inventory, "MODRY KVET", 3);
                        InventoryRemoveItemN(inventory, "PSENICA", 3);
                        herbQ->completed = true;
                        pushNote("Alchymista: \"Dobre. Toto pomoze.\"");
                        alchNode = 0;
                    }
                    if(choice==2) alchNode = 0;
                    if(choice==3) closeTalk();
                }
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(hunter.isTalking){
            dlgNpcName = hunter.name;
            int choice = pickChoice();
            Quest* campsQ = (campsQuestIdx >= 0) ? &quests[campsQuestIdx] : nullptr;
            Quest* locksQ = (locksQuestIdx >= 0) ? &quests[locksQuestIdx] : nullptr;

            if(hunterNode == 0){
                dlgNpcLine = "Chces prace? Svet je velky a tichy. Niekedy az prilis.";
                dlgOptions = {"Mas pre mna pracu?", "Co lovis?", "Dovidenia.", ""};
                dlgEnabled = {true,true,true,false};
                if(choice==1) hunterNode = 10;
                if(choice==2) hunterNode = 1;
                if(choice==3) closeTalk();
            } else if(hunterNode == 1){
                dlgNpcLine = "Nie zvierata. Ludi. A ich stopy. Tabory, ruiny, zamky... vsetko po sebe nechava otlacok.";
                dlgOptions = {"Spat.", "Dovidenia.", "", ""};
                dlgEnabled = {true,true,false,false};
                if(choice==1) hunterNode = 0;
                if(choice==2) closeTalk();
            } else if(hunterNode == 10){
                dlgNpcLine = "Vyber si.";
                std::string o1 = campsQ ? (campsQ->completed ? "TABORY (DONE)" : (campsQ->active ? "TABORY (TAKEN)" : "TABORY")) : "TABORY";
                std::string o2 = locksQ ? (locksQ->completed ? "ZAMKY (DONE)" : (locksQ->active ? "ZAMKY (TAKEN)" : "ZAMKY")) : "ZAMKY";
                dlgOptions = {o1, o2, "Spat.", "Dovidenia."};
                dlgEnabled = {campsQ && !campsQ->active && !campsQ->completed, locksQ && !locksQ->active && !locksQ->completed, true, true};
                if(choice==1 && dlgEnabled[0]){
                    campsQ->active = true;
                    pushNote("Quest prijaty: " + campsQ->title);
                    hunterNode = 0;
                }
                if(choice==2 && dlgEnabled[1]){
                    locksQ->active = true;
                    pushNote("Quest prijaty: " + locksQ->title);
                    hunterNode = 0;
                }
                if(choice==3) hunterNode = 0;
                if(choice==4) closeTalk();
            }

            if((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) && talkCooldown<=0.0f){
                closeTalk();
            }
        }

        if(showShop){
            if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)){
                showShop = false;
            }
        }
        if(showCrafting){
            if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)){
                showCrafting = false;
            }
        }

        // --- Drawing ---
        BeginDrawing();
        Color skyTop = LerpColor(SKY_NIGHT_TOP, SKY_DAY_TOP, daylight);
        Color skyBot = LerpColor(SKY_NIGHT_BOT, SKY_DAY_BOT, daylight);
        ClearBackground(skyTop);
        DrawRectangleGradientV(0,0,screenWidth,screenHeight,skyTop,skyBot);
        if(!inInterior && isNight){
            for(int i=0;i<60;i++){
                int x = (int)fmodf((i*97 + 33)*1.7f, (float)screenWidth);
                int y = (int)fmodf((i*53 + 71)*1.9f, (float)(screenHeight*0.6f));
                DrawCircle(x, y, 1, Fade(WHITE, 0.6f));
            }
        }

        NPC* talkNpc = nullptr;
        if(oldMan.isTalking) talkNpc = &oldMan;
        else if(guard.isTalking) talkNpc = &guard;
        else if(blacksmith.isTalking) talkNpc = &blacksmith;
        else if(innkeeper.isTalking) talkNpc = &innkeeper;
        else if(alchemist.isTalking) talkNpc = &alchemist;
        else if(hunter.isTalking) talkNpc = &hunter;
        bool anyDialogue = (talkNpc != nullptr);

        Camera3D cam = camera;
        if(anyDialogue){
            Vector3 npcHead = Vector3Add(talkNpc->position, {0,1.6f,0});
            Vector3 toNpc = Vector3Subtract(npcHead, Vector3Add(playerPos,{0,1.4f,0}));
            float len = Vector3Length(toNpc);
            if(len < 0.001f) len = 0.001f;
            Vector3 dir = Vector3Scale(toNpc, 1.0f/len);
            cam.target = npcHead;
            cam.position = Vector3Add(playerPos, Vector3Add(Vector3Scale(dir, -2.6f), {0,2.0f,0}));
            cam.fovy = 55.0f;
        }
        if(!anyDialogue && killcam.active && !inInterior){
            Vector3 tgt = Vector3Add(killcam.target, {0,1.3f,0});
            float t = (killcam.duration > 0.0f) ? (1.0f - (killcam.timer/killcam.duration)) : 1.0f;
            t = Clamp(t, 0.0f, 1.0f);
            float ang = playerAngle + (t*1.6f);
            float r = 3.6f;
            cam.target = tgt;
            cam.position = Vector3Add(tgt, {sinf(ang)*r, 1.4f, cosf(ang)*r});
            cam.fovy = 50.0f;
        }
        if(!anyDialogue && intro.active && !inInterior){
            Vector3 tgt = Vector3Add(intro.cartPos, {0,1.1f,0});
            float sway = sinf((float)GetTime()*1.2f) * 0.25f;
            cam.target = tgt;
            cam.position = Vector3Add(intro.cartPos, {5.2f, 2.8f, 8.4f + sway});
            cam.fovy = 55.0f;
        }

	        BeginMode3D(cam);
	        if(!inInterior){
	            Color worldTint = LerpColor({80,80,90,255}, WHITE, daylight);
            float sunAngle = timeOfDay * 2.0f * PI;
            Vector3 sunDir = Vector3Normalize({cosf(sunAngle), sinf(sunAngle), sinf(sunAngle)});
            Vector3 sunPos = Vector3Add(playerPos, Vector3Scale(sunDir, 220.0f));
            Vector3 moonPos = Vector3Add(playerPos, Vector3Scale(Vector3Negate(sunDir), 220.0f));
            DrawSphere(sunPos, 8.0f, SKY_SUN);
            DrawSphere(moonPos, 6.0f, SKY_MOON);
	            DrawModel(terrainModel,{-HALF_WORLD,-2,-HALF_WORLD},1.0f,worldTint);

		            // voda + odlesky/pena
		            Color waterDeep = {0, 86, 170, 120};
		            Color waterSurf = LerpColor(Color{10, 70, 110, 190}, Color{30, 120, 170, 200}, daylight);
		            DrawCube({0,waterLevel-2.5f,0},WORLD_SIZE,5,WORLD_SIZE, waterDeep);
		            DrawPlane({0.0f, waterLevel+0.01f, 0.0f}, {WORLD_SIZE, WORLD_SIZE}, Fade(waterSurf, 0.55f));
		            if(graphicsQuality > 0){
		                float tNow = (float)GetTime();
		                float glint = 0.08f + 0.05f*sinf(tNow*0.8f + timeOfDay*6.0f);
		                float rippleCull2 = 120.0f*120.0f;
		                float foamCull2 = 95.0f*95.0f;
		                int rippleStep = (graphicsQuality==1) ? 2 : 1;
		                int foamStep = (graphicsQuality==1) ? 3 : 1;
		                for(int i=0;i<(int)waterRipples.size();i+=rippleStep){
		                    float dx = waterRipples[i].x - playerPos.x;
		                    float dz = waterRipples[i].z - playerPos.z;
		                    if(dx*dx + dz*dz > rippleCull2) continue;
		                    float a = 0.06f + 0.05f*sinf(tNow*1.6f + waterRipplePhase[i]);
		                    float r = 0.35f + 0.25f*sinf(tNow*0.9f + waterRipplePhase[i]*1.7f);
		                    DrawCircle3D(waterRipples[i], r, {1,0,0}, 90.0f, Fade(SKYBLUE, a));
		                    if(i % 3 == 0) DrawCircle3D(waterRipples[i], r*0.55f, {1,0,0}, 90.0f, Fade(WHITE, a*0.45f + glint));
		                }
		                for(int i=0;i<(int)waterFoam.size();i+=foamStep){
		                    float dx = waterFoam[i].x - playerPos.x;
		                    float dz = waterFoam[i].z - playerPos.z;
		                    if(dx*dx + dz*dz > foamCull2) continue;
		                    float a = 0.08f + 0.06f*sinf(tNow*1.4f + waterFoamPhase[i]);
		                    if(i % 2 == 0) DrawCircle3D(waterFoam[i], 0.30f, {1,0,0}, 90.0f, Fade(WHITE, a));
		                    else DrawCircle3D(waterFoam[i], 0.22f, {1,0,0}, 90.0f, Fade(WHITE, a*0.8f));
		                }
		            }
		            // detail props (kamen/bush/trava)
		            float rockCull2 = 150.0f*150.0f;
		            float bushCull2 = 130.0f*130.0f;
		            float grassCull2 = 110.0f*110.0f;
		            for(int i=0;i<(int)worldRocks.size();i++){
		                Vector3 p = worldRocks[i];
		                float dx = p.x - playerPos.x;
		                float dz = p.z - playerPos.z;
		                if(dx*dx + dz*dz > rockCull2) continue;
		                float s = worldRockScale[i];
		                Vector3 base = Vector3Add(p,{0,0.25f*s,0});
		                DrawSphere(base, 0.35f*s, STONE_DARK);
		                DrawSphere(Vector3Add(base,{0.18f*s,0.05f*s,0.10f*s}), 0.22f*s, GRAY);
		                Vector3 hi = Vector3Add(base, Vector3Scale({sunDir.x, 0.25f, sunDir.z}, 0.16f*s));
		                DrawSphere(hi, 0.14f*s, Fade(STONE_LIGHT, 0.9f));
		                if(i%3==0) DrawSphere(Vector3Add(base,{-0.12f*s,-0.05f*s,-0.14f*s}), 0.18f*s, STONE_LIGHT);
		            }
		            for(int i=0;i<(int)worldBushes.size();i++){
		                Vector3 p = worldBushes[i];
		                float dx = p.x - playerPos.x;
		                float dz = p.z - playerPos.z;
		                if(dx*dx + dz*dz > bushCull2) continue;
		                float s = worldBushScale[i];
		                Vector3 b = Vector3Add(p,{0,0.55f*s,0});
		                DrawSphere(b, 0.55f*s, Fade(MY_DARKGREEN,0.95f));
		                DrawSphere(Vector3Add(b,{0.35f*s,-0.10f*s,0.15f*s}), 0.38f*s, Fade(MY_DARKGREEN,0.90f));
		                DrawSphere(Vector3Add(b,{-0.30f*s,-0.13f*s,-0.20f*s}), 0.36f*s, Fade(MY_DARKGREEN,0.90f));
		                Vector3 hi = Vector3Add(b, Vector3Scale({sunDir.x, 0.35f, sunDir.z}, 0.22f*s));
		                DrawSphere(hi, 0.22f*s, Fade(LIGHTGREEN, 0.55f));
		            }
		            if(graphicsQuality > 0){
		                int grassStep = (graphicsQuality==1) ? 2 : 1;
		                float tNow = (float)GetTime();
		                for(int i=0;i<(int)worldGrass.size();i+=grassStep){
		                Vector3 p = worldGrass[i];
		                    float dx = p.x - playerPos.x;
		                    float dz = p.z - playerPos.z;
		                    if(dx*dx + dz*dz > grassCull2) continue;
		                float s = worldGrassScale[i];
		                    float sway = sinf(tNow*1.4f + p.x*0.08f + p.z*0.09f) * 0.10f*s;
		                    Vector3 tip = Vector3Add(p, {sway, 0.55f*s, -sway*0.6f});
		                    DrawCylinderEx(Vector3Add(p,{0,0.05f*s,0}), tip, 0.03f*s, 0.30f*s, 6, Fade(LIGHTGREEN,0.85f));
		                }
		            }

	            for(auto& t:forest){ DrawCylinder(t.position,0.2f,0.2f,3.5f,8,BROWN); DrawCylinder(Vector3Add(t.position,{0,2.5f,0}),0,2.0f,4.0f,8,MY_DARKGREEN);}
		            float tNow = (float)GetTime();
	            for(auto& c:chests) if(c.active) DrawChestModel(c.position, c.locked, tNow);
	            for(auto& l:lootDrops) if(l.active) DrawLootModel(l, tNow);
		            for(auto& e:enemies) if(e.active){
		                float yaw = YawToFace(e.position, playerPos);
		                float hp01 = (e.maxHealth > 0.0f) ? (e.health/e.maxHealth) : 0.0f;
		                hp01 = Clamp(hp01, 0.0f, 1.0f);
		                float dur = 0.32f;
		                float a01 = (e.attackAnimTimer > 0.0f && dur > 0.0f) ? (1.0f - (e.attackAnimTimer/dur)) : 0.0f;
		                a01 = Clamp(a01, 0.0f, 1.0f);
		                DrawEnemyAvatar(e.position, yaw, e.type, e.isAggressive, hp01, a01);
		                DrawCube({e.position.x,e.position.y+2.8f,e.position.z}, hp01*2.0f, 0.2f, 0.1f, RED);
		            }
		            for(auto& p: enemyProjectiles) if(p.active){
		                DrawSphere(p.position, 0.10f, p.color);
		                DrawSphere(p.position, 0.18f, Fade(p.color, 0.25f));
		            }
            for(auto& f:fireballs) if(f.active){
                Color c = (f.type==0) ? ORANGE : (f.type==1 ? SKYBLUE : GREEN);
                DrawSphere(f.position, 0.36f, c);
                DrawSphere(f.position, 0.55f, Fade(c, 0.20f));
            }
            for(auto& p: playerProjectiles) if(p.active){
                DrawSphere(p.position, 0.10f, p.color);
                DrawSphere(p.position, 0.18f, Fade(p.color, 0.25f));
            }
            if(intro.active || intro.cartDepart){
                DrawCartAndHorses(intro.cartPos, intro.cartYaw);
            }
		            for(auto& r:poiRuins){
		                DrawTexturedBox(mdlStoneBox, {r.x, r.y+0.8f, r.z}, 3.0f, 1.6f, 3.0f, STONE_LIGHT);
		                DrawTexturedBox(mdlStoneBox, {r.x+2.2f, r.y+0.7f, r.z}, 1.5f, 1.2f, 1.5f, STONE_DARK);
		                DrawTexturedBox(mdlStoneBox, {r.x-1.8f, r.y+1.3f, r.z+1.1f}, 0.9f, 2.6f, 0.9f, GRAY);
		                DrawTexturedBox(mdlStoneBox, {r.x-0.2f, r.y+2.0f, r.z-1.1f}, 2.2f, 0.4f, 0.8f, STONE_DARK);
		            }
		            for(auto& c:poiCamps){
		                DrawCylinder(c, 1.2f, 1.2f, 0.22f, 8, DARKBROWN);
		                DrawSphere(Vector3Add(c,{0,0.6f,0}), 0.2f, ORANGE);
		                DrawTexturedBox(mdlWoodBox, {c.x+1.9f, c.y+0.25f, c.z}, 0.7f, 0.5f, 0.7f, Fade(BROWN,0.95f));
		                DrawTexturedBox(mdlWoodBox, {c.x+2.7f, c.y+0.22f, c.z+0.7f}, 0.5f, 0.44f, 0.5f, Fade(BROWN,0.92f));
		                DrawTexturedBox(mdlWoodBox, {c.x-1.9f, c.y+0.18f, c.z-0.5f}, 1.2f, 0.20f, 0.6f, Fade(BROWN,0.85f)); // bedroll
		            }
		            for(auto& v:poiCaves){
		                DrawSphere(v, 2.2f, Fade(BLACK, 0.95f));
		                DrawTexturedBox(mdlStoneBox, {v.x, v.y+0.6f, v.z}, 2.8f, 1.2f, 0.8f, STONE_DARK);
		                DrawTexturedBox(mdlStoneBox, {v.x-1.8f, v.y+0.9f, v.z+0.9f}, 1.2f, 1.8f, 1.2f, GRAY);
		            }
	            // chrám na hore
	            {
	                Vector3 tp = templePos;
	                DrawTexturedBox(mdlStoneBox, {tp.x, tp.y+2.2f, tp.z}, 10.0f, 4.4f, 12.0f, GRAY);
	                DrawTexturedBox(mdlStoneBox, {tp.x, tp.y+4.8f, tp.z-1.0f}, 8.6f, 1.2f, 9.4f, DARKGRAY);
	                // predný portál
	                for(int i=-1;i<=1;i++){
	                    DrawCylinder({tp.x + (float)i*2.3f, tp.y+0.0f, tp.z+5.4f}, 0.55f, 0.55f, 4.0f, 10, STONE_LIGHT);
	                }
	                DrawCube({tp.x, tp.y+1.4f, tp.z+6.2f}, 3.2f, 2.8f, 0.3f, BROWN); // door
	                DrawSphere({tp.x, tp.y+3.0f, tp.z+6.3f}, 0.22f, Fade(SKYRIM_GOLD,0.8f));
	                // schody / plosina
	                for(int i=0;i<4;i++){
	                    DrawCube({tp.x, tp.y+0.15f*(float)i, tp.z+7.8f + (float)i*0.8f}, 7.0f, 0.3f, 1.0f, STONE_DARK);
	                }
	            }
	            // Mesto
		            for(auto& h: cityHouses){
		                bool isStone = (Vector3Distance(h.position, castleCenter) < 0.25f) || (h.size.x >= 8.0f && h.size.z >= 6.8f);
		                const Model& wallMdl = isStone ? mdlStoneBox : mdlWoodBox;
	                const Model& roofMdl = isStone ? mdlStoneBox : mdlRoofBox;
	                DrawTexturedBox(wallMdl, h.position, h.size.x, h.size.y, h.size.z, h.wall);
	                DrawTexturedBox(roofMdl, {h.position.x, h.position.y + h.size.y*0.6f, h.position.z},
	                         h.size.x*0.9f, h.roofHeight, h.size.z*0.9f, h.roof);
	                Vector3 door = GetHouseDoorPos(h);
	                float doorW = 1.2f, doorH = 1.4f;
	                if(fabsf(h.doorDir.x) > 0.5f){
	                    DrawCube({door.x, door.y + 0.4f, door.z}, 0.2f, doorH, doorW, h.roof);
                    DrawCube({door.x, door.y + 0.9f, door.z + (h.doorDir.x>0? -1.0f:1.0f)}, 0.2f, 0.5f, 0.5f, YELLOW);
                } else {
                    DrawCube({door.x, door.y + 0.4f, door.z}, doorW, doorH, 0.2f, h.roof);
                    DrawCube({door.x + (h.doorDir.z>0? 1.0f:-1.0f), door.y + 0.9f, door.z}, 0.5f, 0.5f, 0.2f, YELLOW);
                }
                if(h.chimney){
                    DrawCube({h.position.x + 0.9f, h.position.y + h.size.y*0.7f, h.position.z + 0.6f},
                             0.4f, 1.2f, 0.4f, DARKBROWN);
                }
            }
            // Solitude styl: terasy mesta (jemne, bez obrovskej sivej plochy)
            DrawCube({cityCenter.x, cityCenter.y-0.6f, cityCenter.z-10.0f}, (cityRadius-14.0f)*2.0f, 0.8f, (cityRadius-20.0f)*2.0f, SAND);

            // hradby + brana
	            for(int i=0;i<120;i++){
	                float ang = (float)i/120.0f * 2.0f * PI;
	                Vector3 wp = {cityCenter.x + cosf(ang)*cityRadius, cityCenter.y, cityCenter.z + sinf(ang)*cityRadius};
	                if(Vector3Distance(wp, gatePos) < 5.0f) continue;
	                DrawTexturedBox(mdlStoneBox, {wp.x, wp.y+1.6f, wp.z}, 1.4f, 3.2f, 0.9f, STONE_DARK);
	                DrawTexturedBox(mdlStoneBox, {wp.x, wp.y+3.2f, wp.z}, 1.8f, 0.4f, 1.2f, STONE_LIGHT);
	            }
	            DrawTexturedBox(mdlStoneBox, {gatePos.x, gatePos.y+1.6f, gatePos.z}, 5.0f, 3.2f, 1.0f, STONE_DARK);
	            DrawTexturedBox(mdlStoneBox, {gatePos.x-3.2f, gatePos.y+2.2f, gatePos.z}, 2.0f, 4.4f, 1.6f, STONE_DARK);
	            DrawTexturedBox(mdlStoneBox, {gatePos.x+3.2f, gatePos.y+2.2f, gatePos.z}, 2.0f, 4.4f, 1.6f, STONE_DARK);
	            if(!gateUnlocked){
	                DrawTexturedBox(mdlStoneBox, {gatePos.x, gatePos.y+1.2f, gatePos.z}, 3.2f, 2.4f, 0.4f, GRAY);
	            }

            // brana a mostik
            DrawCube({gatePos.x, gatePos.y+0.2f, gatePos.z+4.0f}, 4.5f, 0.4f, 6.0f, STONE_LIGHT);
            DrawCube({gatePos.x, gatePos.y-0.6f, gatePos.z+5.5f}, 5.0f, 1.2f, 1.0f, STONE_DARK);

            // modre bannery
            for(int i=0;i<6;i++){
                float ang = (float)i/6.0f * 2.0f * PI;
                Vector3 bp = {cityCenter.x + cosf(ang)*(cityRadius-1.5f), cityCenter.y+2.4f, cityCenter.z + sinf(ang)*(cityRadius-1.5f)};
                DrawCube({bp.x, bp.y, bp.z}, 0.2f, 2.0f, 1.0f, BANNER_BLUE);
            }

            // veze
            for(int i=0;i<4;i++){
                float ang = (float)i/4.0f * 2.0f * PI + 0.5f;
                Vector3 tp = {cityCenter.x + cosf(ang)*cityRadius, cityCenter.y, cityCenter.z + sinf(ang)*cityRadius};
                DrawCylinder({tp.x, tp.y+2.2f, tp.z}, 2.2f, 2.4f, 4.6f, 10, STONE_DARK);
                DrawCylinder({tp.x, tp.y+5.0f, tp.z}, 2.6f, 2.6f, 0.6f, 10, STONE_LIGHT);
            }
	            // hrad v strede (Solitude styl)
	            Vector3 castle = {castleCenter.x, castleCenter.y, castleCenter.z};
	            DrawTexturedBox(mdlStoneBox, castle, 14.0f, 7.0f, 12.0f, GRAY);
	            DrawTexturedBox(mdlStoneBox, {castle.x, castle.y+4.6f, castle.z}, 12.0f, 2.2f, 10.0f, DARKGRAY);
	            DrawTexturedBox(mdlStoneBox, {castle.x-6.5f, castle.y+4.8f, castle.z+4.5f}, 3.6f, 4.8f, 3.6f, DARKGRAY);
	            DrawTexturedBox(mdlStoneBox, {castle.x+6.5f, castle.y+4.8f, castle.z+4.5f}, 3.6f, 4.8f, 3.6f, DARKGRAY);
	            DrawCube({castle.x, castle.y+2.0f, castle.z+6.2f}, 3.2f, 4.4f, 1.2f, BROWN);
            // straze na hradbach
            for(auto& g: wallGuards){
                DrawCapsule({g.x, g.y-1.8f, g.z}, {g.x, g.y, g.z}, 0.25f, 8, 3, BLUE);
            }
            // lampy
            for(auto& l: townLamps){
                DrawCylinder(l, 0.12f, 0.12f, 2.2f, 8, DARKBROWN);
                DrawSphere({l.x, l.y+2.2f, l.z}, 0.2f, SKYRIM_GOLD);
            }
            // stanky
            for(auto& s: townStalls){
                DrawCube({s.x, s.y+0.5f, s.z}, 2.0f, 1.0f, 1.4f, BROWN);
                DrawCube({s.x, s.y+1.2f, s.z}, 2.2f, 0.2f, 1.6f, RED);
            }
            // ploty
            for(auto& f: townFences){
                DrawCube({f.x, f.y+0.4f, f.z}, 0.6f, 0.8f, 0.1f, BROWN);
            }
            // doky
            for(auto& d: dockPlanks){
                DrawCube({d.x, d.y+0.1f, d.z}, 3.5f, 0.2f, 3.0f, DARKBROWN);
            }
            for(auto& b: dockBoats){
                DrawCube({b.x, b.y+0.4f, b.z}, 3.0f, 0.6f, 1.6f, BROWN);
                DrawTriangle3D({b.x, b.y+1.2f, b.z}, {b.x+1.2f, b.y+0.6f, b.z}, {b.x-1.2f, b.y+0.6f, b.z}, BEIGE);
            }
            DrawCube(cityCenter,18.0f,0.2f,18.0f,DARKBROWN);
            DrawCubeWires(cityCenter,20.0f,2.5f,20.0f,GRAY);
	            // NPCs (rozpoznatelne modely)
	            DrawNpcAvatar(oldMan.position, YawToFace(oldMan.position, playerPos), LOOK_OLDMAN);
	            DrawNpcAvatar(blacksmith.position, YawToFace(blacksmith.position, playerPos), LOOK_BLACKSMITH);
	            DrawNpcAvatar(guard.position, YawToFace(guard.position, playerPos), LOOK_GUARD);
	            DrawNpcAvatar(innkeeper.position, YawToFace(innkeeper.position, playerPos), LOOK_INNKEEPER);
	            DrawNpcAvatar(alchemist.position, YawToFace(alchemist.position, playerPos), LOOK_ALCHEMIST);
	            DrawNpcAvatar(hunter.position, YawToFace(hunter.position, playerPos), LOOK_HUNTER);
	            DrawNpcAvatar(patrolGuard.position, YawToFace(patrolGuard.position, playerPos), LOOK_PATROL);
		            float playerAttack01 = (playerAttackDuration > 0.0f) ? (1.0f - (playerAttackTimer/playerAttackDuration)) : 0.0f;
		            playerAttack01 = Clamp(playerAttack01, 0.0f, 1.0f);
		            if(playerAttackTimer <= 0.0f) playerAttack01 = 0.0f;
		            WeaponKind wk = WeaponKind::SWORD;
		            for(const auto& it : inventory) if(it.isEquipped && it.category=="WEAPON"){ wk = WeaponKindFromItemName(it.name); break; }
		            if(!intro.active) DrawPlayerAvatar(playerPos, playerAngle, playerWalkPhase, playerIsMoving, playerIsSprinting, playerAttack01, wk);
	        } else {
	            if(interiorKind == 0){
	                float baseY = (interiorScene == INT_CASTLE) ? (float)interiorLevel*3.0f : 0.0f;
	                auto at = [&](float x, float y, float z)->Vector3{ return {x, y + baseY, z}; };

		                if(interiorScene == INT_CASTLE){
		                    if(interiorLevel <= 1){
		                        // main hall + balcony
		                        DrawCube(at(0,-0.6f,0), 24.0f, 1.0f, 24.0f, STONE_DARK);
		                        DrawCubeWires(at(0,2.8f,0), 24.0f, 6.0f, 24.0f, Fade(STONE_LIGHT,0.55f));
		                        for(int i=-2;i<=2;i++){
		                            DrawCylinder(at((float)i*4.2f,0.0f,-2.0f), 0.55f, 0.55f, 5.0f, 10, STONE_LIGHT);
		                        }
		                        // side alcoves / doorways
		                        for(int i=-1;i<=1;i+=2){
		                            DrawCube(at((float)i*11.6f,1.2f,-4.0f), 0.4f, 2.6f, 4.0f, STONE_LIGHT);
		                            DrawCube(at((float)i*11.6f,1.2f,4.0f), 0.4f, 2.6f, 4.0f, STONE_LIGHT);
		                        }
		                        // chandeliers
		                        for(int i=-1;i<=1;i++){
		                            DrawSphere(at((float)i*6.0f,4.6f,-1.0f), 0.25f, SKYRIM_GOLD);
		                            DrawCylinder(at((float)i*6.0f,4.8f,-1.0f), 0.05f, 0.05f, 1.0f, 8, DARKGRAY);
		                        }
		                        // banners
		                        for(int i=-2;i<=2;i+=4){
		                            DrawCube(at((float)i*4.8f,2.0f,-11.2f), 1.0f, 3.6f, 0.2f, BANNER_BLUE);
		                        }
		                        // throne / dais
		                        DrawCube(at(0,0.2f,-10.0f), 8.0f, 0.4f, 4.0f, STONE_LIGHT);
		                        DrawCube(at(0,1.0f,-10.8f), 2.2f, 2.0f, 1.0f, STONE_DARK);
		                        DrawCube(at(0,0.45f,-8.6f), 10.5f, 0.3f, 2.0f, Fade(STONE_LIGHT,0.75f)); // carpet
		                        // stairs marker (right side)
		                        DrawCube(at(9.2f,0.2f,4.2f), 2.0f, 0.4f, 2.0f, STONE_LIGHT);
		                        DrawCube(at(-9.5f,0.2f,-9.5f), 1.8f, 0.4f, 1.8f, STONE_LIGHT); // tower door marker
		                        // exit door
		                        DrawCube(at(0,1.0f,12.2f), 3.0f, 2.2f, 0.2f, BROWN);

		                        if(interiorLevel==1){
		                            // balcony floor
		                            DrawCube(at(0, -0.2f, 0), 24.0f, 0.3f, 24.0f, Fade(STONE_LIGHT,0.65f));
		                            DrawCubeWires(at(0, 0.2f, 0), 24.0f, 0.3f, 24.0f, Fade(STONE_DARK,0.35f));
		                            // railings
		                            DrawCube(at(0,0.9f,-12.0f), 24.0f, 1.0f, 0.3f, Fade(STONE_DARK,0.65f));
		                            DrawCube(at(0,0.9f,12.0f), 24.0f, 1.0f, 0.3f, Fade(STONE_DARK,0.65f));
		                            DrawCube(at(-12.0f,0.9f,0.0f), 0.3f, 1.0f, 24.0f, Fade(STONE_DARK,0.65f));
		                            DrawCube(at(12.0f,0.9f,0.0f), 0.3f, 1.0f, 24.0f, Fade(STONE_DARK,0.65f));
		                        }
		                    } else {
		                        // tower room
		                        DrawCube(at(0,-0.6f,0), 8.0f, 1.0f, 8.0f, STONE_DARK);
		                        DrawCubeWires(at(0,2.8f,0), 8.0f, 6.0f, 8.0f, Fade(STONE_LIGHT,0.55f));
		                        DrawCylinder(at(0,0.0f,0.0f), 0.8f, 0.8f, 4.8f, 12, STONE_LIGHT);
		                        // table + "map"
		                        DrawCylinder(at(1.6f,0.25f,-1.6f), 0.9f, 0.9f, 0.5f, 12, DARKBROWN);
		                        DrawCylinder(at(1.6f,0.65f,-1.6f), 1.3f, 1.3f, 0.15f, 14, BROWN);
		                        DrawCube(at(1.6f,0.75f,-1.6f), 2.0f, 0.02f, 1.2f, Fade(SKYBLUE,0.45f));
		                        // windows
		                        for(int i=-1;i<=1;i+=2){
		                            DrawCube(at((float)i*3.6f,1.8f,0.0f), 0.15f, 1.8f, 2.4f, Fade(SKYBLUE,0.18f));
		                        }
		                        DrawCube(at(0,1.0f,3.6f), 2.2f, 2.0f, 0.2f, BROWN); // ladder marker (down)
		                    }
		                } else if(interiorScene == INT_BLACKSMITH){
		                    // smith interior
		                    DrawCube(at(0,-0.6f,0.6f), 13.0f, 1.0f, 12.4f, DARKBROWN);
		                    DrawCubeWires(at(0,2.0f,0.6f), 13.0f, 4.0f, 12.4f, Fade(GRAY,0.7f));
			                    DrawCube(at(0,1.0f,6.5f), 3.2f, 2.2f, 0.2f, BROWN); // exit
			                    // counter + npc behind
			                    DrawCube(at(0,0.6f,-2.2f), 10.5f, 1.2f, 0.6f, BROWN);
			                    {
			                        Vector3 npcPos = at(0,0.0f,-3.6f);
			                        DrawNpcAvatar(npcPos, YawToFace(npcPos, playerPos), LOOK_BLACKSMITH);
			                    }
			                    // anvil + forge
			                    DrawCube(at(-3.8f,0.4f,0.2f), 1.6f, 0.8f, 1.2f, GRAY);
		                    DrawCylinder(at(-3.8f,0.8f,0.2f), 0.6f, 0.4f, 0.6f, 10, DARKGRAY);
		                    DrawCube(at(4.4f,0.8f,-0.2f), 2.2f, 1.6f, 2.0f, STONE_DARK);
		                    DrawCube(at(4.4f,0.8f,-0.2f), 1.4f, 0.6f, 1.2f, ORANGE);
		                    DrawSphere(at(4.4f,1.6f,-0.2f), 0.25f, Fade(ORANGE,0.7f));
		                    // barrels / crates
		                    DrawCylinder(at(-5.2f,0.0f,3.8f), 0.5f, 0.55f, 1.2f, 10, BROWN);
		                    DrawCube(at(-4.0f,0.35f,3.8f), 1.1f, 0.7f, 1.1f, DARKBROWN);
		                    // armor stand (visual)
		                    DrawCylinder(at(3.8f,0.0f,3.6f), 0.18f, 0.18f, 1.6f, 10, DARKBROWN);
		                    DrawCube(at(3.8f,1.1f,3.6f), 0.9f, 1.2f, 0.4f, Fade(GRAY,0.75f));
		                    DrawSphere(at(3.8f,1.9f,3.6f), 0.25f, Fade(GRAY,0.75f));
		                    // weapon rack
		                    for(int i=0;i<5;i++){
		                        DrawCube(at(-5.6f,1.0f,-1.8f + i*0.8f), 0.2f, 2.0f, 0.2f, DARKBROWN);
		                    }
		                } else if(interiorScene == INT_ALCHEMIST){
		                    DrawCube(at(0,-0.6f,0.6f), 12.4f, 1.0f, 12.4f, DARKBROWN);
		                    DrawCubeWires(at(0,2.0f,0.6f), 12.4f, 4.0f, 12.4f, Fade(GRAY,0.7f));
			                    DrawCube(at(0,1.0f,6.5f), 3.0f, 2.2f, 0.2f, BROWN); // exit
			                    // counter + npc
			                    DrawCube(at(0,0.6f,-2.2f), 10.0f, 1.2f, 0.6f, BROWN);
			                    {
			                        Vector3 npcPos = at(0,0.0f,-3.6f);
			                        DrawNpcAvatar(npcPos, YawToFace(npcPos, playerPos), LOOK_ALCHEMIST);
			                    }
			                    // cauldron + tables + bottles
			                    DrawCylinder(at(2.8f,0.2f,0.0f), 0.8f, 0.9f, 0.8f, 12, DARKGRAY);
		                    DrawCylinder(at(2.8f,0.95f,0.0f), 0.9f, 0.85f, 0.2f, 12, GRAY);
		                    DrawCube(at(-3.2f,0.45f,0.8f), 2.6f, 0.9f, 1.4f, BROWN);
		                    for(int i=0;i<6;i++){
		                        DrawSphere(at(-4.1f + i*0.35f, 1.1f, 0.3f), 0.12f, SKYBLUE);
		                    }
		                    // shelves + books
		                    for(int i=0;i<3;i++){
		                        DrawCube(at(5.4f,0.8f,-3.0f + i*2.6f), 0.8f, 1.6f, 2.0f, BROWN);
		                        DrawCube(at(5.0f,1.4f,-3.0f + i*2.6f), 1.6f, 0.25f, 1.6f, DARKBROWN);
		                        DrawCube(at(5.0f,1.7f,-3.0f + i*2.6f), 1.4f, 0.2f, 1.2f, MAROON);
		                    }
			                } else if(interiorScene == INT_INN){
			                    DrawCube(at(0,-0.6f,0.0f), 17.0f, 1.0f, 14.4f, DARKBROWN);
			                    DrawCubeWires(at(0,2.0f,0.0f), 17.0f, 4.0f, 14.4f, Fade(GRAY,0.7f));
				                    DrawCube(at(0,1.0f,6.9f), 3.4f, 2.2f, 0.2f, BROWN); // exit
				                    // bar + npc
				                    DrawCube(at(0,0.6f,-3.2f), 14.0f, 1.2f, 0.8f, BROWN);
				                    {
				                        Vector3 npcPos = at(0,0.0f,-4.4f);
				                        DrawNpcAvatar(npcPos, YawToFace(npcPos, playerPos), LOOK_INNKEEPER);
				                    }
			                    // fireplace
			                    DrawCube(at(-7.2f,0.8f,-5.2f), 2.2f, 1.6f, 1.2f, STONE_DARK);
		                    DrawCube(at(-7.2f,0.8f,-5.2f), 1.2f, 0.5f, 0.6f, ORANGE);
		                    DrawSphere(at(-7.2f,1.2f,-5.2f), 0.22f, Fade(ORANGE,0.75f));
		                    // tables
		                    for(int i=-1;i<=1;i++){
		                        DrawCylinder(at((float)i*4.0f,0.25f,1.2f), 0.7f, 0.7f, 0.5f, 10, BROWN);
		                        DrawCylinder(at((float)i*4.0f,0.6f,1.2f), 1.4f, 1.4f, 0.2f, 12, DARKBROWN);
		                        DrawSphere(at((float)i*4.0f+0.3f,0.9f,1.2f+0.2f), 0.12f, SKYRIM_GOLD);
		                    }
			                    // stairs to "upper rooms" (visual only)
			                    DrawCube(at(7.2f,0.6f,5.2f), 2.2f, 1.2f, 3.0f, STONE_LIGHT);
			                } else if(interiorScene == INT_TEMPLE){
			                    // stone temple
			                    DrawCube(at(0,-0.6f,0.0f), 16.8f, 1.0f, 19.0f, STONE_DARK);
			                    DrawCubeWires(at(0,2.6f,0.0f), 16.8f, 5.2f, 19.0f, Fade(STONE_LIGHT,0.55f));
			                    DrawCube(at(0,1.0f,9.2f), 3.6f, 2.4f, 0.2f, BROWN); // exit
			                    // columns
			                    for(int i=-1;i<=1;i++){
			                        DrawCylinder(at(-6.2f,0.0f,-5.0f + i*4.6f), 0.45f, 0.45f, 4.8f, 10, STONE_LIGHT);
			                        DrawCylinder(at(6.2f,0.0f,-5.0f + i*4.6f), 0.45f, 0.45f, 4.8f, 10, STONE_LIGHT);
			                    }
			                    // altar + glow
			                    DrawCube(at(0,0.25f,-6.6f), 5.2f, 0.5f, 2.2f, STONE_LIGHT);
			                    DrawCube(at(0,0.85f,-6.6f), 2.2f, 1.2f, 1.2f, STONE_DARK);
			                    DrawSphere(at(0,1.75f,-6.6f), 0.26f, Fade(SKYRIM_GOLD,0.95f));
			                    DrawSphere(at(0,1.75f,-6.6f), 0.48f, Fade(SKYRIM_GOLD,0.18f));
			                    // banners / windows
			                    DrawCube(at(-7.9f,2.1f,0.0f), 0.2f, 2.4f, 5.6f, Fade(SKYBLUE,0.15f));
			                    DrawCube(at(7.9f,2.1f,0.0f), 0.2f, 2.4f, 5.6f, Fade(SKYBLUE,0.15f));
			                    // benches
			                    for(int r=0;r<3;r++){
			                        DrawCube(at(-2.8f,0.25f,1.0f + r*2.4f), 2.8f, 0.4f, 0.8f, DARKBROWN);
			                        DrawCube(at(2.8f,0.25f,1.0f + r*2.4f), 2.8f, 0.4f, 0.8f, DARKBROWN);
			                    }
			                } else if(interiorScene == INT_GENERAL || interiorScene == INT_HUNTER){
			                    DrawCube(at(0,-0.6f,0.6f), 13.0f, 1.0f, 12.4f, DARKBROWN);
			                    DrawCubeWires(at(0,2.0f,0.6f), 13.0f, 4.0f, 12.4f, Fade(GRAY,0.7f));
				                    DrawCube(at(0,1.0f,6.5f), 3.0f, 2.2f, 0.2f, BROWN); // exit
			                    DrawCube(at(0,0.6f,-2.2f), 10.5f, 1.2f, 0.6f, BROWN); // counter
			                    {
			                        Vector3 npcPos = at(0,0.0f,-3.6f);
			                        int look = (interiorScene==INT_HUNTER) ? LOOK_HUNTER : LOOK_SHOPKEEP;
			                        DrawNpcAvatar(npcPos, YawToFace(npcPos, playerPos), look);
			                    }
			                    // shelves
			                    for(int i=0;i<4;i++){
			                        DrawCube(at(-5.6f,0.8f,-3.5f + i*2.0f), 0.8f, 1.6f, 1.6f, BROWN);
		                        DrawCube(at(5.6f,0.8f,-3.5f + i*2.0f), 0.8f, 1.6f, 1.6f, BROWN);
		                    }
		                    // stock / crates
		                    DrawCube(at(-4.6f,0.35f,4.8f), 1.3f, 0.7f, 1.1f, DARKBROWN);
		                    DrawCube(at(4.6f,0.35f,4.8f), 1.3f, 0.7f, 1.1f, DARKBROWN);
		                    for(int i=0;i<4;i++){
		                        DrawSphere(at(-4.6f + (float)(i%2)*0.35f, 0.9f, 4.8f + (float)(i/2)*0.35f), 0.14f, (interiorScene==INT_HUNTER?BEIGE:SKYRIM_GOLD));
		                    }
		                    if(interiorScene==INT_HUNTER){
		                        // target dummy
		                        DrawCube(at(0,1.0f,0.0f), 0.2f, 2.0f, 0.2f, DARKBROWN);
		                        DrawCircle3D(at(0,1.8f,0.0f), 0.7f, {1,0,0}, 90.0f, RED);
		                        // bow rack + pelts
		                        for(int i=0;i<4;i++){
		                            DrawCube(at(-5.3f,1.0f,1.8f + i*0.7f), 0.15f, 1.6f, 0.15f, DARKBROWN);
		                        }
		                        DrawCube(at(5.2f,1.2f,2.6f), 0.2f, 2.0f, 1.6f, MAROON);
		                    }
		                } else {
		                    // generic house
		                    float roomHalf = 5.5f;
		                    DrawCube(at(0,-0.6f,0), roomHalf*2.0f, 1.0f, roomHalf*2.0f, DARKBROWN);
		                    DrawCubeWires(at(0,2.0f,0), roomHalf*2.0f, 4.0f, roomHalf*2.0f, GRAY);
		                    DrawCube(at(0,1.0f, roomHalf-0.3f), 2.0f, 1.0f, 0.2f, BROWN); // door
		                    DrawCube(at(-3.0f,0.3f,2.0f), 2.0f, 0.6f, 1.4f, BROWN); // bed
		                    DrawCube(at(2.8f,0.6f,-2.5f), 1.2f, 0.8f, 1.2f, MY_GOLD); // chest
		                    DrawCube(at(0.0f,0.45f,0.2f), 2.0f, 0.9f, 1.2f, BROWN); // table
		                    DrawCylinder(at(0.9f,0.0f,0.8f), 0.25f, 0.25f, 0.9f, 10, DARKBROWN); // chair
		                }

			                float playerAttack01 = (playerAttackDuration > 0.0f) ? (1.0f - (playerAttackTimer/playerAttackDuration)) : 0.0f;
			                playerAttack01 = Clamp(playerAttack01, 0.0f, 1.0f);
			                if(playerAttackTimer <= 0.0f) playerAttack01 = 0.0f;
			                WeaponKind wk = WeaponKind::SWORD;
			                for(const auto& it : inventory) if(it.isEquipped && it.category=="WEAPON"){ wk = WeaponKindFromItemName(it.name); break; }
			                if(!intro.active) DrawPlayerAvatar(playerPos, playerAngle, playerWalkPhase, playerIsMoving, playerIsSprinting, playerAttack01, wk);
	            } else {
	                // --- Dungeon: 3 miestnosti + pasce + boss ---
	                auto floor = [&](Vector3 c, float sx, float sz){
	                    DrawCube({c.x, -0.6f, c.z}, sx, 1.0f, sz, DARKBROWN);
	                    DrawCubeWires({c.x, 2.0f, c.z}, sx, 4.0f, sz, Fade(GRAY,0.55f));
	                };
                floor({0,0,7.0f}, 10.0f, 10.0f);     // room 1
                floor({0,0,-1.0f}, 3.2f, 6.0f);      // corridor 1
                floor({0,0,-9.0f}, 10.0f, 10.0f);    // room 2
                floor({0,0,-18.0f}, 3.2f, 8.0f);     // corridor 2
                floor({0,0,-28.0f}, 12.0f, 12.0f);   // boss room

                // exit marker
                DrawCube({0,1.0f, 12.2f}, 2.0f, 1.0f, 0.2f, BROWN);

	                // traps
	                for(const auto& tp : dungeonTraps){
	                    DrawCylinder({tp.x, 0.0f, tp.z}, 0.45f, 0.05f, 0.9f, 8, RED);
	                    DrawCylinder({tp.x, 0.0f, tp.z}, 0.20f, 0.02f, 1.2f, 8, MAROON);
	                }

	                // gate + lever
	                if(!dungeonGateOpen){
	                    for(int i=-3;i<=3;i++){
	                        DrawCube({(float)i*0.6f, 1.2f, -22.0f}, 0.18f, 2.6f, 0.25f, DARKGRAY);
	                    }
	                    DrawCube({0.0f, 2.6f, -22.0f}, 4.6f, 0.20f, 0.25f, STONE_DARK);
	                    DrawCube({0.0f, 0.1f, -22.0f}, 4.6f, 0.20f, 0.25f, STONE_DARK);
	                } else {
	                    DrawCube({0.0f, 1.2f, -22.0f}, 4.6f, 0.10f, 0.12f, Fade(STONE_LIGHT,0.25f));
	                }
	                if(!dungeonLeverPulled){
	                    Vector3 lp = {4.0f, 0.6f, -9.0f};
	                    DrawCube(lp, 0.3f, 1.2f, 0.3f, DARKGRAY);
	                    DrawCube({lp.x, lp.y+0.5f, lp.z+0.35f}, 0.15f, 0.15f, 0.9f, STONE_LIGHT);
	                    DrawSphere({lp.x, lp.y+0.95f, lp.z+0.70f}, 0.12f, SKYRIM_GOLD);
	                } else {
	                    Vector3 lp = {4.0f, 0.6f, -9.0f};
	                    DrawCube(lp, 0.3f, 1.2f, 0.3f, DARKGRAY);
	                    DrawCube({lp.x, lp.y+0.5f, lp.z-0.35f}, 0.15f, 0.15f, 0.9f, STONE_LIGHT);
	                }

	                // boss + reward
	                if(dungeonBoss.active){
	                    float yaw = YawToFace(dungeonBoss.position, playerPos);
	                    float hp01 = (dungeonBoss.maxHealth > 0.0f) ? (dungeonBoss.health/dungeonBoss.maxHealth) : 0.0f;
	                    hp01 = Clamp(hp01, 0.0f, 1.0f);
	                    DrawBossAvatar(dungeonBoss.position, yaw, hp01);
	                    DrawCube({dungeonBoss.position.x, dungeonBoss.position.y+4.2f, dungeonBoss.position.z}, hp01*3.2f, 0.25f, 0.15f, RED);
	                }
	                if(dungeonBossDefeated && !dungeonRewardTaken){
	                    // unique reward: glowing artifact
	                    float tNow = (float)GetTime();
	                    Vector3 rp = Vector3Add(dungeonRewardPos, {0.0f, 0.20f + sinf(tNow*2.0f)*0.06f, 0.0f});
	                    DrawCylinder(dungeonRewardPos, 0.9f, 0.9f, 0.25f, 14, STONE_DARK); // pedestal
	                    DrawSphere(rp, 0.26f, MY_GOLD);
	                    DrawSphere(rp, 0.34f, Fade(MY_GOLD,0.22f));
	                    DrawCube(Vector3Add(rp,{0.0f,0.0f,0.0f}), 0.12f, 0.65f, 0.12f, Fade(LIGHTGRAY,0.9f));
	                }

		                float playerAttack01 = (playerAttackDuration > 0.0f) ? (1.0f - (playerAttackTimer/playerAttackDuration)) : 0.0f;
		                playerAttack01 = Clamp(playerAttack01, 0.0f, 1.0f);
		                if(playerAttackTimer <= 0.0f) playerAttack01 = 0.0f;
		                WeaponKind wk = WeaponKind::SWORD;
		                for(const auto& it : inventory) if(it.isEquipped && it.category=="WEAPON"){ wk = WeaponKindFromItemName(it.name); break; }
		                if(!intro.active) DrawPlayerAvatar(playerPos, playerAngle, playerWalkPhase, playerIsMoving, playerIsSprinting, playerAttack01, wk);
            }
        }
        EndMode3D();

        if(!inInterior) DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK, 0.35f*(1.0f - daylight)));

        if(!inInterior && isUnderwater) DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLUE,0.3f));
        if(!inInterior && killcam.active){
            float t = (killcam.duration > 0.0f) ? (1.0f - (killcam.timer/killcam.duration)) : 1.0f;
            t = Clamp(t, 0.0f, 1.0f);
            float pulse = 0.25f + 0.25f*sinf(t*PI);
            DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK, pulse));
            const char* txt = "FINISHER";
            int tw = MeasureText(txt, 30);
            DrawText(txt, screenWidth/2 - tw/2, 110, 30, SKYRIM_GOLD);
        }
        if(!inInterior && intro.active){
            float t = Clamp(intro.t, 0.0f, intro.duration);
            if(t < 0.9f){
                DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK, 1.0f - (t/0.9f)));
            }
            if(t > intro.duration - 0.8f){
                float u = (t - (intro.duration - 0.8f))/0.8f;
                DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK, Clamp(u,0.0f,1.0f)));
            }
            int ci = GetIntroCueIndex(t);
            if(ci >= 0){
                DrawSubtitlePanel(screenWidth, screenHeight, INTRO_CUES[ci].text);
            }
        }
        bool inCityArea = Vector3Distance(playerPos, cityCenter) < (cityRadius - 1.0f);
        if(!inInterior && weatherEnabled){
            if(weatherType==1 && !inCityArea) DrawRectangle(0,0,screenWidth,screenHeight,Fade(GRAY,0.15f));
            if(weatherType==2){
                int drops = (graphicsQuality==0 ? 70 : (graphicsQuality==1 ? 120 : 160));
                if(graphicsQuality != lastGraphicsQuality) rainDrops.clear();
                EnsureParticles(rainDrops, drops, screenWidth, screenHeight, 520.0f, 900.0f, 1.0f, 1.0f, 70.0f);
                UpdateAndDrawRain(rainDrops, screenWidth, screenHeight, dt);
            }
            if(weatherType==3){
                int flakes = (graphicsQuality==0 ? 50 : (graphicsQuality==1 ? 90 : 130));
                if(graphicsQuality != lastGraphicsQuality) snowFlakes.clear();
                EnsureParticles(snowFlakes, flakes, screenWidth, screenHeight, 35.0f, 90.0f, 1.0f, 2.5f, 14.0f);
                UpdateAndDrawSnow(snowFlakes, screenWidth, screenHeight, dt, (float)GetTime());
            }
            lastGraphicsQuality = graphicsQuality;
        }
        // hmla sa v meste nekresli

        // --- Jaskyna svetlo (pochoden) ---
        bool nearCave = false;
        for(auto& v: poiCaves){
            if(Vector3Distance(playerPos, v) < 14.0f){
                nearCave = true; break;
            }
        }
        if(nearCave){
            DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK,0.35f));
            if(torchActive){
                DrawCircleGradient(screenWidth/2, screenHeight/2, 220.0f, Fade(SKYRIM_GOLD,0.35f), Fade(BLANK,0.0f));
            }
        }

        // --- HUD ---
        if(!anyDialogue && !intro.active){
            std::vector<CompassMarker> compassMarkers;
            compassMarkers.reserve(8);
            if(!inInterior){
                compassMarkers.push_back({oldMan.position, SKYRIM_GOLD});
                compassMarkers.push_back({blacksmith.position, SKYRIM_PANEL_EDGE});
                compassMarkers.push_back({guard.position, BANNER_BLUE});
                compassMarkers.push_back({innkeeper.position, SKYRIM_PANEL_EDGE});
                compassMarkers.push_back({alchemist.position, GREEN});
                compassMarkers.push_back({hunter.position, DARKGREEN});
                compassMarkers.push_back({gatePos, SKYRIM_GOLD});

                if(storyQuestIdx >= 0){
                    Quest& storyQHud = quests[storyQuestIdx];
                    if(storyQHud.active && !storyQHud.completed){
                        if(storyStage == 1) compassMarkers.push_back({storyRuin, ORANGE});
                        else if(storyStage == 2) compassMarkers.push_back({oldMan.position, ORANGE});
                        else if(storyStage == 3) compassMarkers.push_back({gatePos, ORANGE});
                    }
                }
	                if(caveQuestIdx >= 0){
	                    Quest& caveQHud = quests[caveQuestIdx];
	                    if(caveQHud.active && !caveQHud.completed){
	                        if(caveStage == 1) compassMarkers.push_back({dungeonEntrance, ORANGE});
	                        else if(caveStage == 2) compassMarkers.push_back({innkeeper.position, ORANGE});
	                    }
	                }
	                if(templeQuestIdx >= 0){
	                    Quest& tq = quests[templeQuestIdx];
	                    if(tq.active && !tq.completed){
	                        compassMarkers.push_back({templePos, ORANGE});
	                    }
	                }

                // najbližšia truhlica
                float bestChest = 9999.0f;
                Vector3 bestChestPos = {0,0,0};
                for(const auto& c : chests) if(c.active){
                    float d = Vector3Distance(playerPos, c.position);
                    if(d < bestChest){
                        bestChest = d;
                        bestChestPos = c.position;
                    }
                }
                if(bestChest < 70.0f) compassMarkers.push_back({bestChestPos, MY_GOLD});
            }
            DrawCompass(playerAngle, playerPos, screenWidth, compassMarkers);
            DrawCrosshair(screenWidth, screenHeight, Fade(SKYRIM_TEXT, 0.85f));

            Rectangle hpBar = {40, (float)screenHeight - 46, 280, 18};
            Rectangle stBar = {(float)screenWidth/2 - 140, (float)screenHeight - 46, 280, 18};
            Rectangle mpBar = {(float)screenWidth - 320, (float)screenHeight - 46, 280, 18};
            DrawSkyrimBar(hpBar, playerHealth, playerMaxHealth, RED, SKYRIM_PANEL_EDGE, "HEALTH");
            DrawSkyrimBar(stBar, playerStamina, playerMaxStamina, GREEN, SKYRIM_PANEL_EDGE, "STAMINA");
            DrawSkyrimBar(mpBar, playerMana, playerMaxMana, SKYBLUE, SKYRIM_PANEL_EDGE, "MAGICKA");

	            DrawText(TextFormat("Lvl %i", level), 20, 52, 18, SKYRIM_TEXT);
	            DrawText(TextFormat("Gold %i", gold), 20, 74, 18, SKYRIM_GOLD);
	            DrawText(TextFormat("Lockpicks %i", lockpicks), 20, 96, 18, SKYRIM_TEXT);
	            DrawText(TextFormat("1H %i  ARC %i  DEST %i  SPEECH %i",
	                                skills[(int)Skill::ONE_HANDED].level,
	                                skills[(int)Skill::ARCHERY].level,
	                                skills[(int)Skill::DESTRUCTION].level,
	                                skills[(int)Skill::SPEECH].level),
	                     20, 118, 16, SKYRIM_MUTED);
	            const char* sp = (activeSpell==0) ? "OHEŇ" : (activeSpell==1 ? "MRAZ" : "JED");
	            DrawText(TextFormat("Spell (Z): %s", sp), 20, 138, 16, SKYRIM_TEXT);
	        }

        // --- Interakčné hinty (Skyrim štýl: pri kurzore) ---
        if(!anyDialogue && !intro.active){
            float bestDist = 999.0f;
            std::string prompt;
            Color promptCol = SKYRIM_TEXT;
            auto consider = [&](float d, const std::string& t, Color c){
                if(d < bestDist){
                    bestDist = d;
                    prompt = t;
                    promptCol = c;
                }
            };

	            if(inInterior){
	                float baseY = (interiorKind==0 && interiorScene==INT_CASTLE) ? (float)interiorLevel*3.0f : 0.0f;
		                auto interiorMaxZ = [&]()->float{
		                    if(interiorKind != 0) return 12.2f;
		                    if(interiorScene == INT_BLACKSMITH) return 6.8f;
		                    if(interiorScene == INT_ALCHEMIST) return 6.8f;
		                    if(interiorScene == INT_GENERAL) return 6.8f;
		                    if(interiorScene == INT_HUNTER) return 6.8f;
		                    if(interiorScene == INT_INN) return 7.2f;
		                    if(interiorScene == INT_TEMPLE) return 9.5f;
		                    if(interiorScene == INT_CASTLE) return 12.2f;
		                    return 5.5f;
		                };
	                auto exitPos = [&]()->Vector3{
	                    if(interiorKind == 0){
	                        if(interiorScene == INT_CASTLE) return {0, baseY, 12.2f};
	                        return {0, baseY, interiorMaxZ()-0.3f};
	                    }
	                    return {0, 0.0f, 12.2f};
	                };

	                float d = Vector3Distance(playerPos, exitPos());
	                if(d < 3.0f) consider(d, std::string(TextFormat("F/E  OPUSTIT (%.1fm)", d)), SKYRIM_GOLD);

			                if(interiorKind==0 && interiorScene != INT_CASTLE){
			                    Vector3 npcPos = {0, baseY, -3.6f};
			                    if(interiorScene == INT_INN) npcPos = {0, baseY, -4.4f};
			                    float nd = Vector3Distance(playerPos, npcPos);
			                    if(nd < 2.6f){
			                        if(interiorScene == INT_BLACKSMITH) consider(nd, std::string(TextFormat("F  ROZPRAVAT  |  S  OBCHOD  |  C  KOVAC (%.1fm)", nd)), SKYRIM_TEXT);
			                        else if(interiorScene == INT_ALCHEMIST) consider(nd, std::string(TextFormat("F  ROZPRAVAT  |  S  OBCHOD  |  C  ALCHEMY (%.1fm)", nd)), SKYRIM_TEXT);
			                        else if(interiorScene == INT_INN) consider(nd, std::string(TextFormat("F  ROZPRAVAT  |  S  HOSTINEC (%.1fm)", nd)), SKYRIM_TEXT);
			                        else if(interiorScene == INT_HUNTER) consider(nd, std::string(TextFormat("F  ROZPRAVAT  |  S  OBCHOD (%.1fm)", nd)), SKYRIM_TEXT);
			                        else if(interiorScene == INT_GENERAL) consider(nd, std::string(TextFormat("S  OBCHOD (%.1fm)", nd)), SKYRIM_TEXT);
			                    }
			                }
			                if(interiorKind==0 && interiorScene == INT_INN){
			                    Vector3 sleepSpot = {7.2f, baseY, 5.2f};
			                    float sd = Vector3Distance(playerPos, sleepSpot);
			                    if(sd < 2.6f){
			                        consider(sd, std::string(TextFormat("F/E  PRESPAT (10 gold) (%.1fm)", sd)), SKYRIM_GOLD);
			                    }
			                }
			                if(interiorKind==0 && interiorScene == INT_TEMPLE){
			                    Vector3 altarPos = {0.0f, baseY, -6.6f};
			                    float ad = Vector3Distance(playerPos, altarPos);
			                    if(ad < 3.0f){
		                        if(!templeBlessingTaken) consider(ad, std::string(TextFormat("F  MODLIT SA PRI OLTARI (%.1fm)", ad)), SKYRIM_GOLD);
		                        else consider(ad, std::string(TextFormat("OLTAR (%.1fm)", ad)), SKYRIM_MUTED);
		                    }
		                }

		                if(interiorKind==0 && interiorScene == INT_CASTLE){
		                    Vector3 stairs = {9.2f, baseY, 4.2f};
		                    Vector3 towerDoor = {-9.5f, baseY, -9.5f};
	                    float sd = Vector3Distance(playerPos, stairs);
	                    float td = Vector3Distance(playerPos, towerDoor);
	                    float cd = Vector3Distance(playerPos, {0, baseY, 0});
	                    if(interiorLevel==0 && sd < 2.4f) consider(sd, std::string(TextFormat("F/E  NA BALKON (%.1fm)", sd)), SKYRIM_GOLD);
	                    if(interiorLevel==1 && sd < 2.4f) consider(sd, std::string(TextFormat("F/E  DO SALY (%.1fm)", sd)), SKYRIM_GOLD);
	                    if(interiorLevel==1 && td < 2.6f) consider(td, std::string(TextFormat("F/E  DO VEZE (%.1fm)", td)), SKYRIM_GOLD);
	                    if(interiorLevel==2 && cd < 2.4f) consider(cd, std::string(TextFormat("F/E  SPAT NA POSCHODIE (%.1fm)", cd)), SKYRIM_GOLD);
	                }
	                if(interiorKind==1 && dungeonBossDefeated && !dungeonRewardTaken){
	                    float rd = Vector3Distance(playerPos, dungeonRewardPos);
	                    if(rd < 3.0f) consider(rd, std::string(TextFormat("F  OTVORIT ODMENU (%.1fm)", rd)), MY_GOLD);
	                }
	                if(interiorKind==1 && !dungeonLeverPulled){
	                    Vector3 leverPos = {4.0f, 0.0f, -9.0f};
	                    float ld = Vector3Distance(playerPos, leverPos);
	                    if(ld < 3.0f) consider(ld, std::string(TextFormat("F  POTIAHNUT PAKU (%.1fm)", ld)), SKYRIM_GOLD);
	                }
	            } else {
                float npcDist = Vector3Distance(playerPos, oldMan.position);
                if(npcDist < 3.0f) consider(npcDist, std::string(TextFormat("F  ROZPRAVAT: %s (%.1fm)", oldMan.name.c_str(), npcDist)), SKYRIM_TEXT);
                float smithDist = Vector3Distance(playerPos, blacksmith.position);
                if(smithDist < 3.0f) consider(smithDist, std::string(TextFormat("F  ROZPRAVAT  |  S  OBCHOD: %s (%.1fm)", blacksmith.name.c_str(), smithDist)), SKYRIM_TEXT);
                float guardDist = Vector3Distance(playerPos, guard.position);
                if(guardDist < 3.0f) consider(guardDist, std::string(TextFormat("F  STRAZCA: %s (%.1fm)", guard.name.c_str(), guardDist)), SKYRIM_TEXT);
                float innDist = Vector3Distance(playerPos, innkeeper.position);
                if(innDist < 3.0f) consider(innDist, std::string(TextFormat("F  HOSTINEC: %s (%.1fm)", innkeeper.name.c_str(), innDist)), SKYRIM_TEXT);
                float alchDist = Vector3Distance(playerPos, alchemist.position);
                if(alchDist < 3.0f) consider(alchDist, std::string(TextFormat("F  ALCHEMY: %s (%.1fm)  |  C  VARENIE", alchemist.name.c_str(), alchDist)), SKYRIM_TEXT);
                float huntDist = Vector3Distance(playerPos, hunter.position);
                if(huntDist < 3.0f) consider(huntDist, std::string(TextFormat("F  LOVEC: %s (%.1fm)", hunter.name.c_str(), huntDist)), SKYRIM_TEXT);

	                float caveDist = Vector3Distance(playerPos, dungeonEntrance);
	                if(caveDist < 3.0f) consider(caveDist, std::string(TextFormat("F  VSTUPIT DO JASKYNE (%.1fm)", caveDist)), GRAY);

	                float templeDist = Vector3Distance(playerPos, templeDoorPos);
	                if(templeDist < 3.0f) consider(templeDist, std::string(TextFormat("F  VSTUPIT DO CHRAMU (%.1fm)", templeDist)), SKYRIM_GOLD);

	                float doorMin = 999.0f;
	                for(auto& h: cityHouses){
	                    Vector3 door = GetHouseDoorPos(h);
                    float d = Vector3Distance(playerPos, door);
                    if(d < doorMin) doorMin = d;
                }
                if(doorMin < 3.0f) consider(doorMin, std::string(TextFormat("F  VSTUPIT  |  S  OBCHOD (%.1fm)", doorMin)), SKYRIM_GOLD);

                float gateDist = Vector3Distance(playerPos, gatePos);
                if(gateDist < 6.0f){
                    consider(gateDist,
                        gateUnlocked ? "BRANA: OTVORENA" : "BRANA: ZATVORENA (porozpravaj sa so strazcom)",
                        gateUnlocked ? SKYRIM_GOLD : SKYRIM_MUTED);
                }
            }

            if(!prompt.empty() && bestDist < 3.0f){
                int fs = 18;
                int w = MeasureText(prompt.c_str(), fs);
                DrawText(prompt.c_str(), screenWidth/2 - w/2, screenHeight/2 + 44, fs, promptCol);
            }
        }

        // --- Notifikacie ---
        if(!anyDialogue && !intro.active){
            int nY = 50;
            for(int i=0; i<(int)notifications.size(); i++){
                DrawRectangle(screenWidth-360, nY, 330, 26, Fade(BLACK,0.6f));
                DrawRectangleLines(screenWidth-360, nY, 330, 26, SKYRIM_PANEL_EDGE);
                DrawText(notifications[i].text.c_str(), screenWidth-350, nY+5, 14, SKYRIM_TEXT);
                nY += 30;
            }
        }

        // --- NPC dialog (Skyrim-like) ---
        if(oldMan.isTalking || guard.isTalking || blacksmith.isTalking || innkeeper.isTalking || alchemist.isTalking || hunter.isTalking){
            if(!dlgNpcName.empty()){
                DrawDialoguePanel(screenWidth, screenHeight, dlgNpcName, dlgNpcLine, dlgOptions, dlgEnabled);
            }
        }

	        if(showShop || shopAnim > 0.01f){
	            float a = Clamp(shopAnim, 0.0f, 1.0f);
	            Rectangle panel = {screenWidth/2.0f - 320.0f, 120.0f + (1.0f-a)*26.0f, 640.0f, 520.0f};
	            DrawRectangleRec(panel, Fade(BLACK,0.88f*a));
		            DrawRectangleLinesEx(panel, 2, Fade(SKYRIM_PANEL_EDGE, a));
		            DrawText(TextFormat("%s - OBCHOD", activeShopName.c_str()), panel.x+20, panel.y+20, 24, SKYRIM_TEXT);
		            DrawText(TextFormat("GOLD: %i", gold), panel.x+460, panel.y+24, 18, SKYRIM_GOLD);

		            Faction shopFaction = Faction::TOWNSFOLK;
		            if(activeShopIndex == 0) shopFaction = Faction::SMITHS;
		            else if(activeShopIndex == 1) shopFaction = Faction::ALCHEMISTS;
		            else if(activeShopIndex == 2) shopFaction = Faction::HUNTERS;
		            else if(activeShopIndex == 4) shopFaction = Faction::TOWNSFOLK;
		            int rep = factionRep[(int)shopFaction];
		            float speechT = Clamp(((float)skills[(int)Skill::SPEECH].level - 15.0f)/85.0f, 0.0f, 1.0f);
		            float priceMult = RepPriceMult(rep) * (1.0f - 0.10f*speechT);
		            DrawText(TextFormat("REP (%s): %i", FactionName(shopFaction), rep), panel.x+20, panel.y+48, 14, SKYRIM_MUTED);
		            DrawText(TextFormat("SPEECH: %i", skills[(int)Skill::SPEECH].level), panel.x+460, panel.y+48, 14, SKYRIM_MUTED);

		            for(int i=0; i<(int)activeShopItems.size(); i++){
		                Rectangle row = {panel.x+20, panel.y+70 + i*48.0f, 400, 40};
		                bool hover = CheckCollisionPointRec(mousePos, row);
		                DrawRectangleRec(row, (i==selectedShopIdx)?Fade(SKYRIM_GOLD,0.3f):(hover?Fade(SKYRIM_GOLD,0.2f):Fade(BLACK,0.3f)));
	                DrawText(activeShopItems[i].name.c_str(), row.x+10, row.y+10, 18, SKYRIM_TEXT);
	                int finalPrice = (int)roundf((float)activeShopItems[i].price * priceMult);
	                if(finalPrice < 1) finalPrice = 1;
	                DrawText(TextFormat("%i gold", finalPrice), row.x+270, row.y+10, 18, SKYRIM_GOLD);
	                if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) selectedShopIdx = i;
	            }

            Rectangle info = {panel.x+440, panel.y+70, 180, 200};
		            DrawRectangleRec(info, Fade(BLACK,0.3f));
		            DrawRectangleLinesEx(info, 1, SKYRIM_PANEL_EDGE);
		            if(selectedShopIdx>=0){
		                Item &it = activeShopItems[selectedShopIdx];
		                int finalPrice = (int)roundf((float)it.price * priceMult);
		                if(finalPrice < 1) finalPrice = 1;
		                DrawText(it.name.c_str(), info.x+10, info.y+10, 18, SKYRIM_TEXT);
		                DrawText(it.description.c_str(), info.x+10, info.y+40, 14, SKYRIM_MUTED);
		                if(it.damage>0) DrawText(TextFormat("DMG: %i", it.damage), info.x+10, info.y+70, 14, RED);
		                if(it.heal>0) DrawText(TextFormat("HEAL: %i", it.heal), info.x+10, info.y+90, 14, GREEN);
		                DrawText(TextFormat("VAHA: %.1f", it.weight), info.x+10, info.y+110, 14, GRAY);
		                DrawText(TextFormat("CENA: %i", finalPrice), info.x+10, info.y+130, 14, SKYRIM_GOLD);

	                Rectangle buyBtn = {info.x+10, info.y+156, 160, 36};
	                bool hover = CheckCollisionPointRec(mousePos, buyBtn);
	                DrawRectangleRec(buyBtn, hover?SKYRIM_GOLD:SKYRIM_PANEL);
	                DrawRectangleLinesEx(buyBtn, 1, SKYRIM_PANEL_EDGE);
	                DrawText("KUPIT", buyBtn.x+50, buyBtn.y+8, 18, SKYRIM_TEXT);
	                if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
	                    if(gold >= finalPrice){
	                        gold -= finalPrice;
	                        if(it.name == "LOCKPICK"){
	                            lockpicks++;
	                        } else {
	                            inventory.push_back(it);
	                        }
	                        pushNote("Nakup uspesny.");
	                        AddSkillXP(skills[(int)Skill::SPEECH], 6.0f + (float)finalPrice*0.18f);
	                        if(finalPrice >= 40 && rep < 100) factionRep[(int)shopFaction] = std::min(100, rep + 1);
	                    } else {
	                        pushNote("Nedostatok gold.");
	                    }
	                }
		            } else {
		                DrawText("Vyber predmet", info.x+10, info.y+10, 16, SKYRIM_MUTED);
		            }

	            DrawText("ESC alebo F - zatvorit", panel.x+20, panel.y+480, 14, SKYRIM_MUTED);
	        }

	        if(showCrafting || craftingAnim > 0.01f){
	            float a = Clamp(craftingAnim, 0.0f, 1.0f);
	            Rectangle panel = {screenWidth/2.0f - 360.0f, 120.0f + (1.0f-a)*26.0f, 720.0f, 520.0f};
	            DrawRectangleRec(panel, Fade(BLACK,0.90f*a));
	            DrawRectangleLinesEx(panel, 2, Fade(SKYRIM_PANEL_EDGE, a));
	            DrawText(craftingMode==0 ? "KOVAC - CRAFTING" : "ALCHYMISTA - ALCHEMY", panel.x+20, panel.y+20, 24, SKYRIM_TEXT);
	            DrawText(TextFormat("GOLD: %i", gold), panel.x+560, panel.y+24, 18, SKYRIM_GOLD);

            struct Req { const char* name; int count; };
            struct RecipeUI { const char* title; int goldCost; bool isUpgrade; Item result; std::vector<Req> reqs; };
            std::vector<RecipeUI> recipes;
            recipes.reserve(4);

            if(craftingMode==0){
                recipes.push_back({"UPGRADE WEAPON (+5 DMG)", 25, true, {"","NONE",0,0,0,0.0f,false,""}, {{"ZELEZNA RUDA",2},{"KOZA",1}}});
                recipes.push_back({"FORGE: ZELEZNY MEC", 60, false, {"ZELEZNY MEC","WEAPON",25,0,0,10.0f,false,"Pevny mec."}, {{"ZELEZNA RUDA",4},{"KOZA",1}}});
            } else {
                recipes.push_back({"VARIT: ELIXIR HP (+50)", 0, false, {"ELIXIR HP","POTION",0,50,0,0.4f,false,"Obnovuje zdravie."}, {{"MODRY KVET",1},{"PSENICA",1}}});
                recipes.push_back({"VARIT: ELIXIR MANY", 0, false, {"ELIXIR MANY","POTION",0,0,0,0.4f,false,"Obnovi manu."}, {{"MODRY KVET",2}}});
            }

            Rectangle list = {panel.x+20, panel.y+70, 420, 380};
            DrawRectangleRec(list, Fade(BLACK,0.35f));
            DrawRectangleLinesEx(list, 1, SKYRIM_PANEL_EDGE);
            for(int i=0;i<(int)recipes.size();i++){
                Rectangle row = {list.x+10, list.y+10 + i*54.0f, list.width-20, 46};
                bool hover = CheckCollisionPointRec(mousePos, row);
                bool sel = (i==selectedRecipeIdx);
                DrawRectangleRec(row, sel ? Fade(SKYRIM_GOLD,0.25f) : (hover ? Fade(SKYRIM_GOLD,0.18f) : Fade(BLACK,0.18f)));
                DrawRectangleLinesEx(row, 1, Fade(SKYRIM_PANEL_EDGE,0.7f));
                DrawText(recipes[i].title, (int)row.x+10, (int)row.y+12, 18, SKYRIM_TEXT);
                if(recipes[i].goldCost > 0){
                    DrawText(TextFormat("%i g", recipes[i].goldCost), (int)(row.x + row.width - 60), (int)row.y+12, 18, SKYRIM_GOLD);
                }
                if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) selectedRecipeIdx = i;
            }

            Rectangle info = {panel.x+460, panel.y+70, 240, 380};
            DrawRectangleRec(info, Fade(BLACK,0.35f));
            DrawRectangleLinesEx(info, 1, SKYRIM_PANEL_EDGE);

            if(selectedRecipeIdx >= 0 && selectedRecipeIdx < (int)recipes.size()){
                RecipeUI &r = recipes[selectedRecipeIdx];
                DrawText("REQUIRES:", (int)info.x+12, (int)info.y+12, 18, SKYRIM_GOLD);
                int y = (int)info.y + 42;
                bool hasAll = true;
                std::string eqName;
                if(r.isUpgrade){
                    for(const auto& it : inventory) if(it.isEquipped && it.category=="WEAPON") eqName = it.name;
                    if(eqName.empty()) hasAll = false;
                }
                for(const auto& req : r.reqs){
                    int have = InventoryCountItem(inventory, req.name);
                    Color c = (have >= req.count) ? SKYRIM_TEXT : RED;
                    if(have < req.count) hasAll = false;
                    DrawText(TextFormat("%s  %i/%i", req.name, have, req.count), (int)info.x+12, y, 16, c);
                    y += 20;
                }
                if(r.goldCost > 0){
                    Color c = (gold >= r.goldCost) ? SKYRIM_GOLD : RED;
                    if(gold < r.goldCost) hasAll = false;
                    DrawText(TextFormat("GOLD  %i/%i", gold, r.goldCost), (int)info.x+12, y+6, 16, c);
                }
                if(r.isUpgrade && eqName.empty()){
                    DrawText("EQUIP WEAPON FIRST", (int)info.x+12, (int)(info.y + info.height - 98), 14, RED);
                }

                Rectangle craftBtn = {info.x+12, info.y + info.height - 54, info.width - 24, 40};
                bool hover = CheckCollisionPointRec(mousePos, craftBtn);
                DrawRectangleRec(craftBtn, (hover && hasAll) ? Fade(SKYRIM_GOLD,0.35f) : Fade(BLACK,0.35f));
                DrawRectangleLinesEx(craftBtn, 2, Fade(SKYRIM_PANEL_EDGE,0.8f));
                DrawText("CRAFT", (int)craftBtn.x + 80, (int)craftBtn.y + 10, 18, hasAll ? SKYRIM_TEXT : SKYRIM_MUTED);

                if(hover && hasAll && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    if(r.goldCost > 0) gold -= r.goldCost;
                    bool ok = true;
                    for(const auto& req : r.reqs){
                        ok = ok && InventoryRemoveItemN(inventory, req.name, req.count);
                    }
	                        if(ok){
	                            if(r.isUpgrade){
	                            AddWeaponUpgradeBonus(weaponUpgrades, eqName, 5, 20);
	                            weaponUpgradesDone++;
	                            pushNote("Zbraň vylepšená (+5 dmg).");
	                            AddSkillXP(skills[(int)Skill::SMITHING], 18.0f);
	                            int& rep = factionRep[(int)Faction::SMITHS];
	                            rep = std::min(100, rep + 1);
	                        } else {
	                            inventory.push_back(r.result);
	                            if(r.result.category == "POTION") potionsBrewed++;
	                            pushNote("Vyrobene.");
	                            if(craftingMode == 0) AddSkillXP(skills[(int)Skill::SMITHING], 14.0f);
	                            if(craftingMode == 1) AddSkillXP(skills[(int)Skill::ALCHEMY], 14.0f);
	                            if(craftingMode == 1){
	                                int& rep = factionRep[(int)Faction::ALCHEMISTS];
	                                rep = std::min(100, rep + 1);
	                            }
	                        }
	                    } else {
	                        pushNote("Chyba: suroviny.");
	                    }
	                }
            } else {
                DrawText("Vyber recept", (int)info.x+12, (int)info.y+12, 18, SKYRIM_MUTED);
            }

            DrawText("ESC alebo F - zatvorit", panel.x+20, panel.y+480, 14, SKYRIM_MUTED);
        }

        if(lockpick.active){
            DrawLockpickUI(screenWidth, screenHeight, lockpick, lockpicks, lockpick.difficulty);
        }

        // --- Mini-mapa ---
        if(minimapEnabled){
            DrawRectangle(screenWidth-160,10,150,150,Fade(BLACK,0.5f));
            DrawRectangleLines(screenWidth-160,10,150,150,GRAY);
            float miniHalf=75.0f;
            float mapX=screenWidth-85+(playerPos.x/HALF_WORLD)*miniHalf;
            float mapZ=85+(playerPos.z/HALF_WORLD)*miniHalf;
            DrawCircle(mapX,mapZ,4,PURPLE);
            for(auto& c:chests) if(c.active) DrawCircle(screenWidth-85+(c.position.x/HALF_WORLD)*miniHalf,85+(c.position.z/HALF_WORLD)*miniHalf,3,GOLD);
            for(auto& e:enemies) if(e.active) DrawCircle(screenWidth-85+(e.position.x/HALF_WORLD)*miniHalf,85+(e.position.z/HALF_WORLD)*miniHalf,3,RED);
            DrawCircle(screenWidth-85+(oldMan.position.x/HALF_WORLD)*miniHalf,85+(oldMan.position.z/HALF_WORLD)*miniHalf,4,SKYRIM_GOLD);
            DrawCircle(screenWidth-85+(blacksmith.position.x/HALF_WORLD)*miniHalf,85+(blacksmith.position.z/HALF_WORLD)*miniHalf,4,SKYRIM_GOLD);
            DrawCircle(screenWidth-85+(guard.position.x/HALF_WORLD)*miniHalf,85+(guard.position.z/HALF_WORLD)*miniHalf,3,BLUE);
            DrawCircle(screenWidth-85+(patrolGuard.position.x/HALF_WORLD)*miniHalf,85+(patrolGuard.position.z/HALF_WORLD)*miniHalf,3,SKYRIM_GOLD);
            for(auto& h: cityHouses){
                DrawCircle(screenWidth-85+(h.position.x/HALF_WORLD)*miniHalf,85+(h.position.z/HALF_WORLD)*miniHalf,2,GRAY);
            }
        }

	        // --- World map + fast travel ---
	        if(showWorldMap || worldMapAnim > 0.01f){
	            float a = Clamp(worldMapAnim, 0.0f, 1.0f);
	            bool mapInteractive = showWorldMap && a > 0.85f;

	            DrawRectangleGradientV(0,0,screenWidth,screenHeight,Fade(BLACK,0.92f*a),Fade(BLACK,0.18f*a));
	            DrawVignette(screenWidth, screenHeight);

	            Rectangle mapRec = {screenWidth/2.0f - 380.0f, 90.0f + (1.0f-a)*24.0f, 760.0f, 560.0f};
	            DrawRectangleRec(mapRec, Fade(BLACK,0.55f*a));
	            DrawRectangleLinesEx(mapRec, 2, Fade(SKYRIM_PANEL_EDGE,0.9f*a));
	            DrawRectangleGradientV((int)mapRec.x, (int)mapRec.y, (int)mapRec.width, 60, Fade(SKYRIM_GLOW, 0.10f*a), Fade(BLANK,0.0f));
	            DrawText("MAP", (int)mapRec.x + 20, (int)mapRec.y + 16, 26, Fade(SKYRIM_TEXT,a));
	            DrawText("Klik na objaveny POI = fast travel", (int)mapRec.x + 20, (int)mapRec.y + 46, 16, Fade(SKYRIM_MUTED,a));
	            DrawText("ESC alebo M - zavriet", (int)mapRec.x + (int)mapRec.width - 210, (int)mapRec.y + 20, 16, Fade(SKYRIM_MUTED,a));

            auto ToMap = [&](Vector3 p){
                float u = (p.x + HALF_WORLD)/WORLD_SIZE;
                float v = (p.z + HALF_WORLD)/WORLD_SIZE;
                u = Clamp(u, 0.0f, 1.0f);
                v = Clamp(v, 0.0f, 1.0f);
                return Vector2{mapRec.x + u*mapRec.width, mapRec.y + v*mapRec.height};
            };

	            // simple grid
	            for(int gx=1; gx<10; gx++){
	                float x = mapRec.x + mapRec.width*(float)gx/10.0f;
	                DrawLine((int)x, (int)mapRec.y, (int)x, (int)(mapRec.y + mapRec.height), Fade(SKYRIM_PANEL_EDGE,0.12f*a));
	            }
	            for(int gy=1; gy<8; gy++){
	                float y = mapRec.y + mapRec.height*(float)gy/8.0f;
	                DrawLine((int)mapRec.x, (int)y, (int)(mapRec.x + mapRec.width), (int)y, Fade(SKYRIM_PANEL_EDGE,0.12f*a));
	            }

	            // player marker
	            Vector2 pMap = ToMap(playerPos);
	            DrawCircleV(pMap, 6, Fade(PURPLE, a));
	            DrawCircleLines((int)pMap.x, (int)pMap.y, 8, Fade(WHITE,0.55f*a));

	            // POIs
	            for(int i=0;i<(int)worldPOIs.size();i++){
	                POI &poi = worldPOIs[i];
	                if(!poi.discovered) continue;
	                Vector2 m = ToMap(poi.position);
	                bool hover = mapInteractive && CheckCollisionPointCircle(mousePos, m, 10.0f);
	                float pulse = 0.75f + 0.25f*sinf((float)GetTime()*4.0f + (float)i);
	                DrawCircleV(m, hover ? 7.0f : 6.0f, Fade(poi.color, a));
	                DrawCircleLines((int)m.x, (int)m.y, hover ? 10.0f : 8.0f, Fade(poi.color, (hover?0.85f:0.55f)*a*pulse));
	                DrawText(poi.name.c_str(), (int)m.x + 10, (int)m.y - 8, 16, Fade(SKYRIM_TEXT,a));

	                if(!inInterior && poi.canFastTravel){
	                    if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
	                        pendingTeleport = true;
	                        pendingTeleportPos = poi.position;
	                        showLoading = true;
	                        loadingTotal = 1.0f;
	                        loadingTimer = loadingTotal;
	                        showWorldMap = false;
	                        showMenu = false;
	                        showShop = false;
	                    }
	                }
	            }
	        }


        // --- MENU (TAB): ikony + inventory + skill tree ---
        if(showMenu){
            DrawRectangleGradientV(0,0,screenWidth,screenHeight,Fade(BLACK,0.9f),Fade(BLACK,0.2f));

            if(menuView == MenuView::HOME){
                DrawText("MENU", 60, 60, 28, SKYRIM_TEXT);
                DrawLine(60, 92, screenWidth-60, 92, SKYRIM_PANEL_EDGE);
                DrawVignette(screenWidth, screenHeight);
                DrawFogOverlay(screenWidth, screenHeight, (float)GetTime());

                Rectangle invIcon = {(float)screenWidth/2 - 340.0f, (float)screenHeight/2 - 90.0f, 220.0f, 180.0f};
                Rectangle questIcon = {(float)screenWidth/2 - 110.0f, (float)screenHeight/2 - 90.0f, 220.0f, 180.0f};
                Rectangle skillIcon = {(float)screenWidth/2 + 120.0f, (float)screenHeight/2 - 90.0f, 220.0f, 180.0f};

                bool invHover = CheckCollisionPointRec(mousePos, invIcon);
                bool questHover = CheckCollisionPointRec(mousePos, questIcon);
                bool skillHover = CheckCollisionPointRec(mousePos, skillIcon);

                DrawRectangleRec(invIcon, invHover ? SKYRIM_GOLD : SKYRIM_PANEL);
                DrawRectangleLinesEx(invIcon, 2, SKYRIM_PANEL_EDGE);
                DrawText("INVENTORY", invIcon.x + 30, invIcon.y + 20, 22, SKYRIM_TEXT);
                DrawRectangleLines((int)invIcon.x + 30, (int)invIcon.y + 70, 50, 40, SKYRIM_PANEL_EDGE);
                DrawRectangleLines((int)invIcon.x + 90, (int)invIcon.y + 70, 50, 40, SKYRIM_PANEL_EDGE);
                DrawRectangleLines((int)invIcon.x + 150, (int)invIcon.y + 70, 50, 40, SKYRIM_PANEL_EDGE);

                DrawRectangleRec(questIcon, questHover ? SKYRIM_GOLD : SKYRIM_PANEL);
                DrawRectangleLinesEx(questIcon, 2, SKYRIM_PANEL_EDGE);
                DrawText("QUESTS", questIcon.x + 55, questIcon.y + 20, 22, SKYRIM_TEXT);
                DrawLine((int)questIcon.x + 40, (int)questIcon.y + 80, (int)questIcon.x + 180, (int)questIcon.y + 80, SKYRIM_PANEL_EDGE);
                DrawLine((int)questIcon.x + 40, (int)questIcon.y + 105, (int)questIcon.x + 160, (int)questIcon.y + 105, SKYRIM_PANEL_EDGE);
                DrawCircleLines((int)questIcon.x + 170, (int)questIcon.y + 110, 6, SKYRIM_PANEL_EDGE);

                DrawRectangleRec(skillIcon, skillHover ? SKYRIM_GOLD : SKYRIM_PANEL);
                DrawRectangleLinesEx(skillIcon, 2, SKYRIM_PANEL_EDGE);
                DrawText("SKILL TREE", skillIcon.x + 30, skillIcon.y + 20, 22, SKYRIM_TEXT);
                DrawCircleLines((int)skillIcon.x + 70, (int)skillIcon.y + 95, 18, SKYRIM_PANEL_EDGE);
                DrawCircleLines((int)skillIcon.x + 140, (int)skillIcon.y + 120, 18, SKYRIM_PANEL_EDGE);
                DrawLine((int)skillIcon.x + 88, (int)skillIcon.y + 102, (int)skillIcon.x + 122, (int)skillIcon.y + 113, SKYRIM_PANEL_EDGE);

                if(invHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::INVENTORY;
                }
                if(questHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::QUESTS;
                    selectedQuestIdx = -1;
                }
                if(skillHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::SKILL_TREE;
                }
            } else if(menuView == MenuView::INVENTORY){
                Rectangle backBtn = {50,50,120,36};
                DrawRectangleRec(backBtn, SKYRIM_PANEL_EDGE);
                DrawText("BACK", backBtn.x+30, backBtn.y+8, 20, SKYRIM_BG_BOT);
                if(CheckCollisionPointRec(mousePos, backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::HOME;
                }

                DrawRectangleGradientH(0,0,screenWidth,screenHeight,Fade(BLACK,0.92f),Fade(BLACK,0.2f));
                DrawVignette(screenWidth, screenHeight);
                DrawText("INVENTORY", 50, 105, 28, SKYRIM_TEXT);
                DrawLine(50, 138, screenWidth-50, 138, Fade(SKYRIM_PANEL_EDGE, 0.8f));

                float totalWeight = 0.0f;
                for(const auto& it : inventory) totalWeight += it.weight;
                float carryCap = 120.0f + playerMaxStamina*0.60f;
                DrawText(TextFormat("CARRY: %.1f / %.0f", totalWeight, carryCap), 50, 150, 18, (totalWeight > carryCap) ? Fade(RED,0.9f) : SKYRIM_MUTED);
                DrawText(TextFormat("GOLD: %i", gold), screenWidth-190, 150, 18, SKYRIM_GOLD);

                // --- KATEGÓRIE INVENTÁRA ---
                for(int i=0;i<4;i++){
                    Rectangle catRec = {50.0f + i*140.0f, 175.0f, 130.0f, 32.0f};
                    bool hover = CheckCollisionPointRec(mousePos, catRec);
                    bool sel = (i == selectedCategory);
                    DrawRectangleRec(catRec, sel ? Fade(SKYRIM_GOLD, 0.25f) : Fade(BLACK, hover ? 0.35f : 0.25f));
                    DrawRectangleLinesEx(catRec, 1, sel ? SKYRIM_GOLD : Fade(SKYRIM_PANEL_EDGE, 0.7f));
                    DrawText(catNames[i].c_str(), (int)catRec.x + 10, (int)catRec.y + 8, 18, sel ? SKYRIM_GOLD : SKYRIM_TEXT);
                    if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                        selectedCategory = i;
                        selectedItemIdx = -1;
                        invScroll = 0;
                    }
                }

                // filtrovanie podľa kategórie
                std::vector<int> fil;
                fil.reserve(inventory.size());
                for(int i=0;i<(int)inventory.size();i++){
                    if(selectedCategory==0 ||
                        (selectedCategory==1 && inventory[i].category=="WEAPON") ||
                        (selectedCategory==2 && inventory[i].category=="MAGIC") ||
                        (selectedCategory==3 && (inventory[i].category=="POTION" || inventory[i].category=="BOOK"))){
                        fil.push_back(i);
                    }
                }

                Rectangle listPanel = {50, 220, 560, (float)screenHeight - 290.0f};
                Rectangle infoPanel = {630, 220, (float)screenWidth - 680.0f, (float)screenHeight - 290.0f};
                DrawRectangleRec(listPanel, Fade(BLACK, 0.30f));
                DrawRectangleLinesEx(listPanel, 2, Fade(SKYRIM_PANEL_EDGE, 0.75f));
                DrawRectangleRec(infoPanel, Fade(BLACK, 0.30f));
                DrawRectangleLinesEx(infoPanel, 2, Fade(SKYRIM_PANEL_EDGE, 0.75f));

                // scroll
                if(CheckCollisionPointRec(mousePos, listPanel)){
                    float wheel = GetMouseWheelMove();
                    if(wheel != 0.0f){
                        invScroll -= (int)wheel;
                    }
                }

                const float rowH = 34.0f;
                int visibleRows = (int)floorf((listPanel.height - 38.0f)/rowH);
                if(visibleRows < 1) visibleRows = 1;
                int maxScroll = (int)fil.size() - visibleRows;
                if(maxScroll < 0) maxScroll = 0;
                invScroll = std::max(0, std::min(invScroll, maxScroll));

                // header
                DrawText("NAME", (int)listPanel.x + 14, (int)listPanel.y + 10, 16, SKYRIM_MUTED);
                DrawText("WG", (int)(listPanel.x + listPanel.width - 120), (int)listPanel.y + 10, 16, SKYRIM_MUTED);
                DrawText("VAL", (int)(listPanel.x + listPanel.width - 65), (int)listPanel.y + 10, 16, SKYRIM_MUTED);
                DrawLine((int)listPanel.x+10, (int)listPanel.y+30, (int)(listPanel.x+listPanel.width-10), (int)listPanel.y+30, Fade(SKYRIM_PANEL_EDGE,0.6f));

                int selectedFilPos = -1;
                for(int i=0;i<(int)fil.size();i++){
                    if(fil[i] == selectedItemIdx){
                        selectedFilPos = i;
                        break;
                    }
                }
                if(IsKeyPressed(KEY_DOWN) && !fil.empty()){
                    if(selectedFilPos < 0) selectedFilPos = 0;
                    else selectedFilPos = std::min(selectedFilPos + 1, (int)fil.size()-1);
                    selectedItemIdx = fil[selectedFilPos];
                }
                if(IsKeyPressed(KEY_UP) && !fil.empty()){
                    if(selectedFilPos < 0) selectedFilPos = 0;
                    else selectedFilPos = std::max(selectedFilPos - 1, 0);
                    selectedItemIdx = fil[selectedFilPos];
                }
                if(selectedFilPos >= 0){
                    if(selectedFilPos < invScroll) invScroll = selectedFilPos;
                    if(selectedFilPos >= invScroll + visibleRows) invScroll = selectedFilPos - visibleRows + 1;
                }

                // rows
                int start = invScroll;
                int end = std::min((int)fil.size(), invScroll + visibleRows);
	                for(int i=start; i<end; i++){
	                    int idx = fil[i];
	                    Rectangle r = {listPanel.x + 10.0f, listPanel.y + 38.0f + (i-start)*rowH, listPanel.width - 20.0f, rowH-2.0f};
	                    bool hover = CheckCollisionPointRec(mousePos, r);
	                    bool sel = (idx == selectedItemIdx);
	                    DrawRectangleRec(r, sel ? Fade(SKYRIM_GOLD, 0.18f) : (hover ? Fade(SKYRIM_GOLD, 0.10f) : Fade(BLACK, 0.18f)));
	                    if(sel) DrawRectangleLinesEx(r, 1, Fade(SKYRIM_GOLD, 0.65f));

	                    const Item& it = inventory[idx];
	                    Color nameCol = it.isEquipped ? SKYRIM_GOLD : SKYRIM_TEXT;
	                    DrawItemIcon2D(it, (int)r.x + 8, (int)r.y + 5, 24);
	                    DrawText(it.name.c_str(), (int)r.x + 38, (int)r.y + 7, 18, nameCol);
	                    if(it.isEquipped) DrawText("E", (int)(r.x + r.width - 165), (int)r.y + 7, 18, SKYRIM_GOLD);
	                    DrawText(TextFormat("%.1f", it.weight), (int)(r.x + r.width - 120), (int)r.y + 7, 18, SKYRIM_MUTED);
	                    DrawText(TextFormat("%i", it.price), (int)(r.x + r.width - 65), (int)r.y + 7, 18, SKYRIM_GOLD);

                    if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                        selectedItemIdx = idx;
                    }
                }

	                // details
	                if(selectedItemIdx >= 0 && selectedItemIdx < (int)inventory.size()){
	                    Item &s = inventory[selectedItemIdx];

	                    DrawText(s.name.c_str(), (int)infoPanel.x + 14, (int)infoPanel.y + 12, 24, SKYRIM_TEXT);
	                    DrawLine((int)infoPanel.x + 12, (int)infoPanel.y + 42, (int)(infoPanel.x + infoPanel.width - 12), (int)infoPanel.y + 42, Fade(SKYRIM_PANEL_EDGE,0.6f));

	                    float previewSize = fminf(200.0f, infoPanel.width - 40.0f);
	                    Rectangle previewRec = {infoPanel.x + infoPanel.width - previewSize - 14.0f, infoPanel.y + 56.0f, previewSize, previewSize};
	                    Rectangle descRec = {infoPanel.x + 14, infoPanel.y + 56, infoPanel.width - 28 - (previewSize + 12.0f), 92};
	                    if(descRec.width < 200.0f){
	                        // panel too narrow: move preview below description
	                        descRec.width = infoPanel.width - 28;
	                        previewRec = {infoPanel.x + 14.0f, infoPanel.y + 156.0f, previewSize, previewSize};
	                    }
	                    DrawTextWrapped(s.description, descRec, 16, SKYRIM_MUTED);

	                    // 3D preview
	                    RenderInventoryPreview(invPreviewRT, s, (float)GetTime());
	                    DrawRectangleRec(previewRec, Fade(BLACK, 0.20f));
	                    DrawRectangleLinesEx(previewRec, 2, Fade(SKYRIM_PANEL_EDGE, 0.75f));
	                    Rectangle src = {0, 0, (float)invPreviewRT.texture.width, (float)-invPreviewRT.texture.height};
	                    DrawTexturePro(invPreviewRT.texture, src, previewRec, {0,0}, 0.0f, WHITE);
	                    DrawText("PREVIEW", (int)previewRec.x + 10, (int)previewRec.y + 10, 14, Fade(SKYRIM_MUTED,0.95f));

	                    int y = (int)infoPanel.y + 156;
	                    if(previewRec.y <= infoPanel.y + 60.0f){
	                        // preview on the right: keep stats below description
	                        y = (int)infoPanel.y + 156;
	                    } else {
	                        // preview moved below: stats go under preview
	                        y = (int)(previewRec.y + previewRec.height + 16.0f);
	                    }
	                    if(s.category=="WEAPON"){
	                        int up = GetWeaponUpgradeBonus(weaponUpgrades, s.name);
	                        if(up > 0) DrawText(TextFormat("DAMAGE: %i (+%i)", s.damage + up, up), (int)infoPanel.x + 14, y, 18, Fade(RED,0.9f));
	                        else DrawText(TextFormat("DAMAGE: %i", s.damage), (int)infoPanel.x + 14, y, 18, Fade(RED,0.9f));
	                        y += 24;
	                    }
                    if(s.category=="POTION") DrawText(TextFormat("HEAL: +%i", s.heal), (int)infoPanel.x + 14, y, 18, Fade(GREEN,0.9f)), y += 24;
                    DrawText(TextFormat("WEIGHT: %.1f", s.weight), (int)infoPanel.x + 14, y, 18, SKYRIM_MUTED), y += 24;
                    DrawText(TextFormat("VALUE: %i", s.price), (int)infoPanel.x + 14, y, 18, SKYRIM_GOLD), y += 24;
                    if(s.isEquipped) DrawText("EQUIPPED", (int)infoPanel.x + 14, y, 18, SKYRIM_GOLD);

                    Rectangle useBtn = {infoPanel.x + 14, infoPanel.y + infoPanel.height - 110, infoPanel.width - 28, 42};
                    Rectangle dropBtn = {infoPanel.x + 14, infoPanel.y + infoPanel.height - 60, infoPanel.width - 28, 42};
                    bool hoverUse = CheckCollisionPointRec(mousePos, useBtn);
                    bool hoverDrop = CheckCollisionPointRec(mousePos, dropBtn);
                    bool questItem = (s.name == storySigilName);
                    const char* lb = (questItem ? "INSPECT" :
                                     (s.category=="BOOK") ? "READ" :
                                     (s.category=="POTION" ? "USE" :
                                      (s.category=="OTHER" ? "TOGGLE" :
                                       (s.isEquipped ? "UNEQUIP" : "EQUIP"))));
                    DrawRectangleRec(useBtn, hoverUse ? Fade(SKYRIM_GOLD,0.35f) : Fade(BLACK,0.35f));
                    DrawRectangleLinesEx(useBtn, 2, Fade(SKYRIM_PANEL_EDGE,0.8f));
                    DrawText(lb, (int)useBtn.x + 16, (int)useBtn.y + 11, 18, SKYRIM_TEXT);
                    DrawRectangleRec(dropBtn, questItem ? Fade(BLACK,0.20f) : (hoverDrop ? Fade(RED,0.20f) : Fade(BLACK,0.35f)));
                    DrawRectangleLinesEx(dropBtn, 2, Fade(SKYRIM_PANEL_EDGE,0.8f));
                    DrawText(questItem ? "QUEST ITEM" : "DROP", (int)dropBtn.x + 16, (int)dropBtn.y + 11, 18, questItem ? SKYRIM_MUTED : SKYRIM_TEXT);

                    auto activateSelected = [&](){
                        if(questItem){
                            pushNote("Sigil je chladny na dotyk. Runy sa na okamih rozsvietia a potom zhasnu.");
                            return;
                        }
                        if(s.category=="BOOK"){
                            for(auto& it:inventory) if(it.category=="MAGIC") it.isEquipped=false;
                            inventory.push_back({"FIREBALL","MAGIC",40,0,0,0.0f,true,"Nasadena abilita (E)."});
                            inventory.erase(inventory.begin()+selectedItemIdx);
                            selectedItemIdx=-1;
                        } else if(s.category=="POTION"){
                            if(s.name=="ELIXIR MANY"){
                                playerMana = fminf(playerMana + 100.0f, playerMaxMana);
                                manaRegenDelay = 1.0f;
                            } else {
                                playerHealth=fminf(playerHealth+s.heal,playerMaxHealth);
                                hpRegenDelay = 1.0f;
                            }
                            inventory.erase(inventory.begin()+selectedItemIdx);
                            selectedItemIdx=-1;
                        } else if(s.category=="OTHER"){
                            if(s.name=="MAPA"){
                                minimapEnabled = !minimapEnabled;
                                pushNote(minimapEnabled ? "Mini mapa zapnuta." : "Mini mapa vypnuta.");
                            } else if(s.name=="TORCH"){
                                torchActive = !torchActive;
                                pushNote(torchActive ? "Pochoden zapnuta." : "Pochoden vypnuta.");
                            } else if(s.name=="NOCNY CIST"){
                                inventory.erase(inventory.begin()+selectedItemIdx);
                                selectedItemIdx=-1;
                            }
                        } else {
                            if(s.isEquipped) s.isEquipped=false;
                            else { for(auto& it:inventory) if(it.category==s.category) it.isEquipped=false; s.isEquipped=true; }
                        }
                    };

                    if((hoverUse && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_ENTER)){
                        activateSelected();
                    }
                    if(!questItem && ((hoverDrop && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_DELETE))){
                        inventory.erase(inventory.begin()+selectedItemIdx);
                        selectedItemIdx=-1;
                        pushNote("Predmet zahodený.");
                    }
                } else {
                    DrawText("Select an item", (int)infoPanel.x + 14, (int)infoPanel.y + 12, 20, SKYRIM_MUTED);
                }
            } else if(menuView == MenuView::SKILL_TREE){
                DrawRectangleGradientV(0,0,screenWidth,screenHeight,SKYRIM_BG_TOP,SKYRIM_BG_BOT);
                DrawRectangle(40, 100, screenWidth-80, screenHeight-180, SKYRIM_PANEL);
                DrawRectangleLinesEx({40,100,(float)screenWidth-80.0f,(float)screenHeight-180.0f}, 2, SKYRIM_PANEL_EDGE);
                DrawText("SKILL TREE", 60, 120, 28, SKYRIM_TEXT);
                DrawText(TextFormat("PERK POINTS: %i", perkPoints), 60, 160, 18, SKYRIM_TEXT);
                DrawLine(60, 155, screenWidth-60, 155, SKYRIM_PANEL_EDGE);
                DrawVignette(screenWidth, screenHeight);

                Rectangle backBtn = {50,50,120,36};
                DrawRectangleRec(backBtn, SKYRIM_PANEL_EDGE);
                DrawText("BACK", backBtn.x+30, backBtn.y+8, 20, SKYRIM_BG_BOT);
                if(CheckCollisionPointRec(mousePos, backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::HOME;
                }

                // Sekcie
                for(size_t i=0;i<skillSections.size();i++){
                    Rectangle r = {50.0f + (float)i*150.0f, (float)(screenHeight - 60), 140.0f, 40.0f};
                    Color tabCol = ((int)i==activeSkillSection)? SKYRIM_GOLD : SKYRIM_PANEL_EDGE;
                    DrawRectangleRec(r, tabCol);
                    DrawRectangleLinesEx(r, 1, SKYRIM_PANEL_EDGE);
                    DrawText(skillSections[i].c_str(), r.x+14, r.y+10, 20, SKYRIM_BG_BOT);
                    if(CheckCollisionPointRec(mousePos,r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                        activeSkillSection = (SkillSection)i;
                    }
                }

                // Perk tree
                for(size_t i=0;i<perks.size();i++){
                    Perk &p = perks[i];
                    if(p.section != activeSkillSection) continue;

                    {
                        for(int c: p.children){
                            Vector2 parentPos = p.pos;
                            Vector2 childPos = perks[c].pos;
                            Color linkCol = perks[c].unlocked ? SKYRIM_GOLD : SKYRIM_MUTED;
                            Vector2 mid = {(parentPos.x+childPos.x)/2.0f, (parentPos.y+childPos.y)/2.0f};
                            Vector2 control = {mid.x, mid.y - 60.0f};
                            Vector2 prev = parentPos;
                            const int segments = 18;
                            for(int s=1; s<=segments; s++){
                                float t = (float)s/(float)segments;
                                float it = 1.0f - t;
                                Vector2 pnt = {
                                    it*it*parentPos.x + 2.0f*it*t*control.x + t*t*childPos.x,
                                    it*it*parentPos.y + 2.0f*it*t*control.y + t*t*childPos.y
                                };
                                DrawLineEx(prev, pnt, 3.0f, linkCol);
                                prev = pnt;
                            }
                        }

                        bool isRoot = (p.levelReq==1 && !p.children.empty());
                        float nodeRadius = isRoot ? 30.0f : 24.0f;
                        bool hover = CheckCollisionPointCircle(mousePos,p.pos,nodeRadius);
                        Color coreCol = p.unlocked ? SKYRIM_GOLD : SKYRIM_MUTED;
                        float pulse = 0.4f + 0.6f*(sinf(GetTime()*2.0f)*0.5f + 0.5f);
                        Color glow = SKYRIM_GLOW;
                        glow.a = (unsigned char)(glow.a * pulse);
                        DrawCircleV(p.pos, nodeRadius + 6.0f, glow);
                        DrawCircleV(p.pos, nodeRadius, p.unlocked ? SKYRIM_GOLD : SKYRIM_PANEL);
                        DrawCircleLines((int)p.pos.x, (int)p.pos.y, nodeRadius, coreCol);
                        if(hover) DrawCircleLines((int)p.pos.x, (int)p.pos.y, nodeRadius + 4.0f, SKYRIM_GOLD);
                        DrawPerkIcon(p.section, p.pos, p.unlocked ? SKYRIM_BG_BOT : SKYRIM_MUTED);

                        int txtWidth = MeasureText(p.name.c_str(),14);
                        DrawText(p.name.c_str(), p.pos.x - txtWidth/2, p.pos.y + 32, 14, SKYRIM_TEXT);

                        if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                            bool hasParent = false;
                            bool parentUnlocked = false;
                            for(int pi=0; pi<(int)perks.size(); pi++){
                                for(int ch: perks[pi].children){
                                    if(ch==(int)i){
                                        hasParent = true;
                                        if(perks[pi].unlocked) parentUnlocked = true;
                                    }
                                }
                            }
                            bool levelOk = level >= p.levelReq;
                            bool parentOk = !hasParent || parentUnlocked;
                            if(levelOk && parentOk && !p.unlocked && perkPoints>=p.cost){
                                p.unlocked = true;
                                perkPoints -= p.cost;
                            }
                        }

                        // Tooltip pri hover
                        if(hover){
                            bool hasParent = false;
                            bool parentUnlocked = false;
                            for(int pi=0; pi<(int)perks.size(); pi++){
                                for(int ch: perks[pi].children){
                                    if(ch==(int)i){
                                        hasParent = true;
                                        if(perks[pi].unlocked) parentUnlocked = true;
                                    }
                                }
                            }
                            std::string state = p.unlocked ? "UNLOCKED" : "LOCKED";
                            std::string cost = "COST: " + std::to_string(p.cost) + " PP";
                            std::string req = "REQ LVL: " + std::to_string(p.levelReq);
                            std::string prereq = hasParent ? (parentUnlocked ? "PARENT: OK" : "PARENT: LOCKED") : "PARENT: NONE";
                            int w = 260, h = 94;
                            int bx = (int)p.pos.x + 30;
                            int by = (int)p.pos.y - 20;
                            if(bx + w > screenWidth) bx = screenWidth - w - 20;
                            if(by + h > screenHeight) by = screenHeight - h - 20;
                            DrawRectangle(bx, by, w, h, Fade(BLACK,0.85f));
                            DrawRectangleLines(bx, by, w, h, SKYRIM_PANEL_EDGE);
                            DrawText(p.name.c_str(), bx+10, by+8, 16, SKYRIM_TEXT);
                            DrawText(p.info.c_str(), bx+10, by+28, 14, SKYRIM_MUTED);
                            DrawText(cost.c_str(), bx+10, by+48, 14, SKYRIM_TEXT);
                            DrawText(state.c_str(), bx+150, by+48, 14, p.unlocked?GREEN:SKYRIM_MUTED);
                            DrawText(req.c_str(), bx+10, by+64, 12, SKYRIM_MUTED);
                            DrawText(prereq.c_str(), bx+130, by+64, 12, parentUnlocked?GREEN:SKYRIM_MUTED);
                        }
                    }
                }
            } else if(menuView == MenuView::QUESTS){
                if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)){
                    menuView = MenuView::HOME;
                }
                Rectangle backBtn = {50,50,120,36};
                DrawRectangleRec(backBtn, SKYRIM_PANEL_EDGE);
                DrawText("BACK", backBtn.x+30, backBtn.y+8, 20, SKYRIM_BG_BOT);
                if(CheckCollisionPointRec(mousePos, backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    menuView = MenuView::HOME;
                }

                DrawRectangleGradientV(0,0,screenWidth,screenHeight,SKYRIM_BG_TOP,SKYRIM_BG_BOT);
                DrawRectangle(40, 100, screenWidth-80, screenHeight-180, SKYRIM_PANEL);
                DrawRectangleLinesEx({40,100,(float)screenWidth-80.0f,(float)screenHeight-180.0f}, 2, SKYRIM_PANEL_EDGE);
                DrawText("QUESTS", 60, 120, 28, SKYRIM_TEXT);
                DrawLine(60, 155, screenWidth-60, 155, SKYRIM_PANEL_EDGE);

                Rectangle listBox = {60, 180, 360, (float)screenHeight - 260.0f};
                DrawRectangleRec(listBox, Fade(BLACK,0.35f));
                DrawRectangleLinesEx(listBox, 1, SKYRIM_PANEL_EDGE);

                std::vector<int> visibleQuests;
                for(int i=0;i<(int)quests.size();i++){
                    if(quests[i].active || quests[i].completed) visibleQuests.push_back(i);
                }
                for(int vi=0; vi<(int)visibleQuests.size(); vi++){
                    int i = visibleQuests[vi];
                    Rectangle row = {listBox.x+10, listBox.y+10 + vi*50.0f, listBox.width-20, 40};
                    bool hover = CheckCollisionPointRec(mousePos, row);
                    Color rowCol = (i==selectedQuestIdx)? Fade(SKYRIM_GOLD,0.35f) : Fade(BLACK,0.35f);
                    if(hover) rowCol = Fade(SKYRIM_GOLD,0.25f);
                    DrawRectangleRec(row, rowCol);
                    DrawText(quests[i].title.c_str(), row.x+10, row.y+10, 18, SKYRIM_TEXT);
                    const char* status =
                        quests[i].completed ? "DONE" :
                        (quests[i].active ? "ACTIVE" : "INACTIVE");
                    Color statusCol = quests[i].completed ? GREEN : (quests[i].active ? SKYRIM_TEXT : SKYRIM_MUTED);
                    DrawText(status, row.x+250, row.y+10, 14, statusCol);
                    if(hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) selectedQuestIdx = i;
                }

                Rectangle infoBox = {460, 180, (float)screenWidth-520.0f, (float)screenHeight-260.0f};
                DrawRectangleRec(infoBox, Fade(BLACK,0.35f));
                DrawRectangleLinesEx(infoBox, 1, SKYRIM_PANEL_EDGE);

                if(selectedQuestIdx >= 0 && (quests[selectedQuestIdx].active || quests[selectedQuestIdx].completed)){
                    Quest &q = quests[selectedQuestIdx];
                    DrawText(q.title.c_str(), infoBox.x+20, infoBox.y+20, 24, SKYRIM_TEXT);
                    DrawText("OBJECTIVE:", infoBox.x+20, infoBox.y+60, 18, SKYRIM_GOLD);
                    DrawText(q.objective.c_str(), infoBox.x+20, infoBox.y+85, 18, SKYRIM_TEXT);
                    DrawText("REWARD:", infoBox.x+20, infoBox.y+140, 18, SKYRIM_GOLD);
                    DrawText(q.reward.c_str(), infoBox.x+20, infoBox.y+165, 18, SKYRIM_TEXT);
                    DrawText(q.completed ? "COMPLETED" : "IN PROGRESS", infoBox.x+20, infoBox.y+220, 16, q.completed?GREEN:SKYRIM_MUTED);
                } else {
                    DrawText("Ziadne aktivne questy.", infoBox.x+20, infoBox.y+20, 18, SKYRIM_MUTED);
                }
            }

            DrawCircleV(mousePos,5,RED);
        }


        EndDrawing();
    }

	    // --- Cleanup ---
	    UnloadModel(mdlWoodBox);
	    UnloadModel(mdlStoneBox);
	    UnloadModel(mdlRoofBox);
	    UnloadRenderTexture(invPreviewRT);
	    UnloadTexture(texWoodWall);
	    UnloadTexture(texStoneWall);
	    UnloadTexture(texRoof);
	    UnloadTexture(terrainTex);
	    UnloadModel(terrainModel);
	    UnloadImage(noiseImage);
	    UnloadImage(biomeNoise);
    UnloadImage(terrainColors);
    if(musicDayLoaded) UnloadMusicStream(musicDay);
    if(musicNightLoaded) UnloadMusicStream(musicNight);
    if(killcamSfxLoaded) UnloadSound(sfxKillcam);
    for(int i=0;i<(int)sfxIntro.size();i++){
        if(introSfxLoaded[i]) UnloadSound(sfxIntro[i]);
    }
    if(audioReady) CloseAudioDevice();
    CloseWindow();
    return 0;
}
