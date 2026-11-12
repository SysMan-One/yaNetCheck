#	/RING without /IFNAME must be refused with a warning (and the sender
#	then falls back to the UDP engine). Run the sender briefly, capture
#	its log, and assert the warning is present.
set(RLOG "${CMAKE_BINARY_DIR}/ring.log")

execute_process(
	COMMAND sh -c "'${YANETCHK}' send /tip=127.0.0.1 /tpn=19911 /ring /delta=100000 /logfile='${RLOG}' & SP=$!; sleep 1; kill -INT $SP 2>/dev/null; true"
	TIMEOUT 10 OUTPUT_QUIET ERROR_QUIET)

if ( NOT EXISTS "${RLOG}" )
	message(FATAL_ERROR "no sender log produced")
endif()
file(READ "${RLOG}" T)
if ( T MATCHES "RING ignored" OR T MATCHES "requires the AF_PACKET" )
	message("PASS: /RING without /IFNAME warned")
else()
	message(FATAL_ERROR "no /RING warning:\n${T}")
endif()
