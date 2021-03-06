from PyQt4.QtCore import pyqtSignal, Qt
from PyQt4.QtGui import QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, QLabel, QDoubleSpinBox
from ert_gui.models.connectors.plot import ReportStepsModel
from ert_gui.tools.plot import ReportStepWidget
from ert_gui.widgets.list_spin_box import ListSpinBox


class PlotMetricsWidget(QWidget):

    VALUE_MIN = "Value Minimum"
    VALUE_MAX = "Value Maximum"

    DEPTH_MIN = "Depth Minimum"
    DEPTH_MAX = "Depth Maximum"

    TIME_MIN = "Time Minimum"
    TIME_MAX = "Time Maximum"

    plotScalesChanged = pyqtSignal()
    reportStepTimeChanged = pyqtSignal()

    def __init__(self):
        QWidget.__init__(self)

        self.__scalers = {}

        self.__layout = QVBoxLayout()
        self.__time_map = ReportStepsModel().getList()
        self.__time_index_map = {}
        for index in range(len(self.__time_map)):
            time = self.__time_map[index]
            self.__time_index_map[time] = index


        self.addScaler(PlotMetricsWidget.VALUE_MIN, self.__createDoubleSpinner())
        self.addScaler(PlotMetricsWidget.VALUE_MAX, self.__createDoubleSpinner())

        self.addScaler(PlotMetricsWidget.TIME_MIN, self.__createTimeSpinner(True))
        self.addScaler(PlotMetricsWidget.TIME_MAX, self.__createTimeSpinner(False))

        self.addScaler(PlotMetricsWidget.DEPTH_MIN, self.__createDoubleSpinner())
        self.addScaler(PlotMetricsWidget.DEPTH_MAX, self.__createDoubleSpinner())

        self.__layout.addSpacing(10)

        self.__report_step_widget = ReportStepWidget()
        self.__report_step_widget.reportStepTimeSelected.connect(self.reportStepTimeChanged)
        self.__layout.addWidget(self.__report_step_widget)

        self.__layout.addStretch()

        self.setLayout(self.__layout)

    def __createDoubleSpinner(self):
        spinner = QDoubleSpinBox()
        spinner.setEnabled(False)
        spinner.setMinimumWidth(75)
        max = 999999999999
        spinner.setRange(-max,max)
        # spinner.valueChanged.connect(self.plotScalesChanged)
        spinner.editingFinished.connect(self.plotScalesChanged)
        return spinner

    def __createTimeSpinner(self, min_value):
        def converter(item):
            return "%s" % (str(item.date()))

        spinner = ListSpinBox(self.__time_map)
        spinner.setEnabled(False)
        spinner.setMinimumWidth(75)
        spinner.valueChanged[int].connect(self.plotScalesChanged)
        spinner.setStringConverter(converter)
        if(min_value):
            spinner.setValue(0)
        return spinner

    def addScaler(self, name, spinner):
        widget = QWidget()

        layout = QHBoxLayout()
        layout.setMargin(0)
        widget.setLayout(layout)

        checkbox = QCheckBox()
        checkbox.setChecked(False)
        checkbox.stateChanged.connect(self.updateScalers)
        layout.addWidget(checkbox)

        label = QLabel(name)
        label.setEnabled(False)
        layout.addWidget(label)

        layout.addStretch()

        layout.addWidget(spinner)

        self.__layout.addWidget(widget)

        self.__scalers[name] = {"checkbox": checkbox, "label": label, "spinner": spinner}



    def updateScalers(self):
        for scaler in self.__scalers.values():
            checked = scaler["checkbox"].isChecked()
            scaler["label"].setEnabled(checked)
            scaler["spinner"].setEnabled(checked)

        self.plotScalesChanged.emit()

    def getTimeMin(self):
        scaler = self.__scalers[PlotMetricsWidget.TIME_MIN]

        if scaler["checkbox"].isChecked():
            index =scaler["spinner"].value()
            return self.__time_map[index]
        else:
            return None

    def getTimeMax(self):
        scaler = self.__scalers[PlotMetricsWidget.TIME_MAX]

        if scaler["checkbox"].isChecked():
            index = scaler["spinner"].value()
            return self.__time_map[index]
        else:
            return None

    def getValueMin(self):
        scaler = self.__scalers[PlotMetricsWidget.VALUE_MIN]

        if scaler["checkbox"].isChecked():
            return scaler["spinner"].value()
        else:
            return None

    def getValueMax(self):
        scaler = self.__scalers[PlotMetricsWidget.VALUE_MAX]

        if scaler["checkbox"].isChecked():
            return scaler["spinner"].value()
        else:
            return None

    def getDepthMin(self):
        scaler = self.__scalers[PlotMetricsWidget.DEPTH_MIN]

        if scaler["checkbox"].isChecked():
            return scaler["spinner"].value()
        else:
            return None

    def getDepthMax(self):
        scaler = self.__scalers[PlotMetricsWidget.DEPTH_MAX]

        if scaler["checkbox"].isChecked():
            return scaler["spinner"].value()
        else:
            return None


    def updateScales(self,time_min, time_max, value_min, value_max, depth_min, depth_max):
        self.setTimeScales(time_min, time_max)
        self.setValueScales(value_min, value_max)
        self.setDepthScales(depth_min, depth_max)
        self.plotScalesChanged.emit()

    def setTimeScales(self, time_min, time_max):
        time_max = self.__time_index_map.get(time_max, None)
        time_min = self.__time_index_map.get(time_min, None)
        self.__updateScale(PlotMetricsWidget.TIME_MIN, time_min)
        self.__updateScale(PlotMetricsWidget.TIME_MAX, time_max)


    def setValueScales(self, value_min, value_max):
        self.__updateScale(PlotMetricsWidget.VALUE_MIN, value_min)
        self.__updateScale(PlotMetricsWidget.VALUE_MAX, value_max)

    def setDepthScales(self, depth_min, depth_max):
        self.__updateScale(PlotMetricsWidget.DEPTH_MIN, depth_min)
        self.__updateScale(PlotMetricsWidget.DEPTH_MAX, depth_max)

    def __updateScale(self, name, value):
        scaler = self.__scalers[name]
        if value is None:
            scaler["checkbox"].blockSignals(True)
            scaler["checkbox"].setCheckState(Qt.Unchecked)
            scaler["checkbox"].blockSignals(False)
        else:
            scaler["checkbox"].blockSignals(True)
            scaler["checkbox"].setCheckState(Qt.Checked)
            scaler["checkbox"].blockSignals(False)

            scaler["spinner"].blockSignals(True)
            scaler["spinner"].setValue(value)
            scaler["spinner"].blockSignals(False)

        checked = scaler["checkbox"].isChecked()
        scaler["label"].setEnabled(checked)
        scaler["spinner"].setEnabled(checked)



    def getSelectedReportStepTime(self):
        """ @rtype: ctime """
        return self.__report_step_widget.getSelectedValue().ctime()