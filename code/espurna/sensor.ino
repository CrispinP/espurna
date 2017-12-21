/*

SENSOR MODULE

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

#include <vector>
#include "filters/MaxFilter.h"
#include "filters/MedianFilter.h"
#include "filters/MovingAverageFilter.h"
#include "sensors/BaseSensor.h"

typedef struct {
    BaseSensor * sensor;
    unsigned char local;        // Local index in its provider
    magnitude_t type;           // Type of measurement
    unsigned char global;       // Global index in its type
    double current;             // Current (last) value, unfiltered
    double filtered;            // Filtered (averaged) value
    double reported;            // Last reported value
    double min_change;          // Minimum value change to report
    BaseFilter * filter;    // Filter object
} sensor_magnitude_t;

std::vector<BaseSensor *> _sensors;
std::vector<sensor_magnitude_t> _magnitudes;

unsigned char _counts[MAGNITUDE_MAX];
bool _sensor_realtime = API_REAL_TIME_VALUES;
unsigned char _sensor_temperature_units = SENSOR_TEMPERATURE_UNITS;
double _sensor_temperature_correction = SENSOR_TEMPERATURE_CORRECTION;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

String _sensorTopic(magnitude_t type) {
    if (type == MAGNITUDE_TEMPERATURE) return String(MAGNITUDE_TEMPERATURE_TOPIC);
    if (type == MAGNITUDE_HUMIDITY) return String(MAGNITUDE_HUMIDITY_TOPIC);
    if (type == MAGNITUDE_PRESSURE) return String(MAGNITUDE_PRESSURE_TOPIC);
    if (type == MAGNITUDE_CURRENT) return String(MAGNITUDE_CURRENT_TOPIC);
    if (type == MAGNITUDE_VOLTAGE) return String(MAGNITUDE_VOLTAGE_TOPIC);
    if (type == MAGNITUDE_POWER_ACTIVE) return String(MAGNITUDE_ACTIVE_POWER_TOPIC);
    if (type == MAGNITUDE_POWER_APPARENT) return String(MAGNITUDE_APPARENT_POWER_TOPIC);
    if (type == MAGNITUDE_POWER_REACTIVE) return String(MAGNITUDE_REACTIVE_POWER_TOPIC);
    if (type == MAGNITUDE_POWER_FACTOR) return String(MAGNITUDE_POWER_FACTOR_TOPIC);
    if (type == MAGNITUDE_ENERGY) return String(MAGNITUDE_ENERGY_TOPIC);
    if (type == MAGNITUDE_ENERGY_DELTA) return String(MAGNITUDE_ENERGY_DELTA_TOPIC);
    if (type == MAGNITUDE_ANALOG) return String(MAGNITUDE_ANALOG_TOPIC);
    if (type == MAGNITUDE_DIGITAL) return String(MAGNITUDE_DIGITAL_TOPIC);
    if (type == MAGNITUDE_EVENTS) return String(MAGNITUDE_EVENTS_TOPIC);
    if (type == MAGNITUDE_PM1dot0) return String(MAGNITUDE_PM1dot0_TOPIC);
    if (type == MAGNITUDE_PM2dot5) return String(MAGNITUDE_PM2dot5_TOPIC);
    if (type == MAGNITUDE_PM10) return String(MAGNITUDE_PM10_TOPIC);
    if (type == MAGNITUDE_CO2) return String(MAGNITUDE_CO2_TOPIC);
    return String(MAGNITUDE_UNKNOWN_TOPIC);
}

unsigned char _sensorDecimals(magnitude_t type) {
    if (type == MAGNITUDE_TEMPERATURE) return MAGNITUDE_TEMPERATURE_DECIMALS;
    if (type == MAGNITUDE_HUMIDITY) return MAGNITUDE_HUMIDITY_DECIMALS;
    if (type == MAGNITUDE_PRESSURE) return MAGNITUDE_PRESSURE_DECIMALS;
    if (type == MAGNITUDE_CURRENT) return MAGNITUDE_CURRENT_DECIMALS;
    if (type == MAGNITUDE_VOLTAGE) return MAGNITUDE_VOLTAGE_DECIMALS;
    if (type == MAGNITUDE_POWER_ACTIVE) return MAGNITUDE_POWER_DECIMALS;
    if (type == MAGNITUDE_POWER_APPARENT) return MAGNITUDE_POWER_DECIMALS;
    if (type == MAGNITUDE_POWER_REACTIVE) return MAGNITUDE_POWER_DECIMALS;
    if (type == MAGNITUDE_POWER_FACTOR) return MAGNITUDE_POWER_FACTOR_DECIMALS;
    if (type == MAGNITUDE_ENERGY) return MAGNITUDE_ENERGY_DECIMALS;
    if (type == MAGNITUDE_ENERGY_DELTA) return MAGNITUDE_ENERGY_DECIMALS;
    if (type == MAGNITUDE_ANALOG) return MAGNITUDE_ANALOG_DECIMALS;
    if (type == MAGNITUDE_EVENTS) return MAGNITUDE_EVENTS_DECIMALS;
    if (type == MAGNITUDE_PM1dot0) return MAGNITUDE_PM1dot0_DECIMALS;
    if (type == MAGNITUDE_PM2dot5) return MAGNITUDE_PM2dot5_DECIMALS;
    if (type == MAGNITUDE_PM10) return MAGNITUDE_PM10_DECIMALS;
    if (type == MAGNITUDE_CO2) return MAGNITUDE_CO2_DECIMALS;
    return 0;
}

String _sensorUnits(magnitude_t type) {
    if (type == MAGNITUDE_TEMPERATURE) return (_sensor_temperature_units == TMP_CELSIUS) ? String("C") : String("F");
    if (type == MAGNITUDE_HUMIDITY) return String("%");
    if (type == MAGNITUDE_PRESSURE) return String("hPa");
    if (type == MAGNITUDE_CURRENT) return String("A");
    if (type == MAGNITUDE_VOLTAGE) return String("V");
    if (type == MAGNITUDE_POWER_ACTIVE) return String("W");
    if (type == MAGNITUDE_POWER_APPARENT) return String("W");
    if (type == MAGNITUDE_POWER_REACTIVE) return String("W");
    if (type == MAGNITUDE_POWER_FACTOR) return String("%");
    if (type == MAGNITUDE_ENERGY) return String("J");
    if (type == MAGNITUDE_ENERGY_DELTA) return String("J");
    if (type == MAGNITUDE_EVENTS) return String("/min");
    if (type == MAGNITUDE_PM1dot0) return String("µg/m3");
    if (type == MAGNITUDE_PM2dot5) return String("µg/m3");
    if (type == MAGNITUDE_PM10) return String("µg/m3");
    if (type == MAGNITUDE_CO2) return String("ppm");
    return String();
}

double _sensorProcess(magnitude_t type, double value) {
    if (type == MAGNITUDE_TEMPERATURE) {
        if (_sensor_temperature_units == TMP_FAHRENHEIT) value = value * 1.8 + 32;
        value = value + _sensor_temperature_correction;
    }
    return roundTo(value, _sensorDecimals(type));
}

#if WEB_SUPPORT

void _sensorWebSocketSendData(JsonObject& root) {

    char buffer[10];
    bool hasTemperature = false;

    JsonArray& list = root.createNestedArray("magnitudes");
    for (unsigned char i=0; i<_magnitudes.size(); i++) {

        sensor_magnitude_t magnitude = _magnitudes[i];
        unsigned char decimals = _sensorDecimals(magnitude.type);
        dtostrf(magnitude.current, 1-sizeof(buffer), decimals, buffer);

        JsonObject& element = list.createNestedObject();
        element["type"] = int(magnitude.type);
        element["value"] = String(buffer);
        element["units"] = _sensorUnits(magnitude.type);
        element["description"] = magnitude.sensor->slot(magnitude.local);
        element["error"] = magnitude.sensor->error();

        if (magnitude.type == MAGNITUDE_TEMPERATURE) hasTemperature = true;

    }

    //root["apiRealTime"] = _sensor_realtime;
    root["tmpUnits"] = _sensor_temperature_units;
    root["tmpCorrection"] = _sensor_temperature_correction;
    if (hasTemperature) root["temperatureVisible"] = 1;

}

void _sensorWebSocketStart(JsonObject& root) {

    bool hasSensors = false;

    for (unsigned char i=0; i<_sensors.size(); i++) {
        BaseSensor * sensor = _sensors[i];

        #if EMON_ANALOG_SUPPORT
            if (sensor->getID() == SENSOR_EMON_ANALOG_ID) {
                root["emonVisible"] = 1;
                root["pwrRatioC"] = ((EmonAnalogSensor *) sensor)->getCurrentRatio(0);
                root["pwrVoltage"] = ((EmonAnalogSensor *) sensor)->getVoltage();
                hasSensors = true;
            }
        #endif

    }

    if (hasSensors) root["sensorsVisible"] = 1;

    /*
    // Sensors manifest
    JsonArray& manifest = root.createNestedArray("manifest");
    #if BMX280_SUPPORT
        BMX280Sensor::manifest(manifest);
    #endif

    // Sensors configuration
    JsonArray& sensors = root.createNestedArray("sensors");
    for (unsigned char i; i<_sensors.size(); i++) {
        JsonObject& sensor = sensors.createNestedObject();
        sensor["index"] = i;
        sensor["id"] = _sensors[i]->getID();
        _sensors[i]->getConfig(sensor);
    }
    */

}

void _sensorAPISetup() {

    for (unsigned char magnitude_id=0; magnitude_id<_magnitudes.size(); magnitude_id++) {

        sensor_magnitude_t magnitude = _magnitudes[magnitude_id];

        String topic = _sensorTopic(magnitude.type);
        if (SENSOR_USE_INDEX || (_counts[magnitude.type] > 1)) topic = topic + "/" + String(magnitude.global);

        apiRegister(topic.c_str(), topic.c_str(), [magnitude_id](char * buffer, size_t len) {
            sensor_magnitude_t magnitude = _magnitudes[magnitude_id];
            unsigned char decimals = _sensorDecimals(magnitude.type);
            double value = _sensor_realtime ? magnitude.current : magnitude.filtered;
            dtostrf(value, 1-len, decimals, buffer);
        });

    }

}
#endif

void _sensorTick() {
    for (unsigned char i=0; i<_sensors.size(); i++) {
        _sensors[i]->tick();
    }
}

void _sensorPre() {
    for (unsigned char i=0; i<_sensors.size(); i++) {
        _sensors[i]->pre();
        if (!_sensors[i]->status()) {
            DEBUG_MSG("[SENSOR] Error reading data from %s (error: %d)\n",
                _sensors[i]->description().c_str(),
                _sensors[i]->error()
            );
        }
    }
}

void _sensorPost() {
    for (unsigned char i=0; i<_sensors.size(); i++) {
        _sensors[i]->post();
    }
}

// -----------------------------------------------------------------------------
// Sensor initialization
// -----------------------------------------------------------------------------

void _sensorInit() {

    #if ANALOG_SUPPORT
    {
        AnalogSensor * sensor = new AnalogSensor();
        _sensors.push_back(sensor);
    }
    #endif

    #if BMX280_SUPPORT
    {
        BMX280Sensor * sensor = new BMX280Sensor();
        sensor->setAddress(BMX280_ADDRESS);
        _sensors.push_back(sensor);
    }
    #endif

    #if DALLAS_SUPPORT
    {
        DallasSensor * sensor = new DallasSensor();
        sensor->setGPIO(DALLAS_PIN);
        _sensors.push_back(sensor);
    }
    #endif

    #if DHT_SUPPORT
    {
        DHTSensor * sensor = new DHTSensor();
        sensor->setGPIO(DHT_PIN);
        sensor->setType(DHT_TYPE);
        _sensors.push_back(sensor);
    }
    #endif

    #if DIGITAL_SUPPORT
    {
        DigitalSensor * sensor = new DigitalSensor();
        sensor->setGPIO(DIGITAL_PIN);
        sensor->setMode(DIGITAL_PIN_MODE);
        sensor->setDefault(DIGITAL_DEFAULT_STATE);
        _sensors.push_back(sensor);
    }
    #endif

    #if ECH1560_SUPPORT
    {
        ECH1560Sensor * sensor = new ECH1560Sensor();
        sensor->setCLK(ECH1560_CLK_PIN);
        sensor->setMISO(ECH1560_MISO_PIN);
        sensor->setInverted(ECH1560_INVERTED);
        _sensors.push_back(sensor);
    }
    #endif

    #if EMON_ADC121_SUPPORT
    {
        EmonADC121Sensor * sensor = new EmonADC121Sensor();
        sensor->setAddress(EMON_ADC121_I2C_ADDRESS);
        sensor->setVoltage(EMON_MAINS_VOLTAGE);
        sensor->setReference(EMON_REFERENCE_VOLTAGE);
        sensor->setCurrentRatio(0, EMON_CURRENT_RATIO);
        _sensors.push_back(sensor);
    }
    #endif

    #if EMON_ADS1X15_SUPPORT
    {
        EmonADS1X15Sensor * sensor = new EmonADS1X15Sensor();
        sensor->setAddress(EMON_ADS1X15_I2C_ADDRESS);
        sensor->setType(EMON_ADS1X15_TYPE);
        sensor->setMask(EMON_ADS1X15_MASK);
        sensor->setGain(EMON_ADS1X15_GAIN);
        sensor->setVoltage(EMON_MAINS_VOLTAGE);
        sensor->setCurrentRatio(0, EMON_CURRENT_RATIO);
        sensor->setCurrentRatio(1, EMON_CURRENT_RATIO);
        sensor->setCurrentRatio(2, EMON_CURRENT_RATIO);
        sensor->setCurrentRatio(3, EMON_CURRENT_RATIO);
        _sensors.push_back(sensor);
    }
    #endif

    #if EMON_ANALOG_SUPPORT
    {
        EmonAnalogSensor * sensor = new EmonAnalogSensor();
        sensor->setVoltage(EMON_MAINS_VOLTAGE);
        sensor->setReference(EMON_REFERENCE_VOLTAGE);
        sensor->setCurrentRatio(0, EMON_CURRENT_RATIO);
        _sensors.push_back(sensor);
    }
    #endif

    #if EVENTS_SUPPORT
    {
        EventSensor * sensor = new EventSensor();
        sensor->setGPIO(EVENTS_PIN);
        sensor->setMode(EVENTS_PIN_MODE);
        sensor->setDebounceTime(EVENTS_DEBOUNCE);
        sensor->setInterruptMode(EVENTS_INTERRUPT_MODE);
        _sensors.push_back(sensor);
    }
    #endif

    #if MHZ19_SUPPORT
    {
        MHZ19Sensor * sensor = new MHZ19Sensor();
        sensor->setRX(MHZ19_RX_PIN);
        sensor->setTX(MHZ19_TX_PIN);
        _sensors.push_back(sensor);
    }
    #endif

    #if PMSX003_SUPPORT
    {
        PMSX003Sensor * sensor = new PMSX003Sensor();
        sensor->setRX(PMS_RX_PIN);
        sensor->setTX(PMS_TX_PIN);
        _sensors.push_back(sensor);
    }
    #endif

    #if SHT3X_I2C_SUPPORT
    {
        SHT3XI2CSensor * sensor = new SHT3XI2CSensor();
        sensor->setAddress(SHT3X_I2C_ADDRESS);
        _sensors.push_back(sensor);
    }
    #endif

    #if SI7021_SUPPORT
    {
        SI7021Sensor * sensor = new SI7021Sensor();
        sensor->setAddress(SI7021_ADDRESS);
        _sensors.push_back(sensor);
    }
    #endif

    #if V9261F_SUPPORT
    {
        V9261FSensor * sensor = new V9261FSensor();
        sensor->setRX(V9261F_PIN);
        sensor->setInverted(V9261F_PIN_INVERSE);
        _sensors.push_back(sensor);
    }
    #endif

}

void _sensorConfigure() {

    for (unsigned char i=0; i<_sensors.size(); i++) {

        BaseSensor * sensor = _sensors[i];

        #if EMON_ANALOG_SUPPORT
            if (sensor->getID() == SENSOR_EMON_ANALOG_ID) {

                unsigned int expected = getSetting("pwrExpectedP", 0).toInt();
                if (expected > 0) {
                    ((EmonAnalogSensor *) sensor)->expectedPower(0, expected);
                    setSetting("pwrRatioC", ((EmonAnalogSensor *) sensor)->getCurrentRatio(0));
                }

                if (getSetting("pwrResetCalibration", 0).toInt() == 1) {
                    ((EmonAnalogSensor *) sensor)->setCurrentRatio(0, EMON_CURRENT_RATIO);
                    delSetting("pwrRatioC");
                }

                ((EmonAnalogSensor *) sensor)->setCurrentRatio(0, getSetting("pwrRatioC", EMON_CURRENT_RATIO).toFloat());
                ((EmonAnalogSensor *) sensor)->setVoltage(getSetting("pwrVoltage", EMON_MAINS_VOLTAGE).toInt());

                delSetting("pwrExpectedP");
                delSetting("pwrResetCalibration");

            }
        #endif

        // Force sensor to reload config
        sensor->begin();

    }

    // General sensor settings
    _sensor_realtime = getSetting("apiRealTime", API_REAL_TIME_VALUES).toInt() == 1;
    _sensor_temperature_units = getSetting("tmpUnits", SENSOR_TEMPERATURE_UNITS).toInt();
    _sensor_temperature_correction = getSetting("tmpCorrection", SENSOR_TEMPERATURE_CORRECTION).toFloat();

    // Save settings
    saveSettings();

}

void _magnitudesInit() {

    for (unsigned char i=0; i<_sensors.size(); i++) {

        BaseSensor * sensor = _sensors[i];

        DEBUG_MSG("[SENSOR] %s\n", sensor->description().c_str());
        if (sensor->error() != 0) DEBUG_MSG("[SENSOR]  -> ERROR %d\n", sensor->error());

        for (unsigned char k=0; k<sensor->count(); k++) {

            magnitude_t type = sensor->type(k);

            sensor_magnitude_t new_magnitude;
            new_magnitude.sensor = sensor;
            new_magnitude.local = k;
            new_magnitude.type = type;
            new_magnitude.global = _counts[type];
            new_magnitude.current = 0;
            new_magnitude.filtered = 0;
            new_magnitude.reported = 0;
            new_magnitude.min_change = 0;
            if (type == MAGNITUDE_DIGITAL) {
                new_magnitude.filter = new MaxFilter();
            } else if (type == MAGNITUDE_EVENTS) {
                new_magnitude.filter = new MovingAverageFilter();
            } else {
                new_magnitude.filter = new MedianFilter();
            }
            _magnitudes.push_back(new_magnitude);

            DEBUG_MSG("[SENSOR]  -> %s:%d\n", _sensorTopic(type).c_str(), _counts[type]);

            _counts[type] = _counts[type] + 1;

        }

    }

}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------

unsigned char sensorCount() {
    return _sensors.size();
}

unsigned char magnitudeCount() {
    return _magnitudes.size();
}

String magnitudeName(unsigned char index) {
    if (index < _magnitudes.size()) {
        sensor_magnitude_t magnitude = _magnitudes[index];
        return magnitude.sensor->slot(magnitude.local);
    }
    return String();
}

unsigned char magnitudeType(unsigned char index) {
    if (index < _magnitudes.size()) {
        return int(_magnitudes[index].type);
    }
    return MAGNITUDE_NONE;
}

// -----------------------------------------------------------------------------

void sensorSetup() {

    // Load sensors
    _sensorInit();

    // Configure stored values
    _sensorConfigure();

    // Load magnitudes
    _magnitudesInit();

    #if WEB_SUPPORT

        // Websockets
        wsOnSendRegister(_sensorWebSocketStart);
        wsOnSendRegister(_sensorWebSocketSendData);
        wsOnAfterParseRegister(_sensorConfigure);

        // API
        _sensorAPISetup();

    #endif

}

void sensorLoop() {

    static unsigned long last_update = 0;
    static unsigned long report_count = 0;

    // Tick hook
    _sensorTick();

    // Check if we should read new data
    if (millis() - last_update > SENSOR_READ_INTERVAL) {

        last_update = millis();
        report_count = (report_count + 1) % SENSOR_REPORT_EVERY;

        double current;
        double filtered;
        char buffer[64];

        // Pre-read hook
        _sensorPre();

        // Get readings
        for (unsigned char i=0; i<_magnitudes.size(); i++) {

            sensor_magnitude_t magnitude = _magnitudes[i];

            if (magnitude.sensor->status()) {

                unsigned char decimals = _sensorDecimals(magnitude.type);

                current = magnitude.sensor->value(magnitude.local);
                magnitude.filter->add(current);

                // Special case
                if (magnitude.type == MAGNITUDE_EVENTS) current = magnitude.filter->result();

                current = _sensorProcess(magnitude.type, current);
                _magnitudes[i].current = current;

                // Debug
                #if SENSOR_DEBUG
                {
                    dtostrf(current, 1-sizeof(buffer), decimals, buffer);
                    DEBUG_MSG("[SENSOR] %s - %s: %s%s\n",
                        magnitude.sensor->slot(magnitude.local).c_str(),
                        _sensorTopic(magnitude.type).c_str(),
                        buffer,
                        _sensorUnits(magnitude.type).c_str()
                    );
                }
                #endif

                // Time to report (we do it every SENSOR_REPORT_EVERY readings)
                if (report_count == 0) {

                    filtered = magnitude.filter->result();
                    magnitude.filter->reset();
                    filtered = _sensorProcess(magnitude.type, filtered);
                    _magnitudes[i].filtered = filtered;

                    // Check if there is a minimum change threshold to report
                    if (fabs(filtered - magnitude.reported) >= magnitude.min_change) {

                        _magnitudes[i].reported = filtered;
                        dtostrf(filtered, 1-sizeof(buffer), decimals, buffer);

                        #if MQTT_SUPPORT
                            if (SENSOR_USE_INDEX || (_counts[magnitude.type] > 1)) {
                                mqttSend(_sensorTopic(magnitude.type).c_str(), magnitude.global, buffer);
                            } else {
                                mqttSend(_sensorTopic(magnitude.type).c_str(), buffer);
                            }
                        #endif

                        #if INFLUXDB_SUPPORT
                            if (SENSOR_USE_INDEX || (_counts[magnitude.type] > 1)) {
                                idbSend(_sensorTopic(magnitude.type).c_str(), magnitude.global, buffer);
                            } else {
                                idbSend(_sensorTopic(magnitude.type).c_str(), buffer);
                            }
                        #endif

                        #if DOMOTICZ_SUPPORT
                        {
                            char key[15];
                            snprintf_P(key, sizeof(key), PSTR("dczSensor%d"), i);
                            if (magnitude.type == MAGNITUDE_HUMIDITY) {
                                int status;
                                if (filtered > 70) {
                                    status = HUMIDITY_WET;
                                } else if (filtered > 45) {
                                    status = HUMIDITY_COMFORTABLE;
                                } else if (filtered > 30) {
                                    status = HUMIDITY_NORMAL;
                                } else {
                                    status = HUMIDITY_DRY;
                                }
                                char status_buf[5];
                                itoa(status, status_buf, 10);
                                domoticzSend(key, buffer, status_buf);
                            } else {
                                domoticzSend(key, 0, buffer);
                            }
                        }
                        #endif

                    } // if (fabs(filtered - magnitude.reported) >= magnitude.min_change)
                } // if (report_count == 0)
            } // if (magnitude.sensor->status())
        } // for (unsigned char i=0; i<_magnitudes.size(); i++)

        // Post-read hook
        _sensorPost();

        #if WEB_SUPPORT
            wsSend(_sensorWebSocketSendData);
        #endif

    }

}
