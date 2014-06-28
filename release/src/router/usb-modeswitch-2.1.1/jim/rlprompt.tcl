# Readline-based interactive shell for Jim
# Copyright(C) 2005 Salvatore Sanfilippo <antirez@invece.org>
#
# In order to automatically have readline-editing features
# put this in your $HOME/.jimrc
#
# if {$jim_interactive} {
#    if {[catch {package require rlprompt}] == 0} {
#       rlprompt.shell
#    }
# }
package require readline

proc rlprompt.shell {} {
    puts "Readline shell loaded"
    puts "Welcome to Jim [info version]!"
    set prompt ". "
    set buf ""
    while 1 {
        set line [readline.readline $prompt]

        if {[string length $line] == 0} {
            continue
        }
        if {$buf eq ""} {
            set buf $line
        } else {
            append buf \n $line
        }
        if {![info complete $buf]} {
            set prompt "> "
            continue
        }
        readline.addhistory $buf

        catch {
            uplevel #0 $buf
        } error
        if {$error ne ""} {
            puts $error
        }
        set buf ""
        set prompt ". "
    }
}
