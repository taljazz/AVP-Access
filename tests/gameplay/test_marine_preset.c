/* Included by test_gameplay_input.c after the actual usr_io.c implementation.
   The fixture is the unmodified legacy Marine pair from a saved profile.
   Keep these checks active even though the engine harness defines NDEBUG. */
#define PRESET_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL: Marine preset: %s (line %d)\n", #condition, __LINE__); \
    return 1; } } while (0)

static const unsigned char legacy_pair[64] = {33,29,66,68,56,11,14,27,36,17,53,58,54,52,119,121,79,78,49,84,51,81,80,91,101,102,42,0,0,0,0,0,255,255,255,255,255,255,255,70,64,67,255,255,120,41,62,74,123,124,255,255,255,255,255,255,255,255,255,0,0,0,0,0};
static void load_legacy(PLAYER_INPUT_CONFIGURATION *primary,
                        PLAYER_INPUT_CONFIGURATION *secondary)
{
    memcpy(primary, legacy_pair, 32);
    memcpy(secondary, legacy_pair + 32, 32);
}

int RunMarinePresetTests(void)
{
    PLAYER_INPUT_CONFIGURATION primary, secondary, before_primary, before_secondary;
    PLAYER_INPUT_CONFIGURATION original_default = DefaultMarineInputPrimaryConfig;
    unsigned char saved[64];
    unsigned int offset, value, custom_checks = 0;
    PRESET_CHECK(sizeof(PLAYER_INPUT_CONFIGURATION) == 32);
    PRESET_CHECK(memcmp(&DefaultMarineInputPrimaryConfig, legacy_pair, 32) == 0);

    PRESET_CHECK(DefaultMarineInputSecondaryConfig.Jump == KEY_JOYSTICK_BUTTON_1);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.Crouch == KEY_JOYSTICK_BUTTON_2);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.Operate == KEY_JOYSTICK_BUTTON_3);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.a.NextWeapon == KEY_JOYSTICK_BUTTON_4);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.b.PreviousWeapon == KEY_JOYSTICK_BUTTON_5);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.e.ThrowFlare == KEY_JOYSTICK_BUTTON_6);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.FireSecondaryWeapon == KEY_JOYSTICK_BUTTON_7);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.FirePrimaryWeapon == KEY_JOYSTICK_BUTTON_8);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.Walk == KEY_JOYSTICK_BUTTON_11);
    PRESET_CHECK(DefaultMarineInputSecondaryConfig.d.ImageIntensifier == KEY_JOYSTICK_BUTTON_13);

    load_legacy(&primary, &secondary);
    before_primary = primary;
    PRESET_CHECK(AccPad_UpgradeLegacyMarineBindings(&primary, &secondary) == 1);
    PRESET_CHECK(memcmp(&primary, &before_primary, 32) == 0);
    PRESET_CHECK(memcmp(&secondary, &DefaultMarineInputSecondaryConfig, 32) == 0);
    memcpy(saved, &primary, 32);
    memcpy(saved + 32, &secondary, 32);
    memset(&primary, 0, sizeof(primary));
    memset(&secondary, 0, sizeof(secondary));
    memcpy(&primary, saved, 32);
    memcpy(&secondary, saved + 32, 32);
    PRESET_CHECK(AccPad_UpgradeLegacyMarineBindings(&primary, &secondary) == 0);
    PRESET_CHECK(memcmp(&secondary, saved + 32, 32) == 0);

    /* Every possible one-byte customization, including unused fields, survives. */
    for (offset = 0; offset < 64; ++offset) {
        for (value = 0; value < 256; ++value) {
            unsigned char *changed;
            if (value == legacy_pair[offset]) continue;
            load_legacy(&primary, &secondary);
            changed = offset < 32 ? (unsigned char *)&primary : (unsigned char *)&secondary;
            changed[offset % 32] = (unsigned char)value;
            before_primary = primary;
            before_secondary = secondary;
            PRESET_CHECK(AccPad_UpgradeLegacyMarineBindings(&primary, &secondary) == 0);
            PRESET_CHECK(memcmp(&primary, &before_primary, 32) == 0);
            PRESET_CHECK(memcmp(&secondary, &before_secondary, 32) == 0);
            ++custom_checks;
        }
    }

    /* A configured primary default stays intact while its stock secondary upgrades. */
    DefaultMarineInputPrimaryConfig.Operate = KEY_E;
    primary = DefaultMarineInputPrimaryConfig;
    memcpy(&secondary, legacy_pair + 32, 32);
    PRESET_CHECK(AccPad_UpgradeLegacyMarineBindings(&primary, &secondary) == 1);
    PRESET_CHECK(primary.Operate == KEY_E);
    DefaultMarineInputPrimaryConfig = original_default;
    printf("PASS: preset, unchanged primary, legacy upgrade, save/reload idempotence, %u custom-byte preservation cases, configured default\n", custom_checks);
    return 0;
}

#undef PRESET_CHECK
