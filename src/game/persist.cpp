#include "game.h"
#include "platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace witness {

namespace {

constexpr const char* SAVE_MAGIC = "IFG1";
constexpr const char* SAVE_MAGIC_LEGACY = "ATF1";

std::string Serialize(const Game& g)
{
    std::string out = SAVE_MAGIC;
    char buf[64];

    std::snprintf(buf, sizeof(buf), "\nstage=%d\nemptied=%d", g.board.stage, g.board.emptied);
    out += buf;

    if (g.finished)
    {
        std::snprintf(buf, sizeof(buf), "\nfinished=1");
        out += buf;
    }

    for (int i = 0; i < MAX_TILES; i++)
    {
        const Domino& d = g.board.tiles[i];
        if (d.pipsLeft == TileTotalPips(d)) continue;

        std::snprintf(buf, sizeof(buf), "\nt=%d:%d", i, d.pipsLeft);
        out += buf;
    }

    out += "\n";
    return out;
}

bool Field(const std::string& text, const char* key, int& out)
{
    char pattern[64];
    std::snprintf(pattern, sizeof(pattern), "\n%s=", key);

    const size_t at = text.find(pattern);
    if (at == std::string::npos) return false;

    out = std::atoi(text.c_str() + at + std::string(pattern).size());
    return true;
}

}

bool LoadSession(Game& g)
{
    std::string text;
    if (!platform::ReadState(text)) return false;

    if (text.rfind(SAVE_MAGIC, 0) != 0 && text.rfind(SAVE_MAGIC_LEGACY, 0) != 0) return false;

    InitBoard(g.board);

    int stage    = 0;
    int emptied  = 0;
    if (!Field(text, "stage", stage)) return false;
    Field(text, "emptied", emptied);

    size_t at = 0;
    while ((at = text.find("\nt=", at)) != std::string::npos)
    {
        const char* p = text.c_str() + at + 3;
        const int index = std::atoi(p);
        const char* colon = std::strchr(p, ':');
        if (!colon) break;

        const int pips = std::atoi(colon + 1);
        if (index >= 0 && index < MAX_TILES)
        {
            Domino& d  = g.board.tiles[index];
            const int total = TileTotalPips(d);
            d.pipsLeft  = pips < 0 ? 0 : (pips > total ? total : pips);
            d.gone      = d.pipsLeft <= 0;
            if (d.gone) g.board.emptiedTotal++;
        }

        at += 3;
    }

    int finished = 0;
    Field(text, "finished", finished);
    g.finished = finished != 0;

    g.board.stage    = stage < 0 ? 0 : (stage >= STAGE_COUNT ? STAGE_COUNT - 1 : stage);
    g.board.emptied  = emptied;
    g.board.sealNeed = StageTuning(g.board.stage).sealNeed;
    g.board.sealOpen = g.board.emptied >= g.board.sealNeed;

    return true;
}

void SaveSession(const Game& g)
{
    platform::WriteState(Serialize(g));
}

void ClearSession()
{
    platform::ClearState();
}

}
