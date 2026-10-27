#include <iostream>
#include <string>

enum class SensorStatus {
    OK,
    FAULT,             // Sensor fail
    ABOVE_UPPER_LIMIT, // Value above limit
    BELOW_LOWER_LIMIT  // Value below limit
};

// Helper function to convert status to string description
std::string statusToString(SensorStatus status) {
    switch (status) {
        case SensorStatus::OK:                return "[OK]";
        case SensorStatus::FAULT :            return "[WARNING: Sensor fault!]";
        case SensorStatus::ABOVE_UPPER_LIMIT: return "[WARNING: Pressure above sea-level norm!]";
        case SensorStatus::BELOW_LOWER_LIMIT: return "[WARNING: Altitude limit exceeded (pressure < 480 hPa)!]";
    }
    return "[UNKNOWN]";
}

class RingBuffer {
private:
    static const size_t BUFFER_SIZE = 16;
    float buffer[BUFFER_SIZE] = {0.0f};
    size_t head = 0;
    size_t count = 0;
    float sum = 0.0f;

public:
    void push(float value) {
        if (count == BUFFER_SIZE) {
            sum -= buffer[head];
        } else {
            count++;
        }
        
        buffer[head] = value;
        sum += value;
        
        head = (head + 1) % BUFFER_SIZE;
    }

    float getAverage() const {
        if (count == 0) return 0.0f;
        return sum / static_cast<float>(count);
    }
};

class PressureSensor {
protected:
    RingBuffer historyBuffer;
    size_t stepCount = 0;
    
    // Test data
    static const size_t TEST_DATA_SIZE = 4;
    virtual bool readRawData(float& rawPressure) = 0;

public:
    PressureSensor() = default;
    virtual ~PressureSensor() = default;

    SensorStatus validatePressure(float pressure) const {
        const float MIN_PRESSURE = 480.0f;
        const float MAX_PRESSURE = 1060.0f;

        if (pressure < MIN_PRESSURE) return SensorStatus::BELOW_LOWER_LIMIT;
        if (pressure > MAX_PRESSURE) return SensorStatus::ABOVE_UPPER_LIMIT;
        return SensorStatus::OK;
    }

    void process(float& outRaw, float& outFiltered, SensorStatus& outStatus) {
        float raw = 0.0f;

        if (!readRawData(raw)) {
            outRaw = 0.0f;
            outFiltered = historyBuffer.getAverage();
            outStatus = SensorStatus::FAULT;
            return;
        }

        outRaw = raw;

        SensorStatus rawStatus = validatePressure(raw);
        if (rawStatus != SensorStatus::OK) {
            outStatus = rawStatus;
            outFiltered = historyBuffer.getAverage();
            return;
        }

        historyBuffer.push(raw);
        outFiltered = historyBuffer.getAverage();
        outStatus = validatePressure(outFiltered);
    }
};

class I2CPressureSensor : public PressureSensor {
private:
    const float testData[TEST_DATA_SIZE] = {
        900.0f,   // 1. OK (normal height)
        1070.0f,  // 2. INVALID: Above upper limit (> 1060 hPa)
        890.0f,   // 3. OK (normal height)
        450.0f    // 4. INVALID: Below lower limit / altitude > 5000m (< 480 hPa)
    };

protected:
    bool readRawData(float& rawPressure) override {
        rawPressure = testData[stepCount % TEST_DATA_SIZE];
        stepCount++;
        return true;
    }
};

class SPIPressureSensor : public PressureSensor {
private:
    const float testData[TEST_DATA_SIZE] = {
        901.2f,   // 1. OK
        899.9f,   // 2. OK
        0.0f,     // 3. Ignored here because communication will fail
        898.4f    // 4. OK
    };

protected:
    bool readRawData(float& rawPressure) override {
        // Simulate a communication fault
        if ((stepCount % TEST_DATA_SIZE) == 2) {
            stepCount++;
            return false; // Hardware read failure
        }

        rawPressure = testData[stepCount % TEST_DATA_SIZE];
        stepCount++;
        return true;
    }
};

int main() {
    I2CPressureSensor i2cSensor;
    SPIPressureSensor spiSensor;

    float rawI2C = 0, filteredI2C = 0;
    SensorStatus statusI2C = SensorStatus::OK;

    float rawSPI = 0, filteredSPI = 0;
    SensorStatus statusSPI = SensorStatus::OK;

    const int TOTAL_STEPS = 4;
    // Loop to poll sensors step-by-step and test validation boundaries & faults
    for (int step = 1; step <= TOTAL_STEPS; ++step) {
        std::cout << "--- Poll Step #" << step << " ---\n";

        // Process I2C Sensor
        i2cSensor.process(rawI2C, filteredI2C, statusI2C);
        std::cout << "[I2C Sensor] Raw: " << rawI2C << " hPa | Filtered: " << filteredI2C 
                  << " hPa | Status: " << statusToString(statusI2C) << "\n";

        // Process SPI Sensor
        spiSensor.process(rawSPI, filteredSPI, statusSPI);
        std::cout << "[SPI Sensor] Raw: " << rawSPI << " hPa | Filtered: " << filteredSPI 
                  << " hPa | Status: " << statusToString(statusSPI) << "\n\n";
    }

    return 0;
}