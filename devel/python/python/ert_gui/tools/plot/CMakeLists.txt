set(PYTHON_SOURCES
    __init__.py
    data_type_keys_list_model.py
    data_type_proxy_model.py
    data_type_keys_widget.py
    plot_case_model.py
    plot_case_selection_widget.py
    plot_metrics_widget.py
    plot_panel.py
    plot_tool.py
    plot_window.py
    report_step_widget.py
    scale_tracker.py
)

add_python_package("python.ert_gui.tools.plot"  ${PYTHON_INSTALL_PREFIX}/ert_gui/tools/plot "${PYTHON_SOURCES}" True)

add_subdirectory(data)
