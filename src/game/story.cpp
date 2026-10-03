#include "game.h"

#include <algorithm>

namespace witness {

namespace {

// Per-stage tuning and lines. Difficulty never rises: what changes is how much of the
// world is still willing to remember. Each stage needs a few more erasures, the trail
// lives shorter, and the music is quieter and further away.
constexpr StageConfig STAGES[STAGE_COUNT] =
{
    // sealNeed  memory  ghostGain  volume  cutoff  dropEvery  dropLen  enter / retry
    {     1,      1.00f,   0.42f,   0.90f, 14000.0f,   0.0f,    0.0f,
      "FIND THE EXIT.",
      "AGAIN." },

    {     2,      0.82f,   0.34f,   0.72f,  5200.0f,  14.0f,   0.22f,
      "SOME OF IT IS ALREADY GONE.",
      "YOU LEFT THIS HERE." },

    {     2,      0.66f,   0.26f,   0.56f,  2600.0f,  11.0f,   0.35f,
      "THE NUMBERS DON'T ADD UP.",
      "YOU'VE BEEN HERE." },

    {     3,      0.50f,   0.18f,   0.42f,  1250.0f,   8.0f,   0.55f,
      "NOTHING IS WHERE YOU LEFT IT.",
      "STOP TRYING TO RESTORE IT." },

    {     3,      0.36f,   0.10f,   0.30f,   620.0f,   6.0f,   0.80f,
      "WHAT DID YOU LEAVE BEHIND?",
      "STILL HERE." },
};

// Shown for a moment after an exit is reached. The final stage goes straight to the
// ending instead, so its slot stays empty.
constexpr const char* CLEAR_LINES[STAGE_COUNT] =
{
    "IT OPENS.",
    "SOMETHING ELSE GAVE WAY ON THE WAY HERE.",
    "THE NUMBERS ARE WRONG NOW.",
    "IT WAS HERE BEFORE.",
    "",
};

} // namespace

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

    // Ignore a repeat of the line that is already on screen so the text does not
    // flicker when the same condition fires twice in a row.
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

} // namespace witness
