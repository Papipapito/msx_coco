# Just capture the SCREEN 0 debug screen (works on any MSX type) then exit.
set sh "C:/Users/alber/msx_coco/tools/shots"
set out $::env(COCO_SHOT)
after time 5.0 "screenshot $sh/$out"
after time 5.4 "exit"
