/**********************************************************
 * Minecraft Java SMP - 边缘潜行防跌落、挖掘CD、画质与封装优化版
 **********************************************************/
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <mmsystem.h>
#include <gl/glew.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

inline float Clamp(float val, float low, float high) {
    if (val < low) return low;
    if (val > high) return high;
    return val;
}

/**************************
 * Game Structs & Data
 **************************/
enum ItemType {
    ITEM_NONE = 0,
    ITEM_DIAMOND_SWORD,
    ITEM_NETHERITE_SWORD,
    ITEM_MACE,
    ITEM_GOLDEN_APPLE,
    ITEM_SHIELD,
    ITEM_BLOCK_GRASS,
    ITEM_BLOCK_STONE
};

struct InventoryItem {
    ItemType type;
    int count;
    int maxStack;
};

struct Vector3 {
    float x, y, z;
};

struct BoundingBox {
    Vector3 min;
    Vector3 max;
};

struct Particle {
    Vector3 position;
    Vector3 velocity;
    int life;
    bool active;
    float r, g, b;
};

#define MAX_PARTICLES 1000
Particle particles[MAX_PARTICLES];

struct DroppedItem {
    Vector3 position;
    Vector3 velocity;
    ItemType type;
    int count;
    bool active;
    int life;
};

#define MAX_DROPS 100
DroppedItem droppedItems[MAX_DROPS];

struct VoxelBlock {
    int type;
    float r, g, b;
};

#define WORLD_SIZE 32
#define WORLD_HEIGHT 12
VoxelBlock world[WORLD_SIZE][WORLD_HEIGHT][WORLD_SIZE];

struct Player {
    Vector3 position;
    Vector3 velocity;
    float health;
    float absorption;
    float yaw;
    float pitch;
    bool isSprinting;
    bool isBlocking;
    bool isCrouching;
    bool isSwinging;
    int swingAnim;
    InventoryItem hotbar[10];
    InventoryItem offhand;
    int selectedSlot;
    int eatTimer;
    int attackCooldown;
    int miningCooldown; // 挖掘方块冷却
    float fallDistance;
    bool thirdPersonView;
};

struct Bot {
    Vector3 position;
    Vector3 velocity;
    float health;
    bool isDead;
    int respawnTimer;
    int attackCooldown;
    bool isSwinging;
    int swingAnim;
    bool isBlocking;
    int blockTimer;
    int strafeTimer;
    float strafeDir;
    int decisionTimer;
    int activeSlot;
    int eatCooldown;
    int hitStunTimer;
    int targetId;
    int id;
};

Player player;
std::vector<Bot> bots;
int score = 0;
int comboCount = 0;
HWND g_hWnd = NULL;

/**************************
 * Function Declarations
 **************************/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC* hDC, HGLRC* hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);
void DrawMCCharacter(Bot* b, int swingFrame, bool isBotBlocking, int itemInHand);
void DrawPlayerFirstPersonHands();
void DrawDetailedDiamondSword();
void DrawDetailedNetheriteSword();
void DrawDetailedMace();
void DrawDetailedShield();
void DrawGoldenAppleItem();
void DrawCrosshair();
void DrawWorld();
void DrawDroppedItems();
void ProcessInput();
void DrawHUD();
void InitWorld();
void SpawnBots();
void UpdateBot(int botIndex);
void CheckCollisions();
bool CheckBoxCollisionWithWorld(float x, float y, float z, float width, float height);
BoundingBox GetBotBoundingBox(const Bot* b);
bool RayIntersectsBox(Vector3 rayOrigin, Vector3 rayDir, BoundingBox box, float* outDist);
bool RayCastBlock(Vector3 origin, Vector3 dir, float maxDist, int* hitX, int* hitY, int* hitZ, int* prevX, int* prevY, int* prevZ);

/**************************
 * Sound & Particle Helper
 **************************/
void PlayGameSound(const char* filename) {
    PlaySoundA(filename, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
}

void SpawnParticleEx(float x, float y, float z, float r, float g, float b, bool isHeavy) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].position = { x, y, z };
            float scale = isHeavy ? 2.5f : 1.0f;
            particles[i].velocity = {
                ((float)(rand() % 60 - 30)) / 60.0f * scale,
                ((float)(rand() % 60)) / 50.0f * scale,
                ((float)(rand() % 60 - 30)) / 60.0f * scale
            };
            particles[i].life = isHeavy ? 50 : 25;
            particles[i].r = r; particles[i].g = g; particles[i].b = b;
            break;
        }
    }
}

void SpawnWindBurstParticles(float x, float y, float z) {
    for (int i = 0; i < 30; i++) {
        float angle = ((float)i / 30.0f) * 2.0f * (float)M_PI;
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (!particles[p].active) {
                particles[p].active = true;
                particles[p].position = { x, y, z };
                particles[p].velocity = { cosf(angle) * 0.35f, 0.2f + ((float)(rand() % 20)) / 50.0f, sinf(angle) * 0.35f };
                particles[p].life = 35;
                particles[p].r = 0.85f; particles[p].g = 0.92f; particles[p].b = 0.98f;
                break;
            }
        }
    }
}

void SpawnDroppedItem(float x, float y, float z, ItemType type, int count) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!droppedItems[i].active) {
            droppedItems[i].active = true;
            droppedItems[i].position = { x, y, z };
            droppedItems[i].velocity = { ((float)(rand() % 20 - 10)) / 100.0f, 0.15f, ((float)(rand() % 20 - 10)) / 100.0f };
            droppedItems[i].type = type;
            droppedItems[i].count = count;
            droppedItems[i].life = 600;
            break;
        }
    }
}

/**************************
 * Voxel Box Drawing (画质与抗曝光调优)
 **************************/
void DrawBox(float x, float y, float z, float dx, float dy, float dz, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(dx, dy, dz);

    glBegin(GL_QUADS);
    // Front
    glColor3f(r * 0.85f, g * 0.85f, b * 0.85f);
    glVertex3f(-0.5f, -0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, 0.5f);
    // Back
    glColor3f(r * 0.45f, g * 0.45f, b * 0.45f);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    // Top
    float rTop = (r * 1.1f > 0.95f) ? 0.95f : (r * 1.1f);
    float gTop = (g * 1.1f > 0.95f) ? 0.95f : (g * 1.1f);
    float bTop = (b * 1.1f > 0.95f) ? 0.95f : (b * 1.1f);
    glColor3f(rTop, gTop, bTop);
    glVertex3f(-0.5f, 0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, -0.5f);
    // Bottom
    glColor3f(r * 0.2f, g * 0.2f, b * 0.2f);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
    // Right
    glColor3f(r * 0.7f, g * 0.7f, b * 0.7f);
    glVertex3f(0.5f, -0.5f, -0.5f); glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f);
    // Left
    glColor3f(r * 0.55f, g * 0.55f, b * 0.55f);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glPopMatrix();
}

/**************************
 * 碰撞与射线检测
 **************************/
bool CheckBoxCollisionWithWorld(float x, float y, float z, float width, float height) {
    float halfW = width / 2.0f;
    float minX = x - halfW, maxX = x + halfW;
    float minZ = z - halfW, maxZ = z + halfW;
    float minY = y, maxY = y + height;

    int startX = (int)floorf(minX + (float)(WORLD_SIZE / 2) + 0.5f);
    int endX = (int)floorf(maxX + (float)(WORLD_SIZE / 2) + 0.5f);
    int startY = (int)floorf(minY + 2.0f + 0.5f);
    int endY = (int)floorf(maxY + 2.0f + 0.5f);
    int startZ = (int)floorf(minZ + (float)(WORLD_SIZE / 2) + 0.5f);
    int endZ = (int)floorf(maxZ + (float)(WORLD_SIZE / 2) + 0.5f);

    for (int ix = startX; ix <= endX; ix++) {
        for (int iy = startY; iy <= endY; iy++) {
            for (int iz = startZ; iz <= endZ; iz++) {
                if (ix >= 0 && ix < WORLD_SIZE && iy >= 0 && iy < WORLD_HEIGHT && iz >= 0 && iz < WORLD_SIZE) {
                    if (world[ix][iy][iz].type != 0) {
                        float bx = (float)ix - (float)(WORLD_SIZE / 2);
                        float by = (float)iy - 2.0f;
                        float bz = (float)iz - (float)(WORLD_SIZE / 2);

                        if (minX < bx + 0.5f && maxX > bx - 0.5f &&
                            minY < by + 0.5f && maxY > by - 0.5f &&
                            minZ < bz + 0.5f && maxZ > bz - 0.5f) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

BoundingBox GetBotBoundingBox(const Bot* b) {
    BoundingBox box;
    box.min = { b->position.x - 0.3f, b->position.y, b->position.z - 0.3f };
    box.max = { b->position.x + 0.3f, b->position.y + 1.8f, b->position.z + 0.3f };
    return box;
}

bool RayIntersectsBox(Vector3 rayOrigin, Vector3 rayDir, BoundingBox box, float* outDist) {
    float tMin = 0.0f;
    float tMax = 1000.0f;

    for (int i = 0; i < 3; i++) {
        float o = (i == 0) ? rayOrigin.x : ((i == 1) ? rayOrigin.y : rayOrigin.z);
        float d = (i == 0) ? rayDir.x : ((i == 1) ? rayDir.y : rayDir.z);
        float bmin = (i == 0) ? box.min.x : ((i == 1) ? box.min.y : box.min.z);
        float bmax = (i == 0) ? box.max.x : ((i == 1) ? box.max.y : box.max.z);

        if (fabsf(d) < 0.00001f) {
            if (o < bmin || o > bmax) return false;
        }
        else {
            float ood = 1.0f / d;
            float t1 = (bmin - o) * ood;
            float t2 = (bmax - o) * ood;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) return false;
        }
    }
    *outDist = tMin;
    return true;
}

bool RayCastBlock(Vector3 origin, Vector3 dir, float maxDist, int* hitX, int* hitY, int* hitZ, int* prevX, int* prevY, int* prevZ) {
    float currDist = 0.0f;
    int lastX = -1, lastY = -1, lastZ = -1;

    while (currDist < maxDist) {
        currDist += 0.05f;
        float cx = origin.x + dir.x * currDist;
        float cy = origin.y + dir.y * currDist;
        float cz = origin.z + dir.z * currDist;

        int ix = (int)floorf(cx + (float)(WORLD_SIZE / 2) + 0.5f);
        int iy = (int)floorf(cy + 2.0f + 0.5f);
        int iz = (int)floorf(cz + (float)(WORLD_SIZE / 2) + 0.5f);

        if (ix >= 0 && ix < WORLD_SIZE && iy >= 0 && iy < WORLD_HEIGHT && iz >= 0 && iz < WORLD_SIZE) {
            if (world[ix][iy][iz].type != 0) {
                *hitX = ix; *hitY = iy; *hitZ = iz;
                *prevX = lastX; *prevY = lastY; *prevZ = lastZ;
                return true;
            }
            lastX = ix; lastY = iy; lastZ = iz;
        }
    }
    return false;
}

/**************************
 * 世界生成
 **************************/
void InitWorld() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                world[x][y][z].type = 0;
            }
        }
    }

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            float heightFactor = sinf((float)x * 0.3f) * cosf((float)z * 0.3f) * 1.5f + 3.0f;
            int surfaceY = (int)heightFactor;

            for (int y = 0; y <= surfaceY; y++) {
                if (y == surfaceY) {
                    world[x][y][z] = { 1, 0.35f, 0.65f, 0.25f };
                }
                else if (y > surfaceY - 2) {
                    world[x][y][z] = { 2, 0.48f, 0.32f, 0.18f };
                }
                else {
                    world[x][y][z] = { 3, 0.42f, 0.42f, 0.42f };
                }
            }
        }
    }
}

void DrawWorld() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                if (world[x][y][z].type != 0) {
                    VoxelBlock& b = world[x][y][z];
                    DrawBox((float)x - (float)(WORLD_SIZE / 2), (float)y - 2.0f, (float)z - (float)(WORLD_SIZE / 2), 1.0f, 1.0f, 1.0f, b.r, b.g, b.b);
                }
            }
        }
    }
}

void DrawDroppedItems() {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (droppedItems[i].active) {
            float r = 0.35f, g = 0.65f, b = 0.25f;
            if (droppedItems[i].type == ITEM_BLOCK_STONE) { r = 0.42f; g = 0.42f; b = 0.42f; }
            DrawBox(droppedItems[i].position.x, droppedItems[i].position.y, droppedItems[i].position.z, 0.28f, 0.28f, 0.28f, r, g, b);
        }
    }
}

/**************************
 * Bot AI
 **************************/
void SpawnBots() {
    bots.clear();
    for (int i = 0; i < 4; i++) {
        Bot b;
        b.position = { (float)((i - 1.5f) * 4.0f), 3.5f, (float)(-5.0f - (i % 2) * 3.0f) };
        b.velocity = { 0.0f, 0.0f, 0.0f };
        b.health = 20.0f;
        b.isDead = false;
        b.respawnTimer = 0;
        b.attackCooldown = 0;
        b.isSwinging = false;
        b.swingAnim = 0;
        b.isBlocking = false;
        b.blockTimer = 0;
        b.strafeTimer = 0;
        b.strafeDir = (i % 2 == 0) ? 1.0f : -1.0f;
        b.decisionTimer = rand() % 15;
        b.activeSlot = (i % 2 == 0) ? 2 : 0;
        b.eatCooldown = 0;
        b.hitStunTimer = 0;
        b.targetId = -1;
        b.id = i;
        bots.push_back(b);
    }
}

void UpdateBot(int botIndex) {
    Bot* b = &bots[botIndex];

    if (b->isDead) {
        b->respawnTimer--;
        if (b->respawnTimer <= 0) {
            b->isDead = false;
            b->health = 20.0f;
            b->position = { (float)((botIndex - 1.5f) * 4.0f), 5.0f, (float)(-4.0f) };
            b->velocity = { 0, 0, 0 };
        }
        return;
    }

    if (b->hitStunTimer > 0) b->hitStunTimer--;

    b->decisionTimer++;
    if (b->decisionTimer > 15) {
        b->decisionTimer = 0;
        b->strafeDir = (rand() % 2 == 0) ? 1.0f : -1.0f;

        float closestDist = sqrtf(powf(player.position.x - b->position.x, 2) + powf(player.position.z - b->position.z, 2));
        int bestTarget = -1;

        for (size_t j = 0; j < bots.size(); j++) {
            if ((int)j == botIndex || bots[j].isDead) continue;
            float d2 = sqrtf(powf(bots[j].position.x - b->position.x, 2) + powf(bots[j].position.z - b->position.z, 2));
            if (d2 < closestDist) {
                closestDist = d2;
                bestTarget = (int)j;
            }
        }
        b->targetId = bestTarget;

        if (b->health < 10.0f && b->eatCooldown == 0 && (rand() % 100 < 75)) {
            b->health = (b->health + 8.0f < 20.0f) ? (b->health + 8.0f) : 20.0f;
            b->eatCooldown = 150;
        }
        else {
            b->activeSlot = (rand() % 100 < 50) ? 2 : 0;
        }

        if (closestDist < 3.2f && (rand() % 100 < 65)) {
            b->isBlocking = true;
            b->blockTimer = 25;
        }
        else {
            b->isBlocking = false;
        }
    }

    if (b->eatCooldown > 0) b->eatCooldown--;
    if (b->blockTimer > 0) {
        b->blockTimer--;
        if (b->blockTimer == 0) b->isBlocking = false;
    }

    b->strafeTimer++;
    if (b->strafeTimer > 20) {
        b->strafeDir *= -1.0f;
        b->strafeTimer = 0;
    }

    Vector3 targetPos;
    if (b->targetId == -1) targetPos = player.position;
    else targetPos = bots[b->targetId].position;

    float dx = targetPos.x - b->position.x;
    float dz = targetPos.z - b->position.z;
    float dist = sqrtf(dx * dx + dz * dz);

    float speed = b->isBlocking ? 0.035f : 0.058f;
    float fX = dx / (dist > 0.001f ? dist : 1.0f);
    float fZ = dz / (dist > 0.001f ? dist : 1.0f);
    float sX = -fZ * b->strafeDir;
    float sZ = fX * b->strafeDir;

    float moveX = (fX * 0.75f + sX * 0.25f) * speed;
    float moveZ = (fZ * 0.75f + sZ * 0.25f) * speed;

    b->position.x += moveX + b->velocity.x;
    if (CheckBoxCollisionWithWorld(b->position.x, b->position.y, b->position.z, 0.6f, 1.8f)) {
        b->position.x -= (moveX + b->velocity.x);
        b->velocity.y = 0.28f;
    }

    b->position.z += moveZ + b->velocity.z;
    if (CheckBoxCollisionWithWorld(b->position.x, b->position.y, b->position.z, 0.6f, 1.8f)) {
        b->position.z -= (moveZ + b->velocity.z);
        b->velocity.y = 0.28f;
    }

    b->velocity.y -= 0.025f;
    b->position.y += b->velocity.y;
    if (CheckBoxCollisionWithWorld(b->position.x, b->position.y, b->position.z, 0.6f, 1.8f)) {
        b->position.y -= b->velocity.y;
        b->velocity.y = 0.0f;
    }

    b->velocity.x *= 0.8f;
    b->velocity.z *= 0.8f;

    if (b->attackCooldown > 0) b->attackCooldown--;
    if (b->isSwinging) {
        b->swingAnim++;
        if (b->swingAnim > 10) { b->isSwinging = false; b->swingAnim = 0; }
    }

    if (dist <= 3.0f && b->attackCooldown == 0) {
        float dmg = (b->activeSlot == 2 ? 5.0f : 2.5f);
        b->attackCooldown = 30;
        b->isSwinging = true;

        if (b->targetId == -1) {
            if (player.isBlocking || (player.offhand.type == ITEM_SHIELD && player.isBlocking)) dmg *= 0.2f;
            if (player.absorption > 0.0f) {
                if (player.absorption >= dmg) { player.absorption -= dmg; dmg = 0.0f; }
                else { dmg -= player.absorption; player.absorption = 0.0f; }
            }
            player.health -= dmg;
            if (player.health <= 0) {
                player.health = 20.0f;
                player.absorption = 4.0f;
                player.position = { 0.0f, 6.0f, 4.0f };
            }
        }
        else {
            Bot* targetBot = &bots[b->targetId];
            if (!targetBot->isDead) {
                if (targetBot->isBlocking) dmg *= 0.2f;
                targetBot->health -= dmg;
                targetBot->hitStunTimer = 15;
                SpawnParticleEx(targetBot->position.x, targetBot->position.y + 1.0f, targetBot->position.z, 0.9f, 0.1f, 0.1f, false);
            }
        }
    }
}

void CheckCollisions() {
    float limit = ((float)WORLD_SIZE / 2.0f) - 1.5f;
    if (player.position.x < -limit) player.position.x = -limit;
    if (player.position.x > limit) player.position.x = limit;
    if (player.position.z < -limit) player.position.z = -limit;
    if (player.position.z > limit) player.position.z = limit;

    for (int i = 0; i < MAX_DROPS; i++) {
        if (droppedItems[i].active) {
            droppedItems[i].position.y += droppedItems[i].velocity.y;
            droppedItems[i].velocity.y -= 0.012f;
            int ix = (int)floorf(droppedItems[i].position.x + (float)(WORLD_SIZE / 2) + 0.5f);
            int iy = (int)floorf(droppedItems[i].position.y + 2.0f + 0.5f);
            int iz = (int)floorf(droppedItems[i].position.z + (float)(WORLD_SIZE / 2) + 0.5f);
            if (iy >= 0 && iy < WORLD_HEIGHT && ix >= 0 && ix < WORLD_SIZE && iz >= 0 && iz < WORLD_SIZE) {
                if (world[ix][iy][iz].type != 0) {
                    droppedItems[i].position.y -= droppedItems[i].velocity.y;
                    droppedItems[i].velocity.y = 0.0f;
                }
            }

            float pdist = sqrtf(powf(player.position.x - droppedItems[i].position.x, 2) +
                powf((player.position.y + 0.9f) - droppedItems[i].position.y, 2) +
                powf(player.position.z - droppedItems[i].position.z, 2));
            if (pdist < 1.5f) {
                for (int s = 0; s < 9; s++) {
                    if (player.hotbar[s].type == droppedItems[i].type && player.hotbar[s].count < player.hotbar[s].maxStack) {
                        player.hotbar[s].count += droppedItems[i].count;
                        if (player.hotbar[s].count > player.hotbar[s].maxStack) player.hotbar[s].count = player.hotbar[s].maxStack;
                        droppedItems[i].active = false;
                        break;
                    }
                    else if (player.hotbar[s].type == ITEM_NONE) {
                        player.hotbar[s].type = droppedItems[i].type;
                        player.hotbar[s].maxStack = 64;
                        player.hotbar[s].count = droppedItems[i].count;
                        droppedItems[i].active = false;
                        break;
                    }
                }
            }
        }
    }
}

/**********************************************************
 * 游戏主循环封装类 (Game Main Loop Packaging)
 **********************************************************/
class MinecraftGameEngine {
public:
    bool Initialize(HINSTANCE hInstance, int iCmdShow) {
        WNDCLASS wc = { 0 };
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "SMPPvPEngineJavaGame";
        RegisterClass(&wc);

        g_hWnd = CreateWindow(
            "SMPPvPEngineJavaGame", "Minecraft Java SMP - Edge Sneak, Mining CD & Anti-Exposure",
            WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
            100, 100, 1024, 768,
            NULL, NULL, hInstance, NULL);

        if (!g_hWnd) return false;

        HDC hDC;
        HGLRC hRC;
        EnableOpenGL(g_hWnd, &hDC, &hRC);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) return false;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_FOG);
        
        GLfloat fogColor[4] = { 0.42f, 0.58f, 0.75f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 15.0f);
        glFogf(GL_FOG_END, 38.0f);

        ShowCursor(FALSE);
        return true;
    }

    void Run() {
        MSG msg;
        BOOL bQuit = FALSE;

        player.position = { 0.0f, 6.0f, 4.0f };
        player.velocity = { 0.0f, 0.0f, 0.0f };
        player.health = 20.0f;
        player.absorption = 0.0f;
        player.yaw = 0.0f;
        player.pitch = 0.0f;
        player.isSprinting = false;
        player.isBlocking = false;
        player.isCrouching = false;
        player.isSwinging = false;
        player.swingAnim = 0;
        player.selectedSlot = 0;
        player.eatTimer = 0;
        player.attackCooldown = 0;
        player.miningCooldown = 0;
        player.fallDistance = 0.0f;
        player.thirdPersonView = false;

        for (int i = 0; i < 10; i++) player.hotbar[i] = { ITEM_NONE, 0, 64 };
        player.hotbar[0] = { ITEM_DIAMOND_SWORD, 1, 1 };
        player.hotbar[1] = { ITEM_NETHERITE_SWORD, 1, 1 };
        player.hotbar[2] = { ITEM_MACE, 1, 1 };
        player.hotbar[3] = { ITEM_GOLDEN_APPLE, 16, 64 };
        player.hotbar[4] = { ITEM_BLOCK_GRASS, 32, 64 };
        player.offhand = { ITEM_SHIELD, 1, 1 };

        InitWorld();
        SpawnBots();

        for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
        for (int i = 0; i < MAX_DROPS; i++) droppedItems[i].active = false;

        HDC hDC = GetDC(g_hWnd);

        while (!bQuit) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) bQuit = TRUE;
                else { TranslateMessage(&msg); DispatchMessage(&msg); }
            }
            else {
                ProcessInput();
                for (size_t i = 0; i < bots.size(); i++) UpdateBot((int)i);
                CheckCollisions();

                if (player.isSwinging) {
                    player.swingAnim++;
                    if (player.swingAnim > 10) { player.isSwinging = false; player.swingAnim = 0; }
                }
                if (player.eatTimer > 0) player.eatTimer--;
                if (player.attackCooldown > 0) player.attackCooldown--;
                if (player.miningCooldown > 0) player.miningCooldown--;

                for (int i = 0; i < MAX_PARTICLES; i++) {
                    if (particles[i].active) {
                        particles[i].position.x += particles[i].velocity.x;
                        particles[i].position.y += particles[i].velocity.y;
                        particles[i].position.z += particles[i].velocity.z;
                        particles[i].velocity.y -= 0.015f;
                        particles[i].life--;
                        if (particles[i].life <= 0) particles[i].active = false;
                    }
                }

                for (auto& bot : bots) {
                    if (!bot.isDead && (bot.position.y < -3.0f || bot.health <= 0)) {
                        bot.isDead = true;
                        bot.respawnTimer = 300;
                        score++;
                    }
                }

                glViewport(0, 0, 1024, 768);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                gluPerspective(65.0f, 1024.0f / 768.0f, 0.1f, 100.0f);
                glMatrixMode(GL_MODELVIEW);

                glClearColor(0.42f, 0.58f, 0.75f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glLoadIdentity();

                float yawRad = player.yaw * ((float)M_PI / 180.0f);
                float pitchRad = player.pitch * ((float)M_PI / 180.0f);
                float eyeHeight = player.isCrouching ? 1.35f : 1.62f;

                if (player.thirdPersonView) {
                    float camDist = 3.5f;
                    float cx = player.position.x - sinf(yawRad) * cosf(pitchRad) * camDist;
                    float cy = (player.position.y + eyeHeight) - sinf(pitchRad) * camDist + 0.5f;
                    float cz = player.position.z + cosf(yawRad) * cosf(pitchRad) * camDist;
                    gluLookAt(cx, cy, cz, player.position.x, player.position.y + eyeHeight, player.position.z, 0.0f, 1.0f, 0.0f);
                }
                else {
                    float lookX = player.position.x + sinf(yawRad) * cosf(pitchRad);
                    float lookY = (player.position.y + eyeHeight) + sinf(pitchRad);
                    float lookZ = player.position.z - cosf(yawRad) * cosf(pitchRad);
                    gluLookAt(player.position.x, player.position.y + eyeHeight, player.position.z, lookX, lookY, lookZ, 0.0f, 1.0f, 0.0f);
                }

                DrawWorld();
                DrawDroppedItems();

                if (player.thirdPersonView) {
                    glPushMatrix();
                    glTranslatef(player.position.x, player.position.y, player.position.z);
                    glRotatef(player.yaw, 0.0f, 1.0f, 0.0f);
                    Bot dummyPlayer = { 0 };
                    dummyPlayer.health = player.health;
                    DrawMCCharacter(&dummyPlayer, player.swingAnim, player.isBlocking || (player.offhand.type == ITEM_SHIELD && player.isBlocking), player.selectedSlot);
                    glPopMatrix();
                }

                for (auto& bot : bots) {
                    if (bot.isDead) continue;
                    glPushMatrix();
                    glTranslatef(bot.position.x, bot.position.y, bot.position.z);
                    float angle = atan2f(player.position.x - bot.position.x, player.position.z - bot.position.z) * (180.0f / (float)M_PI);
                    glRotatef(angle, 0.0f, 1.0f, 0.0f);
                    DrawMCCharacter(&bot, bot.swingAnim, bot.isBlocking, bot.activeSlot);

                    glPushMatrix();
                    glTranslatef(0.0f, 2.1f, 0.0f);
                    float hpRatio = bot.health / 20.0f;
                    DrawBox(0.0f, 0.0f, 0.0f, 0.6f, 0.08f, 0.02f, 0.2f, 0.2f, 0.2f);
                    DrawBox(-0.3f + (0.6f * hpRatio) / 2.0f, 0.0f, 0.01f, 0.6f * hpRatio, 0.06f, 0.02f, 0.9f, 0.1f, 0.1f);
                    glPopMatrix();

                    glPopMatrix();
                }

                for (int i = 0; i < MAX_PARTICLES; i++) {
                    if (particles[i].active) {
                        DrawBox(particles[i].position.x, particles[i].position.y, particles[i].position.z, 0.08f, 0.08f, 0.08f, particles[i].r, particles[i].g, particles[i].b);
                    }
                }

                glDisable(GL_FOG);
                glDisable(GL_DEPTH_TEST);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                DrawHUD();

                if (!player.thirdPersonView) {
                    DrawPlayerFirstPersonHands();
                }

                DrawCrosshair();
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_FOG);

                SwapBuffers(hDC);
                Sleep(16);
            }
        }
        ShowCursor(TRUE);
    }
};

/**************************
 * 输入与边缘潜行防跌落处理
 **************************/
void ProcessInput() {
    float rad = player.yaw * ((float)M_PI / 180.0f);
    float fX = sinf(rad), fZ = -cosf(rad);
    float rX = cosf(rad), rZ = sinf(rad);
    float iX = 0.0f, iZ = 0.0f;

    player.isCrouching = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

    if (GetAsyncKeyState('W') & 0x8000) { iX += fX; iZ += fZ; }
    if (GetAsyncKeyState('S') & 0x8000) { iX -= fX; iZ -= fZ; }
    if (GetAsyncKeyState('A') & 0x8000) { iX -= rX; iZ -= rZ; }
    if (GetAsyncKeyState('D') & 0x8000) { iX += rX; iZ += rZ; }

    float len = sqrtf(iX * iX + iZ * iZ);
    if (len > 0.0001f) { iX /= len; iZ /= len; }

    float accel = player.isSprinting ? 0.085f : 0.052f;
    if (player.isCrouching) accel *= 0.35f;

    player.velocity.x += iX * accel;
    player.velocity.z += iZ * accel;
    player.velocity.x *= 0.54f;
    player.velocity.z *= 0.54f;
    player.velocity.y -= 0.025f;

    // 核心功能：边缘潜行防跌落 (Edge Sneaking)
    if (player.isCrouching) {
        float nextX = player.position.x + player.velocity.x;
        float nextZ = player.position.z + player.velocity.z;
        int footX = (int)floorf(nextX + (float)(WORLD_SIZE / 2) + 0.5f);
        int footY = (int)floorf(player.position.y + 2.0f - 0.1f + 0.5f);
        int footZ = (int)floorf(nextZ + (float)(WORLD_SIZE / 2) + 0.5f);

        bool hasGroundAhead = true;
        if (footX >= 0 && footX < WORLD_SIZE && footY >= 0 && footY < WORLD_HEIGHT && footZ >= 0 && footZ < WORLD_SIZE) {
            if (world[footX][footY][footZ].type == 0) {
                hasGroundAhead = false;
            }
        }
        if (!hasGroundAhead) {
            player.velocity.x = 0.0f;
            player.velocity.z = 0.0f;
        }
    }

    player.position.x += player.velocity.x;
    if (CheckBoxCollisionWithWorld(player.position.x, player.position.y, player.position.z, 0.6f, 1.8f)) {
        player.position.x -= player.velocity.x;
        player.velocity.x = 0.0f;
    }

    player.position.z += player.velocity.z;
    if (CheckBoxCollisionWithWorld(player.position.x, player.position.y, player.position.z, 0.6f, 1.8f)) {
        player.position.z -= player.velocity.z;
        player.velocity.z = 0.0f;
    }

    float prevY = player.position.y;
    player.position.y += player.velocity.y;
    if (CheckBoxCollisionWithWorld(player.position.x, player.position.y, player.position.z, 0.6f, player.isCrouching ? 1.35f : 1.8f)) {
        player.position.y -= player.velocity.y;
        if (player.velocity.y < 0.0f) {
            player.fallDistance = prevY - player.position.y;
        }
        player.velocity.y = 0.0f;
    }
}

/**************************
 * HUD 与武器渲染
 **************************/
void DrawHUD() {
    for (int i = 0; i < 9; i++) {
        float bx = -0.4f + ((float)i * 0.09f);
        float by = -0.85f;
        if (player.selectedSlot == i) {
            DrawBox(bx, by, 0.0f, 0.082f, 0.082f, 0.01f, 0.95f, 0.95f, 0.95f);
            DrawBox(bx, by, 0.0f, 0.072f, 0.072f, 0.01f, 0.18f, 0.18f, 0.18f);
        }
        else {
            DrawBox(bx, by, 0.0f, 0.075f, 0.075f, 0.01f, 0.12f, 0.12f, 0.12f);
        }

        if (player.hotbar[i].type != ITEM_NONE) {
            glPushMatrix();
            glTranslatef(bx, by, 0.02f);
            glScalef(0.1f, 0.1f, 0.1f);
            if (player.hotbar[i].type == ITEM_DIAMOND_SWORD) DrawDetailedDiamondSword();
            else if (player.hotbar[i].type == ITEM_NETHERITE_SWORD) DrawDetailedNetheriteSword();
            else if (player.hotbar[i].type == ITEM_MACE) DrawDetailedMace();
            else if (player.hotbar[i].type == ITEM_SHIELD) DrawDetailedShield();
            else if (player.hotbar[i].type == ITEM_GOLDEN_APPLE) DrawGoldenAppleItem();
            else if (player.hotbar[i].type == ITEM_BLOCK_GRASS) DrawBox(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.35f, 0.65f, 0.25f);
            else if (player.hotbar[i].type == ITEM_BLOCK_STONE) DrawBox(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.42f, 0.42f, 0.42f);
            glPopMatrix();
        }
    }

    DrawBox(-0.52f, -0.85f, 0.0f, 0.075f, 0.075f, 0.01f, 0.22f, 0.22f, 0.22f);
    if (player.offhand.type != ITEM_NONE) {
        glPushMatrix();
        glTranslatef(-0.52f, -0.85f, 0.02f);
        glScalef(0.1f, 0.1f, 0.1f);
        if (player.offhand.type == ITEM_SHIELD) DrawDetailedShield();
        else if (player.offhand.type == ITEM_GOLDEN_APPLE) DrawGoldenAppleItem();
        else if (player.offhand.type == ITEM_BLOCK_GRASS) DrawBox(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.35f, 0.65f, 0.25f);
        glPopMatrix();
    }

    int hearts = (int)(player.health / 2.0f + 0.5f);
    for (int i = 0; i < 10; i++) {
        float hx = -0.95f + ((float)i * 0.035f);
        float hy = -0.72f;
        if (i < hearts) {
            DrawBox(hx, hy, 0.0f, 0.03f, 0.035f, 0.01f, 0.95f, 0.15f, 0.15f);
        }
        else {
            DrawBox(hx, hy, 0.0f, 0.03f, 0.035f, 0.01f, 0.22f, 0.22f, 0.22f);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_MOUSEMOVE:
    {
        RECT rect; POINT pt, centerPt;
        GetClientRect(hWnd, &rect);
        int cx = (rect.right - rect.left) / 2;
        int cy = (rect.bottom - rect.top) / 2;
        GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        int dx = pt.x - cx, dy = pt.y - cy;
        if (dx != 0 || dy != 0) {
            player.yaw += (float)dx * 0.15f;
            player.pitch -= (float)dy * 0.15f;
            player.pitch = Clamp(player.pitch, -89.0f, 89.0f);
            centerPt = { cx, cy }; ClientToScreen(hWnd, &centerPt);
            SetCursorPos(centerPt.x, centerPt.y);
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        WPARAM key = wParam;
        if (key == VK_ESCAPE) PostQuitMessage(0);
        if (key == VK_SHIFT) player.isSprinting = !player.isSprinting;
        if (key == VK_F5) player.thirdPersonView = !player.thirdPersonView;
        if (key == 'F') {
            InventoryItem temp = player.hotbar[player.selectedSlot];
            player.hotbar[player.selectedSlot] = player.offhand;
            player.offhand = temp;
        }
        if (key >= '1' && key <= '9') player.selectedSlot = (int)(key - '1');
        if (key == VK_SPACE) {
            if (player.velocity.y <= 0.001f && player.velocity.y >= -0.05f) {
                player.velocity.y = 0.38f;
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        player.isSwinging = true;
        player.swingAnim = 0;
        PlayGameSound("swing.wav");

        ItemType activeItem = player.hotbar[player.selectedSlot].type;

        if (activeItem == ITEM_GOLDEN_APPLE && player.hotbar[player.selectedSlot].count > 0) {
            player.eatTimer = 30;
            player.health = (player.health + 4.0f < 20.0f) ? (player.health + 4.0f) : 20.0f;
            player.absorption = 8.0f;
            player.hotbar[player.selectedSlot].count--;
            if (player.hotbar[player.selectedSlot].count <= 0) player.hotbar[player.selectedSlot].type = ITEM_NONE;
            PlayGameSound("eat.wav");
            return 0;
        }

        float yawRad = player.yaw * ((float)M_PI / 180.0f);
        float pitchRad = player.pitch * ((float)M_PI / 180.0f);
        Vector3 rayOrigin = { player.position.x, player.position.y + 1.62f, player.position.z };
        Vector3 rayDir = { sinf(yawRad) * cosf(pitchRad), sinf(pitchRad), -cosf(yawRad) * cosf(pitchRad) };

        Bot* hitBot = nullptr;
        float minHitDist = 999.0f;

        for (auto& bot : bots) {
            if (bot.isDead) continue;
            BoundingBox box = GetBotBoundingBox(&bot);
            float hitDist = 0.0f;
            if (RayIntersectsBox(rayOrigin, rayDir, box, &hitDist)) {
                if (hitDist > 0.0f && hitDist <= 5.0f && hitDist < minHitDist) {
                    minHitDist = hitDist;
                    hitBot = &bot;
                }
            }
        }

        if (hitBot != nullptr) {
            float baseDmg = 2.5f;
            bool isWindBurst = false;

            if (activeItem == ITEM_DIAMOND_SWORD) baseDmg = 3.5f;
            else if (activeItem == ITEM_NETHERITE_SWORD) baseDmg = 4.5f;
            else if (activeItem == ITEM_MACE) {
                baseDmg = 5.0f + (player.fallDistance > 0.0f ? player.fallDistance * 15.0f : 0.0f);
                if (player.fallDistance > 1.5f) {
                    isWindBurst = true;
                    player.velocity.y = 0.55f;
                    SpawnWindBurstParticles(hitBot->position.x, hitBot->position.y + 1.0f, hitBot->position.z);
                    PlayGameSound("wind_burst.wav");
                }
            }

            bool isCrit = (player.velocity.y < -0.01f || isWindBurst);
            if (isCrit) {
                baseDmg *= 1.5f;
                SpawnParticleEx(hitBot->position.x, hitBot->position.y + 1.0f, hitBot->position.z, 1.0f, 1.0f, 1.0f, true);
            }

            // 重锤风暴击飞弹起与伤害赋予
            hitBot->health -= baseDmg;
            hitBot->velocity.y = 0.42f;
            hitBot->hitStunTimer = 15;
            comboCount++;
            PlayGameSound("hit.wav");
            SpawnParticleEx(hitBot->position.x, hitBot->position.y + 1.0f, hitBot->position.z, 0.9f, 0.1f, 0.1f, false);
            return 0;
        }

        // 挖掘方块冷却时间
        if (player.miningCooldown == 0) {
            int hX, hY, hZ, pX, pY, pZ;
            if (RayCastBlock(rayOrigin, rayDir, 5.0f, &hX, &hY, &hZ, &pX, &pY, &pZ)) {
                ItemType dropType = (world[hX][hY][hZ].type == 3) ? ITEM_BLOCK_STONE : ITEM_BLOCK_GRASS;
                world[hX][hY][hZ].type = 0;
                player.miningCooldown = 20;
                PlayGameSound("hit.wav");
                SpawnDroppedItem((float)hX - (float)(WORLD_SIZE / 2), (float)hY - 2.0f, (float)hZ - (float)(WORLD_SIZE / 2), dropType, 1);
            }
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
    {
        ItemType activeItem = player.hotbar[player.selectedSlot].type;
        if (activeItem != ITEM_BLOCK_GRASS && activeItem != ITEM_BLOCK_STONE) {
            if (player.offhand.type == ITEM_BLOCK_GRASS || player.offhand.type == ITEM_BLOCK_STONE) {
                activeItem = player.offhand.type;
            }
            else if (player.offhand.type == ITEM_GOLDEN_APPLE) {
                player.eatTimer = 30;
                player.health = (player.health + 4.0f < 20.0f) ? (player.health + 4.0f) : 20.0f;
                player.absorption = 8.0f;
                player.offhand.count--;
                if (player.offhand.count <= 0) player.offhand.type = ITEM_NONE;
                PlayGameSound("eat.wav");
                return 0;
            }
            else {
                player.isBlocking = true;
                return 0;
            }
        }

        bool isFromHotbar = (player.hotbar[player.selectedSlot].type == activeItem);
        int& currentCount = isFromHotbar ? player.hotbar[player.selectedSlot].count : player.offhand.count;

        if (currentCount > 0) {
            float yawRad = player.yaw * ((float)M_PI / 180.0f);
            float pitchRad = player.pitch * ((float)M_PI / 180.0f);
            Vector3 rayOrigin = { player.position.x, player.position.y + 1.62f, player.position.z };
            Vector3 rayDir = { sinf(yawRad) * cosf(pitchRad), sinf(pitchRad), -cosf(yawRad) * cosf(pitchRad) };

            int hX, hY, hZ, pX, pY, pZ;
            if (RayCastBlock(rayOrigin, rayDir, 5.0f, &hX, &hY, &hZ, &pX, &pY, &pZ)) {
                if (pX >= 0 && pY >= 0 && pZ >= 0 && pX < WORLD_SIZE && pY < WORLD_HEIGHT && pZ < WORLD_SIZE) {
                    if (world[pX][pY][pZ].type == 0) {
                        float bx = (float)pX - (float)(WORLD_SIZE / 2);
                        float by = (float)pY - 2.0f;
                        float bz = (float)pZ - (float)(WORLD_SIZE / 2);

                        float pMinX = player.position.x - 0.3f, pMaxX = player.position.x + 0.3f;
                        float pMinZ = player.position.z - 0.3f, pMaxZ = player.position.z + 0.3f;
                        float pMinY = player.position.y, pMaxY = player.position.y + 1.8f;

                        bool collidesWithPlayer = (pMinX < bx + 0.5f && pMaxX > bx - 0.5f &&
                            pMinY < by + 0.5f && pMaxY > by - 0.5f &&
                            pMinZ < bz + 0.5f && pMaxZ > bz - 0.5f);

                        if (!collidesWithPlayer) {
                            world[pX][pY][pZ].type = (activeItem == ITEM_BLOCK_STONE) ? 3 : 1;
                            world[pX][pY][pZ].r = (activeItem == ITEM_BLOCK_STONE) ? 0.42f : 0.35f;
                            world[pX][pY][pZ].g = (activeItem == ITEM_BLOCK_STONE) ? 0.42f : 0.65f;
                            world[pX][pY][pZ].b = (activeItem == ITEM_BLOCK_STONE) ? 0.42f : 0.25f;

                            currentCount--;
                            if (currentCount <= 0) {
                                if (isFromHotbar) player.hotbar[player.selectedSlot].type = ITEM_NONE;
                                else player.offhand.type = ITEM_NONE;
                            }
                            PlayGameSound("hit.wav");
                        }
                    }
                }
            }
        }
        return 0;
    }
    case WM_RBUTTONUP: player.isBlocking = false; return 0;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

/**************************
 * 渲染函数：手臂与武器
 **************************/
void DrawPlayerFirstPersonHands() {
    glPushMatrix();
    glTranslatef(0.48f, -0.38f, -0.55f);
    if (player.isSwinging) {
        float ang = sinf((float)player.swingAnim / 10.0f * (float)M_PI) * 65.0f;
        glRotatef(ang, 1.0f, -1.0f, 0.0f);
    }

    DrawBox(0.0f, -0.15f, 0.0f, 0.15f, 0.45f, 0.15f, 0.88f, 0.72f, 0.58f);

    ItemType curItem = player.hotbar[player.selectedSlot].type;
    glPushMatrix();
    glTranslatef(0.0f, 0.15f, 0.0f);
    if (curItem == ITEM_DIAMOND_SWORD) DrawDetailedDiamondSword();
    else if (curItem == ITEM_NETHERITE_SWORD) DrawDetailedNetheriteSword();
    else if (curItem == ITEM_MACE) DrawDetailedMace();
    else if (curItem == ITEM_SHIELD) DrawDetailedShield();
    else if (curItem == ITEM_GOLDEN_APPLE) DrawGoldenAppleItem();
    else if (curItem == ITEM_BLOCK_GRASS || curItem == ITEM_BLOCK_STONE) {
        float br = (curItem == ITEM_BLOCK_STONE) ? 0.42f : 0.35f;
        float bg = (curItem == ITEM_BLOCK_STONE) ? 0.42f : 0.65f;
        float bb = (curItem == ITEM_BLOCK_STONE) ? 0.42f : 0.25f;
        DrawBox(0.0f, 0.0f, 0.0f, 0.35f, 0.35f, 0.35f, br, bg, bb);
    }
    glPopMatrix();
    glPopMatrix();

    if (player.offhand.type != ITEM_NONE) {
        glPushMatrix();
        glTranslatef(-0.48f, -0.38f, -0.55f);
        if (player.isBlocking || player.offhand.type == ITEM_SHIELD && player.isBlocking) {
            glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
            glRotatef(20.0f, 0.0f, 0.0f, 1.0f);
        }
        DrawBox(0.0f, -0.15f, 0.0f, 0.15f, 0.45f, 0.15f, 0.88f, 0.72f, 0.58f);
        glTranslatef(0.0f, 0.15f, 0.0f);
        if (player.offhand.type == ITEM_SHIELD) DrawDetailedShield();
        else if (player.offhand.type == ITEM_GOLDEN_APPLE) DrawGoldenAppleItem();
        else if (player.offhand.type == ITEM_BLOCK_GRASS || player.offhand.type == ITEM_BLOCK_STONE) {
            float br = (player.offhand.type == ITEM_BLOCK_STONE) ? 0.42f : 0.35f;
            float bg = (player.offhand.type == ITEM_BLOCK_STONE) ? 0.42f : 0.65f;
            float bb = (player.offhand.type == ITEM_BLOCK_STONE) ? 0.42f : 0.25f;
            DrawBox(0.0f, 0.0f, 0.0f, 0.35f, 0.35f, 0.35f, br, bg, bb);
        }
        glPopMatrix();
    }
}

void DrawMCCharacter(Bot* b, int swingFrame, bool isBotBlocking, int itemInHand) {
    float ar = 0.15f, ag = 0.38f, ab = 0.75f;
    DrawBox(0.0f, 0.9f, 0.0f, 0.42f, 0.62f, 0.22f, ar, ag, ab);
    DrawBox(0.0f, 1.38f, 0.0f, 0.38f, 0.38f, 0.38f, 0.88f, 0.72f, 0.58f);
    DrawBox(0.0f, 1.45f, 0.0f, 0.4f, 0.15f, 0.4f, 0.2f, 0.55f, 0.2f);
    DrawBox(-0.11f, 0.32f, 0.0f, 0.19f, 0.52f, 0.21f, 0.12f, 0.12f, 0.35f);
    DrawBox(0.11f, 0.32f, 0.0f, 0.19f, 0.52f, 0.21f, 0.12f, 0.12f, 0.35f);
    DrawBox(-0.31f, 0.9f, 0.0f, 0.19f, 0.61f, 0.21f, ar, ag, ab);

    glPushMatrix();
    glTranslatef(0.31f, 1.1f, 0.0f);
    if (isBotBlocking) {
        glRotatef(-50.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(35.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (swingFrame > 0) {
        float ang = sinf((float)swingFrame / 10.0f * (float)M_PI) * -70.0f;
        glRotatef(ang, 1.0f, 0.0f, 0.0f);
    }
    glTranslatef(-0.31f, -1.1f, 0.0f);
    DrawBox(0.31f, 0.9f, 0.0f, 0.19f, 0.61f, 0.21f, ar, ag, ab);

    glPushMatrix();
    glTranslatef(0.31f, 0.58f, 0.3f);
    if (itemInHand == 2) DrawDetailedMace();
    else DrawDetailedNetheriteSword();
    glPopMatrix();

    glPopMatrix();
}

void DrawDetailedDiamondSword() {
    glPushMatrix();
    DrawBox(0.0f, -0.3f, 0.0f, 0.05f, 0.25f, 0.05f, 0.42f, 0.25f, 0.1f);
    DrawBox(0.0f, -0.44f, 0.0f, 0.08f, 0.07f, 0.08f, 0.22f, 0.22f, 0.22f);
    DrawBox(0.0f, -0.15f, 0.0f, 0.32f, 0.07f, 0.08f, 0.12f, 0.48f, 0.58f);
    DrawBox(0.0f, 0.26f, 0.0f, 0.11f, 0.68f, 0.04f, 0.18f, 0.88f, 0.92f);
    glPopMatrix();
}

void DrawDetailedNetheriteSword() {
    glPushMatrix();
    DrawBox(0.0f, -0.3f, 0.0f, 0.05f, 0.25f, 0.05f, 0.15f, 0.15f, 0.18f);
    DrawBox(0.0f, -0.44f, 0.0f, 0.08f, 0.07f, 0.08f, 0.1f, 0.1f, 0.12f);
    DrawBox(0.0f, -0.15f, 0.0f, 0.35f, 0.07f, 0.08f, 0.25f, 0.24f, 0.28f);
    DrawBox(0.0f, 0.28f, 0.0f, 0.13f, 0.72f, 0.05f, 0.32f, 0.32f, 0.36f);
    glPopMatrix();
}

void DrawDetailedMace() {
    glPushMatrix();
    DrawBox(0.0f, -0.28f, 0.0f, 0.06f, 0.50f, 0.06f, 0.38f, 0.25f, 0.1f);
    DrawBox(0.0f, 0.08f, 0.0f, 0.18f, 0.12f, 0.18f, 0.78f, 0.65f, 0.22f);
    DrawBox(0.0f, 0.32f, 0.0f, 0.42f, 0.38f, 0.42f, 0.85f, 0.75f, 0.3f);
    DrawBox(0.0f, 0.32f, 0.0f, 0.46f, 0.15f, 0.46f, 0.68f, 0.55f, 0.18f);
    glPopMatrix();
}

void DrawDetailedShield() {
    glPushMatrix();
    DrawBox(0.0f, 0.0f, 0.0f, 0.36f, 0.52f, 0.08f, 0.48f, 0.32f, 0.14f);
    DrawBox(0.0f, 0.0f, 0.04f, 0.24f, 0.4f, 0.03f, 0.78f, 0.78f, 0.78f);
    glPopMatrix();
}

void DrawGoldenAppleItem() {
    glPushMatrix();
    DrawBox(0.0f, 0.0f, 0.0f, 0.25f, 0.25f, 0.25f, 0.95f, 0.75f, 0.06f);
    glPopMatrix();
}

void DrawCrosshair() {
    glColor3f(0.9f, 0.9f, 0.9f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(-0.035f, 0.0f, 0.0f); glVertex3f(0.035f, 0.0f, 0.0f);
    glVertex3f(0.0f, -0.045f, 0.0f); glVertex3f(0.0f, 0.045f, 0.0f);
    glEnd();
}

void EnableOpenGL(HWND hWnd, HDC* hDC, HGLRC* hRC) {
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    *hDC = GetDC(hWnd);
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24; pfd.cDepthBits = 24;
    int fmt = ChoosePixelFormat(*hDC, &pfd);
    SetPixelFormat(*hDC, fmt, &pfd);
    *hRC = wglCreateContext(*hDC);
    wglMakeCurrent(*hDC, *hRC);
}

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC) {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
}

/**********************************************************
 * WinMain 入口：通过面向对象游戏类封装启动
 **********************************************************/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow) {
    MinecraftGameEngine game;
    if (!game.Initialize(hInstance, iCmdShow)) {
        return -1;
    }
    game.Run();
    return 0;
}

int main(int argc, char* argv[]) {
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
