/* Standalone checks for the real acc_status.c, with actual engine types and
 * small fixtures at its engine/speech boundaries. No game or speech backend
 * is opened. Each scenario runs in a fresh process.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "fixer.h"
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "bh_types.h"
#include "acc_speech.h"
#include "acc_status.h"

AVP_GAME_DESC AvP;
TEMPLATE_WEAPON_DATA TemplateWeapon[MAX_NO_OF_WEAPON_TEMPLATES];
TEMPLATE_AMMO_DATA TemplateAmmo[MAX_NO_OF_AMMO_TEMPLATES];
GRENADE_LAUNCHER_DATA GrenadeLauncherData;
int AccPadTrace;

static NPC_DATA npc_data[4];
static NPC_TYPES last_npc_type;
static int npc_calls;
static int npc_missing;
static PLAYER_STATUS player;
static char text[2048];
static char spoken[4096];
static int speech_calls, last_interrupt;
static int assertions, failures;
static int long_weapon_name;
static int text_mode, text_lookups;

int AccSpeech_IsAvailable(void) { return 1; }

NPC_DATA *GetThisNpcData(NPC_TYPES type)
{
    int i;
    ++npc_calls;
    last_npc_type = type;
    if (npc_missing) return NULL;
    for (i = 0; i < 4; ++i) if (npc_data[i].Type == type) return &npc_data[i];
    return NULL;
}

char *GetTextString(enum TEXTSTRING_ID id)
{
    static char long_name[4096];
    ++text_lookups;
    if (text_mode == 1) return NULL;
    if (text_mode == 2) return "";
    if (long_weapon_name) {
        memset(long_name, 'W', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = 0;
        return long_name;
    }
    switch (id) {
    case TEXTSTRING_INGAME_PULSERIFLE: return "Fixture pulse rifle";
    case TEXTSTRING_INGAME_SMARTGUN: return "Fixture smartgun";
    case TEXTSTRING_INGAME_FLAMETHROWER: return "Fixture flamethrower";
    case TEXTSTRING_INGAME_GRENADELAUNCHER: return "Fixture grenade launcher";
    case TEXTSTRING_INGAME_MARINE_PISTOL: return "Fixture pistol";
    case TEXTSTRING_INGAME_TWOPISTOLS: return "Fixture dual pistols";
    case TEXTSTRING_AMMO_SHORTNAME_10MM_CULW: return "Rifle ammunition";
    case TEXTSTRING_AMMO_SHORTNAME_SMARTGUN: return "Smartgun ammunition";
    case TEXTSTRING_AMMO_SHORTNAME_FLAMETHROWER: return "Fuel";
    case TEXTSTRING_AMMO_SHORTNAME_PULSE_GRENADE: return "Pulse grenades";
    case TEXTSTRING_AMMO_SHORTNAME_MARINE_PISTOL: return "Pistol ammunition";
    case TEXTSTRING_AMMO_SHORTNAME_GRENADE: return "Standard grenades";
    case TEXTSTRING_AMMO_SHORTNAME_FLARE_GRENADE: return "Flare grenades";
    case TEXTSTRING_AMMO_SHORTNAME_FRAGMENTATION_GRENADE: return "Fragmentation grenades";
    case TEXTSTRING_AMMO_SHORTNAME_PROXIMITY_GRENADE: return "Proximity grenades";
    case TEXTSTRING_SELECTEDGRENADE_STANDARD: return "Standard";
    case TEXTSTRING_SELECTEDGRENADE_FLARE: return "Flare";
    case TEXTSTRING_SELECTEDGRENADE_FRAGMENTATION: return "Fragmentation";
    case TEXTSTRING_SELECTEDGRENADE_PROXIMITY: return "Proximity";
    case TEXTSTRING_ROUNDS: return "Rounds";
    case TEXTSTRING_MAGAZINES: return "Magazines";
    case TEXTSTRING_MAGAZINES_FLAMETHROWER: return "Fuel tanks";
    default: return "Unknown fixture text";
    }
}

void AccSpeech_Say(const char *message, int interrupt)
{
    ++speech_calls;
    last_interrupt = interrupt;
    if (!message) message = "";
    strncpy(spoken, message, sizeof(spoken) - 1);
    spoken[sizeof(spoken) - 1] = 0;
}

static void check(int condition, const char *description)
{
    ++assertions;
    printf("%s: %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) {
        ++failures;
        printf("  Formatted: %s\n", text);
    }
}

static const char *find_ci(const char *haystack, const char *needle)
{
    size_t i, length = strlen(needle);
    for (; *haystack; ++haystack) {
        for (i = 0; i < length; ++i) {
            if (!haystack[i] || tolower((unsigned char)haystack[i]) !=
                                tolower((unsigned char)needle[i])) break;
        }
        if (i == length) return haystack;
    }
    return NULL;
}

static int number_after(const char *message, const char *label, unsigned long expected)
{
    const char *at = find_ci(message, label);
    if (!at) return 0;
    at += strlen(label);
    while (*at && !isdigit((unsigned char)*at)) ++at;
    return *at && strtoul(at, NULL, 10) == expected;
}

/* Compare observable numbers in reading order, independent of punctuation.
 * All fixture names deliberately contain no digits. Expected lists are
 * hand-authored examples, not a second implementation of the formatter.
 */
static int numbers_match(const unsigned long *expected, size_t count)
{
    const char *at = text;
    size_t found = 0;
    while (*at) {
        if (isdigit((unsigned char)*at)) {
            char *end;
            unsigned long actual = strtoul(at, &end, 10);
            if (found >= count || actual != expected[found]) return 0;
            ++found;
            at = end;
        } else ++at;
    }
    return found == count;
}

static PLAYER_WEAPON_DATA *select_weapon(enum WEAPON_ID id)
{
    PLAYER_WEAPON_DATA *weapon;
    player.SelectedWeaponSlot = WEAPON_SLOT_4;
    weapon = &player.WeaponSlot[player.SelectedWeaponSlot];
    memset(weapon, 0, sizeof(*weapon));
    weapon->WeaponIDNumber = id;
    weapon->Possessed = 1;
    return weapon;
}

static void fixture(void)
{
    static const NPC_TYPES types[] = {
        I_PC_Marine_Easy, I_PC_Marine_Medium,
        I_PC_Marine_Hard, I_PC_Marine_Impossible
    };
    int i;
    memset(&AvP, 0, sizeof(AvP));
    memset(&player, 0, sizeof(player));
    memset(TemplateWeapon, 0, sizeof(TemplateWeapon));
    memset(TemplateAmmo, 0, sizeof(TemplateAmmo));
    memset(&GrenadeLauncherData, 0, sizeof(GrenadeLauncherData));
    memset(npc_data, 0, sizeof(npc_data));
    memset(text, 0, sizeof(text));
    memset(spoken, 0, sizeof(spoken));
    npc_calls = npc_missing = speech_calls = last_interrupt = long_weapon_name = 0;
    text_mode = text_lookups = AccPadTrace = 0;
    AvP.PlayerType = I_Marine;
    AvP.Difficulty = I_Medium;
    player.IsAlive = 1;
    player.Health = 100 * ONE_FIXED;
    player.Armour = 20 * ONE_FIXED;
    for (i = 0; i < MAX_NO_OF_WEAPON_SLOTS; ++i)
        player.WeaponSlot[i].WeaponIDNumber = NULL_WEAPON;
    for (i = 0; i < 4; ++i) {
        npc_data[i].Type = types[i];
        npc_data[i].StartingStats.Health = i == 3 ? 25 : 100;
        npc_data[i].StartingStats.Armour = i == 3 ? 8 : 20;
    }
    for (i = 0; i < MAX_NO_OF_WEAPON_TEMPLATES; ++i) {
        TemplateWeapon[i].PrimaryAmmoID = AMMO_NONE;
        TemplateWeapon[i].SecondaryAmmoID = AMMO_NONE;
    }
    TemplateWeapon[WEAPON_PULSERIFLE].Name = TEXTSTRING_INGAME_PULSERIFLE;
    TemplateWeapon[WEAPON_PULSERIFLE].PrimaryAmmoID = AMMO_10MM_CULW;
    TemplateWeapon[WEAPON_PULSERIFLE].SecondaryAmmoID = AMMO_PULSE_GRENADE;
    TemplateWeapon[WEAPON_SMARTGUN].Name = TEXTSTRING_INGAME_SMARTGUN;
    TemplateWeapon[WEAPON_SMARTGUN].PrimaryAmmoID = AMMO_SMARTGUN;
    TemplateWeapon[WEAPON_FLAMETHROWER].Name = TEXTSTRING_INGAME_FLAMETHROWER;
    TemplateWeapon[WEAPON_FLAMETHROWER].PrimaryAmmoID = AMMO_FLAMETHROWER;
    TemplateWeapon[WEAPON_GRENADELAUNCHER].Name = TEXTSTRING_INGAME_GRENADELAUNCHER;
    TemplateWeapon[WEAPON_GRENADELAUNCHER].PrimaryAmmoID = AMMO_GRENADE;
    TemplateWeapon[WEAPON_MARINE_PISTOL].Name = TEXTSTRING_INGAME_MARINE_PISTOL;
    TemplateWeapon[WEAPON_MARINE_PISTOL].PrimaryAmmoID = AMMO_MARINE_PISTOL_PC;
    TemplateWeapon[WEAPON_MARINE_PISTOL].SecondaryAmmoID = AMMO_MARINE_PISTOL_PC;
    TemplateWeapon[WEAPON_TWO_PISTOLS].Name = TEXTSTRING_INGAME_TWOPISTOLS;
    TemplateWeapon[WEAPON_TWO_PISTOLS].PrimaryAmmoID = AMMO_MARINE_PISTOL_PC;
    TemplateWeapon[WEAPON_TWO_PISTOLS].SecondaryAmmoID = AMMO_MARINE_PISTOL_PC;
    /* The shipping table incorrectly gives the cudgel the pulse-rifle name. */
    TemplateWeapon[WEAPON_CUDGEL].Name = TEXTSTRING_INGAME_PULSERIFLE;
    TemplateWeapon[WEAPON_CUDGEL].PrimaryAmmoID = AMMO_CUDGEL;
    TemplateWeapon[WEAPON_CUDGEL].PrimaryIsMeleeWeapon = 1;
    TemplateAmmo[AMMO_10MM_CULW].ShortName = TEXTSTRING_AMMO_SHORTNAME_10MM_CULW;
    TemplateAmmo[AMMO_SMARTGUN].ShortName = TEXTSTRING_AMMO_SHORTNAME_SMARTGUN;
    TemplateAmmo[AMMO_FLAMETHROWER].ShortName = TEXTSTRING_AMMO_SHORTNAME_FLAMETHROWER;
    TemplateAmmo[AMMO_FLAMETHROWER].AmmoPerMagazine = 100;
    TemplateAmmo[AMMO_PULSE_GRENADE].ShortName = TEXTSTRING_AMMO_SHORTNAME_PULSE_GRENADE;
    TemplateAmmo[AMMO_MARINE_PISTOL_PC].ShortName = TEXTSTRING_AMMO_SHORTNAME_MARINE_PISTOL;
    TemplateAmmo[AMMO_GRENADE].ShortName = TEXTSTRING_AMMO_SHORTNAME_GRENADE;
    TemplateAmmo[AMMO_FLARE_GRENADE].ShortName = TEXTSTRING_AMMO_SHORTNAME_FLARE_GRENADE;
    TemplateAmmo[AMMO_FRAGMENTATION_GRENADE].ShortName = TEXTSTRING_AMMO_SHORTNAME_FRAGMENTATION_GRENADE;
    TemplateAmmo[AMMO_PROXIMITY_GRENADE].ShortName = TEXTSTRING_AMMO_SHORTNAME_PROXIMITY_GRENADE;
    select_weapon(WEAPON_SMARTGUN);
}

static void health(void)
{
    static const struct {
        I_HARDANUFF difficulty;
        NPC_TYPES type;
        int health, armour;
    } cases[] = {
        {I_Easy, I_PC_Marine_Easy, 50 * ONE_FIXED, 10 * ONE_FIXED},
        {I_Medium, I_PC_Marine_Medium, 50 * ONE_FIXED, 10 * ONE_FIXED},
        {I_Hard, I_PC_Marine_Hard, 50 * ONE_FIXED, 10 * ONE_FIXED},
        {I_Impossible, I_PC_Marine_Impossible, 12 * ONE_FIXED + ONE_FIXED / 2, 4 * ONE_FIXED}
    };
    int i;
    for (i = 0; i < 4; ++i) {
        AvP.Difficulty = cases[i].difficulty;
        player.Health = cases[i].health;
        player.Armour = cases[i].armour;
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0, "living Marine has a formatted status");
        check(last_npc_type == cases[i].type, "health and armour use the current difficulty's Marine base stats");
        check(number_after(text, "health", 50) &&
              (number_after(text, "armour", 50) || number_after(text, "armor", 50)),
              "half of difficulty-specific health and armour is reported as 50 percent");
    }
    check(speech_calls == 0, "formatting alone never speaks");
}

static void health_edges(void)
{
    static const struct {
        int health, armour;
        unsigned long health_percent, armour_percent;
        const char *name;
    } cases[] = {
        {100 * ONE_FIXED, 20 * ONE_FIXED, 100, 100, "full values report 100 percent"},
        {73 * ONE_FIXED + ONE_FIXED / 4, 11 * ONE_FIXED + ONE_FIXED / 4, 74, 57, "fractional percentages round upward like the HUD"},
        {100 * ONE_FIXED - 1, 20 * ONE_FIXED - 1, 99, 99, "slightly damaged values report 99 percent rather than 100"},
        {1, 1, 1, 1, "small positive values remain one percent"},
        {0, 0, 0, 0, "zero values are safe while the living flag is still set"},
        {-ONE_FIXED, -ONE_FIXED, 0, 0, "negative transient values do not produce negative percentages"},
        {125 * ONE_FIXED, 25 * ONE_FIXED, 125, 125, "above-normal values preserve HUD percentages above 100"},
        {INT_MAX, INT_MAX, 32768, 163840, "large fixed-point statistics do not overflow when multiplied by 100"}
    };
    int i;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        player.Health = cases[i].health;
        player.Armour = cases[i].armour;
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 &&
              number_after(text, "health", cases[i].health_percent) &&
              (number_after(text, "armour", cases[i].armour_percent) ||
               number_after(text, "armor", cases[i].armour_percent)), cases[i].name);
    }
}

static void primary_ammo(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_SMARTGUN);
    static const struct {
        unsigned int raw;
        unsigned long rounds;
    } cases[] = {{0, 0}, {11u * ONE_FIXED, 11}, {11u * ONE_FIXED + 1, 12}, {1, 1}, {UINT_MAX, 65536}};
    int i;
    weapon->PrimaryMagazinesRemaining = 3;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        unsigned long expected[] = {100, 100, cases[i].rounds, 3};
        weapon->PrimaryRoundsRemaining = cases[i].raw;
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(expected, 4),
              "loaded rounds use HUD ceiling, spare magazines remain separate, and maximum raw ammo does not overflow");
        check(find_ci(text, "Fixture smartgun") && find_ci(text, "round") && find_ci(text, "magazine"),
              "selected weapon uses the engine's localized name and labels the ammunition units");
    }
    weapon = select_weapon(WEAPON_MARINE_PISTOL);
    weapon->PrimaryRoundsRemaining = 7u * ONE_FIXED;
    weapon->PrimaryMagazinesRemaining = 2;
    weapon->SecondaryRoundsRemaining = 99u * ONE_FIXED;
    weapon->SecondaryMagazinesRemaining = 98;
    {
        const unsigned long expected[] = {100, 100, 7, 2};
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(expected, 4),
              "single pistol does not announce its shared secondary fire as a second ammunition pool");
    }
}

static void pulse(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_PULSERIFLE);
    const unsigned long expected[] = {100, 100, 45, 2, 4, 1};
    weapon->PrimaryRoundsRemaining = 44u * ONE_FIXED + ONE_FIXED / 2;
    weapon->PrimaryMagazinesRemaining = 2;
    weapon->SecondaryRoundsRemaining = 3u * ONE_FIXED + ONE_FIXED / 4;
    weapon->SecondaryMagazinesRemaining = 1;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(expected, 6),
          "pulse rifle reports separate primary rounds/spares and secondary grenades/spares");
    check(find_ci(text, "Fixture pulse rifle") && find_ci(text, "grenade"),
          "pulse rifle and secondary grenades are named");
}

static void dual_pistols(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_TWO_PISTOLS);
    const unsigned long expected[] = {100, 100, 11, 2, 7, 4};
    weapon->PrimaryRoundsRemaining = 11u * ONE_FIXED;
    weapon->PrimaryMagazinesRemaining = 2;
    weapon->SecondaryRoundsRemaining = 6u * ONE_FIXED + 1;
    weapon->SecondaryMagazinesRemaining = 4;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(expected, 6),
          "dual pistols retain independent loaded and spare counts for both guns");
    check(number_after(text, "right", 11) && number_after(text, "left", 7),
          "primary counts describe the right pistol and secondary counts describe the left pistol");
}

static void grenades(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_GRENADELAUNCHER);
    static const struct { enum AMMO_ID id; const char *name; } cases[] = {
        {AMMO_GRENADE, "Standard"}, {AMMO_FLARE_GRENADE, "Flare"},
        {AMMO_PROXIMITY_GRENADE, "Proximity"}, {AMMO_FRAGMENTATION_GRENADE, "Fragmentation"}
    };
    const unsigned long expected[] = {100, 100, 10, 2};
    int i;
    weapon->PrimaryRoundsRemaining = 9u * ONE_FIXED + ONE_FIXED / 2;
    weapon->PrimaryMagazinesRemaining = 2;
    /* These saved pools are stale while selected; live weapon values win. */
    GrenadeLauncherData.StandardRoundsRemaining = 97u * ONE_FIXED;
    GrenadeLauncherData.FlareRoundsRemaining = 96u * ONE_FIXED;
    GrenadeLauncherData.ProximityRoundsRemaining = 95u * ONE_FIXED;
    GrenadeLauncherData.FragmentationRoundsRemaining = 94u * ONE_FIXED;
    GrenadeLauncherData.StandardMagazinesRemaining = 91;
    GrenadeLauncherData.FlareMagazinesRemaining = 92;
    GrenadeLauncherData.ProximityMagazinesRemaining = 93;
    GrenadeLauncherData.FragmentationMagazinesRemaining = 94;
    for (i = 0; i < 4; ++i) {
        GrenadeLauncherData.SelectedAmmo = cases[i].id;
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(expected, 4),
              "grenade launcher reads live selected-weapon counts rather than stale saved grenade pools");
        check(find_ci(text, "Fixture grenade launcher") && find_ci(text, cases[i].name),
              "grenade launcher identifies the currently selected grenade type");
    }
}

static void fuel(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_FLAMETHROWER);
    const unsigned long expected[] = {100, 100, 42, 3};
    weapon->PrimaryRoundsRemaining = 41u * ONE_FIXED + 1;
    weapon->PrimaryMagazinesRemaining = 3;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0,
          "flamethrower status formats successfully");
    check(find_ci(text, "Fixture flamethrower") && find_ci(text, "fuel") && !find_ci(text, "round"),
          "flamethrower identifies fuel without describing it as rounds");
    check(numbers_match(expected, 4) && find_ci(text, "tank"),
          "fractional fuel rounds upward and reserve containers are identified");
}

static void invalid_weapon(void)
{
    static const int slots[] = {-1, MAX_NO_OF_WEAPON_SLOTS};
    static const int ids[] = {NULL_WEAPON, MAX_NO_OF_WEAPON_TEMPLATES};
    const unsigned long health_only[] = {100, 100};
    PLAYER_WEAPON_DATA *weapon;
    int i;
    for (i = 0; i < 2; ++i) {
        player.SelectedWeaponSlot = (enum WEAPON_SLOT)slots[i];
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
              find_ci(text, "weapon status unavailable"), "out-of-range selected slots report unavailable weapon status while preserving health and armour");
    }
    for (i = 0; i < 2; ++i) {
        select_weapon((enum WEAPON_ID)ids[i]);
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
              find_ci(text, "weapon status unavailable"), "empty or invalid weapon IDs do not index the weapon templates");
    }
    weapon = select_weapon(WEAPON_SMARTGUN);
    weapon->Possessed = 0;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
          find_ci(text, "weapon status unavailable"), "unpossessed weapon slot reports unavailable status");
    weapon->Possessed = -1;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
          find_ci(text, "weapon status unavailable"), "negative inventory possession state is unavailable, not an equipped weapon");
    weapon->Possessed = -2;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
          find_ci(text, "weapon status unavailable"), "the other negative signed possession state is also unavailable");
    weapon = select_weapon(WEAPON_CUDGEL);
    weapon->PrimaryRoundsRemaining = 88u * ONE_FIXED;
    weapon->PrimaryMagazinesRemaining = 7;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && numbers_match(health_only, 2) &&
          find_ci(text, "cudgel") && !find_ci(text, "pulse rifle") && !find_ci(text, "round"),
          "cudgel overrides the mistaken template name and never announces ammunition");
}

static void suppressed(void)
{
    check(AccStatus_FormatMarine(NULL, text, sizeof(text)) == 0, "null player has no status");
    AccStatus_AnnounceMarine(NULL);
    player.IsAlive = 0;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) == 0, "dead Marine has no status");
    AccStatus_AnnounceMarine(&player);
    player.IsAlive = 1;
    AvP.PlayerType = I_Alien;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) == 0, "Alien does not receive Marine status");
    AccStatus_AnnounceMarine(&player);
    AvP.PlayerType = I_Predator;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) == 0, "Predator does not receive Marine status");
    AccStatus_AnnounceMarine(&player);
    AvP.PlayerType = I_Marine;
    player.DemoMode = 1;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) == 0, "demo playback does not receive live player status");
    AccStatus_AnnounceMarine(&player);
    check(speech_calls == 0, "null, dead, non-Marine, and demo requests never call speech");
    check(npc_calls == 0, "suppressed requests do not request Marine base statistics");
}

static void announcement(void)
{
    PLAYER_WEAPON_DATA *weapon = select_weapon(WEAPON_SMARTGUN);
    weapon->PrimaryRoundsRemaining = 19u * ONE_FIXED;
    weapon->PrimaryMagazinesRemaining = 2;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && speech_calls == 0,
          "formatting produces status without speech side effects");
    AccStatus_AnnounceMarine(&player);
    check(speech_calls == 1 && last_interrupt == 1 && !strcmp(spoken, text),
          "one requested announcement speaks the formatted status once and interrupts prior speech");
    AccStatus_AnnounceMarine(&player);
    check(speech_calls == 2 && last_interrupt == 1 && !strcmp(spoken, text),
          "repeating the status request speaks again even when values are unchanged");
    player.Health = 62 * ONE_FIXED;
    weapon->PrimaryRoundsRemaining = 8u * ONE_FIXED;
    AccStatus_AnnounceMarine(&player);
    check(speech_calls == 3 && number_after(spoken, "health", 62) && strcmp(spoken, text),
          "the next request reads current state rather than cached health and ammunition");
}

static void unavailable_data(void)
{
    static const int invalid_text_ids[] = {-1, MIN_NEW_TEXTSTRINGS, MAX_NEW_TEXTSTRINGS};
    static const int invalid_ammo_ids[] = {-1, MAX_NO_OF_AMMO_TEMPLATES};
    int i;
    npc_missing = 1;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 &&
          find_ci(text, "Health unavailable") && find_ci(text, "Armor unavailable"),
          "missing base statistics are described as unavailable without division by zero");
    npc_missing = 0;
    npc_data[1].StartingStats.Health = 0;
    check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 &&
          find_ci(text, "Health unavailable") && number_after(text, "armor", 100),
          "invalid health base does not hide independently valid armour status");
    npc_data[1].StartingStats.Health = 100;
    for (i = 0; i < 3; ++i) {
        TemplateWeapon[WEAPON_SMARTGUN].Name = (enum TEXTSTRING_ID)invalid_text_ids[i];
        text_lookups = 0;
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && find_ci(text, "Weapon.") && text_lookups == 0,
              "invalid localized-name IDs use a fallback without indexing the language table");
    }
    TemplateWeapon[WEAPON_SMARTGUN].Name = TEXTSTRING_INGAME_SMARTGUN;
    for (text_mode = 1; text_mode <= 2; ++text_mode)
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && find_ci(text, "Weapon."),
              "null and empty localized weapon names use a readable fallback");
    text_mode = 0;
    select_weapon(WEAPON_GRENADELAUNCHER);
    for (i = 0; i < 2; ++i) {
        GrenadeLauncherData.SelectedAmmo = (enum AMMO_ID)invalid_ammo_ids[i];
        check(AccStatus_FormatMarine(&player, text, sizeof(text)) != 0 && find_ci(text, "Selected grenades"),
              "invalid selected grenade IDs use a fallback without indexing the ammo table");
    }
}

static void buffers(void)
{
    struct { unsigned char prefix[16]; char data[1024]; unsigned char suffix[16]; } guarded;
    static const size_t sizes[] = {0, 1, 2, 8, 64, 512};
    size_t i, j;
    check(AccStatus_FormatMarine(&player, NULL, 0) == 0, "null zero-sized output is rejected safely");
    check(AccStatus_FormatMarine(&player, NULL, 100) == 0, "null nonzero-sized output is rejected safely");
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        int guards_ok = 1;
        memset(&guarded, 0xa5, sizeof(guarded));
        AccStatus_FormatMarine(&player, guarded.data, sizes[i]);
        for (j = 0; j < sizeof(guarded.prefix); ++j) if (guarded.prefix[j] != 0xa5) guards_ok = 0;
        for (j = sizes[i]; j < sizeof(guarded.data); ++j) if ((unsigned char)guarded.data[j] != 0xa5) guards_ok = 0;
        for (j = 0; j < sizeof(guarded.suffix); ++j) if (guarded.suffix[j] != 0xa5) guards_ok = 0;
        check(guards_ok, "formatter never writes before the buffer or beyond the supplied capacity");
        if (sizes[i]) check(memchr(guarded.data, 0, sizes[i]) != NULL,
                            "every nonempty output capacity is terminated even when truncated");
    }
    long_weapon_name = 1;
    memset(&guarded, 0xa5, sizeof(guarded));
    AccStatus_FormatMarine(&player, guarded.data, sizeof(guarded.data));
    check(memchr(guarded.data, 0, sizeof(guarded.data)) != NULL &&
          guarded.prefix[15] == 0xa5 && guarded.suffix[0] == 0xa5,
          "oversized localized names remain bounded and terminated");
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: status_tests CASE\n"); return 2; }
    fixture();
    if (!strcmp(argv[1], "health")) health();
    else if (!strcmp(argv[1], "health_edges")) health_edges();
    else if (!strcmp(argv[1], "primary_ammo")) primary_ammo();
    else if (!strcmp(argv[1], "pulse")) pulse();
    else if (!strcmp(argv[1], "dual_pistols")) dual_pistols();
    else if (!strcmp(argv[1], "grenades")) grenades();
    else if (!strcmp(argv[1], "fuel")) fuel();
    else if (!strcmp(argv[1], "invalid_weapon")) invalid_weapon();
    else if (!strcmp(argv[1], "suppressed")) suppressed();
    else if (!strcmp(argv[1], "announcement")) announcement();
    else if (!strcmp(argv[1], "unavailable_data")) unavailable_data();
    else if (!strcmp(argv[1], "buffers")) buffers();
    else { fprintf(stderr, "Unknown case: %s\n", argv[1]); return 2; }
    printf("%s: %d assertions, %d failed\n", argv[1], assertions, failures);
    return failures ? 1 : 0;
}
