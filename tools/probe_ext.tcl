# Probe what extensions openMSX knows about, dump to a text file then exit.
after time 0.5 {
    set f [open "C:/Users/alber/msx_coco/tools/shots/extensions.txt" w]
    catch {
        foreach e [openmsx_info extensions] { puts $f $e }
    } err
    if {$err ne ""} { puts $f "ERR: $err" }
    close $f
    after time 0.2 exit
}
