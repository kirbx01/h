#pragma once

#include "raylib.h"

#include <cmath>
#include <cstddef>
#include <string>

namespace witness {

inline Vector2 VAdd(Vector2 a, Vector2 b) { return Vector2{ a.x + b.x, a.y + b.y }; }
inline Vector2 VSub(Vector2 a, Vector2 b) { return Vector2{ a.x - b.x, a.y - b.y }; }
inline Vector2 VScale(Vector2 a, float s) { return Vector2{ a.x * s, a.y * s }; }
inline float   VDot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
inline float   VLen(Vector2 a)           { return std::sqrt(a.x * a.x + a.y * a.y); }

constexpr int SCREEN_W   = 1280;
constexpr int SCREEN_H   = 720;
constexpr int TARGET_FPS = 60;
constexpr float PASSAGE_TIME_LIMIT = 20.0f;

constexpr int DESIGN_W = SCREEN_W;
constexpr int DESIGN_H = SCREEN_H;

constexpr int TITLE_FONT_SIZE = 96;
constexpr int UI_FONT_SIZE    = 48;

constexpr const char* GAME_TITLE = "i forgor";

inline constexpr const char* AUDIO_DIR = "assets/";

inline constexpr const char* AUDIO_STEMS[] =
{
    "openingandclosing", "opening_and_closing", "openingandclose",
    "i_forgor", "opening", "closing"
};

inline constexpr std::size_t AUDIO_STEM_COUNT = sizeof(AUDIO_STEMS) / sizeof(AUDIO_STEMS[0]);

inline constexpr const char* POP_STEMS[] = { "button_pop", "pop", "bubble" };

inline constexpr std::size_t POP_STEM_COUNT = sizeof(POP_STEMS) / sizeof(POP_STEMS[0]);

inline constexpr const char* AUDIO_EXTS[] = { ".ogg", ".wav", ".mp3", ".qoa", ".xm", ".mod" };

constexpr const char* ITCH_URL_FILE = "assets/itch_url.txt";

inline constexpr const char* BG_IMAGE[] =
{
    "assets/bg.png", "res/assets/bg.png", "../assets/bg.png", "../../assets/bg.png"
};

inline constexpr const char* FONT_TITLE_PATHS[] =
{
    "assets/fonts/dosmic/Dosmic.ttf",
    "assets/fonts/dosmic/dosmic.ttf"
};

inline constexpr const char* FONT_BODY_PATHS[] =
{
    "assets/fonts/seratonin/Seratonin-Regular.otf",
    "assets/fonts/seratonin/seratonin-regular.otf",
    "assets/fonts/seratonin/Seratonin.otf"
};

#ifndef IFG_AUTHOR
#define IFG_AUTHOR "kirbx01"
#endif
#ifndef IFG_ITCH_URL
#define IFG_ITCH_URL ""
#endif

constexpr int STAGE_COUNT = 5;

constexpr float DOOR_X = 1136.0f;
constexpr float DOOR_Y =  96.0f;
constexpr float DOOR_W =  70.0f;
constexpr float DOOR_H = 128.0f;

constexpr float PLAY_X =  70.0f;
constexpr float PLAY_Y =  80.0f;
constexpr float PLAY_W = 1140.0f;
constexpr float PLAY_H = 568.0f;

constexpr float BALL_RADIUS      = 12.0f;
constexpr float BALL_ACCEL       = 1500.0f;
constexpr float BALL_MAX_SPEED   = 430.0f;
constexpr float BALL_DRAG        = 3.0f;
constexpr float BALL_RESTITUTION = 0.80f;
constexpr float EROSION_SPEED    = 190.0f;
constexpr float HIT_COOLDOWN     = 0.16f;

constexpr float BALL_SQUASH_STIFFNESS = 150.0f;
constexpr float BALL_SQUASH_DAMPING   = 11.0f;
constexpr float BALL_SQUASH_MAX       = 0.5f;

const Color COL_BG         = { 224, 225, 228, 255 };
const Color COL_BG_DEEP    = { 224, 225, 228, 255 };
const Color COL_PANEL      = { 245, 245, 247, 255 };
const Color COL_EDGE       = { 116, 116, 122, 255 };
const Color COL_TEXT       = {   0,   0,   0, 255 };
const Color COL_TEXT_DIM   = {   0,   0,   0, 255 };
const Color COL_TEXT_FAINT = {   0,   0,   0, 255 };
const Color COL_TEXT_GHOST = {   0,   0,   0, 255 };
const Color COL_ACCENT     = {   0,   0,   0, 255 };
const Color COL_SCRIM      = { 224, 225, 228, 190 };

enum Screen
{
    SCREEN_INTRO = 0,
    SCREEN_MENU,
    SCREEN_PLAYING,
    SCREEN_PAUSE,
    SCREEN_SETTINGS,
    SCREEN_CLEAR,
    SCREEN_ENDING,
    SCREEN_CREDITS,
    SCREEN_HELP,
    SCREEN_CUTSCENE
};

constexpr int CUTSCENE_IMAGE_COUNT = 6;

constexpr int MAX_TILES = 16;

constexpr float TILE_LONG     = 100.0f;
constexpr float TILE_SHORT    =  48.0f;
constexpr float DOMINO_CORNER =  3.0f;
constexpr float CORNER_R      =  7.0f;

struct Domino
{
    Vector2 center   = { 0, 0 };
    bool    horizontal = false;
    int     valueA   = 0;
    int     valueB   = 0;
    bool    consumedA = false;
    bool    consumedB = false;
    int     pipsLeft = 0;
    bool    gone     = false;
    bool    keystone = false;
    float   hitFlash = 0.0f;
    float   hitCool  = 0.0f;
};

struct Board
{
    Domino tiles[MAX_TILES];
    int    stage        = 0;
    int    emptied      = 0;
    int    emptiedTotal = 0;
    int    sealNeed     = 1;
    bool   sealOpen     = false;
    float  doorPulse    = 0.0f;
};

struct Ball
{
    Vector2 pos       = { 0, 0 };
    Vector2 vel       = { 0, 0 };
    float   radius    = BALL_RADIUS;
    float   squash    = 0.0f;
    float   squashVel = 0.0f;
    float   impactAngle = 0.0f;
    int     pipValue  = 1;
    bool    lost      = false;
    float   lostTimer = 0.0f;
};

constexpr int TRAIL_CAP    = 512;
constexpr float TRAIL_SPACING = 7.0f;

struct TrailSample
{
    Vector2 pos  = { 0, 0 };
    float   born = 0.0f;
    bool    ghost = false;
};

struct Trail
{
    TrailSample samples[TRAIL_CAP];
    int    head  = 0;
    int    count = 0;
    Vector2 lastPush = { -1e9f, -1e9f };

    float life      = 2.4f;
    float ghostLife = 9.0f;
    float ghostGain = 0.40f;

    void Push(Vector2 p, float now);
    void MarkGhost();
    void Clear();
};

struct Story
{
    std::string line;
    float hold     = 0.0f;
    float shownAt  = 0.0f;
    float cooldown = 0.0f;

    void Say(const char* text, float seconds = 4.2f);
    void Update(float dt);
};

struct StageConfig
{
    int    sealNeed;
    float  memory;
    float  ghostGain;
    float  volume;
    float  cutoff;
    float  dropoutEvery;
    float  dropoutLen;
    const char* enterLine;
    const char* retryLine;
};

const StageConfig& StageTuning(int stage);
const char* ClearLine(int stage);

struct Settings
{
    float volume        = 1.0f;
    float memory        = 1.0f;
    bool  muted         = false;
    bool  skipIntro     = true;
    bool  introSeen     = false;
};

struct Transition
{
    bool   active   = false;
    bool   toBlack  = false;
    Screen target   = SCREEN_PLAYING;
    float  t        = 0.0f;
    float  speed    = 3.5f;
};

struct Game
{
    Screen screen = SCREEN_INTRO;

    Font  font       = {};
    bool  fontLoaded = false;
    Font  titleFont  = {};
    bool  titleFontLoaded = false;
    Texture bg        = {};
    bool  bgLoaded    = false;
    Texture cutsceneTextures[CUTSCENE_IMAGE_COUNT] = {};

    Board    board;
    Ball     ball;
    Trail    trail;
    Story    story;
    Settings settings;
    Transition transition;

    float clock     = 0.0f;
    float boardReveal = 1.0f;
    float ballReveal  = 1.0f;
    float sceneTime = 0.0f;
    float stageTime = 0.0f;
    int   attempt   = 0;
    int   pendingStage = 0;
    bool  finished   = false;
    Screen returnScreen = SCREEN_MENU;
    bool  doorHintShown = false;
    bool  edgeHintShown = false;
    bool  hasSave   = false;
    bool  quitToMenu = false;
    bool  helpInputLock = false;
    int   helpPage = 0;
    int   cutsceneIndex = 0;
    bool  cutsceneEnding = false;
    bool  cutsceneSkip = false;
    bool  cutsceneInputLock = false;
    bool  cutsceneFade = false;

    int focus = 0;

    std::string itchUrl;
    bool        linkHover = false;
    bool        linkFocus = false;
    std::string statusNote;
};

extern const Vector2 BALL_START;

void GoToScreen(Game& g, Screen screen);
void RequestTransition(Game& g, Screen screen, float speed = 3.5f);
void UpdateGame(Game& g, float dt);
void HandleInput(Game& g);

void StartNewGame(Game& g);
void ContinueGame(Game& g, bool saved);
void EnterStage(Game& g, int stage);
void RestartAttempt(Game& g, bool fromEdge);
void AdvanceStage(Game& g);
void BeginEnding(Game& g);
void OpenCredits(Game& g);

void InitBoard(Board& b);
void UpdateBoard(Game& g, float dt);
void ResetBall(Game& g);
void UpdateBall(Game& g, float dt);
void UpdateTrail(Game& g, float dt);
Rectangle TileRect(const Domino& d);
int  TileTotalPips(const Domino& d);
bool BallInDoor(const Game& g);

bool LoadSession(Game& g);
void SaveSession(const Game& g);
void ClearSession();

void DrawFrame(Game& g);

extern void (*FrameDrawn)();
void DrawIntro(const Game& g);
void DrawPlaying(const Game& g);
void DrawClearBeat(const Game& g);
void DrawEndingSequence(const Game& g);
void DrawBoard(const Game& g, float alpha);
void DrawBallShape(const Game& g, float alpha);
void DrawTrail(const Trail& t, float memoryScale, float now);
void DrawDoor(const Game& g, float alpha);
void DrawStoryLine(const Game& g);
void DrawHints(const Game& g);
void DrawFade(const Game& g);

struct Layout
{
    float screenW = 0.0f;
    float screenH = 0.0f;

    float scale = 1.0f;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float viewW = 0.0f;
    float viewH = 0.0f;

    float margin = 0.0f;

    float hintSize     = 0.0f;
    float hintSpacing  = 0.0f;
    float storySize    = 0.0f;
    float storySpacing = 0.0f;

    float topRowY      = 0.0f;
    float storyY       = 0.0f;
    float controlRow2Y = 0.0f;
    float controlRowY  = 0.0f;
};

Layout ComputeLayoutFor(float screenW, float screenH);
Layout ComputeLayout();
const Layout& CurrentLayout();
void BeginWorldView(const Layout& l);
void EndWorldView();

float SpacedTextWidth(const Font& font, const char* text, float size, float spacing);
void  DrawSpaced(const Font& font, const char* text, float x, float y, float size,
                 float spacing, Color color);
void  DrawSpacedCentered(const Font& font, const char* text, float centerX, float y,
                         float size, float spacing, Color color);

}