/* Included by test_gameplay_input.c after the real usr_io.c implementation. */
#define PREDATOR_PRESET_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL: Predator preset: %s (line %d)\n", #condition, __LINE__); \
    return 1; } } while (0)

static const PLAYER_INPUT_CONFIGURATION legacy_predator_secondary = {
    KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID, KEY_VOID,
    KEY_NUMPAD8, KEY_NUMPAD2, KEY_NUMPAD5, KEY_VOID, KEY_VOID, KEY_MMOUSE,
    KEY_CR, KEY_NUMPAD0, KEY_NUMPADDEL, {KEY_VOID}, {KEY_VOID}, {KEY_VOID},
    {KEY_VOID}, {KEY_VOID}, {KEY_MOUSEWHEELUP}, {KEY_MOUSEWHEELDOWN},
    {KEY_VOID}, {KEY_VOID}, {KEY_VOID}, {KEY_VOID}, KEY_VOID, KEY_VOID,
    KEY_VOID, KEY_VOID
};

static int RunPredatorPresetTests(void)
{
    PLAYER_INPUT_CONFIGURATION primary=DefaultPredatorInputPrimaryConfig;
    PLAYER_INPUT_CONFIGURATION secondary=legacy_predator_secondary, before, profile_primary;
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.Jump==KEY_JOYSTICK_BUTTON_1);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.Operate==KEY_JOYSTICK_BUTTON_3);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.FirePrimaryWeapon==KEY_JOYSTICK_BUTTON_8);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.FireSecondaryWeapon==KEY_JOYSTICK_BUTTON_7);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.a.NextWeapon==KEY_JOYSTICK_BUTTON_4);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.b.PreviousWeapon==KEY_JOYSTICK_BUTTON_5);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.d.Cloak==KEY_JOYSTICK_BUTTON_13);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.e.CycleVisionMode==KEY_JOYSTICK_BUTTON_6);
    PREDATOR_PRESET_CHECK(DefaultPredatorInputSecondaryConfig.k.Predator_MessageHistory==KEY_JOYSTICK_BUTTON_12);
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&DefaultPredatorInputSecondaryConfig,NUMBER_OF_PREDATOR_INPUTS));
    PREDATOR_PRESET_CHECK(secondary.ExpansionSpace7==KEY_VOID && secondary.ExpansionSpace8==0);
    before=secondary;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==0);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&before,sizeof(secondary)));

    /* Profiles that already received the previous exact preset can gain R3
       history, while any other slot change makes the migration ineligible. */
    secondary=DefaultPredatorInputSecondaryConfig;
    secondary.k.Predator_MessageHistory=KEY_VOID;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&DefaultPredatorInputSecondaryConfig,sizeof(secondary)));
    secondary=DefaultPredatorInputSecondaryConfig;
    secondary.k.Predator_MessageHistory=KEY_VOID;
    secondary.h.GrapplingHook=KEY_G;
    before=secondary;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==0);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&before,sizeof(secondary)));

    secondary=legacy_predator_secondary;
    before=primary; primary.Operate=KEY_E;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(primary.Operate==KEY_E);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&DefaultPredatorInputSecondaryConfig,NUMBER_OF_PREDATOR_INPUTS));
    primary=before;

    /* SkyPulse.prf shape: keyboard-only customized primary and exact legacy
       active secondary slots, with zeroed expansion bytes. */
    profile_primary=DefaultPredatorInputPrimaryConfig;
    profile_primary.Forward=KEY_UP;
    secondary=legacy_predator_secondary;
    secondary.ExpansionSpace7=0; secondary.ExpansionSpace8=0;
    before=profile_primary;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&profile_primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(!memcmp(&profile_primary,&before,sizeof(before)));
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&DefaultPredatorInputSecondaryConfig,NUMBER_OF_PREDATOR_INPUTS));
    PREDATOR_PRESET_CHECK(secondary.ExpansionSpace7==0 && secondary.ExpansionSpace8==0);

    /* Existing controller preset migration may also preserve keyboard-only
       primary assignments while filling the one missing R3 action. */
    secondary=DefaultPredatorInputSecondaryConfig;
    secondary.k.Predator_MessageHistory=KEY_VOID;
    secondary.ExpansionSpace7=0; secondary.ExpansionSpace8=0;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&profile_primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(secondary.k.Predator_MessageHistory==KEY_JOYSTICK_BUTTON_12 &&
                          secondary.ExpansionSpace7==0 && secondary.ExpansionSpace8==0);

    /* A primary gamepad binding or any changed active secondary slot makes
       this migration unsafe and must leave the profile alone. */
    secondary=legacy_predator_secondary;
    profile_primary.Jump=KEY_JOYSTICK_BUTTON_1;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&profile_primary,&secondary)==0);
    profile_primary=before;
    secondary=legacy_predator_secondary;
    secondary.Operate=KEY_JOYSTICK_BUTTON_3;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&profile_primary,&secondary)==0);

    secondary=legacy_predator_secondary;
    before=primary; primary.Operate=KEY_E;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==1);
    primary=before;

    secondary=legacy_predator_secondary;
    before=secondary;
    secondary.h.GrapplingHook=KEY_JOYSTICK_BUTTON_12;
    before=secondary;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==0);
    PREDATOR_PRESET_CHECK(!memcmp(&secondary,&before,sizeof(secondary)));

    secondary=legacy_predator_secondary;
    before=secondary;
    /* Actual reserved-byte differences are ignored, but preserved. */
    secondary.ExpansionSpace7=0; secondary.ExpansionSpace8=0;
    before=secondary;
    PREDATOR_PRESET_CHECK(AccPad_UpgradeLegacyPredatorBindings(&primary,&secondary)==1);
    PREDATOR_PRESET_CHECK(secondary.ExpansionSpace7==0 && secondary.ExpansionSpace8==0);
    printf("PASS: Predator pad defaults, legacy profile migration, custom primary/secondary preservation\n");
    return 0;
}

#undef PREDATOR_PRESET_CHECK
