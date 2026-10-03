#pragma once

#include "raylib.h"

#include <cmath>
#include <cstddef>
#include <string>

namespace witness {

// raylib 5.5 keeps its vector maths in raymath.h, which is not pulled in by raylib.h.
// Rather than define RAYMATH_IMPLEMENTATION in a translation unit (and risk a second
// definition), the game carries the four operations it actually needs.
inline Vector2 VAdd(Vector2 a, Vector2 b) { return Vector2{ a.x + b.x, a.y + b.y }; }
inline Vector2 VSub(Vector2 a, Vector2 b) { return Vector2{ a.x - b.x, a.y - b.y }; }
inline Vector2 VScale(Vector2 a, float s) { return Vector2{ a.x * s, a.y * s }; }
inline float   VDot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
inline float   VLen(Vector2 a)           { return std::sqrt(a.x * a.x + a.y * a.y); }

constexpr int SCREEN_W     = 1280;
constexpr int SCREEN_H     = 720;
constexpr int TARGET_FPS   = 60;
constexpr int UI_FONT_SIZE = 28;

// The game. Everything user visible spells this name, including the window title
// and the credits, so it is defined once here.
constexpr const char* GAME_TITLE = "AFTER THE FALL";

// Pre-existing placeholder background from the original shell. Kept loadable but
// disabled by default: the game wants a flat, empty void.
constexpr const char* BG_IMAGE = "assets/bg.png";

// Where the supplied music is looked for. Extension probing covers the formats this
// raylib build can decode (ogg/wav/mp3/qoa/xm/mod).
//
// The passage track is the music the game is played over. The frame track is the opening,
// the ending and the settings screen. Both keep the filenames they were supplied with,
// so the spellings are probed rather than assumed.
inline constexpr const char* AUDIO_DIR = "assets/";

inline constexpr const char* AUDIO_PASSAGE_STEMS[] =
{
    "baseorignal", "baseoriginal", "base2", "base",
    "after_the_fall", "afterthefall", "after-the-fall", "track", "music", "theme"
};

inline constexpr const char* AUDIO_FRAME_STEMS[] =
{
    "openingandclosing", "opening_and_closing", "openingandclose", "opening", "closing"
};

inline constexpr std::size_t AUDIO_PASSAGE_STEM_COUNT =
    sizeof(AUDIO_PASSAGE_STEMS) / sizeof(AUDIO_PASSAGE_STEMS[0]);

inline constexpr std::size_t AUDIO_FRAME_STEM_COUNT =
    sizeof(AUDIO_FRAME_STEMS) / sizeof(AUDIO_FRAME_STEMS[0]);

inline constexpr const char* AUDIO_EXTS[] = { ".ogg", ".wav", ".mp3", ".qoa", ".xm", ".mod" };

// Optional external override so a build can ship a differently named track:
// set the ATF_AUDIO environment variable to a path, or bake a URL in with -DATF_ITCH_URL=.
constexpr const char* ITCH_URL_FILE = "assets/itch_url.txt";

// Credits metadata. Defaults are overridable at build time; nothing is invented here.
#ifndef ATF_AUTHOR
#define ATF_AUTHOR "kirbx01"
#endif
#ifndef ATF_ITCH_URL
#define ATF_ITCH_URL ""
#endif

constexpr int STAGE_COUNT = 5;

// The exit: a doorway in the top-right corner, sealed until enough of the arrangement
// has been erased. Declared here because both the simulation and the renderer need it.
constexpr float DOOR_X = 1150.0f;
constexpr float DOOR_Y =  84.0f;
constexpr float DOOR_W =  60.0f;
constexpr float DOOR_H = 108.0f;

// Ball movement feel.
constexpr float BALL_ACCEL      = 1500.0f;
constexpr float BALL_MAX_SPEED  = 430.0f;
constexpr float BALL_DRAG       = 3.0f;
constexpr float BALL_RESTITUTION = 0.45f;
constexpr float EROSION_SPEED   = 190.0f;   // below this a collision only deforms the ball
constexpr float PIPS_PER_HIT    = 2;        // pips stripped by a solid hit...
constexpr float PIPS_BONUS_HIT  = 1;        // ...plus one more when it is a full-speed dash
constexpr float PIPS_BONUS_SPEED = 360.0f;
constexpr float HIT_COOLDOWN    = 0.16f;    // seconds before the same tile can be hit again

//------------------------------------------------------------------------------------
// Palette: strictly greyscale. No hue anywhere in the game.
//------------------------------------------------------------------------------------
const Color COL_BG         = {   6,   6,   6, 255 };
const Color COL_BG_DEEP    = {   0,   0,   0, 255 };
const Color COL_PANEL      = {  18,  18,  18, 235 };
const Color COL_EDGE       = {  74,  74,  74, 255 };
const Color COL_TEXT       = { 238, 238, 238, 255 };
const Color COL_TEXT_DIM   = { 150, 150, 150, 255 };
const Color COL_TEXT_FAINT = {  98,  98,  98, 255 };
const Color COL_TEXT_GHOST = {  58,  58,  58, 255 };
const Color COL_ACCENT     = { 255, 255, 255, 255 };
const Color COL_SCRIM      = {   0,   0,   0, 160 };

//------------------------------------------------------------------------------------
// Screens
//------------------------------------------------------------------------------------
enum Screen
{
    SCREEN_INTRO = 0,
    SCREEN_MENU,
    SCREEN_PLAYING,
    SCREEN_PAUSE,
    SCREEN_SETTINGS,
    SCREEN_CLEAR,     // short beat after an exit is reached
    SCREEN_ENDING,    // final sequence before the credits
    SCREEN_CREDITS
};

//------------------------------------------------------------------------------------
// Board: a finite arrangement of dominoes and one door.
//------------------------------------------------------------------------------------
constexpr int MAX_TILES = 16;

struct Domino
{
    Vector2 center   = { 0, 0 };
    bool    horizontal = false;   // false = tall tile, true = tile lying on its side
    int     valueA   = 0;         // pips on the first half (subset of a double-six set)
    int     valueB   = 0;         // pips on the second half
    int     pipsLeft = 0;         // pips still standing; every hard hit strips some
    bool    gone     = false;     // fully erased: no longer solid, leaves only a trace
    bool    keystone = false;     // drawn brighter, few pips, breaks almost immediately
    float   hitFlash = 0.0f;      // 0..1 impact glow, decays fast
    float   hitCool  = 0.0f;      // seconds until this tile can be stripped again
};

struct Board
{
    Domino tiles[MAX_TILES];
    int    stage        = 0;      // 0..STAGE_COUNT-1
    int    emptied      = 0;      // tiles erased on this stage (drives the door seal)
    int    emptiedTotal = 0;      // tiles erased since the game began
    int    sealNeed     = 1;      // tiles that must be erased to open the door
    bool   sealOpen     = false;
    float  doorPulse    = 0.0f;
};

//------------------------------------------------------------------------------------
// Ball: momentum, then a procedural squash that relaxes back to a circle.
//------------------------------------------------------------------------------------
struct Ball
{
    Vector2 pos    = { 0, 0 };
    Vector2 vel    = { 0, 0 };
    float   radius = 9.0f;
    float   squash = 0.0f;        // 0 = round, 1 = fully flattened by the last impact
    float   impactAngle = 0.0f;   // normal direction of that impact (radians)
    bool    lost   = false;       // rolled off the edge of the void
    float   lostTimer = 0.0f;     // how long it has been gone
};

//------------------------------------------------------------------------------------
// Movement traces. A fixed ring buffer: the past is recorded, then it stops being
// visible. Ghost samples are the previous attempt's route, kept briefly so the
// player can read their own line before it disappears too.
//------------------------------------------------------------------------------------
constexpr int TRAIL_CAP   = 512;
constexpr float TRAIL_SPACING = 7.0f;   // px between dots -> the trail reads as dotted

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

    float life      = 2.4f;    // seconds a live dot survives
    float ghostLife = 9.0f;    // how long the previous attempt's route is readable
    float ghostGain = 0.40f;   // brightness of that route, shrinks every stage

    void Push(Vector2 p, float now);
    void MarkGhost();                 // called on retry: existing dots become history
    void Clear();
};

//------------------------------------------------------------------------------------
// Story: sparse lines that react to progression and to what the player actually did.
//------------------------------------------------------------------------------------
struct Story
{
    std::string line;
    float hold    = 0.0f;     // seconds remaining on screen
    float shownAt = 0.0f;
    float cooldown = 0.0f;

    void Say(const char* text, float seconds = 4.2f);
    void Update(float dt);
};

//------------------------------------------------------------------------------------
// Per-stage tuning. The world does not get harder; it gets thinner - less memory,
// less sound, fewer marks left behind.
//------------------------------------------------------------------------------------
struct StageConfig
{
    int    sealNeed;      // erasures required to break the door seal
    float  memory;        // multiplier on trail lifetime (shorter = less history)
    float  ghostGain;     // brightness allowed for the previous attempt's route
    float  volume;        // music level on this stage
    float  cutoff;        // low-pass corner frequency in Hz
    float  dropoutEvery;  // seconds between audio dropouts (0 = none)
    float  dropoutLen;    // length of each dropout
    const char* enterLine;
    const char* retryLine;
};

const StageConfig& StageTuning(int stage);
const char* ClearLine(int stage);   // shown for a beat after an exit is reached

//------------------------------------------------------------------------------------
// Settings (Raygui only ever touches this, never the play space)
//------------------------------------------------------------------------------------
struct Settings
{
    float volume          = 1.0f;
    float memory          = 1.0f;   // player-facing trail memory scale
    bool  skipIntro       = true;   // skip the opening on later launches
    bool  showBg          = false;  // placeholder bg.png, off for a clean void
    bool  introSeen       = false;
};

//------------------------------------------------------------------------------------
// Scene transition: fade to black, swap screen, fade back. Used instead of cuts so the
// game never "jumps" between attempts.
//------------------------------------------------------------------------------------
struct Transition
{
    bool   active = false;
    bool   toBlack = false;
    Screen target  = SCREEN_PLAYING;
    float  t       = 0.0f;
    float  speed   = 3.5f;
};

struct Game
{
    Screen screen = SCREEN_INTRO;

    Font  font       = {};
    bool  fontLoaded = false;
    Texture2D bg     = {};
    bool  bgLoaded   = false;

    Board    board;
    Ball     ball;
    Trail    trail;
    Story    story;
    Settings settings;
    Transition transition;

    float clock     = 0.0f;   // seconds since launch, the monotonic clock everything else reads
    float boardReveal = 1.0f; // 0..1 fade-in used by the opening sequence
    float ballReveal  = 1.0f;
    float sceneTime = 0.0f;   // seconds spent in the current screen
    float stageTime = 0.0f;   // seconds spent on the current stage/attempt
    int   attempt   = 0;      // attempts on the current stage
    int   pendingStage = 0;   // stage queued by a reached exit
    bool  finished   = false;      // the final passage was reached at least once
    Screen returnScreen = SCREEN_MENU;   // where SETTINGS should hand control back to
    bool  doorHintShown = false;
    bool  edgeHintShown = false;
    bool  hasSave   = false;  // a stored session exists
    bool  quitToMenu = false;

    std::string itchUrl;      // resolved at startup (file override, then build default)
    bool        linkHover  = false;
    bool        linkFocus  = false;
    std::string statusNote;   // short faint note (audio missing, link not configured...)
};

extern const Vector2 BALL_START;

//------------------------------------------------------------------------------------
// Game flow
//------------------------------------------------------------------------------------
void GoToScreen(Game& g, Screen screen);
void RequestTransition(Game& g, Screen screen, float speed = 3.5f);
void UpdateGame(Game& g, float dt);
void HandleInput(Game& g);

void StartNewGame(Game& g);        // wipes the board and the stored session
void ContinueGame(Game& g, bool saved);
void EnterStage(Game& g, int stage);
void RestartAttempt(Game& g, bool fromEdge);
void AdvanceStage(Game& g);        // exit reached
void BeginEnding(Game& g);
void OpenCredits(Game& g);

//------------------------------------------------------------------------------------
// Board / ball / trail
//------------------------------------------------------------------------------------
void InitBoard(Board& b);
void UpdateBoard(Game& g, float dt);
void ResetBall(Game& g);
void UpdateBall(Game& g, float dt);
void UpdateTrail(Game& g, float dt);
Rectangle TileRect(const Domino& d);
int  TileTotalPips(const Domino& d);
bool BallInDoor(const Game& g);

//------------------------------------------------------------------------------------
// Persistence
//------------------------------------------------------------------------------------
bool LoadSession(Game& g);
void SaveSession(const Game& g);
void ClearSession();

//------------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------------
void DrawFrame(Game& g);

// Called inside DrawFrame, while the finished frame is still in the back buffer. Used by
// tools/capture.cpp to read pixels that a post-swap read would lose. Null in normal play.
extern void (*FrameDrawn)();
void DrawIntro(const Game& g);
void DrawPlaying(const Game& g);
void DrawClearBeat(const Game& g);
void DrawEndingSequence(const Game& g);
void DrawBoard(const Game& g, float alpha);
void DrawBallShape(const Ball& b, float alpha);
void DrawTrail(const Trail& t, float memoryScale, float now);
void DrawDoor(const Game& g, float alpha);
void DrawStoryLine(const Game& g);
void DrawHints(const Game& g);
void DrawFade(const Game& g);

// Letterspaced text, because the typography is the only ornament in the game.
float SpacedTextWidth(const Font& font, const char* text, float size, float spacing);
void  DrawSpaced(const Font& font, const char* text, float x, float y, float size,
                 float spacing, Color color);
void  DrawSpacedCentered(const Font& font, const char* text, float centerX, float y,
                         float size, float spacing, Color color);

} // namespace witness
