#include <unity.h>

#include "records.h"

void setUp(void) {}
void tearDown(void) {}

// ---- bytesToHex (used by ble_scan to encode the raw adv payload) ----

void test_bytesToHex_empty() {
    TEST_ASSERT_EQUAL_STRING("", bt::bytesToHex(nullptr, 0).c_str());
}

void test_bytesToHex_single_byte() {
    uint8_t data[] = {0x0A};
    TEST_ASSERT_EQUAL_STRING("0a", bt::bytesToHex(data, 1).c_str());
}

void test_bytesToHex_multiple_bytes() {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL_STRING("deadbeef", bt::bytesToHex(data, 4).c_str());
}

// ---- ApRecord formatting (wifi_scan output) ----

void test_csvHeaderAp() {
    TEST_ASSERT_EQUAL_STRING("ssid,bssid,channel,rssi,encryption", bt::csvHeaderAp().c_str());
}

void test_toCsv_ApRecord() {
    bt::ApRecord r;
    r.ssid = "MyNetwork";
    r.bssid = "AA:BB:CC:DD:EE:FF";
    r.channel = 6;
    r.rssi = -42;
    r.encryption = "WPA2_PSK";

    TEST_ASSERT_EQUAL_STRING("MyNetwork,AA:BB:CC:DD:EE:FF,6,-42,WPA2_PSK", bt::toCsv(r).c_str());
}

void test_toJson_ApRecord() {
    bt::ApRecord r;
    r.ssid = "MyNetwork";
    r.bssid = "AA:BB:CC:DD:EE:FF";
    r.channel = 6;
    r.rssi = -42;
    r.encryption = "WPA2_PSK";

    TEST_ASSERT_EQUAL_STRING(
        "{\"ssid\":\"MyNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"channel\":6,\"rssi\":-42,\"encryption\":\"WPA2_PSK\"}",
        bt::toJson(r).c_str());
}

void test_toJson_ApRecord_escapes_special_chars() {
    bt::ApRecord r;
    r.ssid = "quote\"and\\backslash";
    r.bssid = "AA:BB:CC:DD:EE:FF";
    r.channel = 1;
    r.rssi = 0;
    r.encryption = "OPEN";

    TEST_ASSERT_EQUAL_STRING(
        "{\"ssid\":\"quote\\\"and\\\\backslash\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"channel\":1,\"rssi\":0,\"encryption\":\"OPEN\"}",
        bt::toJson(r).c_str());
}

// ---- BleRecord formatting (ble_scan output) ----

void test_csvHeaderBle() {
    TEST_ASSERT_EQUAL_STRING("address,name,rssi,adv", bt::csvHeaderBle().c_str());
}

void test_toCsv_BleRecord() {
    bt::BleRecord r;
    r.address = "11:22:33:44:55:66";
    r.name = "MyBeacon";
    r.rssi = -70;
    r.adv = "deadbeef";

    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66,MyBeacon,-70,deadbeef", bt::toCsv(r).c_str());
}

void test_toJson_BleRecord() {
    bt::BleRecord r;
    r.address = "11:22:33:44:55:66";
    r.name = "MyBeacon";
    r.rssi = -70;
    r.adv = "deadbeef";

    TEST_ASSERT_EQUAL_STRING(
        "{\"address\":\"11:22:33:44:55:66\",\"name\":\"MyBeacon\",\"rssi\":-70,\"adv\":\"deadbeef\"}",
        bt::toJson(r).c_str());
}

void test_toJson_BleRecord_empty_name() {
    bt::BleRecord r;
    r.address = "11:22:33:44:55:66";
    r.name = "";
    r.rssi = -70;
    r.adv = "";

    TEST_ASSERT_EQUAL_STRING(
        "{\"address\":\"11:22:33:44:55:66\",\"name\":\"\",\"rssi\":-70,\"adv\":\"\"}",
        bt::toJson(r).c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_bytesToHex_empty);
    RUN_TEST(test_bytesToHex_single_byte);
    RUN_TEST(test_bytesToHex_multiple_bytes);

    RUN_TEST(test_csvHeaderAp);
    RUN_TEST(test_toCsv_ApRecord);
    RUN_TEST(test_toJson_ApRecord);
    RUN_TEST(test_toJson_ApRecord_escapes_special_chars);

    RUN_TEST(test_csvHeaderBle);
    RUN_TEST(test_toCsv_BleRecord);
    RUN_TEST(test_toJson_BleRecord);
    RUN_TEST(test_toJson_BleRecord_empty_name);

    return UNITY_END();
}
