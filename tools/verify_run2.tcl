# RUN 2 - FM present (FM-PAC detection) + no-crash check
set sh "C:/Users/alber/msx_coco/tools/shots"

after time 4.5 "screenshot $sh/dbg_fm.png"
after time 5.0 "keymatrixdown 8 0x01"
after time 5.2 "keymatrixup 8 0x01"
after time 7.0 "screenshot $sh/game_fm.png"
after time 7.5 "exit"
