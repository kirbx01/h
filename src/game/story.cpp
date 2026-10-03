#include "game.h"

#include <algorithm>

namespace witness {

namespace {

constexpr StageConfig STAGES[STAGE_COUNT] =
{

    {     1,      1.00f,   0.42f,   0.90f, 14000.0f,   0.0f,    0.0f,
      "Find the Exit.",
      "Again." },

    {     2,      0.82f,   0.34f,   0.72f,  5200.0f,  14.0f,   0.22f,
      "Some of It Is Already Gone.",
      "You Left This Here." },

    {     2,      0.66f,   0.26f,   0.56f,  2600.0f,  11.0f,   0.35f,
      "The Numbers Don't Add Up.",
      "You've Been Here." },

    {     3,      0.50f,   0.18f,   0.42f,  1250.0f,   8.0f,   0.55f,
      "Nothing Is Where You Left It.",
      "Stop Trying to Restore It." },

    {     3,      0.36f,   0.10f,   0.30f,   620.0f,   6.0f,   0.80f,
      "What Did You Leave Behind?",
      "Still Here." },
};

constexpr const char* CLEAR_LINES[STAGE_COUNT] =
{
    "It Opens.",
    "Something Else Gave Way on the Way Here.",
    "The Numbers Are Wrong Now.",
    "It Was Here Before.",
    "",
};

}

const char* ClearLine(int stage)
{
    const int i = stage < 0 ? 0 : (stage >= STAGE_COUNT ? STAGE_COUNT - 1 : stage);
    return CLEAR_LINES[i];
}

const StageConfig& StageTuning(int stage)
{
    const int i = stage < 0 ? 0 : (stage >= STAGE_COUNT ? STAGE_COUNT - 1 : stage);
    return STAGES[i];
}

void Story::Say(const char* text, float seconds)
{
    if (!text || !*text) return;

    if (line == text && hold > 0.0f) return;

    line      = text;
    hold      = seconds;
    cooldown  = seconds + 1.6f;
}

void Story::Update(float dt)
{
    if (hold > 0.0f)      hold      = std::max(0.0f, hold - dt);
    if (cooldown > 0.0f)  cooldown -= dt;
}

}
