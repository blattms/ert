set(PYTHON_SOURCES
    __init__.py
    tool.py
)

add_python_package("python.ert_gui.tools"  ${PYTHON_INSTALL_PREFIX}/ert_gui/tools "${PYTHON_SOURCES}" True)

add_subdirectory(export)
add_subdirectory(ide)
add_subdirectory(manage_cases)
add_subdirectory(plot)
add_subdirectory(workflows)
