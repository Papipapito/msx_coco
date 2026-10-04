# Verifica el fix del scroll suave (R#25 MAK + reorder UpdateScroll).
# C-BIOS ~4s para bootear el cartucho. Pulsamos SPACE para saltar la pantalla
# de debug, mantenemos RIGHT y capturamos durante el scroll. La columna izq
# tiene que verse SOLIDA NEGRA en todos los frames (mask activa).
after time 5.0 "keymatrixdown 8 0x01"
after time 5.2 "keymatrixup 8 0x01"
after time 5.5 "keymatrixdown 8 0x80"
after time 7.0 { catch { screenshot     "C:/Users/alber/msx_coco/tools/shots/scroll_mask_a.png" } }
after time 7.3 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/shots/scroll_mask_a_raw.png" } }
after time 7.6 { catch { screenshot     "C:/Users/alber/msx_coco/tools/shots/scroll_mask_b.png" } }
after time 7.9 { catch { screenshot -raw "C:/Users/alber/msx_coco/tools/shots/scroll_mask_b_raw.png" } }
after time 8.3 "keymatrixup 8 0x80"
after time 8.8 "exit"
