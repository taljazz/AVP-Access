# Loot guidance fixture

Run `run_tests.bat` from a Visual Studio developer command prompt. It compiles
the real `src/access/acc_loot.c` against engine boundary stubs; no game or
retail data is needed. The fixture checks usefulness filtering, selection
lifecycle, collection confirmation, and proximity/LOS behavior.

Covered controls in the shipped feature are **L** or Xbox **View/Back + D-pad
Left** to browse useful Marine supplies, and **K** or **View/Back + D-pad Down**
to guide to the selected supply or cancel that supply guidance. The production
code is single-player Marine-scoped: needed medkits are offered before armor,
ammunition and weapons; touching a medkit or armor pickup uses it automatically;
ammunition is inventory; weapons still use the normal **Y/LB** selection and
**RT** fire controls. Custom gameplay bindings win over these shortcuts.
