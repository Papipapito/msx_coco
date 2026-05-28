# Verifica las 4 orientaciones de la boca y el movimiento
after time 5.0  { catch { screenshot "C:/Users/alber/msx_coco/tools/d_idle.png" } }
after time 5.2  "keymatrixdown 8 0x40"
after time 6.3  { catch { screenshot "C:/Users/alber/msx_coco/tools/d_down.png" } }
after time 6.4  "keymatrixup 8 0x40"
after time 6.6  "keymatrixdown 8 0x80"
after time 7.9  { catch { screenshot "C:/Users/alber/msx_coco/tools/d_right.png" } }
after time 8.0  "keymatrixup 8 0x80"
after time 8.2  "keymatrixdown 8 0x10"
after time 9.5  { catch { screenshot "C:/Users/alber/msx_coco/tools/d_left.png" } }
after time 9.6  "keymatrixup 8 0x10"
after time 9.8  "keymatrixdown 8 0x20"
after time 11.1 { catch { screenshot "C:/Users/alber/msx_coco/tools/d_up.png" } }
after time 11.2 "keymatrixup 8 0x20"
after time 11.5 "exit"
