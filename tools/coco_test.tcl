# Secuencia: deja arrancar C-BIOS, luego mantiene DERECHA y captura el avance/scroll
after time 5.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/s0.png" } }
after time 5.2 "keymatrixdown 8 0x80"
after time 6.5 { catch { screenshot "C:/Users/alber/msx_coco/tools/s1.png" } }
after time 8.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/s2.png" } }
after time 10.0 { catch { screenshot "C:/Users/alber/msx_coco/tools/s3.png" } }
after time 10.1 "keymatrixup 8 0x80"
after time 10.4 "exit"
