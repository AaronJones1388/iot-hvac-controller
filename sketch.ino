#include <Arduino.h>
#include "DHT.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include <LiquidCrystal_I2C.h>

#define DHTPIN          4
#define DHTTYPE         DHT22

#define BUTTON_PIN      15
#define RELAY_PIN       23
#define BUZZER_PIN      19
#define LED_PIN         16
#define POT_PIN         34

#define DEBOUNCE_MS     200
#define ANOMALY_JUMP    15.0f
#define TEMP_MIN        -40.0f
#define TEMP_MAX        100.0f
#define SETPOINT_MAX    50.0f
#define OVERHEAT_THRESH 35.0f
#define FILTER_ALPHA    0.2f
#define WDT_TIMEOUT_S   5

typedef struct {
    float temperature;
    float hvacSetpoint;
    int   potValue;
    bool  emergencyStop;
} SystemState;

DHT dht(DHTPIN, DHTTYPE);
SystemState sysState;
SemaphoreHandle_t mutex;
LiquidCrystal_I2C lcd(0x27, 16, 2);
TaskHandle_t sensorHandle = NULL;
TaskHandle_t controlHandle = NULL;
TaskHandle_t logHandle = NULL;

volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR buttonISR() {
    unsigned long now = esp_timer_get_time() / 1000;
    if (now - lastInterruptTime > DEBOUNCE_MS) {
        sysState.emergencyStop = true;
        lastInterruptTime = now;
    }
}

float lastTemp = NAN;
float filteredTemp = 0.0f;

void sensorTask(void *pvParameters) {
    SystemState *state = (SystemState *)pvParameters;

    while (1) {
        esp_task_wdt_reset();

        float temp = dht.readTemperature();
        int adc = analogRead(POT_PIN);

        bool anomaly = false;
        if (isnan(temp) || temp < TEMP_MIN || temp > TEMP_MAX) {
            anomaly = true;
        } else if (!isnan(lastTemp) && fabs(temp - lastTemp) > ANOMALY_JUMP) {
            anomaly = true;
        }

        if (anomaly) {
            xSemaphoreTake(mutex, portMAX_DELAY);
            state->emergencyStop = true;
            xSemaphoreGive(mutex);
            Serial.println("Sensor anomaly detected! Emergency stop triggered.");
        } else {
            lastTemp = temp;

            filteredTemp = (1.0f - FILTER_ALPHA) * filteredTemp + FILTER_ALPHA * temp;

            float newSetpoint = (adc / 4095.0f) * SETPOINT_MAX;

            xSemaphoreTake(mutex, portMAX_DELAY);
            state->temperature = filteredTemp;
            state->hvacSetpoint = newSetpoint;
            state->potValue = adc;
            xSemaphoreGive(mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void controlTask(void *pvParameters) {
    SystemState *state = (SystemState *)pvParameters;
    static bool lastEStop = false;

    while (1) {
        esp_task_wdt_reset();

        xSemaphoreTake(mutex, portMAX_DELAY);
        float temp = state->temperature;
        float setpoint = state->hvacSetpoint;
        bool estop = state->emergencyStop;
        xSemaphoreGive(mutex);

        if (estop && !lastEStop) {
            Serial.println("EMERGENCY STOP ACTIVATED");
        }
        lastEStop = estop;

        if (estop) {
            digitalWrite(RELAY_PIN, LOW);
            digitalWrite(BUZZER_PIN, HIGH);
            digitalWrite(LED_PIN, HIGH);
        } else {
            if (temp >= OVERHEAT_THRESH) {
                digitalWrite(LED_PIN, HIGH);
                digitalWrite(BUZZER_PIN, HIGH);
            } else {
                digitalWrite(LED_PIN, LOW);
                digitalWrite(BUZZER_PIN, LOW);
            }

            digitalWrite(RELAY_PIN, temp < setpoint ? HIGH : LOW);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void logTask(void *pvParameters) {
    SystemState *state = (SystemState *)pvParameters;

    while (1) {
        esp_task_wdt_reset();

        xSemaphoreTake(mutex, portMAX_DELAY);
        float temp = state->temperature;
        float setpoint = state->hvacSetpoint;
        int pot = state->potValue;
        bool estop = state->emergencyStop;
        xSemaphoreGive(mutex);

        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temp, 1);
        lcd.print((char)223);
        lcd.print("C  ");

        lcd.setCursor(0, 1);
        if (estop) {
            lcd.print("E-STOP ACTIVE   ");
        } else if (temp >= OVERHEAT_THRESH) {
            lcd.print("OVERHEAT!       ");
        } else {
            lcd.print("Set: ");
            lcd.print(setpoint, 1);
            lcd.print((char)223);
            lcd.print("C  ");
        }

        Serial.print("Temp:");
        Serial.print(temp, 1);
        Serial.print(" Setpoint:");
        Serial.print(setpoint, 1);
        Serial.print(" E-Stop:");
        Serial.println(estop ? "1" : "0");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup() {
    Serial.begin(115200);

    analogReadResolution(12);

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    dht.begin();

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("HVAC Controller");
    lcd.setCursor(0, 1);
    lcd.print("Initialising...");

    mutex = xSemaphoreCreateMutex();

    float initTemp = dht.readTemperature();
    if (!isnan(initTemp)) {
        filteredTemp = initTemp;
    }

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);

    xTaskCreate(sensorTask, "Sensor", 2048, &sysState, 2, &sensorHandle);
    xTaskCreate(controlTask, "Control", 2048, &sysState, 3, &controlHandle);
    xTaskCreate(logTask, "Log", 2048, &sysState, 1, &logHandle);

    esp_task_wdt_add(sensorHandle);
    esp_task_wdt_add(controlHandle);
    esp_task_wdt_add(logHandle);

    Serial.println("=== Industrial IoT Smart HVAC Controller ===");
    Serial.println("System initialised. Monitoring...");
}

void loop() {
}