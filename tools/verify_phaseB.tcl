# Phase B verification: debug screen must show UNAPI: NO CART under C-BIOS
# (no UNAPI extension), then SPACE must enter the game with no regression.
# - Capture debug at t=5.0s (C-BIOS boot + ROMDelayBoot tax)
# - SPACE at t=5.5s
# - Capture game at t=8.0s (gameplay running)
after time 5.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/shots/phaseB_debug.png" } }
after time 5.5 "keymatrixdown 8 0x01"
after time 5.7 "keymatrixup 8 0x01"
after time 8.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/shots/phaseB_game.png" } }
after time 8.5 "exit"
