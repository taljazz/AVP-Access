/* AVP Access: read live Marine status when the player requests it. */
#include "3dc.h"
#include "module.h"
#include "stratdef.h"
#include "gamedef.h"
#include "bh_types.h"
#include "language.h"

#include "acc_status.h"
#include "acc_speech.h"
#include "acc_pad.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int Append(char *text, size_t size, const char *format, ...)
{
    size_t used = strlen(text);
    int written;
    va_list args;
    if (used >= size) return 0;
    va_start(args, format);
    written = vsnprintf(text + used, size - used, format, args);
    va_end(args);
    text[size - 1] = 0;
    return written >= 0 && (size_t)written < size - used;
}

/* GetTextString itself does not reject negative IDs. */
static const char *TextOrFallback(enum TEXTSTRING_ID id, const char *fallback)
{
    const char *text;
    if (!((id >= 0 && id < MAX_NO_OF_TEXTSTRINGS) ||
          (id > MIN_NEW_TEXTSTRINGS && id < MAX_NEW_TEXTSTRINGS))) return fallback;
    text = GetTextString(id);
    return text && text[0] ? text : fallback;
}

static const NPC_DATA *MarineStartingStats(void)
{
    switch (AvP.Difficulty) {
        case I_Easy: return GetThisNpcData(I_PC_Marine_Easy);
        case I_Hard: return GetThisNpcData(I_PC_Marine_Hard);
        case I_Impossible: return GetThisNpcData(I_PC_Marine_Impossible);
        default: return GetThisNpcData(I_PC_Marine_Medium);
    }
}

static int AppendPercent(char *text, size_t size, const char *label, int value, int base)
{
    int percent;
    int64_t scaled;
    if (base <= 0) return Append(text, size, "%s unavailable. ", label);
    if (value < 0) value = 0;
    /* Match the HUD: round up, but show at most 99 when below full health or
       armour. Widen before multiplying so large health values cannot overflow. */
    scaled = (int64_t)value * 100 / base;
    percent = (int)((scaled + 65535) / 65536);
    if ((int64_t)value < (int64_t)base * 65536 && percent > 99) percent = 99;
    return Append(text, size, "%s %d percent. ", label, percent);
}

static unsigned int Rounds(unsigned int raw)
{
    /* Unlike adding 65535 first, this also works at UINT_MAX. */
    return (raw >> 16) + ((raw & 65535) != 0);
}

int AccStatus_FormatMarine(const struct player_status *player, char *text, size_t size)
{
    const NPC_DATA *npc;
    const PLAYER_WEAPON_DATA *weapon;
    const TEMPLATE_WEAPON_DATA *template;
    const char *name;
    int slot, id;
    if (!text || !size) return 0;
    text[0] = 0;
    if (!player || AvP.PlayerType != I_Marine || !player->IsAlive || player->DemoMode) return 0;

    npc = MarineStartingStats();
    if (!AppendPercent(text, size, "Health", player->Health, npc ? npc->StartingStats.Health : 0) ||
        !AppendPercent(text, size, "Armor", player->Armour, npc ? npc->StartingStats.Armour : 0)) return 0;

    slot = (int)player->SelectedWeaponSlot;
    if (slot < 0 || slot >= MAX_NO_OF_WEAPON_SLOTS)
        return Append(text, size, "Weapon status unavailable.");
    weapon = &player->WeaponSlot[slot];
    id = (int)weapon->WeaponIDNumber;
    if (weapon->Possessed != 1 || id < 0 || id >= MAX_NO_OF_WEAPON_TEMPLATES)
        return Append(text, size, "Weapon status unavailable.");
    template = &TemplateWeapon[id];
    /* The original Cudgel template mistakenly calls it a Pulse rifle. */
    name = id == WEAPON_CUDGEL ? "Cudgel" : TextOrFallback(template->Name, "Weapon");
    if (!Append(text, size, "%.128s. ", name)) return 0;

    if (template->PrimaryIsMeleeWeapon || template->PrimaryAmmoID == AMMO_NONE)
        return Append(text, size, "No ammunition required.");
    if (id == WEAPON_FLAMETHROWER)
        return Append(text, size, "Fuel: %u units, %u spare tanks.",
                      Rounds(weapon->PrimaryRoundsRemaining), (unsigned int)weapon->PrimaryMagazinesRemaining);
    if (id == WEAPON_TWO_PISTOLS)
        return Append(text, size,
                      "Right pistol: %u rounds loaded, %u spare magazines. "
                      "Left pistol: %u rounds loaded, %u spare magazines.",
                      Rounds(weapon->PrimaryRoundsRemaining), (unsigned int)weapon->PrimaryMagazinesRemaining,
                      Rounds(weapon->SecondaryRoundsRemaining), (unsigned int)weapon->SecondaryMagazinesRemaining);
    if (id == WEAPON_GRENADELAUNCHER) {
        int ammo = (int)GrenadeLauncherData.SelectedAmmo;
        const char *label = "Selected grenades";
        if (ammo >= 0 && ammo < MAX_NO_OF_AMMO_TEMPLATES)
            label = TextOrFallback(TemplateAmmo[ammo].ShortName, label);
        /* Stored per-type grenade counts may lag; the equipped weapon has the
           current loaded rounds and spare magazines, just as in the HUD. */
        return Append(text, size, "%.128s: %u loaded, %u spare magazines.", label,
                      Rounds(weapon->PrimaryRoundsRemaining), (unsigned int)weapon->PrimaryMagazinesRemaining);
    }
    if (!Append(text, size, "%u rounds loaded, %u spare magazines.",
                Rounds(weapon->PrimaryRoundsRemaining), (unsigned int)weapon->PrimaryMagazinesRemaining)) return 0;
    if (id == WEAPON_PULSERIFLE)
        return Append(text, size, " Grenades: %u loaded, %u spare magazines.",
                      Rounds(weapon->SecondaryRoundsRemaining), (unsigned int)weapon->SecondaryMagazinesRemaining);
    return 1;
}

void AccStatus_AnnounceMarine(const struct player_status *player)
{
    char text[768];
    if (!AccStatus_FormatMarine(player, text, sizeof(text))) return;
    if (AccPadTrace) {
        fprintf(stderr, "ACCSTATUS: speech=%d %s\n", AccSpeech_IsAvailable(), text);
        fflush(stderr);
    }
    /* An explicit request should be immediate and can repeat identical status. */
    AccSpeech_Say(text, 1);
}
