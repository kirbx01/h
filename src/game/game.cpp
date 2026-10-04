#include "game.h"
#include "sound.h"

#include <algorithm>
#include <cmath>

namespace witness {

namespace {

constexpr float INTRO_LENGTH = 13.4f;

constexpr float END_OUT = 9.4f;
constexpr float OPEN_CUTSCENE_IMAGE_LENGTH = 4.2f;
constexpr float END_CUTSCENE_IMAGE_LENGTH = 4.5f;

void BeginCutscene(Game& g, bool ending)
{
    g.cutsceneEnding = ending;
    g.cutsceneIndex = 0;
    g.cutsceneSkip = false;
    g.cutsceneInputLock = true;
    g.cutsceneFade = true;
    GoToScreen(g, SCREEN_CUTSCENE);
    g.sceneTime = 0.0f;
    g.transition.active = true;
    g.transition.toBlack = false;
    g.transition.target = SCREEN_CUTSCENE;
    g.transition.t = 1.0f;
    g.transition.speed = 2.0f;
    sound::PlayCutscene(ending);
}

void FinishCutscene(Game& g)
{
    const Screen target = g.cutsceneEnding ? SCREEN_CREDITS : SCREEN_PLAYING;
    g.cutsceneInputLock = !g.cutsceneEnding;
    g.cutsceneSkip = false;
    g.cutsceneFade = true;
    GoToScreen(g, target);
    g.transition.active = true;
    g.transition.toBlack = false;
    g.transition.target = target;
    g.transition.t = 1.0f;
    g.transition.speed = 2.0f;
    sound::StopCutscene(!g.cutsceneEnding);
}

void UpdateCutscene(Game& g)
{
    if (g.cutsceneSkip)
    {
        FinishCutscene(g);
        return;
    }

    const float imageLength = g.cutsceneEnding ? END_CUTSCENE_IMAGE_LENGTH
                                               : OPEN_CUTSCENE_IMAGE_LENGTH;
    while (g.sceneTime + 0.0001f >= imageLength)
    {
        g.sceneTime = std::max(0.0f, g.sceneTime - imageLength);
        g.cutsceneIndex++;
        if (g.cutsceneIndex >= 3)
        {
            FinishCutscene(g);
            return;
        }
    }
}

float EaseInOut(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
}

void UpdateIntro(Game& g, float dt)
{
    (void)dt;

    const float t = g.sceneTime;

    const float roll = EaseInOut((t - 9.4f) / 2.9f);
    g.ball.pos = { BALL_START.x + (-BALL_START.x - 60.0f) * (1.0f - roll), BALL_START.y };
    g.ball.vel = { 0.0f, 0.0f };
    g.ballReveal = std::clamp((t - 9.4f) / 1.6f, 0.0f, 1.0f);
    g.boardReveal = std::clamp((t - 10.6f) / 2.2f, 0.0f, 1.0f);

    if (t >= INTRO_LENGTH)
    {
        g.settings.introSeen = true;
        StartNewGame(g);
    }
}

void UpdateEnding(Game& g)
{
    if (g.sceneTime >= END_OUT) BeginCutscene(g, true);
}

}

void GoToScreen(Game& g, Screen screen)
{
    if (g.screen == screen) return;
    g.screen     = screen;
    g.sceneTime  = 0.0f;
    if (screen == SCREEN_MENU || screen == SCREEN_PLAYING) sound::FadeIn();
}

void RequestTransition(Game& g, Screen screen, float speed)
{
    g.transition.active  = true;
    g.transition.toBlack = true;
    g.transition.target  = screen;
    g.transition.speed   = speed;

    if (g.transition.t < 0.0f) g.transition.t = 0.0f;
    if (g.transition.t > 1.0f) g.transition.t = 1.0f;
}

void ResetBall(Game& g)
{
    g.ball.pos    = BALL_START;
    g.ball.vel    = { 0.0f, 0.0f };
    g.ball.squash = 0.0f;
    g.ball.lost   = false;
    g.ball.lostTimer = 0.0f;
    g.ballReveal  = 1.0f;
}

void StartNewGame(Game& g)
{
    InitBoard(g.board);
    g.trail.Clear();
    g.story.line.clear();
    g.boardReveal = 1.0f;
    g.attempt   = 0;
    g.finished  = false;
    g.hasSave   = false;

    ClearSession();
    EnterStage(g, 0);
    g.ball.pipValue = g.board.startPip;
    BeginCutscene(g, false);
}

void ContinueGame(Game& g, bool saved)
{
    if (saved && LoadSession(g))
    {
        g.hasSave = true;
        g.trail.Clear();
        EnterStage(g, g.board.stage);
        g.ball.pipValue = g.board.startPip;
        g.statusNote = "resumed from where you left it.";
        RequestTransition(g, SCREEN_PLAYING, 2.4f);
    }
    else
    {
        StartNewGame(g);
    }
}

void EnterStage(Game& g, int stage)
{
    ConfigureBoardForStage(g.board, stage);
    g.board.emptied  = 0;
    g.board.doorPulse = 0.0f;

    g.attempt   = 0;
    g.stageTime = 0.0f;
    g.doorHintShown = false;
    g.edgeHintShown = false;
    g.boardReveal  = 1.0f;

    g.trail.MarkGhost();
    ResetBall(g);
    g.ball.pipValue = g.board.startPip;

    const char* line = StageTuning(g.board.stage).enterLine;
    if (g.board.stage == 0) g.story.Say(line, 3.4f);
    else                   g.story.Say(line, 4.2f);

    SaveSession(g);
}

void RestartAttempt(Game& g, bool fromEdge)
{
    if (g.ball.lost)
    {
        const int stage = g.board.stage;
        const int emptiedTotal = g.board.emptiedTotal;
        InitBoard(g.board, stage);
        g.board.emptiedTotal = emptiedTotal;
        g.ball.pipValue = g.board.startPip;
    }

    g.trail.Clear();
    ResetBall(g);
    g.stageTime = 0.0f;
    g.attempt++;

    if (fromEdge && !g.edgeHintShown)
    {
        g.edgeHintShown = true;
        g.story.Say("It Went Past the Edge.", 3.4f);
        return;
    }

    g.story.Say(StageTuning(g.board.stage).retryLine, 3.4f);
}

void AdvanceStage(Game& g)
{
    if (g.board.stage >= STAGE_COUNT - 1)
    {
        BeginEnding(g);
        return;
    }

    g.pendingStage = g.board.stage + 1;
    RequestTransition(g, SCREEN_CLEAR, 4.0f);
}

void BeginEnding(Game& g)
{
    g.trail.Clear();
    g.finished = true;
    sound::FadeOut();

    SaveSession(g);

    RequestTransition(g, SCREEN_ENDING, 2.0f);
}

void OpenCredits(Game& g)
{
    BeginCutscene(g, true);
}

Layout ComputeLayoutFor(float screenW, float screenH)
{
    Layout l;
    l.screenW = std::max(1.0f, screenW);
    l.screenH = std::max(1.0f, screenH);

    const float shortEdge = std::min(l.screenW, l.screenH);
    l.margin    = std::clamp(shortEdge * 0.0220f,  8.0f, 34.0f);
    l.hintSize  = 16.0f;
    l.hintSpacing = l.hintSize * 0.16f;
    l.storySize = 18.0f;
    l.storySpacing = l.storySize * 0.20f;

    l.storyY   = l.margin * 0.5f + l.storySize * 0.85f;
    l.topRowY  = l.storyY + l.storySize * 1.45f;

    l.controlRow2Y = l.screenH - l.margin - l.hintSize * 3.0f;
    l.controlRowY  = l.screenH - l.margin - l.hintSize * 1.1f;

    const float bandTop = l.topRowY + l.hintSize + l.margin * 0.35f;
    const float bandBot = l.screenH - l.margin - l.hintSize * 3.4f;

    const float fit    = std::min(l.screenW / (float)DESIGN_W, l.screenH / (float)DESIGN_H);
    const float byBand = std::max(1.0f, bandBot - bandTop) / PLAY_H;

    l.scale = std::max(0.05f, std::min(fit, byBand));

    l.viewW = (float)DESIGN_W * l.scale;
    l.viewH = (float)DESIGN_H * l.scale;
    l.viewX = (l.screenW - l.viewW) * 0.5f;
    l.viewY = (l.scale < fit) ? std::max(0.0f, bandTop - PLAY_Y * l.scale)
                              : (l.screenH - l.viewH) * 0.5f;

    return l;
}

void UpdateGame(Game& g, float dt)
{
    if (g.screen == SCREEN_HELP) return;

    if (g.screen == SCREEN_PLAYING && g.ball.lost)
    {
        g.clock += dt;
        return;
    }

    g.clock    += dt;
    g.sceneTime += dt;

    g.story.Update(dt);
    UpdateBoard(g, dt);

    if (g.transition.active)
    {
        if (g.transition.toBlack)
        {
            g.transition.t += g.transition.speed * dt;
            if (g.transition.t >= 1.0f)
            {
                g.transition.t = 1.0f;
                GoToScreen(g, g.transition.target);
                g.transition.toBlack = false;
            }
        }
        else
        {
            g.transition.t -= g.transition.speed * dt;
            if (g.transition.t <= 0.0f)
            {
                g.transition.t = 0.0f;
                g.transition.active = false;
                g.cutsceneFade = false;
            }
        }
    }

    switch (g.screen)
    {
        case SCREEN_INTRO:
            UpdateIntro(g, dt);
            break;

        case SCREEN_PLAYING:
        {
            if (!g.transition.active)
            {
                g.stageTime += dt;
                if (g.stageTime >= PASSAGE_TIME_LIMIT)
                {
                    g.ball.lost = true;
                    g.ball.lostTimer = 0.0f;
                    g.ball.vel = { 0.0f, 0.0f };
                    break;
                }

                UpdateBall(g, dt);
                UpdateTrail(g, dt);

                if (BallInDoor(g))                       AdvanceStage(g);
            }
            break;
        }

        case SCREEN_CLEAR:
            if (g.sceneTime > 1.9f && !g.transition.active)
            {
                EnterStage(g, g.pendingStage);
                RequestTransition(g, SCREEN_PLAYING, 2.6f);
            }
            break;

        case SCREEN_ENDING:
            UpdateEnding(g);
            break;

        case SCREEN_CUTSCENE:
            UpdateCutscene(g);
            break;

        default:
            break;
    }
}

}
