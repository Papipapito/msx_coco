# On non-MSX2+ the game shows "Press [C] to continue anyway". Press C (key matrix
# row 2 bit 3 = 'C'), then capture the debug screen that follows to read FM line.
set sh "C:/Users/alber/msx_coco/tools/shots"
set tag $::env(COCO_TAG)
# wait for the error prompt, then press C
after time 9.0 "keymatrixdown 2 0x08"
after time 9.2 "keymatrixup 2 0x08"
after time 10.5 "screenshot $sh/bypass_${tag}_a.png"
after time 12.0 "screenshot $sh/bypass_${tag}_b.png"
after time 12.4 "exit"
