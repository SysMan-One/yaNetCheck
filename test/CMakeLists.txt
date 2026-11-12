#
#	Functional tests of the YANETCHK utility.
#
add_test(NAME help_overview
	COMMAND yanetchk help)
set_tests_properties(help_overview PROPERTIES
	PASS_REGULAR_EXPRESSION "SEND|send|Usage|Information|Available")

add_test(NAME help_topic
	COMMAND yanetchk help send tmac)
set_tests_properties(help_topic PROPERTIES
	PASS_REGULAR_EXPRESSION "TMAC|SEND")

add_test(NAME help_switch
	COMMAND yanetchk send /HELP)
set_tests_properties(help_switch PROPERTIES
	PASS_REGULAR_EXPRESSION "SEND|Usage")

add_test(NAME sendrecv
	COMMAND ${CMAKE_COMMAND}
		-DYANETCHK=$<TARGET_FILE:yanetchk>
		-P ${CMAKE_CURRENT_SOURCE_DIR}/sendrecv.cmake)
set_tests_properties(sendrecv PROPERTIES
	PASS_REGULAR_EXPRESSION "PASS")

add_executable(win_unit ${CMAKE_CURRENT_SOURCE_DIR}/win_unit.c)

add_test(NAME window_accounting COMMAND win_unit)
set_tests_properties(window_accounting PROPERTIES
	PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME ring_needs_ifname
	COMMAND ${CMAKE_COMMAND}
		-DYANETCHK=$<TARGET_FILE:yanetchk>
		-P ${CMAKE_CURRENT_SOURCE_DIR}/ring.cmake)
set_tests_properties(ring_needs_ifname PROPERTIES
	PASS_REGULAR_EXPRESSION "PASS")
