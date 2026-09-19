/******************************************
 * CCSDS Temperature and Humidity Collector
 * Author: Cody Carter
 * Date: May 2026
 * Version: 1.2.0
 * 
 * Fixed big-endian byte ordering for CCSDS/cFS compatibility
 ******************************************/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdint.h>
#include "DHT.h"

uint16_t sequenceCount = 0;

#define version_Shift               13
#define packet_Type_Shift           12
#define secondary_Header_Flag_Shift 11
#define process_ID_Mask             0x07FF

#define sequence_Flag_Shift         14
#define sequence_CountName_Mask     0x3FFF

#define DHT11_PIN 21
DHT dht11(DHT11_PIN, DHT11);

QueueHandle_t TempQueue;
QueueHandle_t HumidQueue;

struct Primary_Header {
    uint16_t packet_ID;
    uint16_t sequence_Control;
    uint16_t data_Length;
} __attribute__((packed));

struct Secondary_Header {
    uint16_t seconds;
    uint16_t subseconds;
    int16_t  temperature_C;
    uint16_t humidity;
} __attribute__((packed));

struct Telemetry_Packet {
    Primary_Header primary;
    Secondary_Header secondary;
} __attribute__((packed));

static inline uint16_t build_PacketID(
    uint8_t version,
    uint8_t packet_Type,
    uint8_t secondary_Header_Flag,
    uint16_t process_ID
) {
    return ((version & 0x7) << version_Shift) |
           ((packet_Type & 0x1) << packet_Type_Shift) |
           ((secondary_Header_Flag & 0x1) << secondary_Header_Flag_Shift) |
           (process_ID & process_ID_Mask);
}

static inline uint16_t build_Sequence_Control(
    uint8_t sequence_Flags,
    uint16_t sequence_CountName
) {
    return ((sequence_Flags & 0x3) << sequence_Flag_Shift) |
           (sequence_CountName & sequence_CountName_Mask);
}

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite valueSprite = TFT_eSprite(&tft); // off-screen buffer, avoids flicker on redraw

void drawDashboardFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("cFS Telemetry", tft.width() / 2, 6, 2);
    tft.drawFastHLine(0, 24, tft.width(), TFT_DARKGREY);

    tft.setTextDatum(ML_DATUM);
    tft.drawString("TEMP", 10, 45, 2);
    tft.drawString("HUM",  10, 100, 2);
    tft.drawFastHLine(0, 78, tft.width(), TFT_DARKGREY);
}

void updateData(float temperature, float humidity) {
    valueSprite.fillSprite(TFT_BLACK);
    valueSprite.setTextColor(TFT_GREEN, TFT_BLACK);
    valueSprite.drawString(String(temperature, 1) + " C", 0, 15, 4);
    valueSprite.pushSprite(50, 32);

    valueSprite.fillSprite(TFT_BLACK);
    valueSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    valueSprite.drawString(String(humidity, 1) + " %", 0, 15, 4);
    valueSprite.pushSprite(50, 87);
}

void readDHT11(void *parameter) {
    for (;;) {
        float temp = dht11.readTemperature();
        float humid = dht11.readHumidity();
        xQueueOverwrite(TempQueue, &temp);
        xQueueOverwrite(HumidQueue, &humid);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void updateGUI(void *parameter) {
    float temp, humid;
    for (;;) {
        xQueuePeek(TempQueue, &temp, 0);
        xQueuePeek(HumidQueue, &humid, 0);
        updateData(temp, humid);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

const uint8_t SYNC_BYTES[2] = {0xAA, 0x55};

static inline uint16_t hton16(uint16_t v) {
    return ((v >> 8) & 0xFF) | ((v & 0xFF) << 8);
}

void createPacketAndSend(void *parameter) {
    for (;;) {
        Telemetry_Packet packet{};

        uint16_t pid = build_PacketID(0, 0, 1, 0x096);
        packet.primary.packet_ID = hton16(pid);

        uint16_t seq = build_Sequence_Control(3, sequenceCount++);
        packet.primary.sequence_Control = hton16(seq);

        uint16_t dlen = sizeof(Secondary_Header) - 1;
        packet.primary.data_Length = hton16(dlen);

        float temp = 0, humid = 0;
        xQueuePeek(TempQueue, &temp, 0);
        xQueuePeek(HumidQueue, &humid, 0);

        packet.secondary.seconds       = hton16(0);
        packet.secondary.subseconds    = hton16(0);
        packet.secondary.temperature_C = hton16((uint16_t)(int16_t)(temp * 100));
        packet.secondary.humidity      = hton16((uint16_t)(humid * 100));

        uint16_t len = sizeof(Telemetry_Packet); // 14 bytes: 6-byte primary + 8-byte secondary

        Serial.write(SYNC_BYTES, sizeof(SYNC_BYTES)); // marker — leave native, bridge scans for it
        Serial.write((uint8_t*)&len, sizeof(len));     // length prefix — leave native little-endian
        Serial.write((uint8_t*)&packet, len);          // packet body — now big-endian throughout

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    dht11.begin();
    tft.init();
    tft.setRotation(0); // adjust 0-3 depending on how the display sits in your enclosure

    valueSprite.createSprite(100, 30);
    valueSprite.setTextDatum(ML_DATUM);

    drawDashboardFrame();

    TempQueue  = xQueueCreate(1, sizeof(float));
    HumidQueue = xQueueCreate(1, sizeof(float));
    xTaskCreatePinnedToCore(readDHT11, "Read DHT11", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(updateGUI, "Update GUI", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(createPacketAndSend, "Create Packet and Send", 4096, NULL, 1, NULL, 1);
}

void loop() {
    // Nothing yet — sensor/data source to be wired back in later
}