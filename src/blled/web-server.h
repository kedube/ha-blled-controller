#ifndef _BLLEDWEB_SERVER
#define _BLLEDWEB_SERVER

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include "leds.h"
#include "filesystem.h"

// Forward declarations for mqttmanager.h globals (included after web-server.h in main.cpp)
extern volatile unsigned long g_mqttRxMsgsPerMin;
extern volatile unsigned long g_mqttRxBytesPerMin;

AsyncWebServer webServer(80);
AsyncWebSocket ws("/ws");

#include "../www/www.h"

unsigned long lastWsPush = 0;
const unsigned long wsPushInterval = 1000; // alle 1000ms

bool isAuthorized(AsyncWebServerRequest *request)
{
    if (strlen(securityVariables.HTTPUser) == 0 || strlen(securityVariables.HTTPPass) == 0)
    {
        return true;
    }
    return request->authenticate(securityVariables.HTTPUser, securityVariables.HTTPPass);
}

// Requests that change something must come from the controller's own pages: they carry an
// X-BLLED header. A page on another site can't add a custom header without the browser first
// asking this server (a CORS preflight), which never agrees, so it can't change settings,
// restore, re-flash or reset the controller from someone's browser. This matters most when
// no WebUI login is set, and a browser that has saved the login would otherwise send it too.
bool fromOwnPage(AsyncWebServerRequest *request)
{
    return request->hasHeader("X-BLLED");
}

void refuseForeign(AsyncWebServerRequest *request)
{
    request->send(403, "text/plain", "Refused: use the controller's own pages.");
}

void handleSetup(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, setupPage_html_gz_mime, setupPage_html_gz, setupPage_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleUpdatePage(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, updatePage_html_gz_mime, updatePage_html_gz, updatePage_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleGetIcon(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, blled_svg_gz_mime, blled_svg_gz, blled_svg_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleGetfavicon(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, favicon_png_gz_mime, favicon_png_gz, favicon_png_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleGetPCC(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, particleCanvas_js_gz_mime, (const uint8_t *)particleCanvas_js_gz, particleCanvas_js_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleGetConfig(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }

    JsonDocument doc;

    doc["firmwareversion"] = globalVariables.FWVersion.c_str();
    doc["wifiStrength"] = WiFi.RSSI();
    doc["host"] = globalVariables.Host;
    doc["ip"] = printerConfig.printerIP;
    doc["id"] = printerConfig.serialNumber;
    doc["apMAC"] = printerConfig.BSSID;
    doc["brightness"] = printerConfig.brightness;
    doc["maintMode"] = printerConfig.maintMode;
    doc["discoMode"] = printerConfig.discoMode;
    doc["replicateled"] = printerConfig.replicatestate;
    doc["runningRGB"] = printerConfig.runningColor.RGBhex;
    doc["runningWW"] = printerConfig.runningColor.ww;
    doc["runningCW"] = printerConfig.runningColor.cw;
    doc["showtestcolor"] = printerConfig.testcolorEnabled;
    doc["testRGB"] = printerConfig.testColor.RGBhex;
    doc["testWW"] = printerConfig.testColor.ww;
    doc["testCW"] = printerConfig.testColor.cw;
    doc["debugwifi"] = printerConfig.debugwifi;
    doc["finishindication"] = printerConfig.finishindication;
    doc["finishColor"] = printerConfig.finishColor.RGBhex;
    doc["finishWW"] = printerConfig.finishColor.ww;
    doc["finishCW"] = printerConfig.finishColor.cw;
    doc["finishExit"] = printerConfig.finishExit;
    doc["finishTimerMins"] = (int)(printerConfig.finishTimeOut / 60000);
    doc["inactivityEnabled"] = printerConfig.inactivityEnabled;
    doc["inactivityMins"] = (int)(printerConfig.inactivityTimeOut / 60000);
    doc["debuging"] = printerConfig.debuging;
    doc["debugingchange"] = printerConfig.debugingchange;
    doc["mqttdebug"] = printerConfig.mqttdebug;
    doc["p1Printer"] = printerVariables.isP1Printer;
    doc["doorSwitch"] = printerVariables.useDoorSwtich;
    doc["stage14RGB"] = printerConfig.stage14Color.RGBhex;
    doc["stage14WW"] = printerConfig.stage14Color.ww;
    doc["stage14CW"] = printerConfig.stage14Color.cw;
    doc["stage1RGB"] = printerConfig.stage1Color.RGBhex;
    doc["stage1WW"] = printerConfig.stage1Color.ww;
    doc["stage1CW"] = printerConfig.stage1Color.cw;
    doc["stage8RGB"] = printerConfig.stage8Color.RGBhex;
    doc["stage8WW"] = printerConfig.stage8Color.ww;
    doc["stage8CW"] = printerConfig.stage8Color.cw;
    doc["stage9RGB"] = printerConfig.stage9Color.RGBhex;
    doc["stage9WW"] = printerConfig.stage9Color.ww;
    doc["stage9CW"] = printerConfig.stage9Color.cw;
    doc["stage10RGB"] = printerConfig.stage10Color.RGBhex;
    doc["stage10WW"] = printerConfig.stage10Color.ww;
    doc["stage10CW"] = printerConfig.stage10Color.cw;
    doc["errordetection"] = printerConfig.errordetection;
    doc["wifiRGB"] = printerConfig.wifiRGB.RGBhex;
    doc["wifiWW"] = printerConfig.wifiRGB.ww;
    doc["wifiCW"] = printerConfig.wifiRGB.cw;
    doc["pauseRGB"] = printerConfig.pauseRGB.RGBhex;
    doc["pauseWW"] = printerConfig.pauseRGB.ww;
    doc["pauseCW"] = printerConfig.pauseRGB.cw;
    doc["firstlayerRGB"] = printerConfig.firstlayerRGB.RGBhex;
    doc["firstlayerWW"] = printerConfig.firstlayerRGB.ww;
    doc["firstlayerCW"] = printerConfig.firstlayerRGB.cw;
    doc["nozzleclogRGB"] = printerConfig.nozzleclogRGB.RGBhex;
    doc["nozzleclogWW"] = printerConfig.nozzleclogRGB.ww;
    doc["nozzleclogCW"] = printerConfig.nozzleclogRGB.cw;
    doc["hmsSeriousRGB"] = printerConfig.hmsSeriousRGB.RGBhex;
    doc["hmsSeriousWW"] = printerConfig.hmsSeriousRGB.ww;
    doc["hmsSeriousCW"] = printerConfig.hmsSeriousRGB.cw;
    doc["hmsFatalRGB"] = printerConfig.hmsFatalRGB.RGBhex;
    doc["hmsFatalWW"] = printerConfig.hmsFatalRGB.ww;
    doc["hmsFatalCW"] = printerConfig.hmsFatalRGB.cw;
    doc["filamentRunoutRGB"] = printerConfig.filamentRunoutRGB.RGBhex;
    doc["filamentRunoutWW"] = printerConfig.filamentRunoutRGB.ww;
    doc["filamentRunoutCW"] = printerConfig.filamentRunoutRGB.cw;
    doc["frontCoverRGB"] = printerConfig.frontCoverRGB.RGBhex;
    doc["frontCoverWW"] = printerConfig.frontCoverRGB.ww;
    doc["frontCoverCW"] = printerConfig.frontCoverRGB.cw;
    doc["nozzleTempRGB"] = printerConfig.nozzleTempRGB.RGBhex;
    doc["nozzleTempWW"] = printerConfig.nozzleTempRGB.ww;
    doc["nozzleTempCW"] = printerConfig.nozzleTempRGB.cw;
    doc["bedTempRGB"] = printerConfig.bedTempRGB.RGBhex;
    doc["bedTempWW"] = printerConfig.bedTempRGB.ww;
    doc["bedTempCW"] = printerConfig.bedTempRGB.cw;
    // HMS Error Handling
    doc["hmsIgnoreList"] = printerConfig.hmsIgnoreList;
    // control chamber light
    doc["controlChamberLight"] = printerConfig.controlChamberLight;
    // Home Assistant integration & operating mode
    doc["ledControlMode"] = printerConfig.ledControlMode;
    doc["haEnabled"] = printerConfig.haEnabled;
    doc["haMqttHost"] = printerConfig.haMqttHost;
    doc["haMqttPort"] = printerConfig.haMqttPort;
    doc["haMqttUser"] = printerConfig.haMqttUser;
    doc["hasHaMqttPass"] = strlen(printerConfig.haMqttPass) > 0; // never send the secret itself
    doc["haMqttTls"] = printerConfig.haMqttTls;
    doc["haMqttCaCert"] = printerConfig.haMqttCaCert; // a CA certificate is public
    doc["offlineDimEnabled"] = printerConfig.offlineDimEnabled;
    doc["offlineDimAfterSec"] = printerConfig.offlineDimAfterSec;
    doc["offlineDimBrightness"] = printerConfig.offlineDimBrightness;
    doc["haMasterEnable"] = haVariables.masterEnable;
    doc["haConnected"] = haVariables.connected;
    // WiFi RF tuning
    doc["wifiTxPower"] = printerConfig.wifiTxPower;
    doc["wifiSleepEnabled"] = printerConfig.wifiSleepEnabled;
    // Subsystem isolation
    doc["wifiRadioEnabled"] = printerConfig.wifiRadioEnabled;
    doc["diagEnableMqtt"] = printerConfig.diagEnableMqtt;
    doc["diagEnableHa"] = printerConfig.diagEnableHa;
    doc["diagEnableSsdp"] = printerConfig.diagEnableSsdp;
    doc["diagEnableBblScan"] = printerConfig.diagEnableBblScan;

    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
}

void handlePrinterConfigJson(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    // Secrets (WiFi password, access code, WebUI password) are never sent back to the
    // browser - only whether one is stored. Blank fields on submit keep the stored value.
    JsonDocument doc;
    doc["ssid"] = globalVariables.SSID;
    doc["hasPass"] = strlen(globalVariables.APPW) > 0;
    doc["host"] = globalVariables.Host;
    doc["printerIP"] = printerConfig.printerIP;
    doc["printerSerial"] = printerConfig.serialNumber;
    doc["hasAccessCode"] = strlen(printerConfig.accessCode) > 0;
    doc["webUser"] = securityVariables.HTTPUser;
    doc["hasWebPass"] = strlen(securityVariables.HTTPPass) > 0;
    doc["isAPMode"] = (WiFi.getMode() & WIFI_AP);

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleStyleCss(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, style_css_gz_mime, style_css_gz, style_css_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleSubmitNetworking(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    if (!fromOwnPage(request))
    {
        return refuseForeign(request);
    }

    auto getSafeParamInt = [](AsyncWebServerRequest *req, const char *name, int fallback = 0) -> int
    {
        return req->hasParam(name, true) ? req->getParam(name, true)->value().toInt() : fallback;
    };

    // WiFi RF tuning — apply immediately (no reboot needed)
    printerConfig.wifiTxPower = getSafeParamInt(request, "wifiTxPower", 11);
    printerConfig.wifiSleepEnabled = request->hasParam("wifiSleepEnabled", true);
    switch (printerConfig.wifiTxPower) {
        case 2:  WiFi.setTxPower(WIFI_POWER_2dBm); break;
        case 5:  WiFi.setTxPower(WIFI_POWER_5dBm); break;
        case 8:  WiFi.setTxPower(WIFI_POWER_8_5dBm); break;
        case 11: WiFi.setTxPower(WIFI_POWER_11dBm); break;
        case 13: WiFi.setTxPower(WIFI_POWER_13dBm); break;
        case 15: WiFi.setTxPower(WIFI_POWER_15dBm); break;
        case 17: WiFi.setTxPower(WIFI_POWER_17dBm); break;
        default: WiFi.setTxPower(WIFI_POWER_19_5dBm); break;
    }
    WiFi.setSleep(printerConfig.wifiSleepEnabled);
    Serial.printf("[WiFi] TxPower=%d%sdBm Sleep=%s (applied from networking page)\n",
                  printerConfig.wifiTxPower,
                  (printerConfig.wifiTxPower == 8 || printerConfig.wifiTxPower == 19) ? ".5" : "",
                  printerConfig.wifiSleepEnabled ? "ON" : "OFF");

    // Subsystem isolation — take effect after reboot
    bool oldWifiRadio  = printerConfig.wifiRadioEnabled;
    bool oldDiagMqtt   = printerConfig.diagEnableMqtt;
    bool oldDiagHa     = printerConfig.diagEnableHa;
    bool oldDiagSsdp   = printerConfig.diagEnableSsdp;
    bool oldDiagBbl    = printerConfig.diagEnableBblScan;
    printerConfig.wifiRadioEnabled  = request->hasParam("wifiRadioEnabled", true);
    printerConfig.diagEnableMqtt    = request->hasParam("diagEnableMqtt", true);
    printerConfig.diagEnableHa      = request->hasParam("diagEnableHa", true);
    printerConfig.diagEnableSsdp    = request->hasParam("diagEnableSsdp", true);
    printerConfig.diagEnableBblScan = request->hasParam("diagEnableBblScan", true);
    bool diagChanged = (oldWifiRadio != printerConfig.wifiRadioEnabled)
                    || (oldDiagMqtt  != printerConfig.diagEnableMqtt)
                    || (oldDiagHa    != printerConfig.diagEnableHa)
                    || (oldDiagSsdp  != printerConfig.diagEnableSsdp)
                    || (oldDiagBbl   != printerConfig.diagEnableBblScan);

    saveFileSystem();

    if (diagChanged) {
        request->send(200, "text/plain", "OK - restarting to apply subsystem isolation settings");
        LogSerial.println(F("[Web] Subsystem isolation settings changed — restarting"));
        shouldRestart = true;
        restartRequestTime = millis();
        return;
    }

    request->send(200, "text/plain", "OK");
}

void handleSubmitConfig(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    if (!fromOwnPage(request))
    {
        return refuseForeign(request);
    }

    auto getSafeParamValue = [](AsyncWebServerRequest *req, const char *name, const char *fallback = "") -> String
    {
        return req->hasParam(name, true) ? req->getParam(name, true)->value() : fallback;
    };

    auto getSafeParamInt = [](AsyncWebServerRequest *req, const char *name, int fallback = 0) -> int
    {
        return req->hasParam(name, true) ? req->getParam(name, true)->value().toInt() : fallback;
    };

    // Refuse a CA certificate that isn't PEM before anything is changed, rather than save one
    // the TLS connection can't use.
    String haCaCert = getSafeParamValue(request, "haMqttCaCert");
    haCaCert.replace("\r\n", "\n");
    haCaCert.trim();
    if (haCaCert.length() > 6144 ||
        (haCaCert.length() > 0 && haCaCert.indexOf("-----BEGIN CERTIFICATE-----") < 0))
    {
        request->send(400, "text/plain", "The CA certificate must be in PEM format (-----BEGIN CERTIFICATE-----), up to 6 kB.");
        return;
    }

        String newHost = getSafeParamValue(request, "deviceName", globalVariables.Host.c_str());
        newHost.trim();
        if (newHost.length() == 0)
        {
            newHost = "BLLED";
        }

        if (newHost != globalVariables.Host)
        {
            LogSerial.print(F("[MDNS] Updating hostname from "));
            LogSerial.print(globalVariables.Host);
            LogSerial.print(F(" to "));
            LogSerial.println(newHost);

            globalVariables.Host = newHost;

            MDNS.end();
            if (!MDNS.begin(globalVariables.Host.c_str()))
            {
                LogSerial.println(F("[MDNS] Error resetting MDNS responder!"));
            }
            else
            {
                MDNS.addService("http", "tcp", 80); // re-advertise web UI under the new hostname
                LogSerial.print(F("[MDNS] Hostname now: "));
                LogSerial.print(globalVariables.Host);
                LogSerial.println(F(".local"));
            }
        }
    

    printerConfig.brightness = getSafeParamInt(request, "brightnessslider");
    printerConfig.rescanWiFiNetwork = request->hasParam("rescanWiFiNetwork", true);
    printerConfig.maintMode = request->hasParam("maintMode", true);
    printerConfig.discoMode = request->hasParam("discoMode", true);
    printerConfig.replicatestate = request->hasParam("replicateLedState", true);
    printerConfig.runningColor = hex2rgb(getSafeParamValue(request, "runningRGB").c_str(), getSafeParamInt(request, "runningWW"), getSafeParamInt(request, "runningCW"));
    printerConfig.testcolorEnabled = request->hasParam("showtestcolor", true);
    printerConfig.testColor = hex2rgb(getSafeParamValue(request, "testRGB").c_str(), getSafeParamInt(request, "testWW"), getSafeParamInt(request, "testCW"));
    printerConfig.debugwifi = request->hasParam("debugwifi", true);
    printerConfig.finishindication = request->hasParam("finishIndication", true);
    printerConfig.finishColor = hex2rgb(getSafeParamValue(request, "finishColor").c_str(), getSafeParamInt(request, "finishWW"), getSafeParamInt(request, "finishCW"));
    printerConfig.finishExit = !request->hasParam("finishEndTimer", true);
    printerConfig.finishTimeOut = getSafeParamInt(request, "finishTimerMins") * 60000;
    printerConfig.inactivityEnabled = request->hasParam("inactivityEnabled", true);
    printerConfig.inactivityTimeOut = getSafeParamInt(request, "inactivityMins") * 60000;
    printerConfig.debuging = request->hasParam("debuging", true);
    printerConfig.debugingchange = request->hasParam("debugingchange", true);
    printerConfig.mqttdebug = request->hasParam("mqttdebug", true);
    printerVariables.isP1Printer = request->hasParam("p1Printer", true);
    printerVariables.useDoorSwtich = request->hasParam("doorSwitch", true);

    printerConfig.stage14Color = hex2rgb(getSafeParamValue(request, "stage14RGB").c_str(), getSafeParamInt(request, "stage14WW"), getSafeParamInt(request, "stage14CW"));
    printerConfig.stage1Color = hex2rgb(getSafeParamValue(request, "stage1RGB").c_str(), getSafeParamInt(request, "stage1WW"), getSafeParamInt(request, "stage1CW"));
    printerConfig.stage8Color = hex2rgb(getSafeParamValue(request, "stage8RGB").c_str(), getSafeParamInt(request, "stage8WW"), getSafeParamInt(request, "stage8CW"));
    printerConfig.stage9Color = hex2rgb(getSafeParamValue(request, "stage9RGB").c_str(), getSafeParamInt(request, "stage9WW"), getSafeParamInt(request, "stage9CW"));
    printerConfig.stage10Color = hex2rgb(getSafeParamValue(request, "stage10RGB").c_str(), getSafeParamInt(request, "stage10WW"), getSafeParamInt(request, "stage10CW"));
    printerConfig.errordetection = request->hasParam("errorDetection", true);
    printerConfig.wifiRGB = hex2rgb(getSafeParamValue(request, "wifiRGB").c_str(), getSafeParamInt(request, "wifiWW"), getSafeParamInt(request, "wifiCW"));
    printerConfig.pauseRGB = hex2rgb(getSafeParamValue(request, "pauseRGB").c_str(), getSafeParamInt(request, "pauseWW"), getSafeParamInt(request, "pauseCW"));
    printerConfig.firstlayerRGB = hex2rgb(getSafeParamValue(request, "firstlayerRGB").c_str(), getSafeParamInt(request, "firstlayerWW"), getSafeParamInt(request, "firstlayerCW"));
    printerConfig.nozzleclogRGB = hex2rgb(getSafeParamValue(request, "nozzleclogRGB").c_str(), getSafeParamInt(request, "nozzleclogWW"), getSafeParamInt(request, "nozzleclogCW"));
    printerConfig.hmsSeriousRGB = hex2rgb(getSafeParamValue(request, "hmsSeriousRGB").c_str(), getSafeParamInt(request, "hmsSeriousWW"), getSafeParamInt(request, "hmsSeriousCW"));
    printerConfig.hmsFatalRGB = hex2rgb(getSafeParamValue(request, "hmsFatalRGB").c_str(), getSafeParamInt(request, "hmsFatalWW"), getSafeParamInt(request, "hmsFatalCW"));
    printerConfig.filamentRunoutRGB = hex2rgb(getSafeParamValue(request, "filamentRunoutRGB").c_str(), getSafeParamInt(request, "filamentRunoutWW"), getSafeParamInt(request, "filamentRunoutCW"));
    printerConfig.frontCoverRGB = hex2rgb(getSafeParamValue(request, "frontCoverRGB").c_str(), getSafeParamInt(request, "frontCoverWW"), getSafeParamInt(request, "frontCoverCW"));
    printerConfig.nozzleTempRGB = hex2rgb(getSafeParamValue(request, "nozzleTempRGB").c_str(), getSafeParamInt(request, "nozzleTempWW"), getSafeParamInt(request, "nozzleTempCW"));
    printerConfig.bedTempRGB = hex2rgb(getSafeParamValue(request, "bedTempRGB").c_str(), getSafeParamInt(request, "bedTempWW"), getSafeParamInt(request, "bedTempCW"));
    // HMS Error handling
    printerConfig.hmsIgnoreList = getSafeParamValue(request, "hmsIgnoreList");
    // Control Chamber Light
    printerConfig.controlChamberLight = request->hasParam("controlChamberLight", true);

    // === Home Assistant integration & operating mode ===
    bool oldHaEnabled = printerConfig.haEnabled;
    String oldHaHost = printerConfig.haMqttHost;
    int oldHaPort = printerConfig.haMqttPort;
    String oldHaUser = printerConfig.haMqttUser;
    String oldHaPass = printerConfig.haMqttPass;
    bool oldHaTls = printerConfig.haMqttTls;
    String oldHaCaCert = printerConfig.haMqttCaCert;

    printerConfig.ledControlMode = getSafeParamInt(request, "ledControlMode", printerConfig.ledControlMode);
    // haEnabled is derived from mode — any mode that uses HA enables the connection
    printerConfig.haEnabled = (printerConfig.ledControlMode != LED_MODE_PRINTER);
    strlcpy(printerConfig.haMqttHost, getSafeParamValue(request, "haMqttHost").c_str(), sizeof(printerConfig.haMqttHost));
    printerConfig.haMqttPort = getSafeParamInt(request, "haMqttPort", 1883);
    strlcpy(printerConfig.haMqttUser, getSafeParamValue(request, "haMqttUser").c_str(), sizeof(printerConfig.haMqttUser));
    // The broker password is never sent to the page, so a blank field keeps the stored one.
    // Clearing the username removes it.
    String haPass = getSafeParamValue(request, "haMqttPass");
    if (strlen(printerConfig.haMqttUser) == 0)
        printerConfig.haMqttPass[0] = '\0';
    else if (haPass.length() > 0)
        strlcpy(printerConfig.haMqttPass, haPass.c_str(), sizeof(printerConfig.haMqttPass));
    printerConfig.haMqttTls = request->hasParam("haMqttTls", true);
    printerConfig.haMqttCaCert = haCaCert;
    printerConfig.offlineDimEnabled = request->hasParam("offlineDimEnabled", true);
    printerConfig.offlineDimAfterSec = getSafeParamInt(request, "offlineDimAfterSec", 60);
    printerConfig.offlineDimBrightness = getSafeParamInt(request, "offlineDimBrightness", 5);
    haVariables.masterEnable = request->hasParam("haMasterEnable", true);

    if (printerConfig.ledControlMode == LED_MODE_PRINTER)
        haVariables.overrideActive = false;

    // Push these changes (master enable, mode) back to Home Assistant so the HA
    // entities reflect what was set from the web UI instead of going out of sync.
    haVariables.stateDirty = true;

    // If the broker connection settings changed, reboot so the HA MQTT client
    // re-initialises cleanly against the new broker.
    bool haConnChanged = (oldHaEnabled != printerConfig.haEnabled) ||
                         (oldHaHost != String(printerConfig.haMqttHost)) ||
                         (oldHaPort != printerConfig.haMqttPort) ||
                         (oldHaUser != String(printerConfig.haMqttUser)) ||
                         (oldHaPass != String(printerConfig.haMqttPass)) ||
                         (oldHaTls != printerConfig.haMqttTls) ||
                         (oldHaCaCert != printerConfig.haMqttCaCert);

    saveFileSystem();
    LogSerial.println(F("Packet received from setuppage"));
    printerConfig.inactivityStartms = millis();
    printerConfig.isIdleOFFActive = false;
    printerConfig.replicate_update = true;
    printerConfig.maintMode_update = true;
    printerConfig.discoMode_update = true;
    printerConfig.testcolor_update = true;
    updateleds();

    if (haConnChanged)
    {
        request->send(200, "text/plain", "OK - restarting to apply Home Assistant settings");
        LogSerial.println(F("[Web] Home Assistant settings changed — restarting"));
        shouldRestart = true;
        restartRequestTime = millis();
        return;
    }

    request->send(200, "text/plain", "OK");
}

// Apply the master "Enable BLLED Light Control" immediately (no full Save needed),
// persist it, and sync the new state to Home Assistant. A POST from our own page, so a
// link or image on another site can't switch the lights off.
void handleSetEnable(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();
    if (!fromOwnPage(request))
        return refuseForeign(request);

    bool on = true;
    if (request->hasParam("on"))
    {
        String v = request->getParam("on")->value();
        on = (v == "1" || v == "true" || v == "on");
    }

    haVariables.masterEnable = on;
    haVariables.stateDirty = true; // push new state to Home Assistant
    saveFileSystem();              // persist
    updateleds();

    LogSerial.print(F("[Web] BLLED light control "));
    LogSerial.println(on ? F("enabled") : F("disabled"));

    request->send(200, "application/json", String("{\"masterEnable\":") + (on ? "true" : "false") + "}");
}

void sendJsonToAll(JsonDocument &doc)
{
    String jsonString;
    serializeJson(doc, jsonString);
    ws.textAll(jsonString);
}

void handleWiFiScan(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    // Asynchronous scan so we never block the AsyncTCP task. A blocking
    // WiFi.scanNetworks() in this context fails/hangs while connected (STA).
    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_FAILED || n == -2)
    {
        // No scan in progress -> kick one off. Make sure a station interface
        // exists for scanning without tearing down the AP (captive portal).
        wifi_mode_t mode = WiFi.getMode();
        if (mode == WIFI_AP)
            WiFi.mode(WIFI_AP_STA);
        else if (mode == WIFI_OFF)
            WiFi.mode(WIFI_STA);

        WiFi.scanNetworks(true); // async
        request->send(202, "application/json", "{\"scanning\":true}");
        return;
    }

    if (n == WIFI_SCAN_RUNNING || n == -1)
    {
        request->send(202, "application/json", "{\"scanning\":true}");
        return;
    }

    // n >= 0 : results ready
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; ++i)
    {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["bssid"] = WiFi.BSSIDstr(i);
        net["rssi"] = WiFi.RSSI(i);
        net["enc"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? false : true;
    }
    WiFi.scanDelete(); // free results so the next request starts a fresh scan

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleWiFiSetupPage(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    AsyncWebServerResponse *response = request->beginResponse(200, wifiSetup_html_gz_mime, wifiSetup_html_gz, wifiSetup_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleSubmitWiFi(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    if (!fromOwnPage(request))
    {
        return refuseForeign(request);
    }

    auto param = [request](const char *name) -> String
    {
        String value = request->hasParam(name, true) ? request->getParam(name, true)->value() : "";
        value.trim();
        return value;
    };

    // Blank fields keep the stored value, because /config.json no longer sends secrets
    // back to the page.
    String ssid = param("ssid");
    String pass = param("pass");
    String bssid = param("bssid");
    if (ssid.length() > 0 && ssid != globalVariables.SSID && pass.length() == 0)
    {
        // The stored password belongs to the old network; don't silently reuse it.
        request->send(400, "text/plain", "Enter the WiFi password for the new network.");
        return;
    }
    if (ssid.length() > 0)
    {
        LogSerial.print(F("[WiFiSetup] SSID: "));
        LogSerial.println(ssid);
        strlcpy(globalVariables.SSID, ssid.c_str(), sizeof(globalVariables.SSID));
    }
    if (pass.length() > 0)
    {
        LogSerial.println(F("[WiFiSetup] WiFi password updated"));
        strlcpy(globalVariables.APPW, pass.c_str(), sizeof(globalVariables.APPW));
    }
    if (bssid.length() > 0)
        strlcpy(printerConfig.BSSID, bssid.c_str(), sizeof(printerConfig.BSSID));

    globalVariables.Host = param("host");

    String printerIP = param("printerIP");
    String printerSerial = param("printerSerial");
    String accessCode = param("accessCode");
    if (printerIP.length() > 0)
        strlcpy(printerConfig.printerIP, printerIP.c_str(), sizeof(printerConfig.printerIP));
    if (printerSerial.length() > 0)
        strlcpy(printerConfig.serialNumber, printerSerial.c_str(), sizeof(printerConfig.serialNumber));
    if (accessCode.length() > 0)
        strlcpy(printerConfig.accessCode, accessCode.c_str(), sizeof(printerConfig.accessCode));

    // WebUI login: an empty username turns protection off; an empty password keeps the
    // stored one.
    String webUser = param("webUser");
    String webPass = param("webPass");
    strlcpy(securityVariables.HTTPUser, webUser.c_str(), sizeof(securityVariables.HTTPUser));
    if (webUser.length() == 0)
        securityVariables.HTTPPass[0] = '\0';
    else if (webPass.length() > 0)
        strlcpy(securityVariables.HTTPPass, webPass.c_str(), sizeof(securityVariables.HTTPPass));

    saveFileSystem();

    request->send(200, "text/plain", "Settings saved, restarting...");
    shouldRestart = true;
    restartRequestTime = millis();
}

void websocketLoop()
{
    if (ws.count() == 0)
        return;
    if (millis() - lastWsPush > wsPushInterval)
    {
        lastWsPush = millis();

        JsonDocument doc;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["ip"] = WiFi.localIP().toString();
        doc["uptime"] = millis() / 1000;
        doc["doorOpen"] = printerVariables.doorOpen;
        doc["printerConnection"] = printerVariables.online;
        doc["clients"] = ws.count();
        doc["stg_cur"] = printerVariables.stage;
        doc["masterEnable"] = haVariables.masterEnable; // live state so the web toggle reflects HA changes
        doc["haConnected"] = haVariables.connected;
        doc["haConnectState"] = haVariables.haLastConnectState;
        sendJsonToAll(doc);
    }
}

void handleConfigPage(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();
    AsyncWebServerResponse *response = request->beginResponse(200, backupRestore_html_gz_mime, backupRestore_html_gz, backupRestore_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleDownloadConfigFile(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }

    if (!LittleFS.exists(configPath))
    {
        request->send(404, "text/plain", "Config file not found");
        return;
    }

    File configFile = LittleFS.open(configPath, "r");
    if (!configFile)
    {
        request->send(500, "text/plain", "Failed to open config file");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error)
    {
        request->send(500, "text/plain", "Failed to parse config file");
        return;
    }

    String jsonString;
    serializeJsonPretty(doc, jsonString);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonString);
    response->addHeader("Content-Disposition", "attachment; filename=\"blledconfig.json\"");
    request->send(response);
}

void handleWebSerialPage(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();
    AsyncWebServerResponse *response = request->beginResponse(200, webSerialPage_html_gz_mime, webSerialPage_html_gz, webSerialPage_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handlePrinterList(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
    {
        return request->requestAuthentication();
    }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < bblKnownPrinterCount; i++)
    {
        JsonObject obj = arr.add<JsonObject>();
        obj["ip"] = bblLastKnownPrinters[i].ip.toString();
        obj["usn"] = bblLastKnownPrinters[i].usn;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// Force-republish HA MQTT Discovery configs.  Useful when HA's MQTT
// integration was set up after BLLED connected, or when retained messages
// were lost.  Hit GET /haDiscovery from a browser to trigger.
void handleHaRepublishDiscovery(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();

    if (!haVariables.connected)
    {
        request->send(503, "text/plain", "HA broker not connected - check settings");
        return;
    }
    // Signal the haTask to republish — do NOT call haMqtt.publish() here.
    // PubSubClient is not thread-safe; calling it from this async handler
    // while haTask runs haMqtt.loop() concurrently corrupts its state.
    haVariables.discoveryRepublishPending = true;
    request->send(200, "text/plain", "Discovery republished - check Home Assistant");
}

void handleWifiDiag(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();

    JsonDocument doc;
    doc["rssi"] = WiFi.RSSI();
    doc["txPower"] = printerConfig.wifiTxPower;
    doc["sleep"] = WiFi.getSleep();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleDiag(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();

    JsonDocument doc;
    doc["wifiRadioEnabled"]  = printerConfig.wifiRadioEnabled;
    doc["mqttEnabled"]       = printerConfig.diagEnableMqtt;
    doc["haEnabled"]         = printerConfig.diagEnableHa;
    doc["ssdpEnabled"]       = printerConfig.diagEnableSsdp;
    doc["bblScanEnabled"]    = printerConfig.diagEnableBblScan;
    doc["wifiSleep"]         = WiFi.getSleep();
    doc["txPower"]           = printerConfig.wifiTxPower;
    doc["rssi"]              = WiFi.RSSI();
    doc["mqttMsgsPerMin"]    = g_mqttRxMsgsPerMin;
    doc["mqttBytesPerMin"]   = g_mqttRxBytesPerMin;
    doc["freeHeap"]          = ESP.getFreeHeap();    // each TLS connection takes tens of kB
    doc["minFreeHeap"]       = ESP.getMinFreeHeap();
    doc["maxAllocHeap"]      = ESP.getMaxAllocHeap(); // a printer report must fit in one block
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// Opening /factoryreset in a browser shows a confirm button; the reset itself is a POST from
// that page (a link or image on another site can't trigger it).
void handleFactoryResetPage(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();
    request->send(200, "text/html",
                  "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Factory reset</title></head>"
                  "<body style='font-family:sans-serif;padding:16px'><h1>Factory reset</h1>"
                  "<p>This erases all settings and restarts the controller.</p>"
                  "<button id='go'>Erase all settings</button> <p id='msg' role='status'></p>"
                  "<script>document.getElementById('go').onclick=async()=>{const m=document.getElementById('msg');"
                  "try{const r=await fetch('/factoryreset',{method:'POST',headers:{'X-BLLED':'1'}});m.textContent=await r.text();}"
                  "catch(e){m.textContent='Device is likely restarting.';}};</script></body></html>");
}

void handleFactoryReset(AsyncWebServerRequest *request)
{
    if (!isAuthorized(request))
        return request->requestAuthentication();
    if (!fromOwnPage(request))
        return refuseForeign(request);

    LogSerial.println(F("[FactoryReset] Performing full reset..."));

    deleteFileSystem(); // delete LittleFS config
    request->send(200, "text/plain", "Factory reset complete. Restarting...");

    shouldRestart = true;
    restartRequestTime = millis();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        LogSerial.printf("[WS] Client connected: %u\n", client->id());
        websocketLoop();
        break;
    case WS_EVT_DISCONNECT:
        LogSerial.printf("[WS] Client disconnected: %u\n", client->id());
        ws.cleanupClients();
        break;
    case WS_EVT_DATA:
        LogSerial.printf("[WS] Data received from client %u\n", client->id());
        break;
    case WS_EVT_PONG:
        LogSerial.printf("[WS] Pong received from %u\n", client->id());
        break;
    case WS_EVT_ERROR:
        LogSerial.printf("[WS] Error on connection %u\n", client->id());
        ws.cleanupClients();
        break;
    }
}

// What to tell the person when a firmware upload fails. The running firmware is untouched:
// the image goes to the other update slot and is only booted once it is complete and valid.
String otaErrorMessage(uint8_t error)
{
    switch (error)
    {
    case UPDATE_ERROR_SPACE:
        return F("This firmware is larger than this controller's update slot. Flash the full .bin "
                 "once over USB to switch to the larger flash layout. Nothing was changed.");
    case UPDATE_ERROR_MAGIC_BYTE:
        return F("This is not a firmware update image. Use the .bin.ota file; the full .bin is for "
                 "USB flashing. Nothing was changed.");
    default:
        return String("Update failed: ") + Update.errorString() + ". Nothing was changed.";
    }
}

void setupWebserver()
{
    // Without mDNS the web UI is still reachable by IP, so carry on: stopping here would leave
    // the controller unreachable until it is reflashed over USB.
    if (!MDNS.begin(globalVariables.Host.c_str()))
    {
        LogSerial.println(F("[MDNS] Error setting up MDNS responder - web UI only reachable by IP"));
    }
    else
    {
        // Advertise the web UI over mDNS so it is reachable at http://<host>.local
        // (default http://blled.local) and discoverable by network/HA scanners.
        MDNS.addService("http", "tcp", 80);
        LogSerial.print(F("[MDNS] Web UI available at http://"));
        LogSerial.print(globalVariables.Host);
        LogSerial.println(F(".local"));
    }

    LogSerial.println(F("Setting up webserver"));

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
    if (WiFi.getMode() == WIFI_AP) {
        LogSerial.println(F("[WebServer] Captive Portal activ – redirect to /wifi"));
        request->redirect("/wifi");
    } else {
        handleSetup(request);
    } });
    webServer.on("/fwupdate", HTTP_GET, handleUpdatePage);
    webServer.on("/getConfig", HTTP_GET, handleGetConfig);
    webServer.on("/submitConfig", HTTP_POST, handleSubmitConfig);
    webServer.on("/submitNetworking", HTTP_POST, handleSubmitNetworking);
    webServer.on("/setEnable", HTTP_POST, handleSetEnable);
    webServer.on("/blled.svg", HTTP_GET, handleGetIcon);
    webServer.on("/favicon.ico", HTTP_GET, handleGetfavicon);
    webServer.on("/particleCanvas.js", HTTP_GET, handleGetPCC);
    webServer.on("/config.json", HTTP_GET, handlePrinterConfigJson);
    webServer.on("/wifi", HTTP_GET, handleWiFiSetupPage);
    webServer.on("/wifiScan", HTTP_GET, handleWiFiScan);
    webServer.on("/submitWiFi", HTTP_POST, handleSubmitWiFi);
    webServer.on("/style.css", HTTP_GET, handleStyleCss);
    webServer.on("/backuprestore", HTTP_GET, handleConfigPage);
    webServer.on("/configfile.json", HTTP_GET, handleDownloadConfigFile);
    webServer.on("/webserial", HTTP_GET, handleWebSerialPage);
    webServer.on("/printerList", HTTP_GET, handlePrinterList);
    webServer.on("/factoryreset", HTTP_GET, handleFactoryResetPage);
    webServer.on("/factoryreset", HTTP_POST, handleFactoryReset);
    webServer.on("/haDiscovery", HTTP_GET, handleHaRepublishDiscovery);
    webServer.on("/wifi_diag", HTTP_GET, handleWifiDiag);
    webServer.on("/diag", HTTP_GET, handleDiag);
    // Uploads: the upload callback runs before the completion handler, so the login has
    // to be checked there too or the data is written before anyone is authenticated.
    webServer.on("/configrestore", HTTP_POST, [](AsyncWebServerRequest *request)
                 {
        if (!isAuthorized(request)) {
            return request->requestAuthentication();
        }
        if (!fromOwnPage(request)) {
            return refuseForeign(request);
        }
        // The upload callback leaves its verdict in _tempObject: NULL = no file arrived,
        // "" = accepted, otherwise why the file was refused.
        const char *refused = (const char *)request->_tempObject;
        if (refused == NULL || refused[0] != '\0') {
            request->send(400, "text/plain", refused ? refused : "No config file received. Nothing was changed.");
            return;
        }
        request->send(200, "text/plain", "Config uploaded. Restarting...");
        shouldRestart = true;
        restartRequestTime = millis(); }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                 {
        // The upload goes to a temporary file and only replaces the config once it parses as
        // one, so a wrong or cut-off file can't wipe the settings.
        auto refuse = [request](const char *why) {
            LogSerial.printf("[ConfigUpload] Refused: %s\n", why);
            if (request->_tempFile) {
                request->_tempFile.close();
                LittleFS.remove(configUploadPath);
            }
            request->_tempObject = strdup(why);
        };

        if (!index) {
            if (!(isAuthorized(request) && fromOwnPage(request))) {
                return refuse("Not authorized, or not from the controller's pages.");
            }
            LogSerial.printf("[ConfigUpload] Start: %s\n", filename.c_str());
            request->_tempFile = LittleFS.open(configUploadPath, "w");
            if (!request->_tempFile) {
                return refuse("Could not write to the controller's storage. Nothing was changed.");
            }
        }
        if (request->_tempObject != NULL) {
            return; // already refused
        }
        if (index + len > configUploadMaxBytes) {
            return refuse("File is too large to be a BLLED config. Nothing was changed.");
        }
        request->_tempFile.write(data, len);
        if (final) {
            request->_tempFile.close();
            if (!isValidConfigFile(configUploadPath)) {
                LittleFS.remove(configUploadPath);
                return refuse("Not a BLLED config file. Nothing was changed.");
            }
            // littlefs replaces the target in one step; remove it first only if that fails.
            if (!LittleFS.rename(configUploadPath, configPath)) {
                LittleFS.remove(configPath);
                LittleFS.rename(configUploadPath, configPath);
            }
            LogSerial.println(F("[ConfigUpload] Finished"));
            request->_tempObject = strdup("");
        } });

    webServer.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
                 {
        if (!isAuthorized(request)) {
            return request->requestAuthentication();
        }
        if (!fromOwnPage(request)) {
            return refuseForeign(request);
        }
        // As for /configrestore: NULL = no complete image arrived, "" = written, otherwise
        // why it failed. Only a written image restarts the controller.
        const char *failed = (const char *)request->_tempObject;
        if (failed == NULL || failed[0] != '\0') {
            request->send(500, "text/plain", failed ? failed : "No complete firmware received. Nothing was changed.");
            return;
        }
        request->send(200, "text/plain", "OK");
        LogSerial.println(F("OTA Upload done. Marking for restart."));
        shouldRestart = true;
        restartRequestTime = millis(); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                 {
        static bool otaAuthorized = false;

        if (!index) {
            otaAuthorized = isAuthorized(request) && fromOwnPage(request);
            if (!otaAuthorized) {
                LogSerial.println(F("[OTA] Rejected: not authorized, or not from the controller's pages"));
                return;
            }
            LogSerial.printf("[OTA] Start: %s\n", filename.c_str());
            if (Update.isRunning()) {
                Update.abort(); // an earlier upload was cut off; begin() would refuse
            }
            Update.begin(UPDATE_SIZE_UNKNOWN);
        }
        if (!otaAuthorized || request->_tempObject != NULL) {
            return;
        }

        if (!Update.hasError()) {
            Update.write(data, len);
        }
        if (final && !Update.hasError()) {
            Update.end(true);
        }

        if (Update.hasError()) {
            Update.printError(LogSerial);
            request->_tempObject = strdup(otaErrorMessage(Update.getError()).c_str());
        } else if (final) {
            LogSerial.printf("[OTA] Success (%u bytes). Awaiting reboot...\n", index + len);
            request->_tempObject = strdup("");
        } });

    // The log can contain the printer IP/serial; protect it like the other pages.
    LogSerial.setAuthentication(securityVariables.HTTPUser, securityVariables.HTTPPass);
    LogSerial.begin(&webServer);

    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);

    webServer.begin();

    LogSerial.println(F("Webserver started"));
}

#endif
