# One-shot guidance facing tests

Run `run_tests.bat` to compile the real `acc_snap.c` with engine boundaries
mocked. Checks cover cardinal yaw, pitch sign and Marine limits, level navigation
view, synchronized orientation state, removal of old turning inertia, missing
targets, gameplay gates, and preservation of position/movement/fire requests.

The gameplay fixture separately verifies View+R3 consumption, ordinary R3
history, preserved L3 binding, held-button behavior and custom binding conflicts.
Route and combat fixtures check target selection and live identity/LOS validity.
These are not screen-reader or physical controller listening tests.
