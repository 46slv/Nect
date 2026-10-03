file(MAKE_DIRECTORY "${PACKAGE_ROOT}/bin")
file(COPY_FILE "${BINARY}" "${PACKAGE_ROOT}/bin/nect_testop.dll")
file(SHA256 "${PACKAGE_ROOT}/bin/nect_testop.dll" BINARY_SHA256)
configure_file("${TEMPLATE}" "${PACKAGE_ROOT}/extension.json" @ONLY)
