# Capturas RAW (resolucion nativa) del comecocos moviendose a la derecha
after time 5.0 "keymatrixdown 8 0x80"
after time 5.30 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r0.png" } }
after time 5.45 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r1.png" } }
after time 5.60 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r2.png" } }
after time 5.75 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r3.png" } }
after time 5.90 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r4.png" } }
after time 6.05 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/r5.png" } }
after time 6.2 "keymatrixup 8 0x80"
after time 6.5 "exit"
