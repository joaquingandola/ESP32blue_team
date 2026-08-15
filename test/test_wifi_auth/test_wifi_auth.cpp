#include <unity.h>

#include "wifi_auth.h"

void setUp(void) {}
void tearDown(void) {}

void test_wifiAuthModeToString_open() {
    TEST_ASSERT_EQUAL_STRING("OPEN", bt::wifiAuthModeToString(bt::kWifiAuthOpen).c_str());
}

void test_wifiAuthModeToString_wep() {
    TEST_ASSERT_EQUAL_STRING("WEP", bt::wifiAuthModeToString(bt::kWifiAuthWep).c_str());
}

void test_wifiAuthModeToString_wpa_psk() {
    TEST_ASSERT_EQUAL_STRING("WPA_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWpaPsk).c_str());
}

void test_wifiAuthModeToString_wpa2_psk() {
    TEST_ASSERT_EQUAL_STRING("WPA2_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWpa2Psk).c_str());
}

void test_wifiAuthModeToString_wpa_wpa2_psk() {
    TEST_ASSERT_EQUAL_STRING("WPA_WPA2_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWpaWpa2Psk).c_str());
}

void test_wifiAuthModeToString_wpa2_enterprise() {
    TEST_ASSERT_EQUAL_STRING("WPA2_ENTERPRISE", bt::wifiAuthModeToString(bt::kWifiAuthWpa2Enterprise).c_str());
}

void test_wifiAuthModeToString_wpa3_psk() {
    TEST_ASSERT_EQUAL_STRING("WPA3_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWpa3Psk).c_str());
}

void test_wifiAuthModeToString_wpa2_wpa3_psk() {
    TEST_ASSERT_EQUAL_STRING("WPA2_WPA3_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWpa2Wpa3Psk).c_str());
}

void test_wifiAuthModeToString_wapi_psk() {
    TEST_ASSERT_EQUAL_STRING("WAPI_PSK", bt::wifiAuthModeToString(bt::kWifiAuthWapiPsk).c_str());
}

void test_wifiAuthModeToString_unknown_value() {
    // WIFI_AUTH_WPA3_ENT_192 / WIFI_AUTH_MAX and beyond aren't mapped by this
    // project (see wifi_scan.cpp) and should fall back to "UNKNOWN".
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", bt::wifiAuthModeToString(9).c_str());
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", bt::wifiAuthModeToString(-1).c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_wifiAuthModeToString_open);
    RUN_TEST(test_wifiAuthModeToString_wep);
    RUN_TEST(test_wifiAuthModeToString_wpa_psk);
    RUN_TEST(test_wifiAuthModeToString_wpa2_psk);
    RUN_TEST(test_wifiAuthModeToString_wpa_wpa2_psk);
    RUN_TEST(test_wifiAuthModeToString_wpa2_enterprise);
    RUN_TEST(test_wifiAuthModeToString_wpa3_psk);
    RUN_TEST(test_wifiAuthModeToString_wpa2_wpa3_psk);
    RUN_TEST(test_wifiAuthModeToString_wapi_psk);
    RUN_TEST(test_wifiAuthModeToString_unknown_value);

    return UNITY_END();
}
