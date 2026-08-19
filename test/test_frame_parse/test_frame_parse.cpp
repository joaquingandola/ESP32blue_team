#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "frame_parse.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

// Frame-control first byte for the subtypes we parse. Bits: subtype<<4 | type<<2.
constexpr uint8_t kFcBeacon    = 0x80;  // type 0 (mgmt), subtype 8
constexpr uint8_t kFcProbeReq  = 0x40;  // type 0 (mgmt), subtype 4
constexpr uint8_t kFcProbeResp = 0x50;  // type 0 (mgmt), subtype 5 (ignored)
constexpr uint8_t kFcData      = 0x08;  // type 2 (data)  (ignored)

// Builds the 24-byte management header with recognizable addresses:
// addr2 (src)   = 11:22:33:44:55:66
// addr3 (bssid) = AA:BB:CC:DD:EE:FF
std::vector<uint8_t> macHeader(uint8_t fc0) {
    std::vector<uint8_t> h;
    h.push_back(fc0);
    h.push_back(0x00);              // fc byte 1 (flags)
    h.insert(h.end(), {0x00, 0x00});  // duration
    h.insert(h.end(), 6, 0xFF);       // addr1: broadcast
    h.insert(h.end(), {0x11, 0x22, 0x33, 0x44, 0x55, 0x66});  // addr2 (src)
    h.insert(h.end(), {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF});  // addr3 (bssid)
    h.insert(h.end(), {0x00, 0x00});  // sequence control
    return h;
}

// Appends a tagged element (id + length + payload).
void addTag(std::vector<uint8_t>& v, uint8_t tag, const std::vector<uint8_t>& val) {
    v.push_back(tag);
    v.push_back(static_cast<uint8_t>(val.size()));
    v.insert(v.end(), val.begin(), val.end());
}

std::vector<uint8_t> ssidBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Packs raw bytes into a SniffFrame with the given radio metadata.
bt::SniffFrame makeFrame(const std::vector<uint8_t>& bytes, uint8_t channel = 6,
                         int8_t rssi = -42) {
    bt::SniffFrame f;
    memset(f.payload, 0, sizeof(f.payload));
    f.channel = channel;
    f.rssi    = rssi;
    const size_t n = bytes.size() > bt::kSniffFrameMaxLen ? bt::kSniffFrameMaxLen
                                                          : bytes.size();
    memcpy(f.payload, bytes.data(), n);
    f.len = static_cast<uint16_t>(bytes.size());
    return f;
}

// A complete beacon: header + 12 fixed bytes + SSID element.
bt::SniffFrame beaconWithSsid(const std::string& ssid) {
    auto v = macHeader(kFcBeacon);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);  // timestamp/interval/caps
    addTag(v, bt::kTagSsid, ssidBytes(ssid));
    return makeFrame(v);
}

// A complete probe request: header + SSID element (no fixed params).
bt::SniffFrame probeReqWithSsid(const std::string& ssid) {
    auto v = macHeader(kFcProbeReq);
    addTag(v, bt::kTagSsid, ssidBytes(ssid));
    return makeFrame(v);
}

}  // namespace

// ---- Beacons ----

void test_beacon_parses_all_fields() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(beaconWithSsid("MyNetwork"), r));

    TEST_ASSERT_EQUAL_STRING("BEACON", r.type.c_str());
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", r.src.c_str());
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", r.bssid.c_str());
    TEST_ASSERT_EQUAL_STRING("MyNetwork", r.ssid.c_str());
    TEST_ASSERT_EQUAL_UINT8(6, r.channel);
    TEST_ASSERT_EQUAL_INT32(-42, r.rssi);
}

// The SSID element is not guaranteed to come first; the parser must walk tags.
void test_beacon_ssid_not_first_tag() {
    auto v = macHeader(kFcBeacon);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);
    addTag(v, 0x01, {0x82, 0x84, 0x8B});  // supported rates, before the SSID
    addTag(v, bt::kTagSsid, ssidBytes("LateSsid"));

    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(makeFrame(v), r));
    TEST_ASSERT_EQUAL_STRING("LateSsid", r.ssid.c_str());
}

// Hidden networks advertise a zero-length SSID element.
void test_beacon_hidden_ssid_zero_length() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(beaconWithSsid(""), r));
    TEST_ASSERT_EQUAL_STRING("BEACON", r.type.c_str());
    TEST_ASSERT_TRUE(r.ssid.empty());
}

// ...others pad it with NUL bytes; those must not end up inside the string.
void test_beacon_hidden_ssid_all_nul_bytes() {
    auto v = macHeader(kFcBeacon);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);
    addTag(v, bt::kTagSsid, std::vector<uint8_t>(6, 0x00));

    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(makeFrame(v), r));
    TEST_ASSERT_TRUE(r.ssid.empty());
    TEST_ASSERT_EQUAL_UINT32(0, r.ssid.size());
}

void test_beacon_ssid_clamped_to_32_bytes() {
    auto v = macHeader(kFcBeacon);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);
    // Craft an over-long SSID element (spec caps it at 32).
    addTag(v, bt::kTagSsid, std::vector<uint8_t>(40, 'A'));

    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(makeFrame(v), r));
    TEST_ASSERT_EQUAL_UINT32(bt::kSsidMaxLen, r.ssid.size());
}

// ---- Probe requests (the phase-2 success metric) ----

void test_probe_request_parses_ssid() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(probeReqWithSsid("HomeWiFi"), r));

    TEST_ASSERT_EQUAL_STRING("PROBE_REQ", r.type.c_str());
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", r.src.c_str());
    TEST_ASSERT_EQUAL_STRING("HomeWiFi", r.ssid.c_str());
}

// A broadcast probe ("who's out there?") carries an empty SSID element.
void test_probe_request_broadcast_has_empty_ssid() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(probeReqWithSsid(""), r));
    TEST_ASSERT_EQUAL_STRING("PROBE_REQ", r.type.c_str());
    TEST_ASSERT_TRUE(r.ssid.empty());
}

// A probe request has no fixed params: tags start right after the header. If the
// parser used the beacon offset it would read garbage instead of the SSID.
void test_probe_request_does_not_skip_beacon_fixed_params() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(probeReqWithSsid("Offset"), r));
    TEST_ASSERT_EQUAL_STRING("Offset", r.ssid.c_str());
}

// ---- Frames we must ignore ----

void test_data_frame_rejected() {
    bt::SniffRecord r;
    TEST_ASSERT_FALSE(bt::parseFrame(makeFrame(macHeader(kFcData)), r));
}

void test_other_mgmt_subtype_rejected() {
    auto v = macHeader(kFcProbeResp);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);
    addTag(v, bt::kTagSsid, ssidBytes("NotParsed"));

    bt::SniffRecord r;
    TEST_ASSERT_FALSE(bt::parseFrame(makeFrame(v), r));
}

// ---- Malformed / truncated input (must not read out of bounds) ----

void test_frame_shorter_than_mac_header_rejected() {
    auto v = macHeader(kFcBeacon);
    v.resize(10);  // truncated mid-header

    bt::SniffRecord r;
    TEST_ASSERT_FALSE(bt::parseFrame(makeFrame(v), r));
}

// Header-only beacon: valid frame, nothing to read for the SSID.
void test_beacon_without_tags_yields_empty_ssid() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(makeFrame(macHeader(kFcBeacon)), r));
    TEST_ASSERT_EQUAL_STRING("BEACON", r.type.c_str());
    TEST_ASSERT_TRUE(r.ssid.empty());
}

// An element whose declared length runs past the captured bytes must be
// dropped, not read past the buffer.
void test_truncated_ssid_element_is_ignored() {
    auto v = macHeader(kFcBeacon);
    v.insert(v.end(), bt::kBeaconFixedLen, 0x00);
    v.push_back(bt::kTagSsid);
    v.push_back(20);                       // claims 20 bytes...
    const auto partial = ssidBytes("only4");  // ...but only 5 are present
    v.insert(v.end(), partial.begin(), partial.end());

    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(makeFrame(v), r));
    TEST_ASSERT_TRUE(r.ssid.empty());
}

// A len larger than the payload buffer must be clamped, never trusted.
void test_len_larger_than_buffer_is_clamped() {
    bt::SniffFrame f = beaconWithSsid("Clamped");
    f.len = 0xFFFF;  // lying length

    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(f, r));
    TEST_ASSERT_EQUAL_STRING("Clamped", r.ssid.c_str());
}

// ---- Record formatting round-trip (what the logger will emit) ----

void test_sniff_record_csv_and_json() {
    bt::SniffRecord r;
    TEST_ASSERT_TRUE(bt::parseFrame(probeReqWithSsid("HomeWiFi"), r));

    TEST_ASSERT_EQUAL_STRING("type,src,bssid,ssid,channel,rssi",
                             bt::csvHeaderSniff().c_str());
    TEST_ASSERT_EQUAL_STRING(
        "PROBE_REQ,11:22:33:44:55:66,AA:BB:CC:DD:EE:FF,HomeWiFi,6,-42",
        bt::toCsv(r).c_str());
    TEST_ASSERT_EQUAL_STRING(
        "{\"type\":\"PROBE_REQ\",\"src\":\"11:22:33:44:55:66\","
        "\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"ssid\":\"HomeWiFi\","
        "\"channel\":6,\"rssi\":-42}",
        bt::toJson(r).c_str());
}

void test_macToString_formats_uppercase_colons() {
    const uint8_t mac[6] = {0x0A, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F};
    TEST_ASSERT_EQUAL_STRING("0A:1B:2C:3D:4E:5F", bt::macToString(mac).c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_beacon_parses_all_fields);
    RUN_TEST(test_beacon_ssid_not_first_tag);
    RUN_TEST(test_beacon_hidden_ssid_zero_length);
    RUN_TEST(test_beacon_hidden_ssid_all_nul_bytes);
    RUN_TEST(test_beacon_ssid_clamped_to_32_bytes);

    RUN_TEST(test_probe_request_parses_ssid);
    RUN_TEST(test_probe_request_broadcast_has_empty_ssid);
    RUN_TEST(test_probe_request_does_not_skip_beacon_fixed_params);

    RUN_TEST(test_data_frame_rejected);
    RUN_TEST(test_other_mgmt_subtype_rejected);

    RUN_TEST(test_frame_shorter_than_mac_header_rejected);
    RUN_TEST(test_beacon_without_tags_yields_empty_ssid);
    RUN_TEST(test_truncated_ssid_element_is_ignored);
    RUN_TEST(test_len_larger_than_buffer_is_clamped);

    RUN_TEST(test_sniff_record_csv_and_json);
    RUN_TEST(test_macToString_formats_uppercase_colons);

    return UNITY_END();
}
