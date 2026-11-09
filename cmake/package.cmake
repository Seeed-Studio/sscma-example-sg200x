set(CPACK_GENERATOR "DEB")

if(NOT DEFINED PROJECT_DESCRIPTION)
    set(PROJECT_DESCRIPTION "${PROJECT_NAME}) for ${CMAKE_SYSTEM_NAME}")
endif()

if(NOT DEFINED PROJECT_VENDOR)
    set(PROJECT_VENDOR "Seeed Technology Co.,Ltd.")
endif()

if(NOT DEFINED PROJECT_CONTACT)
    set(PROJECT_CONTACT "techsupport@seeed.cc")
endif()

if(NOT DEFINED CPACK_DEBIAN_PACKAGE_MAINTAINER)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${PROJECT_CONTACT})
endif()

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
set(CPACK_PACKAGE_CONTACT ${PROJECT_CONTACT})

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "riscv64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${CPACK_DEBIAN_PACKAGE_MAINTAINER})
set(CPACK_DEBIAN_COMPRESSION_TYPE "gzip")
set(CPACK_DEBIAN_PACKAGE_COMPRESSION_TYPE "gzip")


set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)

if(EXISTS ${PROJECT_DIR}/rootfs AND IS_DIRECTORY ${PROJECT_DIR}/rootfs)
    install(DIRECTORY ${PROJECT_DIR}/rootfs/ DESTINATION / USE_SOURCE_PERMISSIONS)
endif()

# ---------------------------------------------------------------------------
# App manifest version: derived, never hand-maintained.
#
# The deb's version comes from project(<id> VERSION x.y.z) -- CPack builds the
# filename from it and opkg records it. The gallery manifest carries its own
# "version" field, which the console renders on the app card, and for a long
# time that field was a hand-written second copy with nothing tying it to the
# first. Every application had drifted: cards advertised 0.1.0 while the
# installed package was 0.2.0, and two of them advertised a version *ahead* of
# any package that ever existed.
#
# So the field is generated here instead. The source manifest's value is
# overwritten with PROJECT_VERSION on the way into the package (this install()
# runs after the rootfs copy above, so it wins for the same destination path).
# Editing the version in the source JSON has no effect on what ships -- bump
# project(VERSION) and the card follows.
#
# scripts/check-manifest-versions.sh keeps the source files honest anyway, and
# covers the one thing generation cannot reach: the manifest copies that ship
# inside the supervisor package for apps that are not installed yet.
# ---------------------------------------------------------------------------
set(_manifest_src "${PROJECT_DIR}/rootfs/usr/share/${PROJECT_NAME}/${PROJECT_NAME}.json")
if(EXISTS ${_manifest_src})
    file(READ ${_manifest_src} _manifest_text)
    string(REGEX REPLACE
        "\"version\"[ \t]*:[ \t]*\"[^\"]*\""
        "\"version\": \"${PROJECT_VERSION}\""
        _manifest_text "${_manifest_text}")
    set(_manifest_gen "${CMAKE_BINARY_DIR}/manifest/${PROJECT_NAME}.json")
    file(WRITE ${_manifest_gen} "${_manifest_text}")
    install(FILES ${_manifest_gen} DESTINATION /usr/share/${PROJECT_NAME})
    message(STATUS "Manifest version pinned to ${PROJECT_VERSION}: ${PROJECT_NAME}.json")
endif()

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_DIR}/control/preinst;${PROJECT_DIR}/control/postinst;${PROJECT_DIR}/control/prerm;${PROJECT_DIR}/control/postrm")

include(CPack)