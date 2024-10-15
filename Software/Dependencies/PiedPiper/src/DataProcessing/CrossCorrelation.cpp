template <class T> class CrossCorrelation::CrossCorrelation() {
    this->templatePtr = NULL;

    this->templateSumSquared = 0;
    this->numRows = 0;
    this->numCols = 0;
}

void CrossCorrelation::computeTemplate() {
    this->templateSumSquared = 0;
}

void CrossCorrelation::setTemplate(T **input, uint16_t numRows, uint16_t numCols) {
    this->templatePtr = input;
    this->numRows = numRows;
    this->numCols = numCols;
}

float CrossCorrelation::correlate(T **input) {

}