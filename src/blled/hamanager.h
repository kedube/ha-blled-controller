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
//   * printer brightness, maintenance mode, and buttons to resume printer
//     control and wake the LEDs
//   * the LED behaviour settings from the setup page (entity_category config)
//   * printer connection / error and HA override status, WiFi signal, memory
//     and the current LED source (entity_category diagnostic)
//
// Every command arrives on blled/<node>/<key>/set (one wildcard subscription).
// The settings and status entities read their state from one JSON topic.
//
// The printer connection (TLS, port 8883) is untouched; this is a plain MQTT
// connection (default port 1883) to Home Assistant.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "types.h"
#include "leds.h"
#include "filesystem.h"

WiFiClient haWifiClient;
PubSubClient haMqtt(haWifiClient);

static String haNodeId;       // unique id, e.g. blled_a1b2c3d4e5f6
static String haDeviceName;   // friendly device name (= mDNS host)

// Topics
static String haBaseTopic;        // blled/<node>; commands arrive on <base>/<key>/set
static String haAvailTopic;       // availability (LWT)
static String haLightStateTopic;  // light state    (JSON)
static String haModeStateTopic;   // mode state     (string)
static String haEnableStateTopic; // enable state   (ON/OFF)
static String haSettingsTopic;    // settings, printer status, LED source (JSON)
static String haDiagTopic;        // WiFi signal and memory (JSON)

static const unsigned long HA_RECONNECT_INTERVAL = 5000;
static const unsigned long HA_DIAG_INTERVAL = 60000;
static unsigned long haLastSaveRequestMs = 0;
static bool haSavePending = false;
static uint32_t haPublishedSettings = 0; // fingerprint of the settings JSON last sent
static unsigned long haLastDiagMs = 0;

// Diagnostic counter — read by wifiMonitorTask in main.cpp
int g_haConnectAttempts = 0;

// Defined in leds.h
void updateleds();

// Request a (debounced) config save so HA-driven changes survive a reboot
// without hammering the flash on every brightness step.
static void haRequestSave()
{
    haSavePending = true;
    haLastSaveRequestMs = millis();
}

// --- settings HA can change -------------------------------------------------
// Each switch and number controls one setting. Its state is the field of the
// settings JSON named after its key.
struct HaSwitchSetting
{
    const char *key;      // topic part, unique_id suffix and settings JSON field
    const char *name;
    const char *icon;
    const char *category; // entity_category, or nullptr for a main control
    bool *flag;
    void (*apply)();      // redraws the strip after a change (nullptr = nothing to redraw)
};

struct HaNumberSetting
{
    const char *key;
    const char *name;
    const char *icon;
    const char *category;
    int *value;
    int scale;            // stored units per unit shown in HA (60000 = ms shown as minutes)
    int min;              // range shown in HA
    int max;
    const char *unit;
    void (*apply)();
};

// Maintenance mode is one of the setup page's "choose one" LED behaviours: switching it on
// turns off the other always-on ones (RGB cycle, test colour, WiFi strength), and in Hybrid
// mode takes the strip back from a Home Assistant override so the white light shows.
static void haApplyMaintenance()
{
    if (printerConfig.maintMode)
    {
        printerConfig.discoMode = false;
        printerConfig.testcolorEnabled = false;
        printerConfig.debugwifi = false;
        haVariables.overrideActive = false;
    }
    refreshLeds();
}

// refreshLeds() restarts the idle timer, so it is only used where the setup page's
// behaviour (lights wake up after a change) is wanted. Brightness and the offline dim
// settings are what an automation is likely to change at night, so they leave it alone.
static const HaSwitchSetting haSwitches[] = {
    // key              name                       icon                           category  setting                              after a change
    {"maintenance",     "Maintenance mode",        "mdi:wrench",                  nullptr,  &printerConfig.maintMode,            haApplyMaintenance},
    {"replicate",       "Replicate chamber light", "mdi:link-variant",            "config", &printerConfig.replicatestate,       refreshLeds},
    {"chamber_light",   "Control chamber light",   "mdi:lightbulb-auto",          "config", &printerConfig.controlChamberLight,  nullptr},
    {"error_detection", "Error detection",         "mdi:lightbulb-alert-outline", "config", &printerConfig.errordetection,       refreshLeds},
    {"finish",          "Finish indication",       "mdi:flag-checkered",          "config", &printerConfig.finishindication,     nullptr},
    {"inactivity",      "Inactivity timeout",      "mdi:sleep",                   "config", &printerConfig.inactivityEnabled,    refreshLeds},
    {"offline_dim",     "Offline dim",             "mdi:lan-disconnect",          "config", &printerConfig.offlineDimEnabled,    updateleds},
};

static const HaNumberSetting haNumbers[] = {
    // key                     name                         icon                       category  setting                              scale  min max   unit   after a change
    {"brightness",             "Printer brightness",        "mdi:brightness-6",        nullptr,  &printerConfig.brightness,           1,     1,  100,  "%",   reapplyBrightness},
    {"finish_minutes",         "Finish indication timer",   "mdi:timer-outline",       "config", &printerConfig.finishTimeOut,        60000, 1,  999,  "min", nullptr},
    {"inactivity_minutes",     "Inactivity timeout length", "mdi:timer-sand",          "config", &printerConfig.inactivityTimeOut,    60000, 1,  999,  "min", nullptr},
    {"offline_dim_delay",      "Offline dim delay",         "mdi:timer-alert-outline", "config", &printerConfig.offlineDimAfterSec,   1,     5,  3600, "s",   nullptr},
    {"offline_dim_brightness", "Offline dim brightness",    "mdi:brightness-5",        "config", &printerConfig.offlineDimBrightness, 1,     0,  100,  "%",   updateleds},
};

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

static bool haIsOn(const String &value)
{
    return value == "ON" || value == "on" || value == "1" || value == "true";
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

// Changes whenever anything haPublishSettings() sends changes, so haLoop() can tell when
// to send it without building the JSON on every pass. The values change from many places
// (Home Assistant, the setup page, the printer, the LED logic).
static uint32_t haSettingsFingerprint()
{
    uint32_t h = 2166136261u; // FNV-1a, one value at a time
    auto mix = [&h](uint32_t v) { h = (h ^ v) * 16777619u; };
    for (const HaSwitchSetting &s : haSwitches)
        mix(*s.flag);
    for (const HaNumberSetting &n : haNumbers)
        mix((uint32_t)*n.value);
    mix(printerConfig.finishExit);
    mix(haVariables.overrideActive);
    mix(printerVariables.online);
    mix(isPrinterErrorPresent());
    mix(hmsSeverity());
    mix((uint32_t)printerVariables.parsedHMScode);
    mix((uint32_t)(printerVariables.parsedHMScode >> 32));
    mix((uint32_t)(uintptr_t)ledSource);
    return h;
}

// Settings, printer status and LED source, for every entity that reads its state
// from haSettingsTopic.
void haPublishSettings()
{
    if (!haMqtt.connected())
        return;
    haPublishedSettings = haSettingsFingerprint();

    JsonDocument doc;
    for (const HaSwitchSetting &s : haSwitches)
        doc[s.key] = *s.flag ? "ON" : "OFF";
    for (const HaNumberSetting &n : haNumbers)
        doc[n.key] = *n.value / n.scale;
    doc["finish_exit"] = printerConfig.finishExit ? "Door" : "Timer";
    doc["override"] = haVariables.overrideActive ? "ON" : "OFF";
    doc["printer"] = printerVariables.online ? "ON" : "OFF";
    doc["printer_error"] = (printerVariables.online && isPrinterErrorPresent()) ? "ON" : "OFF";
    if (hmsSeverity() > 0)
    {
        // Same form as the setup page's HMS ignore list, so it can be pasted there.
        uint64_t code = printerVariables.parsedHMScode;
        char hmsCode[32];
        snprintf(hmsCode, sizeof(hmsCode), "HMS_%04X_%04X_%04X_%04X",
                 (int)(code >> 48), (int)((code >> 32) & 0xFFFF), (int)((code >> 16) & 0xFFFF), (int)(code & 0xFFFF));
        doc["hms_code"] = hmsCode;
        doc["hms_severity"] = ParseHMSSeverity((int)(code & 0xFFFFFFFF));
    }
    else
    {
        doc["hms_code"] = nullptr;
        doc["hms_severity"] = nullptr;
    }
    doc["led_source"] = ledSource;

    String payload;
    serializeJson(doc, payload);
    if (!haMqtt.publish(haSettingsTopic.c_str(), payload.c_str(), true))
        LogSerial.printf("[HA] Settings state not sent (%u bytes)\n", payload.length());
}

void haPublishDiagnostics()
{
    if (!haMqtt.connected())
        return;
    haLastDiagMs = millis();

    JsonDocument doc;
    doc["rssi"] = WiFi.RSSI();
    doc["free_memory"] = freeMemory();
    doc["largest_block"] = largestFreeBlock();
    String payload;
    serializeJson(doc, payload);
    haMqtt.publish(haDiagTopic.c_str(), payload.c_str(), true);
}

// Shared device block so all entities group under one device in HA.
static void haAddDeviceBlock(JsonDocument &doc)
{
    JsonObject device = doc["device"].to<JsonObject>();
    JsonArray ids = device["identifiers"].to<JsonArray>();
    ids.add(haNodeId);
    device["name"] = haDeviceName;
    device["manufacturer"] = "DutchDeveloper";
    device["model"] = "BLLED Controller";
    device["sw_version"] = globalVariables.FWVersion;
    // "Visit device" link in Home Assistant opens the BLLED web interface.
    // Uses the mDNS hostname (DHCP-stable); falls back-friendly to the IP if set.
    String cfgHost = globalVariables.Host;
    cfgHost.toLowerCase();
    device["configuration_url"] = String("http://") + cfgHost + ".local";
}

// Fields every entity shares. key names its unique_id (and command topic and settings
// field, where it has them).
static void haEntity(JsonDocument &doc, const char *key, const char *name, const char *icon, const char *category)
{
    doc["name"] = name;
    doc["unique_id"] = haNodeId + "_" + key;
    if (icon)
        doc["icon"] = icon;
    if (category)
        doc["entity_category"] = category;
}

static String haCommandTopic(const char *key)
{
    return haBaseTopic + "/" + key + "/set";
}

static void haStateFrom(JsonDocument &doc, const String &topic, const char *key)
{
    doc["state_topic"] = topic;
    doc["value_template"] = String("{{ value_json.") + key + " }}";
}

// objectId is the discovery topic's node part. Entities keep the one they were first
// published under, or Home Assistant would see a second config for the same unique_id.
static void haPublishConfig(JsonDocument &doc, const char *component, const String &objectId)
{
    doc["availability_topic"] = haAvailTopic;
    haAddDeviceBlock(doc);

    String topic = String("homeassistant/") + component + "/" + objectId + "/config";
    String payload;
    serializeJson(doc, payload);
    if (!haMqtt.publish(topic.c_str(), payload.c_str(), true))
        LogSerial.printf("[HA] Discovery for %s not sent (%u bytes)\n", objectId.c_str(), payload.length());
}

void haPublishDiscovery()
{
    if (!haMqtt.connected())
        return;

    // ---- Light ----
    {
        JsonDocument doc;
        haEntity(doc, "light", "LED", "mdi:led-strip-variant", nullptr);
        doc["schema"] = "json";
        doc["command_topic"] = haCommandTopic("light");
        doc["state_topic"] = haLightStateTopic;
        doc["brightness"] = true;
        JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
        modes.add("color_temp"); // warm/cold white channels
        modes.add("rgb");        // colour channels
        doc["min_mireds"] = HA_MIRED_MIN;
        doc["max_mireds"] = HA_MIRED_MAX;
        haPublishConfig(doc, "light", haNodeId);
    }

    // ---- Mode select ----
    {
        JsonDocument doc;
        haEntity(doc, "mode", "Mode", "mdi:cog-transfer", nullptr);
        doc["command_topic"] = haCommandTopic("mode");
        doc["state_topic"] = haModeStateTopic;
        JsonArray options = doc["options"].to<JsonArray>();
        options.add("Printer");
        options.add("HomeAssistant");
        options.add("Hybrid");
        haPublishConfig(doc, "select", haNodeId + "_mode");
    }

    // ---- Enable switch ----
    {
        JsonDocument doc;
        haEntity(doc, "enable", "Enable", "mdi:power", nullptr);
        doc["command_topic"] = haCommandTopic("enable");
        doc["state_topic"] = haEnableStateTopic;
        haPublishConfig(doc, "switch", haNodeId + "_enable");
    }

    // ---- Setting switches and numbers ----
    for (const HaSwitchSetting &s : haSwitches)
    {
        JsonDocument doc;
        haEntity(doc, s.key, s.name, s.icon, s.category);
        doc["command_topic"] = haCommandTopic(s.key);
        haStateFrom(doc, haSettingsTopic, s.key);
        haPublishConfig(doc, "switch", haNodeId + "_" + s.key);
    }
    for (const HaNumberSetting &n : haNumbers)
    {
        JsonDocument doc;
        haEntity(doc, n.key, n.name, n.icon, n.category);
        doc["command_topic"] = haCommandTopic(n.key);
        haStateFrom(doc, haSettingsTopic, n.key);
        doc["min"] = n.min;
        doc["max"] = n.max;
        doc["step"] = 1;
        doc["unit_of_measurement"] = n.unit;
        doc["mode"] = strcmp(n.unit, "%") == 0 ? "slider" : "box"; // percentages slide, durations are typed
        haPublishConfig(doc, "number", haNodeId + "_" + n.key);
    }

    // ---- Finish indication exit select ----
    {
        JsonDocument doc;
        haEntity(doc, "finish_exit", "Finish indication exit", "mdi:timer-cog-outline", "config");
        doc["command_topic"] = haCommandTopic("finish_exit");
        haStateFrom(doc, haSettingsTopic, "finish_exit");
        JsonArray options = doc["options"].to<JsonArray>();
        options.add("Door");
        options.add("Timer");
        haPublishConfig(doc, "select", haNodeId + "_finish_exit");
    }

    // ---- Buttons ----
    {
        JsonDocument doc;
        haEntity(doc, "resume", "Resume printer control", "mdi:printer-3d", nullptr);
        doc["command_topic"] = haCommandTopic("resume");
        haPublishConfig(doc, "button", haNodeId + "_resume");
    }
    {
        JsonDocument doc;
        haEntity(doc, "wake", "Wake LEDs", "mdi:lightbulb-on-outline", nullptr);
        doc["command_topic"] = haCommandTopic("wake");
        haPublishConfig(doc, "button", haNodeId + "_wake");
    }
    {
        JsonDocument doc;
        haEntity(doc, "restart", "Restart", nullptr, "config"); // icon from the device class
        doc["device_class"] = "restart";
        doc["command_topic"] = haCommandTopic("restart");
        haPublishConfig(doc, "button", haNodeId + "_restart");
    }

    // ---- Binary sensors ----
    {
        JsonDocument doc;
        haEntity(doc, "override", "Home Assistant override", "mdi:home-lightbulb-outline", nullptr);
        haStateFrom(doc, haSettingsTopic, "override");
        haPublishConfig(doc, "binary_sensor", haNodeId + "_override");
    }
    {
        JsonDocument doc;
        haEntity(doc, "printer", "Printer connection", nullptr, nullptr); // icon from the device class
        doc["device_class"] = "connectivity";
        haStateFrom(doc, haSettingsTopic, "printer");
        haPublishConfig(doc, "binary_sensor", haNodeId + "_printer");
    }
    {
        JsonDocument doc;
        haEntity(doc, "printer_error", "Printer error", nullptr, nullptr); // icon from the device class
        doc["device_class"] = "problem";
        haStateFrom(doc, haSettingsTopic, "printer_error");
        doc["json_attributes_topic"] = haSettingsTopic;
        doc["json_attributes_template"] =
            "{{ {'hms_code': value_json.hms_code, 'hms_severity': value_json.hms_severity} | tojson }}";
        haPublishConfig(doc, "binary_sensor", haNodeId + "_printer_error");
    }

    // ---- Diagnostic sensors ----
    {
        JsonDocument doc;
        haEntity(doc, "led_source", "LED source", "mdi:state-machine", "diagnostic");
        haStateFrom(doc, haSettingsTopic, "led_source");
        haPublishConfig(doc, "sensor", haNodeId + "_led_source");
    }
    {
        JsonDocument doc;
        haEntity(doc, "rssi", "WiFi signal", nullptr, "diagnostic"); // icon from the device class
        doc["device_class"] = "signal_strength";
        doc["unit_of_measurement"] = "dBm";
        doc["state_class"] = "measurement";
        haStateFrom(doc, haDiagTopic, "rssi");
        haPublishConfig(doc, "sensor", haNodeId + "_rssi");
    }
    const char *memorySensors[][2] = {{"free_memory", "Free memory"}, {"largest_block", "Largest free memory block"}};
    for (const auto &m : memorySensors)
    {
        JsonDocument doc;
        haEntity(doc, m[0], m[1], "mdi:memory", "diagnostic");
        doc["device_class"] = "data_size";
        doc["unit_of_measurement"] = "B";
        doc["state_class"] = "measurement";
        haStateFrom(doc, haDiagTopic, m[0]);
        haPublishConfig(doc, "sensor", haNodeId + "_" + m[0]);
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
    redrawLeds(); // the printer logic may take the strip back from Home Assistant
}

static void haHandleEnableCommand(const String &value)
{
    bool en = haIsOn(value);
    if (en != haVariables.masterEnable)
    {
        haVariables.masterEnable = en;
        haRequestSave();
        LogSerial.print(F("[HA] Master enable -> "));
        LogSerial.println(en ? F("ON") : F("OFF"));
    }
    haVariables.stateDirty = true;
    redrawLeds(); // after being off, the strip needs the colour the printer logic is holding
}

static void haHandleFinishExitCommand(const String &value)
{
    if (value != "Door" && value != "Timer")
        return;
    bool door = (value == "Door");
    if (door != printerConfig.finishExit)
    {
        printerConfig.finishExit = door;
        haRequestSave();
        LogSerial.print(F("[HA] Finish indication exit -> "));
        LogSerial.println(value);
    }
}

// Hybrid: hand the strip back to the printer without waiting for a print to start.
static void haHandleResumeCommand()
{
    LogSerial.println(F("[HA] Resume printer control"));
    haVariables.overrideActive = false;
    refreshLeds();
}

static void haHandleWakeCommand()
{
    LogSerial.println(F("[HA] Wake LEDs"));
    wakeLeds();
}

static void haHandleRestartCommand()
{
    LogSerial.println(F("[HA] Restart requested"));
    if (haSavePending)
    {
        // The debounced save would otherwise be lost with the restart.
        haSavePending = false;
        saveFileSystem();
    }
    haPublishAvailability(false);
    shouldRestart = true; // main loop restarts after a short delay
    restartRequestTime = millis();
}

// A switch or number from the settings tables.
static void haHandleSettingCommand(const String &key, const String &value)
{
    for (const HaSwitchSetting &s : haSwitches)
    {
        if (key != s.key)
            continue;
        bool on = haIsOn(value);
        if (on != *s.flag)
        {
            *s.flag = on;
            haRequestSave();
            LogSerial.printf("[HA] %s -> %s\n", s.name, on ? "ON" : "OFF");
            if (s.apply)
                s.apply();
        }
        return;
    }
    for (const HaNumberSetting &n : haNumbers)
    {
        if (key != n.key)
            continue;
        int shown = (int)lroundf(value.toFloat());
        shown = constrain(shown, n.min, n.max);
        if (shown * n.scale != *n.value)
        {
            *n.value = shown * n.scale;
            haRequestSave();
            LogSerial.printf("[HA] %s -> %d %s\n", n.name, shown, n.unit);
            if (n.apply)
                n.apply();
        }
        return;
    }
    LogSerial.print(F("[HA] Unknown command topic key: "));
    LogSerial.println(key);
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

    // Commands arrive on <base>/<key>/set.
    String t(topic);
    String prefix = haBaseTopic + "/";
    if (!t.startsWith(prefix) || !t.endsWith("/set"))
        return;
    String key = t.substring(prefix.length(), t.length() - 4);

    if (key == "light")
    {
        haHandleLightCommand(payload, length);
        return;
    }

    String v;
    v.reserve(length);
    for (unsigned int i = 0; i < length; i++)
        v += (char)payload[i];

    if (key == "mode")
        haHandleModeCommand(v);
    else if (key == "enable")
        haHandleEnableCommand(v);
    else if (key == "finish_exit")
        haHandleFinishExitCommand(v);
    else if (key == "resume")
        haHandleResumeCommand();
    else if (key == "wake")
        haHandleWakeCommand();
    else if (key == "restart")
        haHandleRestartCommand();
    else
        haHandleSettingCommand(key, v);
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
    LogSerial.printf("[HA] connect #%d -> %s:%d\n",
                     g_haConnectAttempts, printerConfig.haMqttHost, printerConfig.haMqttPort);

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
        return false;
    }

    LogSerial.printf("[HA] Connected (free memory %u, largest free block %u, unused task stack %u)\n",
                     freeMemory(), largestFreeBlock(), uxTaskGetStackHighWaterMark(NULL));
    haVariables.connected = true;
    haVariables.haLastConnectState = 0;

    haMqtt.subscribe((haBaseTopic + "/+/set").c_str());

    // Publish discovery before availability so HA knows the entity config
    // before it sees the "online" availability message.
    haPublishDiscovery();
    haPublishAvailability(true);
    haVariables.lastAvailabilityPublishMs = millis();
    haPublishState();
    haPublishSettings();
    haPublishDiagnostics();
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

    haBaseTopic = "blled/" + haNodeId;
    haAvailTopic = haBaseTopic + "/status";
    haLightStateTopic = haBaseTopic + "/light/state";
    haModeStateTopic = haBaseTopic + "/mode/state";
    haEnableStateTopic = haBaseTopic + "/enable/state";
    haSettingsTopic = haBaseTopic + "/settings/state";
    haDiagTopic = haBaseTopic + "/diagnostics/state";

    haMqtt.setServer(printerConfig.haMqttHost, printerConfig.haMqttPort);
    haMqtt.setBufferSize(1024);
    haMqtt.setSocketTimeout(5); // cap blocking socket ops at 5 s
    haWifiClient.setTimeout(5); // cap TCP connect at 5 s
    haMqtt.setCallback(haCallback);

    LogSerial.print(F("[HA] Configured, node id: "));
    LogSerial.println(haNodeId);

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
        haPublishSettings();
        haPublishDiagnostics();
        LogSerial.println(F("[HA] Discovery republished via web request"));
    }

    if (haVariables.stateDirty)
        haPublishState();

    if (haSettingsFingerprint() != haPublishedSettings)
        haPublishSettings();

    if (millis() - haLastDiagMs >= HA_DIAG_INTERVAL)
        haPublishDiagnostics();

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

    BaseType_t result;
#if CONFIG_FREERTOS_UNICORE
    result = xTaskCreate(haTask, "haTask", 10240, NULL, 1, &haTaskHandle);
#else
    result = xTaskCreatePinnedToCore(haTask, "haTask", 10240, NULL, 1, &haTaskHandle, 1);
#endif

    if (result == pdPASS)
        LogSerial.println(F("[HA] MQTT task started"));
    else
        LogSerial.println(F("[HA] Failed to create MQTT task!"));
}

#endif
