# Menu accessibility fixture

`run_tests.bat` compiles and runs the actual `src/access/acc_menu.c` against
mocked speech, localized strings, profiles, and the briefing accessor. The
briefing cases verify that `acc_menu.c` appends lines supplied by
`AccMenu_BriefingLine`; the fixture mocks that accessor based on the requested
menu ID. They do **not** execute or verify the production menu gate inside
`avp_menus.c`. Live bridge testing separately confirmed the basic briefing
gate and full briefing announcement.
