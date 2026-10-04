# Time-series capture to see what the game shows over time on this machine.
set sh "C:/Users/alber/msx_coco/tools/shots"
set tag $::env(COCO_TAG)
after time 2.0 "screenshot $sh/ts_${tag}_2.png"
after time 3.5 "screenshot $sh/ts_${tag}_35.png"
after time 5.0 "screenshot $sh/ts_${tag}_5.png"
after time 7.0 "screenshot $sh/ts_${tag}_7.png"
after time 9.0 "screenshot $sh/ts_${tag}_9.png"
after time 9.4 "exit"
