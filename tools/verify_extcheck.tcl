# Confirm whether the fmpac extension is actually attached, and probe the
# YM2413 (MSX-Music) sound device presence, then screenshot the debug screen.
set sh "C:/Users/alber/msx_coco/tools/shots"
after time 4.5 {
    set f [open "$sh/extinfo.txt" w]
    puts $f "=== machine_info extension (slots) ==="
    catch {puts $f [machine_info extension]} e1
    puts $f "=== list_extensions style: 'set' command sound devices ==="
    catch {puts $f [machine_info sounddevice]} e2
    puts $f "=== debuggables (look for MSX-Music / YM2413) ==="
    catch {puts $f [debug list]} e3
    close $f
    screenshot "$sh/dbg_extcheck.png"
    after time 0.3 "exit"
}
