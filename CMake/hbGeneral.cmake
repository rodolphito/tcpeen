# Generate a test driver executable with the given name
function(tn_list_add list_var list_val)
	if(NOT ${list_var})
		set(${list_var} ${list_val} PARENT_SCOPE)
	else()
		set(${list_var} ${${list_var}} " " ${list_val} PARENT_SCOPE)
	endif()
endfunction()