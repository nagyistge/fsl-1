
proc FmribWebHelp { prefix file } {

    global OSFLAVOUR

    regsub -all "//" $file "/" file

    if { $OSFLAVOUR == "macos" } {

        catch { exec sh -c "open $file" & }

    } elseif { $OSFLAVOUR == "cygwin" } {

	set url [ exec sh -c "cygpath -w $file" ]
	eval exec [auto_execok start] {"${prefix}//$url"}

    } else {

## There is no need for this trial-and-error search on a Debian system
## -> there is 'sensible-browser'

#	foreach executable {mozilla netscape iexplorer opera lynx w3m links galeon konquerer mosaic firefox amaya browsex elinks} {
#	    set executable [auto_execok $executable]
#	    if [string length $executable] {
#		catch { exec sh -c "$executable -remote \"openURL(${prefix}//${file},new-window)\" || $executable ${prefix}//${file} " 2> /dev/null & }
#		break
#	    }
#	}
	catch { exec sensible-browser ${prefix}//${file} 2> /dev/null & }
    }
}

