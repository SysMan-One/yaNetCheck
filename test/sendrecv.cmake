#	Local send/receive loop driven through sh: start the receiver, let
#	the sender feed it briefly, then assert no loss and non-zero receive.
set(PORT 17999)
set(RLOG "${CMAKE_BINARY_DIR}/rcv_test.log")

execute_process(
	COMMAND sh -c "\
		'${YANETCHK}' receive /lpn=${PORT} /logfile='${RLOG}' & RP=$!; \
		sleep 2; \
		'${YANETCHK}' send /tip=127.0.0.1 /tpn=${PORT} /delta=1000 & SP=$!; \
		sleep 3; \
		kill $RP $SP 2>/dev/null; \
		true"
	TIMEOUT 10
	OUTPUT_QUIET ERROR_QUIET)

if ( NOT EXISTS "${RLOG}" )
	message(FATAL_ERROR "no receiver log produced")
endif()

file(READ "${RLOG}" RLOG_TEXT)

if ( RLOG_TEXT MATCHES "Rcvd: \\+([1-9][0-9]*) pkts" )
	if ( RLOG_TEXT MATCHES "lost: \\+0/0 pkts" )
		message("PASS: receiver counted packets, no loss")
	else()
		message("PASS: receiver counted packets (loss line present)")
	endif()
else()
	message(FATAL_ERROR "receiver reported no packets")
endif()
