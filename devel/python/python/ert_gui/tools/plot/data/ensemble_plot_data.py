from PyQt4.QtCore import QObject, pyqtSlot, QString


class EnsemblePlotData(QObject):
    def __init__(self, name, case_name, parent=None):
        QObject.__init__(self, parent)

        self.__name = name
        self.__case_name = case_name

        self.__x_values = None
        self.__y_values = None
        self.__y_min_values = None
        self.__y_max_values = None

        self.__has_data = False

        self.__report_step_time_indexes = {}
        self.__first_report_step_time = None
        self.__last_report_step_time = None

        self.__min_x = None
        self.__max_x = None
        self.__min_y = None
        self.__max_y = None



    def setEnsembleData(self, x_values, y_values, y_min_values, y_max_values, histogram_support=True):
        if x_values is not None and y_values is not None and y_min_values is not None and y_max_values is not None:
            self.__x_values = x_values
            self.__y_values = y_values

            self.__y_min_values = y_min_values
            self.__y_max_values = y_max_values

            self.__has_data = True

            if histogram_support:
                for index in range(len(x_values)):
                    self.__report_step_time_indexes[x_values[index]] = index

                self.__first_report_step_time = min(x_values)
                self.__last_report_step_time = max(x_values)


    def updateBoundaries(self, min_x, max_x, min_y, max_y):
        if min_x is not None and (self.__min_x is None or self.__min_x > min_x):
            self.__min_x = min_x

        if max_x is not None and (self.__max_x is None or self.__max_x < max_x):
            self.__max_x = max_x

        if min_y is not None and (self.__min_y is None or self.__min_y > min_y):
            self.__min_y = min_y

        if max_y is not None and (self.__max_y is None or self.__max_y < max_y):
            self.__max_y = max_y



    @pyqtSlot(result=str)
    def name(self):
        return self.__name

    @pyqtSlot(result=str)
    def caseName(self):
        return self.__case_name



    @pyqtSlot(result="QVariantList")
    def xValues(self):
        return self.__x_values

    @pyqtSlot(result="QVariantList")
    def yValues(self):
        return self.__y_values

    @pyqtSlot(result="QVariantList")
    def yMinValues(self):
        return self.__y_min_values

    @pyqtSlot(result="QVariantList")
    def yMaxValues(self):
        return self.__y_max_values

    @pyqtSlot(QString, result=int)
    def realizationCount(self):
        return len(self.__y_values)

    @pyqtSlot(result=float)
    def minX(self):
        return self.__min_x

    @pyqtSlot(result=float)
    def maxX(self):
        return self.__max_x

    @pyqtSlot(result=float)
    def minY(self):
        return self.__min_y

    @pyqtSlot(result=float)
    def maxY(self):
        return self.__max_y


    @pyqtSlot(result=bool)
    def isValid(self):
        return self.hasBoundaries() and self.hasData()

    @pyqtSlot(result=bool)
    def hasBoundaries(self):
        return self.__min_x is not None and self.__max_x is not None and self.__min_y is not None and self.__max_y is not None

    @pyqtSlot(result=int)
    def lastReportStepTime(self):
        return self.__last_report_step_time

    @pyqtSlot(result=bool)
    def hasData(self):
        return self.__has_data

    @pyqtSlot(int, result=bool)
    def hasSample(self, report_step_time):
        return report_step_time in self.__report_step_time_indexes

    @pyqtSlot(int, result="QVariantList")
    def getSample(self, report_step_time):
        index = self.__report_step_time_indexes[report_step_time]

        result = []
        for realization in self.__y_values:
            if len(realization) > 0:
                result.append(realization[index])  # todo check length

        return result

