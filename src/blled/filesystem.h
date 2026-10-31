#ifndef _BLLEDFILESYSTEM
#define _BLLEDFILESYSTEM

#include <WiFi.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <LittleFS.h>
#include "types.h"

const char *configPath = "/blledconfig.json";
// A restore upload is written here first and only replaces the config once it checks out.
const char *configUploadPath = "/blledconfig.upload";
const size_t configUploadMaxBytes = 16384; // a saved config is a few kB

char *generateRandomString(int length)
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int charsetLength = strlen(charset);

    char *randomString = new char[length + 1];
    for (int i = 0; i < length; i++)
    {
        int randomIndex = random(0, charsetLength);
        randomString[i] = charset[randomIndex];
    }
    randomString[length] = '\0';

    return randomString;
}

void saveFileSystem()
{
    LogSerial.println(F("[Filesystem] Saving config"));

    JsonDocument json;
    json["ssid"] = globalVariables.SSID;
    json["appw"] = globalVariables.APPW;
    json["HTTPUser"] = securityVariables.HTTPUser;
    json["HTTPPass"] = securityVariables.HTTPPass;
    json["host"] = globalVariables.Host;
    json["printerIp"] = printerConfig.printerIP;
    json["accessCode"] = printerConfig.accessCode;
    json["serialNumber"] = printerConfig.serialNumber;
    // json["webpagePassword"] = printerConfig.webpagePassword;
    json["bssi"] = printerConfig.BSSID;
    json["brightness"] = printerConfig.brightness;
    // LED Behaviour (Choose One)
    json["maintMode"] = printerConfig.maintMode;
    json["discoMode"] = printerConfig.discoMode;
    json["replicatestate"] = printerConfig.replicatestate;
    // Running Color
    json["runningRGB"] = printerConfig.runningColor.RGBhex;
    json["runningWW"] = printerConfig.runningColor.ww;
    json["runningCW"] = printerConfig.runningColor.cw;
    // Test LED Colors
    json["showtestcolor"] = printerConfig.testcolorEnabled;
    json["testRGB"] = printerConfig.testColor.RGBhex;
    json["testWW"] = printerConfig.testColor.ww;
    json["testCW"] = printerConfig.testColor.cw;
    json["debugwifi"] = printerConfig.debugwifi;
    // Options
    json["finishindication"] = printerConfig.finishindication;
    json["finishColor"] = printerConfig.finishColor.RGBhex;
    json["finishWW"] = printerConfig.finishColor.ww;
    json["finishCW"] = printerConfig.finishColor.cw;
    json["finishExit"] = printerConfig.finishExit;
    json["finish_check"] = printerConfig.finish_check;
    json["finishTimerMins"] = printerConfig.finishTimeOut;
    json["inactivityEnabled"] = printerConfig.inactivityEnabled;
    json["inactivityTimeOut"] = printerConfig.inactivityTimeOut;
    json["controlChamberLight"] = printerConfig.controlChamberLight; //control chamber light
    // Debugging
    json["debuging"] = printerConfig.debuging;
    json["debugingchange"] = printerConfig.debugingchange;
    json["mqttdebug"] = printerConfig.mqttdebug;
    // Printer Dependant
    json["p1Printer"] = printerVariables.isP1Printer;
    json["doorSwitch"] = printerVariables.useDoorSwtich;
    // Customise LED Colors (during Lidar)
    json["stage14RGB"] = printerConfig.stage14Color.RGBhex;
    json["stage14WW"] = printerConfig.stage14Color.ww;
    json["stage14CW"] = printerConfig.stage14Color.cw;
    json["stage1RGB"] = printerConfig.stage1Color.RGBhex;
    json["stage1WW"] = printerConfig.stage1Color.ww;
    json["stage1CW"] = printerConfig.stage1Color.cw;
    json["stage8RGB"] = printerConfig.stage8Color.RGBhex;
    json["stage8WW"] = printerConfig.stage8Color.ww;
    json["stage8CW"] = printerConfig.stage8Color.cw;
    json["stage9RGB"] = printerConfig.stage9Color.RGBhex;
    json["stage9WW"] = printerConfig.stage9Color.ww;
    json["stage9CW"] = printerConfig.stage9Color.cw;
    json["stage10RGB"] = printerConfig.stage10Color.RGBhex;
    json["stage10WW"] = printerConfig.stage10Color.ww;
    json["stage10CW"] = printerConfig.stage10Color.cw;
    // Customise LED Colors
    json["errordetection"] = printerConfig.errordetection;
    json["wifiRGB"] = printerConfig.wifiRGB.RGBhex;
    json["wifiWW"] = printerConfig.wifiRGB.ww;
    json["wifiCW"] = printerConfig.wifiRGB.cw;
    json["pauseRGB"] = printerConfig.pauseRGB.RGBhex;
    json["pauseWW"] = printerConfig.pauseRGB.ww;
    json["pauseCW"] = printerConfig.pauseRGB.cw;
    json["firstlayerRGB"] = printerConfig.firstlayerRGB.RGBhex;
    json["firstlayerWW"] = printerConfig.firstlayerRGB.ww;
    json["firstlayerCW"] = printerConfig.firstlayerRGB.cw;
    json["nozzleclogRGB"] = printerConfig.nozzleclogRGB.RGBhex;
    json["nozzleclogWW"] = printerConfig.nozzleclogRGB.ww;
    json["nozzleclogCW"] = printerConfig.nozzleclogRGB.cw;
    json["hmsSeriousRGB"] = printerConfig.hmsSeriousRGB.RGBhex;
    json["hmsSeriousWW"] = printerConfig.hmsSeriousRGB.ww;
    json["hmsSeriousCW"] = printerConfig.hmsSeriousRGB.cw;
    json["hmsFatalRGB"] = printerConfig.hmsFatalRGB.RGBhex;
    json["hmsFatalWW"] = printerConfig.hmsFatalRGB.ww;
    json["hmsFatalCW"] = printerConfig.hmsFatalRGB.cw;
    json["filamentRunoutRGB"] = printerConfig.filamentRunoutRGB.RGBhex;
    json["filamentRunoutWW"] = printerConfig.filamentRunoutRGB.ww;
    json["filamentRunoutCW"] = printerConfig.filamentRunoutRGB.cw;
    json["frontCoverRGB"] = printerConfig.frontCoverRGB.RGBhex;
    json["frontCoverWW"] = printerConfig.frontCoverRGB.ww;
    json["frontCoverCW"] = printerConfig.frontCoverRGB.cw;
    json["nozzleTempRGB"] = printerConfig.nozzleTempRGB.RGBhex;
    json["nozzleTempWW"] = printerConfig.nozzleTempRGB.ww;
    json["nozzleTempCW"] = printerConfig.nozzleTempRGB.cw;
    json["bedTempRGB"] = printerConfig.bedTempRGB.RGBhex;
    json["bedTempWW"] = printerConfig.bedTempRGB.ww;
    json["bedTempCW"] = printerConfig.bedTempRGB.cw;
    //HMS Error handling
    json["hmsIgnoreList"] = printerConfig.hmsIgnoreList;
    // Home Assistant integration & operating mode
    json["ledControlMode"] = printerConfig.ledControlMode;
    json["haEnabled"] = printerConfig.haEnabled;
    json["haMqttHost"] = printerConfig.haMqttHost;
    json["haMqttPort"] = printerConfig.haMqttPort;
    json["haMqttUser"] = printerConfig.haMqttUser;
    json["haMqttPass"] = printerConfig.haMqttPass;
    json["haMqttTls"] = printerConfig.haMqttTls;
    json["haMqttCaCert"] = printerConfig.haMqttCaCert;
    json["offlineDimEnabled"] = printerConfig.offlineDimEnabled;
    json["offlineDimAfterSec"] = printerConfig.offlineDimAfterSec;
    json["offlineDimBrightness"] = printerConfig.offlineDimBrightness;
    // WiFi RF tuning
    json["wifiTxPower"] = printerConfig.wifiTxPower;
    json["wifiSleepEnabled"] = printerConfig.wifiSleepEnabled;
    // Subsystem isolation
    json["wifiRadioEnabled"] = printerConfig.wifiRadioEnabled;
    json["diagEnableMqtt"] = printerConfig.diagEnableMqtt;
    json["diagEnableHa"] = printerConfig.diagEnableHa;
    json["diagEnableSsdp"] = printerConfig.diagEnableSsdp;
    json["diagEnableBblScan"] = printerConfig.diagEnableBblScan;
    // Home Assistant runtime state (persisted so HA-only mode restores after a reboot)
    json["haMasterEnable"] = haVariables.masterEnable;
    json["haLightOn"] = haVariables.lightOn;
    json["haR"] = haVariables.r;
    json["haG"] = haVariables.g;
    json["haB"] = haVariables.b;
    json["haBrightness"] = haVariables.brightness;
    json["haColorMode"] = haVariables.colorMode;
    json["haColorTemp"] = haVariables.colorTempMireds;

    File configFile = LittleFS.open(configPath, "w");
    if (!configFile)
    {
        LogSerial.println(F("[Filesystem] Failed to save config"));
        return;
    }
    serializeJson(json, configFile);
    configFile.close();
    LogSerial.println(F("[Filesystem] Config Saved"));
}

void loadFileSystem()
{
    LogSerial.println(F("[Filesystem] Loading config"));

    File configFile;
    int attempts = 0;
    while (attempts < 2)
    {
        configFile = LittleFS.open(configPath, "r");
        if (configFile)
        {
            break;
        }
        attempts++;
        LogSerial.println(F("[Filesystem] Failed to open config file, retrying.."));
        delay(2000);
    }
    if (!configFile)
    {
        LogSerial.print(F("[Filesystem] Failed to open config file after "));
        LogSerial.print(attempts);
        LogSerial.println(F(" retries"));

        LogSerial.println(F("[Filesystem] Clearing config"));
        saveFileSystem();
        return;
    }
    JsonDocument json;
    auto deserializeError = deserializeJson(json, configFile);

    if (!deserializeError)
    {
        // strlcpy + "| \"\"": a missing key or an over-long value must not crash or
        // overflow at boot (that would be a boot loop needing a USB reflash).
        strlcpy(globalVariables.SSID, json["ssid"] | "", sizeof(globalVariables.SSID));
        strlcpy(globalVariables.APPW, json["appw"] | "", sizeof(globalVariables.APPW));
        const char *hostValue = json["host"] | "BLLED";
        globalVariables.Host = hostValue;

        strlcpy(securityVariables.HTTPUser, json["HTTPUser"] | "", sizeof(securityVariables.HTTPUser));
        strlcpy(securityVariables.HTTPPass, json["HTTPPass"] | "", sizeof(securityVariables.HTTPPass));
        strlcpy(printerConfig.printerIP,    json["printerIp"]     | "", sizeof(printerConfig.printerIP));
        strlcpy(printerConfig.accessCode,   json["accessCode"]    | "", sizeof(printerConfig.accessCode));
        strlcpy(printerConfig.serialNumber, json["serialNumber"]  | "", sizeof(printerConfig.serialNumber));
        strlcpy(printerConfig.BSSID,        json["bssi"]          | "", sizeof(printerConfig.BSSID));

        {
            int rawBr = json["brightness"] | -1;
            printerConfig.brightness = constrain(rawBr <= 0 ? 20 : rawBr, 1, 100);
            if (rawBr <= 0 || rawBr > 100)
                Serial.printf("[CFG] brightness corrected: raw=%d -> %d\n", rawBr, printerConfig.brightness);
        }
        // LED Behaviour (Choose One)
        printerConfig.maintMode = json["maintMode"];
        printerConfig.discoMode = json["discoMode"];
        if (printerConfig.discoMode) {
            Serial.println(F("[CFG] discoMode=true in stored config — RESET to false at boot (transient state)"));
            printerConfig.discoMode = false;
        }
        printerConfig.replicatestate = json["replicatestate"];
        // Running Color
        printerConfig.runningColor = hex2rgb(json["runningRGB"], json["runningWW"], json["runningCW"]);
        // Test LED Colors
        printerConfig.testcolorEnabled = json["showtestcolor"];
        printerConfig.testColor = hex2rgb(json["testRGB"], json["testWW"], json["testCW"]);
        printerConfig.debugwifi = json["debugwifi"];
        // Options
        printerConfig.finishindication = json["finishindication"];
        printerConfig.finishColor = hex2rgb(json["finishColor"], json["finishWW"], json["finishCW"]);
        printerConfig.finishExit = json["finishExit"];
        printerConfig.finishTimeOut = json["finishTimerMins"] | 600000;
        printerConfig.finish_check = json["finish_check"];
        printerConfig.inactivityEnabled = json["inactivityEnabled"];
        {
            int rawTimeout = json["inactivityTimeOut"] | 3600000;
            printerConfig.inactivityTimeOut = max(rawTimeout, 60000);
            if (rawTimeout < 60000)
                Serial.printf("[CFG] inactivityTimeOut corrected: raw=%d -> min 60000ms\n", rawTimeout);
        }
        printerConfig.controlChamberLight = json["controlChamberLight"]; //control chamber light
        // Debugging
        printerConfig.debuging = json["debuging"];
        printerConfig.debugingchange = json["debugingchange"];
        printerConfig.mqttdebug = json["mqttdebug"];
        // Printer Dependant
        printerVariables.isP1Printer = json["p1Printer"];
        printerVariables.useDoorSwtich = json["doorSwitch"];
        printerConfig.stage14Color = hex2rgb(json["stage14RGB"], json["stage14WW"], json["stage14CW"]);
        printerConfig.stage1Color = hex2rgb(json["stage1RGB"], json["stage1WW"], json["stage1CW"]);
        printerConfig.stage8Color = hex2rgb(json["stage8RGB"], json["stage8WW"], json["stage8CW"]);
        printerConfig.stage9Color = hex2rgb(json["stage9RGB"], json["stage9WW"], json["stage9CW"]);
        printerConfig.stage10Color = hex2rgb(json["stage10RGB"], json["stage10WW"], json["stage10CW"]);
        // Customise LED Colors
        printerConfig.errordetection = json["errordetection"];
        printerConfig.wifiRGB = hex2rgb(json["wifiRGB"], json["wifiWW"], json["wifiCW"]);

        printerConfig.pauseRGB = hex2rgb(json["pauseRGB"], json["pauseWW"], json["pauseCW"]);
        printerConfig.firstlayerRGB = hex2rgb(json["firstlayerRGB"], json["firstlayerWW"], json["firstlayerCW"]);
        printerConfig.nozzleclogRGB = hex2rgb(json["nozzleclogRGB"], json["nozzleclogWW"], json["nozzleclogCW"]);
        printerConfig.hmsSeriousRGB = hex2rgb(json["hmsSeriousRGB"], json["hmsSeriousWW"], json["hmsSeriousCW"]);
        printerConfig.hmsFatalRGB = hex2rgb(json["hmsFatalRGB"], json["hmsFatalWW"], json["hmsFatalCW"]);
        printerConfig.filamentRunoutRGB = hex2rgb(json["filamentRunoutRGB"], json["filamentRunoutWW"], json["filamentRunoutCW"]);
        printerConfig.frontCoverRGB = hex2rgb(json["frontCoverRGB"], json["frontCoverWW"], json["frontCoverCW"]);
        printerConfig.nozzleTempRGB = hex2rgb(json["nozzleTempRGB"], json["nozzleTempWW"], json["nozzleTempCW"]);
        printerConfig.bedTempRGB = hex2rgb(json["bedTempRGB"], json["bedTempWW"], json["bedTempCW"]);
        // HMS Error handling
        printerConfig.hmsIgnoreList = json["hmsIgnoreList"] | "";
        // Home Assistant integration & operating mode (defaults keep old configs working)
        printerConfig.ledControlMode = json["ledControlMode"] | 2;
        printerConfig.haEnabled = json["haEnabled"] | false;
        strlcpy(printerConfig.haMqttHost, json["haMqttHost"] | "", sizeof(printerConfig.haMqttHost));
        printerConfig.haMqttPort = json["haMqttPort"] | 1883;
        strlcpy(printerConfig.haMqttUser, json["haMqttUser"] | "", sizeof(printerConfig.haMqttUser));
        strlcpy(printerConfig.haMqttPass, json["haMqttPass"] | "", sizeof(printerConfig.haMqttPass));
        printerConfig.haMqttTls = json["haMqttTls"] | false;
        printerConfig.haMqttCaCert = json["haMqttCaCert"] | "";
        printerConfig.offlineDimEnabled = json["offlineDimEnabled"] | true;
        printerConfig.offlineDimAfterSec = json["offlineDimAfterSec"] | 60;
        printerConfig.offlineDimBrightness = json["offlineDimBrightness"] | 5;
        // WiFi RF tuning
        printerConfig.wifiTxPower = json["wifiTxPower"] | 11;
        printerConfig.wifiSleepEnabled = json["wifiSleepEnabled"] | true;
        // Subsystem isolation — default true so existing configs keep full behaviour
        printerConfig.wifiRadioEnabled = json["wifiRadioEnabled"] | true;
        printerConfig.diagEnableMqtt = json["diagEnableMqtt"] | true;
        printerConfig.diagEnableHa = json["diagEnableHa"] | true;
        printerConfig.diagEnableSsdp = json["diagEnableSsdp"] | true;
        printerConfig.diagEnableBblScan = json["diagEnableBblScan"] | true;
        // Home Assistant runtime state
        haVariables.masterEnable = json["haMasterEnable"] | true;
        haVariables.lightOn = json["haLightOn"] | false;
        haVariables.r = json["haR"] | 255;
        haVariables.g = json["haG"] | 255;
        haVariables.b = json["haB"] | 255;
        haVariables.brightness = json["haBrightness"] | 100;
        haVariables.colorMode = json["haColorMode"] | 1;
        haVariables.colorTempMireds = json["haColorTemp"] | 326;

        // === STARTUP CONFIG DUMP — compare against a fresh-erase boot to isolate noise cause ===
        Serial.println(F("=== CONFIG DUMP ==="));
        Serial.printf("[CFG] accessCode     : len=%d (first4='%.4s')\n",
                      (int)strlen(printerConfig.accessCode), printerConfig.accessCode);
        Serial.printf("[CFG] printerIP      : %s\n", printerConfig.printerIP);
        Serial.printf("[CFG] serialNumber   : %s\n", printerConfig.serialNumber);
        Serial.printf("[CFG] brightness     : %d\n", printerConfig.brightness);
        Serial.printf("[CFG] discoMode      : %d  maintMode=%d  testcolorEn=%d  debugwifi=%d\n",
                      (int)printerConfig.discoMode, (int)printerConfig.maintMode,
                      (int)printerConfig.testcolorEnabled, (int)printerConfig.debugwifi);
        Serial.printf("[CFG] inactivityEn   : %d  timeoutMs=%d (%.1fh)\n",
                      (int)printerConfig.inactivityEnabled, printerConfig.inactivityTimeOut,
                      printerConfig.inactivityTimeOut / 3600000.0f);
        Serial.printf("[CFG] finishIndic    : %d  timeoutMs=%d\n",
                      (int)printerConfig.finishindication, printerConfig.finishTimeOut);
        Serial.printf("[CFG] haEnabled      : %d  host='%s'  port=%d\n",
                      (int)printerConfig.haEnabled, printerConfig.haMqttHost, printerConfig.haMqttPort);
        Serial.printf("[CFG] ledControlMode : %d  offlineDim: en=%d after=%ds br=%d%%\n",
                      printerConfig.ledControlMode, (int)printerConfig.offlineDimEnabled,
                      printerConfig.offlineDimAfterSec, printerConfig.offlineDimBrightness);
        Serial.println(F("=== END CONFIG DUMP ==="));

        LogSerial.println(F("[Filesystem] Loaded config"));
    }
    else
    {
        LogSerial.println(F("[Filesystem] Failed loading config"));
        LogSerial.println(F("[Filesystem] Clearing config"));
        LittleFS.remove(configPath);

        // LogSerial.println(F("Generating new password"));
        // char* pw = generateRandomString(8);
        // strcpy(printerConfig.webpagePassword, pw);
    }

    configFile.close();
}

void deleteFileSystem()
{
    LogSerial.println(F("[Filesystem] Deleting LittleFS"));
    LittleFS.remove(configPath);
}

bool hasFileSystem()
{
    return LittleFS.exists(configPath);
}

// A restore file must look like a saved config (a JSON object with the WiFi SSID) before it
// replaces the current one; otherwise the next boot would discard it and every setting.
bool isValidConfigFile(const char *path)
{
    File file = LittleFS.open(path, "r");
    if (!file)
        return false;
    JsonDocument json;
    DeserializationError error = deserializeJson(json, file);
    file.close();
    return !error && json.is<JsonObject>() && json["ssid"].is<const char *>();
}

void setupFileSystem()
{
    LogSerial.println(F("[Filesystem] Mounting LittleFS"));
    if (!LittleFS.begin())
    {
        LogSerial.println(F("[Filesystem] Failed to mount LittleFS"));
        LittleFS.format();
        LogSerial.println(F("[Filesystem] Formatting LittleFS"));
        LogSerial.println(F("[Filesystem] Restarting Device"));
        delay(1000);
        ESP.restart();
    }
    LogSerial.println(F("[Filesystem] Mounted LittleFS"));
};

#endif