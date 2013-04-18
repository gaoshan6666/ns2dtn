# Get the simulation parameters (-param1 value1 -param2 value2...)
# -Name opt
for {set i 0} {$i < $argc} {incr i} {
    global opt
    #lindex get argv's no.i
    set arg [lindex $argv $i]
    if {[string range $arg 0 0] != "-"} continue
    set name [string range $arg 1 end]
    #opt array, use name as index.
    set opt($name) [lindex $argv [expr $i+1]]
}

global defaultRNG
$defaultRNG seed $opt(seed)

# Create a simulator object
set ns_ [new Simulator]

# Schedule events
# Parameters: destination, bundle size, lifetime [s], custody transfer: 0/1, return receipt: 0/1,
#             priority (not yet in use): bulk=0, normal=1, expedited=2
for { set i 0} {$i < $opt(nn)} {incr i} {
    for { set j 0} {$j < 25} {incr j} {
    #任取一个节点为dest
	set dest [expr [ns-random] % $opt(nn)]
	if { $dest == $i } { 
	    if { $dest > 0 } {
		set dest [expr $dest -1]
	    } else {
		set dest [expr $dest + 1]
	    }
	}
	set send_time  [expr 10.0 + $j*200.0 + double([ns-random] % 19999)/100.0]
	puts "\$ns_ at $send_time \"\$bundle_($i) send $dest $opt(bundlesize) $opt(lifetime) 0 1 1\""
    }
}

# Run the simulation
$ns_ run