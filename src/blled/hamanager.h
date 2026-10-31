#ifndef _HAMANAGER
#define _HAMANAGER

// ---------------------------------------------------------------------------
// Home Assistant integration.
//
// Opens a SECOND MQTT connection (separate from the printer connection in
// mqttmanager.h) to the user's Home Assistant MQTT broker (Mosquitto) and
// exposes the LED strip to Home Assistant using MQTT Discovery:
//
//   * light  -> RGB + brightness light entity
//   * select -> operating mode (Printer / HomeAssistant / Hybrid)
//   * switch -> master enable (force the strip fully off)
//
// The printer connection (TLS, port 8883) is untouched; this connection to Home
// Assistant is plain MQTT (default port 1883), or TLS (usually port 8883) when
// haMqttTls is set, so the broker username/password aren't sent in the clear.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "types.h"
#include "leds.h"
#include "filesystem.h"

WiFiClient haWifiClient;
WiFiClientSecure *haSecureClient = nullptr; // created in setupHa() only when haMqttTls is set
PubSubClient haMqtt(haWifiClient);

// setCACert() keeps only a pointer, so the certificate in use is a copy taken at setup that
// never changes while running (changing it in the web UI reboots the controller).
static String haActiveCaCert;

static String haNodeId;       // unique id, e.g. blled_a1b2c3d4e5f6
static String haDeviceName;   // friendly device name (= mDNS host)

// Topics
static String haAvailTopic;       // availability (LWT)
static String haLightCmdTopic;    // light command  (JSON)
static String haLightStateTopic;  // light state    (JSON)
static String haModeCmdTopic;     // mode command   (string)
static String haModeStateTopic;   // mode state     (string)
static String haEnableCmdTopic;   // enable command (ON/OFF)
static String haEnableStateTopic; // enable state   (ON/OFF)

static const unsigned long HA_RECONNECT_INTERVAL = 5000;
static unsigned long haLastSaveRequestMs = 0;
static bool haSavePending = false;

// Diagnostic counter — read by wifiMonitorTask in main.cpp
int g_haConnectAttempts = 0;

// Defined in leds.h
void updateleds();

// --- mode <-> string helpers ------------------------------------------------
const char *haModeToString(int mode)
{
    switch (mode)
    {
    case LED_MODE_PRINTER:
        return "Printer";
    case LED_MODE_HA:
        return "HomeAssistant";
    default:
        return "Hybrid";
    }
}

int haStringToMode(const String &s)
{
    if (s == "Printer")
        return LED_MODE_PRINTER;
    if (s == "HomeAssistant")
        return LED_MODE_HA;
    return LED_MODE_HYBRID;
}

// Request a (debounced) config save so HA-driven changes survive a reboot
// without hammering the flash on every brightness step.
static void haRequestSave()
{
    haSavePending = true;
    haLastSaveRequestMs = millis();
}

// --- publishing -------------------------------------------------------------
void haPublishAvailability(bool online)
{
    if (!haMqtt.connected())
        return;
    haMqtt.publish(haAvailTopic.c_str(), online ? "online" : "offline", true);
}

void haPublishState()
{
    if (!haMqtt.connected())
        return;

    // Light state (JSON schema) - reflects the ACTUAL strip output (printer- or
    // HA-driven) so Home Assistant always mirrors what is physically lit.
    {
        JsonDocument doc;
        doc["state"] = haVariables.reportedOn ? "ON" : "OFF";
        doc["brightness"] = haVariables.reportedBrightness;
        if (haVariables.reportedIsTemp)
        {
            doc["color_mode"] = "color_temp";
            doc["color_temp"] = haVariables.reportedMireds;
        }
        else
        {
            doc["color_mode"] = "rgb";
            JsonObject color = doc["color"].to<JsonObject>();
            color["r"] = haVariables.reportedR;
            color["g"] = haVariables.reportedG;
            color["b"] = haVariables.reportedB;
        }
        String payload;
        serializeJson(doc, payload);
        haMqtt.publish(haLightStateTopic.c_str(), payload.c_str(), true);
    }

    haMqtt.publish(haModeStateTopic.c_str(), haModeToString(printerConfig.ledControlMode), true);
    haMqtt.publish(haEnableStateTopic.c_str(), haVariables.masterEnable ? "ON" : "OFF", true);

    haVariables.stateDirty = false;
}

// Shared device block so all three entities group under one device in HA.
static void haAddDeviceBlock(JsonDocument &doc)
{
    JsonObject device = doc["device"].to<JsonObject>();
    JsonArray ids = device["identifiers"].to<JsonArray>();
    ids.add(haNodeId);
    device["name"] = haDeviceName;
    device["manufacturer"] = "BLLED";
    device["model"] = "BL Led Controller";
    device["sw_version"] = globalVariables.FWVersion;
    // "Visit device" link in Home Assistant opens the BLLED web interface.
    // Uses the mDNS hostname (DHCP-stable); falls back-friendly to the IP if set.
    String cfgHost = globalVariables.Host;
    cfgHost.toLowerCase();
    device["configuration_url"] = String("http://") + cfgHost + ".local";
}

void haPublishDiscovery()
{
    if (!haMqtt.connected())
        return;

    // ---- Light ----
    {
        JsonDocument doc;
        doc["name"] = "LED";
        doc["unique_id"] = haNodeId + "_light";
        doc["schema"] = "json";
        doc["command_topic"] = haLightCmdTopic;
        doc["state_topic"] = haLightStateTopic;
        doc["availability_topic"] = haAvailTopic;
        doc["brightness"] = true;
        JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
        modes.add("color_temp"); // warm/cold white channels
        modes.add("rgb");        // colour channels
        doc["min_mireds"] = HA_MIRED_MIN;
        doc["max_mireds"] = HA_MIRED_MAX;
        haAddDeviceBlock(doc);

        String topic = String("homeassistant/light/") + haNodeId + "/config";
        String payload;
        serializeJson(doc, payload);
        haMqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    // ---- Mode select ----
    {
        JsonDocument doc;
        doc["name"] = "Mode";
        doc["unique_id"] = haNodeId + "_mode";
        doc["command_topic"] = haModeCmdTopic;
        doc["state_topic"] = haModeStateTopic;
        doc["availability_topic"] = haAvailTopic;
        JsonArray options = doc["options"].to<JsonArray>();
        options.add("Printer");
        options.add("HomeAssistant");
        options.add("Hybrid");
        doc["icon"] = "mdi:cog-transfer";
        haAddDeviceBlock(doc);

        String topic = String("homeassistant/select/") + haNodeId + "_mode/config";
        String payload;
        serializeJson(doc, payload);
        haMqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    // ---- Enable switch ----
    {
        JsonDocument doc;
        doc["name"] = "Enable";
        doc["unique_id"] = haNodeId + "_enable";
        doc["command_topic"] = haEnableCmdTopic;
        doc["state_topic"] = haEnableStateTopic;
        doc["availability_topic"] = haAvailTopic;
        doc["icon"] = "mdi:led-strip-variant";
        haAddDeviceBlock(doc);

        String topic = String("homeassistant/switch/") + haNodeId + "_enable/config";
        String payload;
        serializeJson(doc, payload);
        haMqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    LogSerial.println(F("[HA] Discovery configs published"));
}

// --- command handling -------------------------------------------------------
static void haHandleLightCommand(byte *payload, unsigned int length)
{
    JsonDocument doc;
    if (deserializeJson(doc, payload, length))
    {
        LogSerial.println(F("[HA] Light command parse error"));
        return;
    }

    if (!doc["state"].isNull())
        haVariables.lightOn = (doc["state"].as<String>() == "ON");

    if (!doc["brightness"].isNull())
    {
        int b255 = doc["brightness"].as<int>();
        haVariables.brightness = constrain((int)round(b255 * 100.0 / 255.0), 0, 100);
    }

    // Colour temperature (white) vs RGB. HA sends "color_temp" (mireds) or, on
    // newer versions, "color_temp_kelvin" for the white slider (-> warm/cold
    // channels), and "color" for the RGB wheel.
    if (!doc["color_temp"].isNull())
    {
        haVariables.colorMode = 1;
        haVariables.colorTempMireds = constrain(doc["color_temp"].as<int>(), HA_MIRED_MIN, HA_MIRED_MAX);
    }
    else if (!doc["color_temp_kelvin"].isNull())
    {
        int kelvin = doc["color_temp_kelvin"].as<int>();
        if (kelvin > 0)
        {
            haVariables.colorMode = 1;
            haVariables.colorTempMireds = constrain((int)(1000000L / kelvin), HA_MIRED_MIN, HA_MIRED_MAX);
        }
    }
    if (!doc["color"].isNull())
    {
        haVariables.colorMode = 0;
        haVariables.r = doc["color"]["r"] | haVariables.r;
        haVariables.g = doc["color"]["g"] | haVariables.g;
        haVariables.b = doc["color"]["b"] | haVariables.b;
    }

    // Honour Home Assistant's "transition" (seconds) so colour/brightness changes
    // fade. Default to a BLLED-style fade when HA doesn't specify one, and cap it
    // so a very long transition can't block the main loop for too long.
    if (!doc["transition"].isNull())
        haVariables.transitionMs = constrain((int)(doc["transition"].as<float>() * 1000.0), 0, 3000);
    else
        haVariables.transitionMs = 500;

    // In Hybrid mode an HA light command takes over from the printer.
    if (printerConfig.ledControlMode == LED_MODE_HYBRID)
        haVariables.overrideActive = true;

    haVariables.stateDirty = true;
    haRequestSave();
    updateleds();

    if (haVariables.colorMode == 1)
        LogSerial.printf("[HA] Light cmd -> %s b:%d temp:%d mireds\n",
                         haVariables.lightOn ? "ON" : "OFF", haVariables.brightness,
                         haVariables.colorTempMireds);
    else
        LogSerial.printf("[HA] Light cmd -> %s b:%d rgb:%d,%d,%d\n",
                         haVariables.lightOn ? "ON" : "OFF", haVariables.brightness,
                         haVariables.r, haVariables.g, haVariables.b);
}

static void haHandleModeCommand(const String &value)
{
    int newMode = haStringToMode(value);
    if (newMode != printerConfig.ledControlMode)
    {
        printerConfig.ledControlMode = newMode;
        // Switching to Printer-only releases any HA override.
        if (newMode == LED_MODE_PRINTER)
            haVariables.overrideActive = false;
        haRequestSave();
        LogSerial.print(F("[HA] Mode -> "));
        LogSerial.println(haModeToString(newMode));
    }
    haVariables.stateDirty = true;
    updateleds();
}

static void haHandleEnableCommand(const String &value)
{
    bool en = (value == "ON" || value == "on" || value == "1" || value == "true");
    if (en != haVariables.masterEnable)
    {
        haVariables.masterEnable = en;
        haRequestSave();
        LogSerial.print(F("[HA] Master enable -> "));
        LogSerial.println(en ? F("ON") : F("OFF"));
    }
    haVariables.stateDirty = true;
    updateleds();
}

void haCallback(char *topic, byte *payload, unsigned int length)
{
    if (printerConfig.mqttdebug)
    {
        LogSerial.printf("[HA] RX %s [%u]: ", topic, length);
        for (unsigned int i = 0; i < length; i++)
            LogSerial.write(payload[i]);
        LogSerial.println();
    }

    String t(topic);
    if (t == haLightCmdTopic)
    {
        haHandleLightCommand(payload, length);
    }
    else if (t == haModeCmdTopic)
    {
        String v;
        v.reserve(length);
        for (unsigned int i = 0; i < length; i++)
            v += (char)payload[i];
        haHandleModeCommand(v);
    }
    else if (t == haEnableCmdTopic)
    {
        String v;
        v.reserve(length);
        for (unsigned int i = 0; i < length; i++)
            v += (char)payload[i];
        haHandleEnableCommand(v);
    }
}

// --- connection -------------------------------------------------------------
bool haConnect()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    if (strlen(printerConfig.haMqttHost) == 0)
        return false;

    g_haConnectAttempts++;
    Serial.printf("[HA] connect attempt #%d host=%s port=%d\n",
                  g_haConnectAttempts, printerConfig.haMqttHost, printerConfig.haMqttPort);
    LogSerial.printf("[HA] connect #%d -> %s:%d%s\n",
                     g_haConnectAttempts, printerConfig.haMqttHost, printerConfig.haMqttPort,
                     printerConfig.haMqttTls ? " (TLS)" : "");

    String clientId = haNodeId + "-" + String(random(0xffff), HEX);

    const char *user = strlen(printerConfig.haMqttUser) > 0 ? printerConfig.haMqttUser : nullptr;
    const char *pass = strlen(printerConfig.haMqttPass) > 0 ? printerConfig.haMqttPass : nullptr;

    unsigned long t0 = millis();
    bool ok = haMqtt.connect(clientId.c_str(), user, pass,
                             haAvailTopic.c_str(), 0, true, "offline");
    unsigned long dur = millis() - t0;

    Serial.printf("[HA] connect result=%d duration=%lums state=%d\n",
                  (int)ok, dur, haMqtt.state());

    if (!ok)
    {
        haVariables.connected = false;
        haVariables.haLastConnectState = haMqtt.state();
        LogSerial.printf("[HA] connect FAILED state=%d\n", haMqtt.state());
        // lastError() is negative only when the TLS layer failed (e.g. certificate not trusted).
        // -1 is the TLS client's catch-all, which mbedTLS words as "Generic error": the TCP
        // connection was refused or got no answer, or the handshake timed out (10 s).
        char tlsError[96];
        int tlsResult = haSecureClient ? haSecureClient->lastError(tlsError, sizeof(tlsError)) : 0;
        if (tlsResult == -1 && dur >= 10000)
            LogSerial.printf("[HA] TLS handshake with %s:%d timed out\n",
                             printerConfig.haMqttHost, printerConfig.haMqttPort);
        else if (tlsResult == -1)
            LogSerial.printf("[HA] No connection to %s:%d (refused or no answer) - is the broker's TLS listener on this port?\n",
                             printerConfig.haMqttHost, printerConfig.haMqttPort);
        else if (tlsResult < 0)
            LogSerial.printf("[HA] TLS error: %s\n", tlsError);
        return false;
    }

    LogSerial.println(F("[HA] Connected"));
    haVariables.connected = true;
    haVariables.haLastConnectState = 0;

    haMqtt.subscribe(haLightCmdTopic.c_str());
    haMqtt.subscribe(haModeCmdTopic.c_str());
    haMqtt.subscribe(haEnableCmdTopic.c_str());

    // Publish discovery before availability so HA knows the entity config
    // before it sees the "online" availability message.
    haPublishDiscovery();
    haPublishAvailability(true);
    haVariables.lastAvailabilityPublishMs = millis();
    haPublishState();
    haVariables.discoverySent = true;
    return true;
}

void setupHa()
{
    if (!printerConfig.haEnabled)
    {
        LogSerial.println(F("[HA] Integration disabled"));
        return;
    }
    if (strlen(printerConfig.haMqttHost) == 0)
    {
        LogSerial.println(F("[HA] No broker host configured - skipping"));
        return;
    }

    // Build a stable unique id from the WiFi MAC.
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    haNodeId = "blled_" + mac;
    haDeviceName = globalVariables.Host.length() ? globalVariables.Host : String("BLLED");

    String base = "blled/" + haNodeId;
    haAvailTopic = base + "/status";
    haLightCmdTopic = base + "/light/set";
    haLightStateTopic = base + "/light/state";
    haModeCmdTopic = base + "/mode/set";
    haModeStateTopic = base + "/mode/state";
    haEnableCmdTopic = base + "/enable/set";
    haEnableStateTopic = base + "/enable/state";

    haMqtt.setServer(printerConfig.haMqttHost, printerConfig.haMqttPort);
    haMqtt.setBufferSize(1024);
    haMqtt.setSocketTimeout(5); // cap blocking socket ops at 5 s
    if (printerConfig.haMqttTls)
    {
        haSecureClient = new WiFiClientSecure();
        haActiveCaCert = printerConfig.haMqttCaCert;
        if (haActiveCaCert.length() > 0)
            haSecureClient->setCACert(haActiveCaCert.c_str());
        else
            haSecureClient->setInsecure(); // encrypted, but the broker's identity isn't checked
        haSecureClient->setTimeout(5);           // cap TCP connect at 5 s
        haSecureClient->setHandshakeTimeout(10); // default 120 s would stall the reconnect loop
        haMqtt.setClient(*haSecureClient);
    }
    else
    {
        haWifiClient.setTimeout(5); // cap TCP connect at 5 s
    }
    haMqtt.setCallback(haCallback);

    LogSerial.print(F("[HA] Configured, node id: "));
    LogSerial.println(haNodeId);
    if (printerConfig.haMqttTls)
        LogSerial.println(haActiveCaCert.length() > 0 ? F("[HA] TLS on, broker certificate checked against the CA")
                                                      : F("[HA] TLS on, broker certificate NOT checked (no CA set)"));

    // A TLS handshake needs more stack than setup() has (8 kB), so with TLS the first
    // connection is left to haTask, which starts right after this.
    if (!printerConfig.haMqttTls)
        haConnect();
}

void haLoop()
{
    if (!printerConfig.haEnabled || strlen(printerConfig.haMqttHost) == 0)
        return;
    if (WiFi.status() != WL_CONNECTED)
        return;

    if (!haMqtt.connected())
    {
        if (haVariables.connected)
        {
            // Log the moment the connection is lost so it appears in WebSerial.
            LogSerial.printf("[HA] Disconnected (broker state=%d)\n", haMqtt.state());
            haVariables.connected = false;
        }
        if (millis() - haVariables.lastReconnectMs >= HA_RECONNECT_INTERVAL)
        {
            haVariables.lastReconnectMs = millis();
            haConnect();
        }
        return;
    }

    // If loop() returns false the TCP connection was dropped during servicing.
    // Mark disconnected immediately so the reconnect path fires on the next tick.
    if (!haMqtt.loop()) {
        LogSerial.println(F("[HA] loop() returned false — scheduling reconnect"));
        haVariables.connected = false;
        return;
    }

    // Periodic availability heartbeat: re-publish "online" every 30 s.
    // Guards against the LWT firing on a brief TCP hiccup and not recovering
    // (e.g. modem sleep jitter, HA broker restart with cleared retained messages).
    if (millis() - haVariables.lastAvailabilityPublishMs >= 30000) {
        haPublishAvailability(true);
        haVariables.lastAvailabilityPublishMs = millis();
    }

    // Service discovery republish requested by the web handler.
    // The web handler must NOT call haMqtt.publish() directly — PubSubClient
    // is not thread-safe and concurrent access from the async web context
    // corrupts its state and triggers spurious disconnects.
    if (haVariables.discoveryRepublishPending) {
        haVariables.discoveryRepublishPending = false;
        haPublishDiscovery();
        haPublishAvailability(true);
        haVariables.lastAvailabilityPublishMs = millis();
        haVariables.stateDirty = true;
        LogSerial.println(F("[HA] Discovery republished via web request"));
    }

    if (haVariables.stateDirty)
        haPublishState();

    // Debounced persistence of HA-driven changes.
    if (haSavePending && (millis() - haLastSaveRequestMs) > 5000)
    {
        haSavePending = false;
        saveFileSystem();
        LogSerial.println(F("[HA] Config saved"));
    }
}

// Run the HA MQTT client in its own FreeRTOS task so it is serviced continuously
// and is NOT blocked by the main loop (e.g. the 5 s printer SSDP discovery scan).
// This keeps Home Assistant commands near-instant.
TaskHandle_t haTaskHandle = NULL;

void haTask(void *parameter)
{
    for (;;)
    {
        haLoop();
        // 50 ms gives ≤50 ms command latency and is well within PubSubClient's
        // 15-second keepalive window. Polling at 10 ms caused continuous WiFi
        // radio bursts that induced audible noise in the LED driver circuit.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void startHaMqttTask()
{
    if (!printerConfig.haEnabled || strlen(printerConfig.haMqttHost) == 0)
        return;
    if (haTaskHandle != NULL)
        return;

    // A TLS handshake needs far more stack than plain MQTT (the printer's TLS task has 20 kB).
    const uint32_t stackSize = printerConfig.haMqttTls ? 20480 : 10240;

    BaseType_t result;
#if CONFIG_FREERTOS_UNICORE
    result = xTaskCreate(haTask, "haTask", stackSize, NULL, 1, &haTaskHandle);
#else
    result = xTaskCreatePinnedToCore(haTask, "haTask", stackSize, NULL, 1, &haTaskHandle, 1);
#endif

    if (result == pdPASS)
        LogSerial.println(F("[HA] MQTT task started"));
    else
        LogSerial.println(F("[HA] Failed to create MQTT task!"));
}

#endif
