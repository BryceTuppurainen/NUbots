# Find our globally shared libraries:
FIND_PACKAGE(NUClear REQUIRED)
FIND_PACKAGE(Tcmalloc REQUIRED)
FIND_PACKAGE(ZMQ REQUIRED)
FIND_PACKAGE(OpenPGM REQUIRED)
FIND_PACKAGE(Armadillo REQUIRED)
FIND_PACKAGE(LibGFortran REQUIRED)
FIND_PACKAGE(OpenBLAS)
FIND_PACKAGE(Protobuf REQUIRED)
FIND_PACKAGE(CATCH REQUIRED)
FIND_PACKAGE(YAML REQUIRED)
FIND_PACKAGE(MuParser REQUIRED)

# Resolve problems:
IF(${OPENBLAS_FOUND})
	SET(BLAS_LIBRARIES    ${OPENBLAS_LIBRARIES})
	SET(BLAS_INCLUDE_DIRS ${OPENBLAS_INCLUDE_DIRS})
ELSE()
	FIND_PACKAGE(BLAS REQUIRED)
	MESSAGE(WARNING "OpenBLAS was not found. Using BLAS instead.")
ENDIF()


# Set include directories and libraries:
INCLUDE_DIRECTORIES(SYSTEM ${NUCLEAR_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${BLAS_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${ZMQ_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${PROTOBUF_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${CATCH_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${YAML_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM ${MUPARSER_INCLUDE_DIRS})

SET(NUBOTS_SHARED_LIBRARIES
    ${TCMALLOC_LIBRARIES}
    ${NUCLEAR_LIBRARIES}
    ${BLAS_LIBRARIES}
    ${LIBGFORTRAN_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${OPENPGM_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    ${YAML_LIBRARIES}
    ${MUPARSER_LIBRARIES}
)
