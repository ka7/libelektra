file (GLOB TESTS test_*.c)
foreach (file ${TESTS})
	get_filename_component (name ${file} NAME_WE)
	if (NOT ENABLE_OPTIMIZATIONS AND ${name} MATCHES "opmphm")
		continue()
	endif(NOT ENABLE_OPTIMIZATIONS AND ${name} MATCHES "opmphm")
	do_test (${name})
	target_link_elektra(${name} elektra-kdb)
endforeach (file ${TESTS})

target_link_elektra(test_array elektra-ease)
target_link_elektra(test_backend elektra-plugin)
target_link_elektra(test_keyname elektra-ease)

target_link_elektra(test_mount elektra-plugin)
target_link_elektra(test_plugin elektra-plugin)
target_link_elektra(test_mountsplit elektra-plugin)
target_link_elektra(test_split elektra-plugin)
target_link_elektra(test_splitget elektra-plugin)
target_link_elektra(test_splitset elektra-plugin)

target_link_elektra(test_meta elektra-meta)
target_link_elektra(test_meta elektra-proposal)
target_link_elektra(test_key elektra-meta)
target_link_elektra(test_key elektra-proposal)

target_link_elektra(test_operation elektra-meta)
target_link_elektra(test_operation elektra-proposal)

target_link_elektra(test_proposal elektra-proposal)
