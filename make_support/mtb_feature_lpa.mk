# This file contains the description of sources that are required to enable the LPA
# feature in amazon-freertos.

################################################################################
# Additional Source files and includes needed for LPA support
################################################################################

CY_LPA_LIB_DIR=$(CY_AFR_ROOT)/vendors/cypress/MTB/libraries/lpa

SOURCES+=\
    $(wildcard ${CY_LPA_LIB_DIR}/source/*.c)\
    $(wildcard ${CY_LPA_LIB_DIR}/target/COMPONENT_AFR/*.c)\
    $(wildcard ${CY_LPA_LIB_DIR}/helpers/net_activity/COMPONENT_AFR/*.c)\

INCLUDES+=\
    ${CY_LPA_LIB_DIR}/source\
    ${CY_LPA_LIB_DIR}/target\
    ${CY_LPA_LIB_DIR}/target/COMPONENT_AFR\
    ${CY_LPA_LIB_DIR}/helpers/net_activity\
    ${CY_LPA_LIB_DIR}/helpers/net_activity/COMPONENT_AFR\
    ${CY_LPA_LIB_DIR}/include
