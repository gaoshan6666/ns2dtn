# Get the simulation parameters (-param1 value1 -param2 value2...)
for {set i 0} {$i < $argc} {incr i} {
    global opt
    set arg [lindex $argv $i]
    if {[string range $arg 0 0] != "-"} continue
    set name [string range $arg 1 end]
    set opt($name) [lindex $argv [expr $i+1]]
}

#modify the path depend on reality
load ../ns-allinone-2.35/dei80211mr-1.1.4/src/.libs/libdei80211mr.so

Node/MobileNode instproc getIfq { param0} {
    $self instvar ifq_    
    return $ifq_($param0) 
}

Node/MobileNode instproc getPhy { param0} {
    $self instvar netif_    
    return $netif_($param0) 
}

set per [new PER]
$per loadPERTable80211gTrivellato
# Pn = kTB. k is Boltzmann's constant, 1.38*10^-23 J/K, T is the temperature in Kelvin (room temperature, 290 K), and B is the system bandwidth [Hz].
$per set noise_ 9.75e-12 ; # -80 dBm

# HP iPAQ 500 Series Voice Messenger
# Power consumption: transmit mode: 1.33 W, receive mode: 1.235 W
Phy/WirelessPhy set CSThresh_ 1e-10 ; # -70 dBm
Phy/WirelessPhy set Pt_ 0.0178      ; # 12.5 dBm
Phy/WirelessPhy set freq_ 2437e6
Phy/WirelessPhy set L_ 1.0

LL set disable_ARP 1

Mac/802_11/Multirate set useShortPreamble_ true
Mac/802_11/Multirate set gSyncInterval_ 0.00001
Mac/802_11/Multirate set CWMin_         16
Mac/802_11/Multirate set CWMax_         1024
Mac/802_11/Multirate set VerboseCounters_ 1
Mac/802_11/Multirate set RTSThreshold_ 0
Mac/802_11/Multirate set ShortRetryLimit_ 8
Mac/802_11/Multirate set LongRetryLimit_  5
Mac/802_11/Multirate set SlotTime_ 0.000009
Mac/802_11/Multirate set SIFS_ 0.000016

Mac/802_11 set useShortPreamble_ true
Mac/802_11 set gSyncInterval_ 0.00001
Mac/802_11 set CWMin_         16
Mac/802_11 set CWMax_         1024
Mac/802_11 set VerboseCounters_ 1
Mac/802_11 set RTSThreshold_ 0
Mac/802_11 set ShortRetryLimit_ 8
Mac/802_11 set LongRetryLimit_  5
Mac/802_11 set SlotTime_ 0.000009
Mac/802_11 set SIFS_ 0.000016

PeerStatsDB set VerboseCounters_ 1

Agent/Bundle set helloInterval_ 100  ; # [ms]
Agent/Bundle set retxTimeout_ 1000.0 ; # [s]
Agent/Bundle set deleteForwarded_ 0  ; # Do not delete forwarded bundles when epidemic routing is used
Agent/Bundle set cqsize_ 0
Agent/Bundle set retxqsize_ 0
Agent/Bundle set qsize_ 0
Agent/Bundle set ifqCongestionDuration_ 0
Agent/Bundle set sentBundles_ 0
Agent/Bundle set receivedBundles_ 0
Agent/Bundle set duplicateReceivedBundles_ 0
Agent/Bundle set duplicateBundles_ 0
Agent/Bundle set deletedBundles_ 0
Agent/Bundle set forwardedBundles_ 0
Agent/Bundle set sentReceipts_ 0
Agent/Bundle set receivedReceipts_ 0
Agent/Bundle set duplicateReceivedReceipts_ 0
Agent/Bundle set duplicateReceipts_ 0
Agent/Bundle set forwardedReceipts_ 0
Agent/Bundle set avBundleDelay_ 0
Agent/Bundle set avReceiptDelay_ 0
Agent/Bundle set avBundleHopCount_ 0
Agent/Bundle set avReceiptHopCount_ 0
Agent/Bundle set avLinkLifetime_ 0
Agent/Bundle set CVRR_ 1000.0
Agent/Bundle set limitRR_ 1.0
Agent/Bundle set avNumNeighbors_ 0
Agent/Bundle set antiPacket_ 1
Agent/Bundle set routingProtocol_ 1
Agent/Bundle set initialSpray_ 16
Agent/Bundle set dropStrategy_ 0            ; # Drop tail
Agent/Bundle set bundleStorageSize_ 100000000 ; # Bytes
Agent/Bundle set congestionControl_ 0
Agent/Bundle set bundleStorageThreshold_ 0.8

#####

# ======================================================================
# Define options
# ======================================================================
set val(chan)           Channel/WirelessChannel/PowerAware
set val(prop)           Propagation/FreeSpace/PowerAware
set val(netif)          Phy/WirelessPhy/PowerAware
set val(mac)            Mac/802_11/Multirate
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         100000                     ;# max packet in ifq
set val(nn)             $opt(nn)                   ;# number of mobilenodes
set val(rp)             DumbAgent                  ;# routing protocol
set val(energyModel)    EnergyModel     

set peerstats [new PeerStatsDB/Static]
$peerstats numpeers $val(nn)

global defaultRNG
$defaultRNG seed $opt(seed)

# Create a simulator object
set ns_ [new Simulator]

# Open a trace file
set tracefd [open /dev/null w]
$ns_ trace-all $tracefd

set qtrace [open qtrace.tr w]

# Define a 'finish' procedure
proc finish {} {
    global ns_ tracefd qtrace
    $ns_ flush-trace
    close $tracefd
    close $qtrace
    exit 0
}

# Set up topography object
set topo [new Topography]
$topo load_flatgrid 2000.0 2000.0 ; # NOTE: Depends on the mobility trace file!

# Create God
set god_ [create-god $val(nn)]

# Node configuration
set chan [new $val(chan)]
$ns_ node-config -adhocRouting $val(rp) \
    -llType $val(ll) \
    -macType $val(mac) \
    -ifqType $val(ifq) \
    -ifqLen $val(ifqlen) \
    -antType $val(ant) \
    -propType $val(prop) \
    -phyType $val(netif) \
    -channel $chan \
    -topoInstance $topo \
    -energyModel $val(energyModel) \
    -idlePower 0.83 \
    -rxPower 1.0 \
    -txPower 1.4 \
    -sleepPower 0.001 \
    -transitionPower 0.2 \
    -transitionTime 0.005 \
    -initialEnergy 100000 \
    -agentTrace ON \
    -routerTrace ON \
    -macTrace ON \
    -phyTrace ON \
    -movementTrace ON

for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node] 

    set mac_($i) [$node_($i) getMac 0]
    set ifq_($i) [$node_($i) getIfq 0]
    set phy_($i) [$node_($i) getPhy 0]
    
    $mac_($i) nodes $val(nn)
    $mac_($i) basicMode_ Mode6Mb
    $mac_($i) dataMode_ Mode54Mb
    $mac_($i) per $per
    $mac_($i) PeerStatsDB $peerstats
    set pp_($i) [new PowerProfile]
    $mac_($i) powerProfile $pp_($i) 
    $phy_($i) powerProfile $pp_($i) 
    
    set ra_($i) [new RateAdapter/SNR]
    $ra_($i) set pktsize_ 1500
    $ra_($i) set maxper_ 0.1778
    $ra_($i) attach2mac $mac_($i)
    $ra_($i) use80211g
    $ra_($i) setmodeatindex 0
    
    $node_($i) random-motion 0
    $node_($i) start
}

if {($opt(nn) == 40)&&($opt(simtime) == 5000)} {
    source scen_n40_pt2_ms20_t5000_x2000_y2000
} else {
    puts "Wrong set of parameters for any mobility tracefile!"
}

proc print-queue-stats {} {
    global ns_ bundle_ val qtrace node_
    for {set i 0} {$i < $val(nn) } {incr i} {
	puts $qtrace "[format "%.1f" [$ns_ now]] [$bundle_($i) set cqsize_] [$bundle_($i) set retxqsize_] [$bundle_($i) set qsize_] [format "%.4f" [$bundle_($i) set ifqCongestionDuration_]] $i [format "%.4f" [$node_($i) energy]]." 
    }
    $ns_ at [expr [$ns_ now] + 1.0] "print-queue-stats"
}

proc print-average-stats {} {
    global ns_ bundle_ val
    set delaysum 0
    set sent 0
    set received 0
    set receipt_delaysum 0
    set sent_receipts 0
    set received_receipts 0
    set bundlehopcountsum 0
    set receipthopcountsum 0
    for {set i 0} {$i < $val(nn) } {incr i} {
	set delaysum [expr $delaysum + [$bundle_($i) set avBundleDelay_]*[$bundle_($i) set receivedBundles_]]
	set sent [expr $sent + [$bundle_($i) set sentBundles_]]
	set received [expr $received + [$bundle_($i) set receivedBundles_]]
	set receipt_delaysum [expr $receipt_delaysum + [$bundle_($i) set avReceiptDelay_]*[$bundle_($i) set receivedReceipts_]]
	set sent_receipts [expr $sent_receipts + [$bundle_($i) set sentReceipts_]]
	set received_receipts [expr $received_receipts + [$bundle_($i) set receivedReceipts_]]
	set bundlehopcountsum [expr $bundlehopcountsum + [$bundle_($i) set avBundleHopCount_]*[$bundle_($i) set receivedBundles_]]
	set receipthopcountsum [expr $receipthopcountsum + [$bundle_($i) set avReceiptHopCount_]*[$bundle_($i) set receivedReceipts_]]
    }
    puts "Bundle end-to-end delay at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $delaysum/$received.0]]."
    puts "Bundle delivery ratio at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $received/$sent.0]] ($received/$sent)."
    puts "Bundle hop count at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $bundlehopcountsum/$received.0]]."
    puts "Receipt end-to-end delay at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $receipt_delaysum/$received_receipts.0]]."
    puts "Receipt delivery ratio at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $received_receipts/$sent_receipts.0]] ($received_receipts/$sent_receipts)."
    puts "Receipt hop count at t = [format "%.6f" [$ns_ now]]: [format "%.6f" [expr $receipthopcountsum/$received_receipts.0]]."
}

Agent/Bundle instproc print-stats {time} {
    $self instvar node_ cqsize_ retxqsize_ qsize_ sentBundles_ receivedBundles_ duplicateReceivedBundles_ duplicateBundles_ deletedBundles_ forwardedBundles_\
	sentReceipts_ receivedReceipts_ duplicateReceivedReceipts_ duplicateReceipts_ forwardedReceipts_ avBundleDelay_ avReceiptDelay_ avBundleHopCount_ avReceiptHopCount_
    puts "Node [$node_ id] stats at t = $time: $cqsize_ $retxqsize_ $qsize_ $sentBundles_ $receivedBundles_ $duplicateReceivedBundles_ $duplicateBundles_ $deletedBundles_ $forwardedBundles_\
	$sentReceipts_ $receivedReceipts_ $duplicateReceivedReceipts_ $duplicateReceipts_ $forwardedReceipts_ [format "%.6f" $avBundleDelay_] [format "%.6f" $avReceiptDelay_]\
        [format "%.6f" $avBundleHopCount_] [format "%.6f" $avReceiptHopCount_]."
}

# Define a 'recv' function for the class 'Agent/Bundle'
Agent/Bundle instproc recv {source from delay hc time} {
    $self instvar node_
    puts "Destination [$node_ id] received bundle, source: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time."
}

Agent/Bundle instproc retx_recv {nretx source from delay hc time} {
    $self instvar node_ retxTimeout_
    puts "Destination [$node_ id] received retxbundle, source: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time."
}

Agent/Bundle instproc mid_recv {source from delay hc time} {
    $self instvar node_
    #puts "Node [$node_ id] received bundle, source: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time."
}

Agent/Bundle instproc duplicate_recv {source from delay hc time} {
    $self instvar node_
    #puts "Destination [$node_ id] received duplicate bundle, source: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time. New bundle deleted."
}

Agent/Bundle instproc duplicate_mid_recv {source from delay hc time} {
    $self instvar node_
    #puts "Node [$node_ id] received duplicate bundle, source: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time. New bundle deleted."
}

Agent/Bundle instproc ret_recv {source from delay hc time} {
    $self instvar node_
    puts "Source [$node_ id] received receipt, destination: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time."
}

Agent/Bundle instproc ret_mid_recv {source from delay hc time} {
    $self instvar node_
    #puts "Node [$node_ id] received receipt, destination: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time."
}

Agent/Bundle instproc ret_duplicate_recv {source from delay hc time} {
    $self instvar node_
    #puts "Source [$node_ id] received duplicate receipt, destination: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time. New receipt deleted."
}

Agent/Bundle instproc ret_duplicate_mid_recv {source from delay hc time} {
    $self instvar node_
    #puts "Node [$node_ id] received duplicate receipt, destination: $source, from: $from, delay: $delay ms, hop count: $hc, at t = $time. New receipt deleted."
}

Agent/Bundle instproc hello_recv {from time} {
    $self instvar node_
    #puts "Node [$node_ id] received hello from node $from at t = $time."
}

Agent/Bundle instproc ack_recv {from time} {
    $self instvar node_
    #puts "Node [$node_ id] received custody ack from node $from at t = $time."
}

# Create bundle agents and attach them to the nodes
for { set i 0} {$i < $val(nn)} {incr i} {
    set bundle_($i) [new Agent/Bundle]
    $ns_ attach-agent $node_($i) $bundle_($i)
    set myll [$node_($i) set ll_(0)]
    $bundle_($i) if-queue [$myll down-target] ; # We want to know the IFQ length
}

source ./trafficgen.tcl

$ns_ at [expr $opt(simtime) - 0.01] "puts \"cqsize_ retxqsize_ qsize_ sentBundles_ receivedBundles_ duplicateReceivedBundles_ duplicateBundles_ deletedBundles_ forwardedBundles_ sentReceipts_ receivedReceipts_ duplicateReceivedReceipts_ duplicateReceipts_ forwardedReceipts_ avBundleDelay_ avReceiptDelay_ avBundleHopCount_ avReceiptHopCount_.\""

for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at [expr $opt(simtime) - 0.01] "$bundle_($i) print-stats [expr $opt(simtime) - 0.01]"
    $ns_ at $opt(simtime) "$node_($i) reset"
}

$ns_ at 0.1 "print-queue-stats"

$ns_ at [expr $opt(simtime) - 0.01] "print-average-stats"

$ns_ at [expr $opt(simtime) + 0.01] "finish"

# Run the simulation
$ns_ run