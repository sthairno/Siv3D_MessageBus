cmake_minimum_required(VERSION 3.16)

foreach(_v IN ITEMS INPUT_PATH OUTPUT_PATH TITLE FUNCTION_NAME FORMAT)
	if(NOT DEFINED ${_v})
		message(FATAL_ERROR "${_v} is required")
	endif()
endforeach()

foreach(_v IN ITEMS INPUT_PATH OUTPUT_PATH TITLE FUNCTION_NAME FORMAT)
	# Xcode generator may pass values as a quoted string literal (e.g. \"...\"), which
	# becomes a leading/trailing quote in the actual -D value. Strip them if present.
	if("${${_v}}" MATCHES "^\"(.*)\"$")
		set(${_v} "${CMAKE_MATCH_1}")
	endif()
endforeach()

file(READ "${INPUT_PATH}" _raw)
string(REPLACE "\r\n" "\n" _raw "${_raw}")
string(REPLACE "\r" "\n" _raw "${_raw}")

set(_copyright_text "")
set(_license_text "")

if(FORMAT STREQUAL "bsd3")
	# Same behavior as the former Python generator:
	# If the marker appears exactly once, split there (marker belongs to copyright).
	set(_marker "All rights reserved.")
	string(FIND "${_raw}" "${_marker}" _first)
	string(FIND "${_raw}" "${_marker}" _last REVERSE)

	if(_first LESS 0)
		message(FATAL_ERROR "FORMAT=bsd3 expects marker '${_marker}' to exist in: ${INPUT_PATH}")
	endif()
	if(NOT _first EQUAL _last)
		message(FATAL_ERROR "FORMAT=bsd3 expects marker '${_marker}' to appear exactly once in: ${INPUT_PATH}")
	endif()

	string(LENGTH "${_marker}" _marker_len)
	math(EXPR _after "${_first}+${_marker_len}")

	string(SUBSTRING "${_raw}" 0 ${_first} _pre)
	string(SUBSTRING "${_raw}" ${_after} -1 _post)
	set(_copyright_text "${_pre}${_marker}")
	set(_license_text "${_post}")
elseif(FORMAT STREQUAL "mit")
	# Expected format:
	#   MIT License
	#
	#   Copyright...
	#   (possibly multiple lines)
	#
	#   <license body...>
	string(REPLACE "\n" ";" _lines "${_raw}")

	set(_state "inHeader")
	set(_copyright_lines "")
	set(_body_lines "")

	foreach(_line IN LISTS _lines)
		string(STRIP "${_line}" _s)

		if(_state STREQUAL "inHeader")
			if(NOT _s STREQUAL "MIT License")
				message(FATAL_ERROR "FORMAT=mit expects first non-empty line to be 'MIT License' in: ${INPUT_PATH}")
			endif()

			set(_state "seekCopyright")
		elseif(_state STREQUAL "seekCopyright")
			if(_s STREQUAL "")
				continue()
			endif()
			if(NOT _s MATCHES "^Copyright.*$")
				message(FATAL_ERROR "FORMAT=mit expects copyright line in: ${INPUT_PATH}")
			endif()

			list(APPEND _copyright_lines "${_s}")
			set(_state "inCopyright")
		elseif(_state STREQUAL "inCopyright")
			if(_s MATCHES "^Copyright.*$")
				list(APPEND _copyright_lines "${_s}")
				continue()
			endif()
			set(_state "inBody")
		else() # inBody
			list(APPEND _body_lines "${_line}")
		endif()
	endforeach()

	if(NOT _state STREQUAL "inBody")
		message(FATAL_ERROR "FORMAT=mit expects to be in body after copyright block in: ${INPUT_PATH}")
	endif()

	list(JOIN _copyright_lines "\n" _copyright_text)
	list(JOIN _body_lines "\n" _license_text)
else()
	message(FATAL_ERROR "Unknown FORMAT: ${FORMAT} (expected bsd3 or mit)")
endif()

string(STRIP "${_copyright_text}" _copyright_text)
string(STRIP "${_license_text}" _license_text)

set(_cpp "// This file is auto-generated. Do not edit manually.\n")
string(APPEND _cpp "#include \"MessageBus/detail/GeneratedLicenses.hpp\"\n")
string(APPEND _cpp "\n")
string(APPEND _cpp "namespace MessageBus::Generated\n")
string(APPEND _cpp "{\n")
string(APPEND _cpp "\tstatic const s3d::LicenseInfo license = {\n")
string(APPEND _cpp "\t\tU\"${TITLE}\",\n")
string(APPEND _cpp "\t\tUR\"LICENSE_COPY(${_copyright_text})LICENSE_COPY\",\n")
string(APPEND _cpp "\t\tUR\"LICENSE_TEXT(${_license_text})LICENSE_TEXT\"\n")
string(APPEND _cpp "\t};\n")
string(APPEND _cpp "\n")
string(APPEND _cpp "\tconst s3d::LicenseInfo& ${FUNCTION_NAME}()\n")
string(APPEND _cpp "\t{\n")
string(APPEND _cpp "\t\treturn license;\n")
string(APPEND _cpp "\t}\n")
string(APPEND _cpp "}\n")

get_filename_component(_out_dir "${OUTPUT_PATH}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")

file(WRITE "${OUTPUT_PATH}" "${_cpp}")
message(STATUS "[GenerateThirdPartyLicenseCpp] generated: ${OUTPUT_PATH}")
