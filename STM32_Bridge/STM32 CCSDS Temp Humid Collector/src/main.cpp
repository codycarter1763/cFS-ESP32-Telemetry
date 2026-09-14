/******************************************
 * CCSDS Temperature and Humidity Collector
 * Author: Cody Carter
 * Date: May 2026
 * Version: 1.2.0
 * 
 * Fixed big-endian byte ordering for CCSDS/cFS compatibility
 ******************************************/

#include <Arduino.h>
#include <stdint.h>
#include "DHT.h"

uint16_t sequenceCount = 0;

#define version_Shift               13
#define packet_Type_Shift           12
#define secondary_Header_Flag_Shift 11
#define process_ID_Mask             0x07FF

#define sequence_Flag_Shift         14
#define sequence_CountName_Mask     0x3FFF

#define DHT11_PIN PB0
DHT dht11(DHT11_PIN, DHT11);

struct Primary_Header {
    uint16_t packet_ID;
    uint16_t sequence_Control;
    uint16_t data_Length;
} __attribute__((packed));

struct CFS_Secondary_Header {
    uint16_t seconds;
    uint16_t subseconds;
} __attribute__((packed));

struct Sensor_Payload {
    int16_t  temperature_C;
    uint16_t humidity;
} __attribute__((packed));

struct Telemetry_Packet {
    Primary_Header       primary;
    CFS_Secondary_Header cfs_secondary;
    Sensor_Payload       payload;
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

void sendPacket(const Telemetry_Packet& packet) {
    uint16_t len = sizeof(Telemetry_Packet);
    Serial.write((uint8_t*)&len, sizeof(len));
    Serial.write((uint8_t*)&packet, len);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    sequenceCount = 0;
    dht11.begin();
}

void loop() {
    Telemetry_Packet packet{};

    uint16_t pid = build_PacketID(0, 0, 1, 0x096);
    packet.primary.packet_ID = ((pid >> 8) & 0xFF) | ((pid & 0xFF) << 8);

    uint16_t seq = build_Sequence_Control(3, sequenceCount++);
    packet.primary.sequence_Control = ((seq >> 8) & 0xFF) | ((seq & 0xFF) << 8);

    uint16_t dlen = sizeof(CFS_Secondary_Header) + sizeof(Sensor_Payload) - 1;
    packet.primary.data_Length = ((dlen >> 8) & 0xFF) | ((dlen & 0xFF) << 8);

    packet.cfs_secondary.seconds    = 0;
    packet.cfs_secondary.subseconds = 0;

    float temperature = dht11.readTemperature();
    float humidity    = dht11.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        int16_t  t = -9999;
        uint16_t h = 0;
        packet.payload.temperature_C = ((t >> 8) & 0xFF) | ((t & 0xFF) << 8);
        packet.payload.humidity      = ((h >> 8) & 0xFF) | ((h & 0xFF) << 8);
    } else {
        int16_t  t = (int16_t)(temperature * 100);
        uint16_t h = (uint16_t)(humidity * 100);
        packet.payload.temperature_C = ((t >> 8) & 0xFF) | ((t & 0xFF) << 8);
        packet.payload.humidity      = ((h >> 8) & 0xFF) | ((h & 0xFF) << 8);
    }

    sendPacket(packet);
    delay(2000);
}