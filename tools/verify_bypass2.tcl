# Retry the [C] bypass using openMSX 'type' (handles keymatrix automatically),
# then capture the debug screen to read the FM AUDIO line on a no-internal-FM box.
set sh "C:/Users/alber/msx_coco/tools/shots"
set tag $::env(COCO_TAG)
after time 9.0 "type C"
after time 11.0 "screenshot $sh/bp2_${tag}_a.png"
after time 13.0 "screenshot $sh/bp2_${tag}_b.png"
after time 13.4 "exit"
