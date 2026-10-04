# RUN 1 - no FM (PSG fallback) + debug + eye + gameplay
# Uses EMULATED time so screenshots land at deterministic points.
set sh "C:/Users/alber/msx_coco/tools/shots"

after time 4.5 "screenshot $sh/dbg_nofm.png"
after time 5.0 "keymatrixdown 8 0x01"
after time 5.2 "keymatrixup 8 0x01"
after time 6.0 "keymatrixdown 8 0x80"
after time 8.0 "screenshot -raw $sh/pac_right_raw.png"
after time 8.2 "keymatrixup 8 0x80"
after time 8.4 "keymatrixdown 8 0x40"
after time 10.0 "screenshot -raw $sh/pac_down_raw.png"
after time 10.2 "keymatrixup 8 0x40"
after time 10.5 "screenshot $sh/game_nofm.png"
after time 11.0 "exit"
