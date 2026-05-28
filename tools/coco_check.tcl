# Verifica scroll hardware: arranque, y comecocos avanzando (fondo scrollea, sprite alineado)
after time 5.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/c0_init.png" } }
after time 5.2 "keymatrixdown 8 0x80"
after time 6.8 { catch { screenshot "C:/Users/alber/msx_coco/tools/c1_scroll.png" } }
after time 9.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/c2_scroll.png" } }
after time 9.1 "keymatrixup 8 0x80"
after time 9.4 "exit"
