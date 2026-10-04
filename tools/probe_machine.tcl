# Probe a machine's sound devices + MSX type, write to file, exit. No cart needed.
set sh "C:/Users/alber/msx_coco/tools/shots"
after time 3.0 {
    set f [open "$sh/probe.txt" a]
    puts $f "MACHINE=[machine_info config_name]  TYPE=[catch {machine_info type}; machine_info type]"
    catch {puts $f "  SOUND=[machine_info sounddevice]"} e
    close $f
    after time 0.2 "exit"
}
