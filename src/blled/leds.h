#ifndef _LED
#define _LED

#include <Arduino.h>
#include "mqttparsingutility.h"
void controlChamberLight(bool on); // Forward declaration

const int redPin = 19;
const int greenPin = 18;
const int bluePin = 21;
const int warmPin = 22;
const int coldPin = 23;

const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
const int warmChannel = 3;
const int coldChannel = 4;

const int pwmFreq = 5000;
const int pwmResolution = 8; // 8-bit PWM = 0-255

int currentRed = 0;
int currentGreen = 0;
int currentBlue = 0;
int currentWarm = 0;
int currentCold = 0;

// The colour tweenToColor() last drew, before brightness, so a brightness change can redraw
// it. False once something else (Home Assistant, offline dim, RGB cycle) has set the strip directly.
int brightTargetRed = 0;
int brightTargetGreen = 0;
int brightTargetBlue = 0;
int brightTargetWarm = 0;
int brightTargetCold = 0;
bool brightTargetShown = false;

// What is driving the strip right now, in the same words as the log. Shown in Home Assistant.
const char *ledSource = "Starting";
const char *brightTargetSource = "Starting"; // ledSource for the brightTarget colour

// Counts tweenToColor() and setLedsRaw() calls, so redrawLeds() can tell whether
// updateleds() drew anything.
unsigned long ledDrawCount = 0;

// Names what is driving the strip. A name given for a colour tweenToColor() drew stays
// with that colour, for redrawLeds() to put back.
void setLedSource(const char *source)
{
    ledSource = source;
    if (brightTargetShown)
        brightTargetSource = source;
}

unsigned long lastUpdatems = 0;
unsigned long oldms = 0;

COLOR hex2rgb(String hex, short ww_value = 0, short cw_value = 0)
{
    COLOR color;
    long hexcolor;
    strcpy(color.RGBhex, hex.c_str());
    if (hex.charAt(0) == '#')
    {
        hex.remove(0, 1);
    }
    while (hex.length() != 6)
    {
        hex += "0";
    }
    hexcolor = strtol(hex.c_str(), NULL, 16);
    color.r = (hexcolor & 0xFF0000) >> 16;
    color.g = (hexcolor & 0x00FF00) >> 8;
    color.b = (hexcolor & 0x0000FF);
    color.ww = ww_value;
    color.cw = cw_value;

    return color;
}

// Mirror the ACTUAL strip output to Home Assistant. Called whenever the strip
// changes (printer-driven or HA-driven) so the HA light entity reflects what is
// physically lit and can always be toggled from HA.
void markHaReported()
{
    int rgbMax = currentRed;
    if (currentGreen > rgbMax) rgbMax = currentGreen;
    if (currentBlue > rgbMax) rgbMax = currentBlue;
    int allMax = rgbMax;
    if (currentWarm > allMax) allMax = currentWarm;
    if (currentCold > allMax) allMax = currentCold;

    int whiteMax = currentWarm;
    if (currentCold > whiteMax) whiteMax = currentCold;

    bool on = (allMax > 0);
    bool isTemp = false;
    short rr = 0, gg = 0, bb = 0;
    int mireds = haVariables.reportedMireds;

    if (rgbMax > 0)
    {
        // RGB takes priority: normalise so HA shows the pure hue, brightness carries level.
        rr = (short)((long)currentRed * 255 / rgbMax);
        gg = (short)((long)currentGreen * 255 / rgbMax);
        bb = (short)((long)currentBlue * 255 / rgbMax);
    }
    else if (whiteMax > 0)
    {
        // Warm/cold white -> report as a colour temperature so HA shows the white tint.
        isTemp = true;
        int total = currentWarm + currentCold;
        float warmFrac = total > 0 ? (float)currentWarm / (float)total : 0.5f;
        mireds = HA_MIRED_MIN + (int)(warmFrac * (HA_MIRED_MAX - HA_MIRED_MIN));
    }

    if (on != haVariables.reportedOn || isTemp != haVariables.reportedIsTemp ||
        rr != haVariables.reportedR || gg != haVariables.reportedG || bb != haVariables.reportedB ||
        mireds != haVariables.reportedMireds || allMax != haVariables.reportedBrightness)
    {
        haVariables.reportedOn = on;
        haVariables.reportedIsTemp = isTemp;
        haVariables.reportedR = rr;
        haVariables.reportedG = gg;
        haVariables.reportedB = bb;
        haVariables.reportedMireds = mireds;
        haVariables.reportedBrightness = allMax;
        haVariables.stateDirty = true; // HA task will publish it
    }
}

void tweenToColor(int targetRed, int targetGreen, int targetBlue, int targetWarm, int targetCold, int duration = 500)
{
    ledDrawCount++;
    brightTargetRed = targetRed;
    brightTargetGreen = targetGreen;
    brightTargetBlue = targetBlue;
    brightTargetWarm = targetWarm;
    brightTargetCold = targetCold;
    brightTargetShown = true;

    float brightness = (float)printerConfig.brightness / 100.0;

    /*     int brightenedRed = round(targetRed * brightness);
        int brightenedGreen = round(targetGreen * brightness);
        int brightenedBlue = round(targetBlue * brightness);
        int brightenedWarm = round(targetWarm * brightness);
        int brightenedCold = round(targetCold * brightness); */
    //round implizit downward
    int brightenedRed = (int)(targetRed * brightness);
    int brightenedGreen = (int)(targetGreen * brightness);
    int brightenedBlue = (int)(targetBlue * brightness);
    int brightenedWarm = (int)(targetWarm * brightness);
    int brightenedCold = (int)(targetCold * brightness);

    if (brightenedRed == currentRed && brightenedGreen == currentGreen && brightenedBlue == currentBlue && brightenedWarm == currentWarm && brightenedCold == currentCold)
    {
        // Already set to the requested color
        if (printerConfig.debuging)
        {
            LogSerial.print(F("LEDS already at color: ("));
            LogSerial.print(currentRed);
            LogSerial.print(F(", "));
            LogSerial.print(currentGreen);
            LogSerial.print(F(", "));
            LogSerial.print(currentBlue);
            LogSerial.print(F(", "));
            LogSerial.print(currentWarm);
            LogSerial.print(F(", "));
            LogSerial.print(currentCold);
            LogSerial.print(F(" Brightness: "));
            LogSerial.print(printerConfig.brightness);
            LogSerial.println(F(")"));
        };
        return;
    }
    float stepTime = (float)duration / 255.0;
    int redStep = (brightenedRed - currentRed) / 255;
    int greenStep = (brightenedGreen - currentGreen) / 255;
    int blueStep = (brightenedBlue - currentBlue) / 255;
    int warmStep = (brightenedWarm - currentWarm) / 255;
    int coldStep = (brightenedCold - currentCold) / 255;

    for (int i = 0; i < 256; i++)
    {
        currentRed += redStep;
        currentGreen += greenStep;
        currentBlue += blueStep;
        currentWarm += warmStep;
        currentCold += coldStep;

        ledcWrite(redChannel, currentRed);
        ledcWrite(greenChannel, currentGreen);
        ledcWrite(blueChannel, currentBlue);
        ledcWrite(warmChannel, currentWarm);
        ledcWrite(coldChannel, currentCold);
        delay(stepTime);
    }

    currentRed = brightenedRed;
    currentGreen = brightenedGreen;
    currentBlue = brightenedBlue;
    currentWarm = brightenedWarm;
    currentCold = brightenedCold;

    ledcWrite(redChannel, currentRed);
    ledcWrite(greenChannel, currentGreen);
    ledcWrite(blueChannel, currentBlue);
    ledcWrite(warmChannel, currentWarm);
    ledcWrite(coldChannel, currentCold);
    markHaReported();
}

// Helper functions to allow changes colors by using a string or integer hex code
void tweenToColor(COLOR targetcolor)
{
    tweenToColor(targetcolor.r, targetcolor.g, targetcolor.b, targetcolor.ww, targetcolor.cw);
}
// Example:  tweenToColor("#33FD67")
void tweenToColor(String strTargetColor, short ww_value = 0, short cw_value = 0)
{
    COLOR targetcolor;
    targetcolor = hex2rgb(strTargetColor, ww_value, cw_value);
    tweenToColor(targetcolor.r, targetcolor.g, targetcolor.b, targetcolor.ww, targetcolor.cw);
}
// Example:  tweenToColor(0xFFACA5)
/* void tweenToColor(int hexValue, short ww_value = 0, short cw_value = 0)
{
    COLOR targetcolor;
    targetcolor.r = ((hexValue >> 16) & 0xFF) / 255.0;
    targetcolor.g = ((hexValue >> 8) & 0xFF) / 255.0;
    targetcolor.b = ((hexValue) & 0xFF) / 255.0;
    targetcolor.ww = ww_value;
    targetcolor.cw = cw_value;
    tweenToColor(targetcolor.r, targetcolor.g, targetcolor.b, targetcolor.ww, targetcolor.cw);
} */
void tweenToColor(int hexValue, short ww_value = 0, short cw_value = 0)
{
    COLOR targetcolor;
    targetcolor.r = (hexValue >> 16) & 0xFF;
    targetcolor.g = (hexValue >> 8) & 0xFF;
    targetcolor.b = hexValue & 0xFF;
    targetcolor.ww = ww_value;
    targetcolor.cw = cw_value;
    tweenToColor(targetcolor.r, targetcolor.g, targetcolor.b, targetcolor.ww, targetcolor.cw);
}
// ---------------------------------------------------------------------------
// Raw LED setter (used by the Home Assistant integration and the offline-dim
// indicator). Unlike tweenToColor() it does NOT apply the global
// printerConfig.brightness multiplier - the caller passes final 0-255 values.
// ---------------------------------------------------------------------------
void setLedsRaw(int targetRed, int targetGreen, int targetBlue, int targetWarm, int targetCold, int duration = 300)
{
    ledDrawCount++;
    brightTargetShown = false;
    targetRed = constrain(targetRed, 0, 255);
    targetGreen = constrain(targetGreen, 0, 255);
    targetBlue = constrain(targetBlue, 0, 255);
    targetWarm = constrain(targetWarm, 0, 255);
    targetCold = constrain(targetCold, 0, 255);

    if (targetRed == currentRed && targetGreen == currentGreen && targetBlue == currentBlue &&
        targetWarm == currentWarm && targetCold == currentCold)
    {
        return; // already there - nothing to do
    }

    float stepTime = (float)duration / 255.0;
    int redStep = (targetRed - currentRed) / 255;
    int greenStep = (targetGreen - currentGreen) / 255;
    int blueStep = (targetBlue - currentBlue) / 255;
    int warmStep = (targetWarm - currentWarm) / 255;
    int coldStep = (targetCold - currentCold) / 255;

    for (int i = 0; i < 256; i++)
    {
        currentRed += redStep;
        currentGreen += greenStep;
        currentBlue += blueStep;
        currentWarm += warmStep;
        currentCold += coldStep;
        ledcWrite(redChannel, currentRed);
        ledcWrite(greenChannel, currentGreen);
        ledcWrite(blueChannel, currentBlue);
        ledcWrite(warmChannel, currentWarm);
        ledcWrite(coldChannel, currentCold);
        delay(stepTime);
    }

    currentRed = targetRed;
    currentGreen = targetGreen;
    currentBlue = targetBlue;
    currentWarm = targetWarm;
    currentCold = targetCold;
    ledcWrite(redChannel, currentRed);
    ledcWrite(greenChannel, currentGreen);
    ledcWrite(blueChannel, currentBlue);
    ledcWrite(warmChannel, currentWarm);
    ledcWrite(coldChannel, currentCold);
    markHaReported();
}

// Severity of the HMS code the printer last reported: 1 = Fatal, 2 = Serious, 3 = Common,
// 4 = Info, 0 = none. Worked out from the code rather than parsedHMSlevel, because the
// printer task replaces that String while the Home Assistant task reads this.
static int hmsSeverity()
{
    if (!printerVariables.hmsstate)
        return 0;
    return (int)((printerVariables.parsedHMScode & 0xFFFFFFFF) >> 16);
}

// Returns true when the printer reports an error BLLED shows in an error colour,
// whether or not error detection is switched on.
static bool isPrinterErrorPresent()
{
    if (printerVariables.stage == 6  || printerVariables.overridestage == 6)  return true; // filament runout
    if (printerVariables.stage == 17 || printerVariables.overridestage == 17) return true; // front cover
    if (printerVariables.stage == 20 || printerVariables.overridestage == 20) return true; // nozzle temp
    if (printerVariables.stage == 21 || printerVariables.overridestage == 21) return true; // bed temp
    int severity = hmsSeverity();
    return severity == 1 || severity == 2; // Fatal or Serious
}

// Returns true when a printer error is active that should override any HA colour
// in Hybrid mode (so the error colour is always visible).
static bool isPrinterErrorActive()
{
    return printerConfig.errordetection && isPrinterErrorPresent();
}

// Apply the Home Assistant light state to the strip. HA owns brightness here,
// so we scale the stored RGB colour by the HA brightness (0-100%).
void applyHaLight()
{
    if (!haVariables.lightOn || haVariables.brightness <= 0)
    {
        setLedsRaw(0, 0, 0, 0, 0, haVariables.transitionMs);
        return;
    }
    float scale = (float)haVariables.brightness / 100.0;

    if (haVariables.colorMode == 1)
    {
        // Colour-temperature mode -> drive the warm/cold white channels.
        // More mireds = warmer = more warm white.
        float warmFrac = (float)(haVariables.colorTempMireds - HA_MIRED_MIN) /
                         (float)(HA_MIRED_MAX - HA_MIRED_MIN);
        warmFrac = constrain(warmFrac, 0.0f, 1.0f);
        int ww = (int)(255 * scale * warmFrac);
        int cw = (int)(255 * scale * (1.0f - warmFrac));
        setLedsRaw(0, 0, 0, ww, cw, haVariables.transitionMs);
    }
    else
    {
        // RGB mode -> drive the colour channels.
        setLedsRaw((int)(haVariables.r * scale),
                   (int)(haVariables.g * scale),
                   (int)(haVariables.b * scale),
                   0, 0, haVariables.transitionMs);
    }
}

// Dim "connecting" indicator shown when the printer is unreachable. The MQTT
// task keeps retrying the connection in the background, so this is purely a
// low-power visual hint rather than a stuck full-brightness colour.
void applyOfflineDim()
{
    if (printerConfig.offlineDimBrightness <= 0)
    {
        setLedsRaw(0, 0, 0, 0, 0);
        return;
    }
    float scale = (float)printerConfig.offlineDimBrightness / 100.0;
    setLedsRaw((int)(printerConfig.wifiRGB.r * scale),
               (int)(printerConfig.wifiRGB.g * scale),
               (int)(printerConfig.wifiRGB.b * scale),
               0, 0);
}

float hue = 0.0;

void RGBCycle()
{
    if (printerConfig.discoMode == false)
    {
        // Skip changing colors if RGB Mode not enabled
        printerConfig.discoMode_update = true;
        return;
    }
    if (printerConfig.discoMode_update)
    {
        printerConfig.discoMode_update = false;
        if (printerConfig.debugingchange)
        {
            LogSerial.print(F("["));
            LogSerial.print(millis());
            LogSerial.print(F("]"));
            LogSerial.println(F(" ** RGB Cycle Mode **"));
        };
    }

    // Leave the strip alone while it is disabled or Home Assistant has it (same rules as updateleds()).
    if (!haVariables.masterEnable || printerConfig.ledControlMode == LED_MODE_HA ||
        (printerConfig.ledControlMode == LED_MODE_HYBRID && haVariables.overrideActive && !isPrinterErrorActive()))
    {
        return;
    }
    brightTargetShown = false; // the cycle, not tweenToColor()'s last colour, is on the strip

    if (printerVariables.online == false)
    {
        ledcWrite(redChannel, 0);
        ledcWrite(greenChannel, 0);
        ledcWrite(blueChannel, 0);
        ledcWrite(warmChannel, 0);
        ledcWrite(coldChannel, 0);
        return;
    }
    hue += 0.1;
    if (hue > 360.0)
    {
        hue = 0.0;
    }

    // A channel is off while its cosine is negative; a negative duty would wrap to a huge one.
    float brightness = (float)printerConfig.brightness / 100.0;
    currentRed = (int)(fmax(0.0, cos(hue * 3.14 / 180.0)) * 255 * brightness);
    currentGreen = (int)(fmax(0.0, cos((hue + 120.0) * 3.14 / 180.0)) * 255 * brightness);
    currentBlue = (int)(fmax(0.0, cos((hue + 240.0) * 3.14 / 180.0)) * 255 * brightness);
    currentWarm = 0;
    currentCold = 0;

    ledcWrite(redChannel, currentRed);
    ledcWrite(greenChannel, currentGreen);
    ledcWrite(blueChannel, currentBlue);
    ledcWrite(warmChannel, currentWarm);
    ledcWrite(coldChannel, currentCold);
}

/* void printLogs(String Desc, COLOR thisColor)
{
    if (printerConfig.debuging || printerConfig.debugingchange)
    {
        LogSerial.print(Desc);
        LogSerial.print(F(" - Turning LEDs to: "));
        if ((thisColor.r + thisColor.g + thisColor.b + thisColor.ww + thisColor.cw) == 0)
        {
            LogSerial.println(F(" OFF"));
            return;
        }
        LogSerial.print(F(" r:"));
        LogSerial.print(thisColor.r);
        LogSerial.print(F(" g:"));
        LogSerial.print(thisColor.g);
        LogSerial.print(F(" b:"));
        LogSerial.print(thisColor.b);
        LogSerial.print(F(" ww:"));
        LogSerial.print(thisColor.ww);
        LogSerial.print(F(" cw:"));
        LogSerial.print(thisColor.cw);
        LogSerial.print(F(" Brightness: "));
        LogSerial.println(printerConfig.brightness);
    };
} */
void printLogs(const char *Desc, COLOR thisColor)
{
    static COLOR lastColor = {-1, -1, -1, -1, -1};
    static const char *lastDesc = "";
    static unsigned long lastPrintTime = 0;

    setLedSource(Desc);

    // Skip if same state and printed less than 3 seconds ago
    if (strcmp(Desc, lastDesc) == 0 &&
        memcmp(&thisColor, &lastColor, sizeof(COLOR)) == 0 &&
        millis() - lastPrintTime < 3000)
    {
        return;
    }

    if (printerConfig.debuging || printerConfig.debugingchange)
    {
        LogSerial.printf("%s - Turning LEDs to:", Desc);
        if ((thisColor.r + thisColor.g + thisColor.b + thisColor.ww + thisColor.cw) == 0)
        {
            LogSerial.println(" OFF");
        }
        else
        {
            LogSerial.printf(" r:%d g:%d b:%d ww:%d cw:%d Brightness: %d\n",
                             thisColor.r, thisColor.g, thisColor.b,
                             thisColor.ww, thisColor.cw, printerConfig.brightness);
        }
    }

    lastColor = thisColor;
    lastDesc = Desc;
    lastPrintTime = millis();
}

void printLogs(const char *Desc, short r, short g, short b, short ww, short cw)
{
    COLOR tempColor;
    tempColor.r = r;
    tempColor.g = g;
    tempColor.b = b;
    tempColor.ww = ww;
    tempColor.cw = cw;
    printLogs(Desc, tempColor);
}
void updateleds()
{
    // Prevent replicate OFF immediately after door event
    if ((millis() - printerVariables.lastdoorOpenms) < 1000 || (millis() - printerVariables.lastdoorClosems) < 1000)
    {
        printerConfig.replicate_update = false;
    }

    // Log-once trackers so the auto-off / dim states don't spam the console when
    // updateleds() is re-evaluated periodically.
    static bool masterOffLogged = false;
    static bool offlineDimLogged = false;
    if (printerVariables.online)
        offlineDimLogged = false; // allow logging again on the next offline episode

    // === BLLED operating mode + Home Assistant override =========================
    // "Enable BLLED Light Control" (master enable) forces the strip OFF regardless
    // of everything else - this is what an HA automation toggles to fully disable
    // the LEDs (e.g. when the printer is powered down).
    if (haVariables.masterEnable == false)
    {
        setLedsRaw(0, 0, 0, 0, 0);
        setLedSource("BLLED control disabled - OFF");
        if (!masterOffLogged)
        {
            printLogs("BLLED control disabled - OFF", 0, 0, 0, 0, 0);
            masterOffLogged = true;
        }
        return;
    }
    masterOffLogged = false;
    // HA-only mode: the printer state is ignored, Home Assistant fully owns the strip.
    if (printerConfig.ledControlMode == LED_MODE_HA)
    {
        applyHaLight();
        setLedSource("Home Assistant");
        return;
    }
    // Hybrid mode: if HA has issued a command it overrides the printer until the
    // next significant printer event (print start) clears the override.
    // Active printer errors always take priority so the error colour is shown
    // even when HA has set a colour.
    if (printerConfig.ledControlMode == LED_MODE_HYBRID && haVariables.overrideActive
        && !isPrinterErrorActive())
    {
        applyHaLight();
        setLedSource("Home Assistant override");
        return;
    }
    // Otherwise (Printer-only mode, or Hybrid with no active override) fall through
    // to the original printer-driven logic below.
    // ===========================================================================

    // Maintenance Mode - White lights on regardless of printer power, WiFi or MQTT connection
    // priortised over Wifi Strength Display or Custom TEST color
    if (printerConfig.maintMode && printerConfig.maintMode_update)
    {
        tweenToColor(0, 0, 0, 255, 255); // WHITE
        printerConfig.maintMode_update = false;
        printLogs("Maintenance Mode", 0, 0, 0, 255, 255);
        LogSerial.print(F("["));
        LogSerial.print(millis());
        LogSerial.print(F("]"));
        LogSerial.println(F(" ** Maintenance Mode **"));
        return;
    }

    // Use LED to show WIFI Strength (enabled via Setup Webpage)
    if (printerConfig.debugwifi)
    {
        //<=-50 dBm Green, <= -60 dBm LightGreen, <= -70 dBm Yellow, <= -80 dBm Orange, >80 Red
        if (WiFi.status() == WL_CONNECTED)
        {
            long wifiNow = WiFi.RSSI();
            if (printerConfig.debugingchange)
            {
                LogSerial.print(F("WiFi Strength Visialisation, display LEDs for: "));
                LogSerial.println(wifiNow);
            }
            if (wifiNow >= -50)
                tweenToColor(0, 255, 0, 0, 0); // GREEN
            else if (wifiNow >= -60)
                tweenToColor(128, 255, 0, 0, 0); // LIGHTGREEN
            else if (wifiNow >= -70)
                tweenToColor(255, 255, 0, 0, 0); // YELLOW
            else if (wifiNow >= -80)
                tweenToColor(255, 128, 0, 0, 0); // ORANGE
            else if (wifiNow < -80)
                tweenToColor(255, 0, 0, 0, 0); // RED
            else
                tweenToColor(0, 0, 255, 0, 0); // BLUE
        };
        setLedSource("WiFi Strength");
        return;
    }

    // TEST Color Enabled - LED ON regardless of printer state
    if (printerConfig.testcolorEnabled && printerConfig.testcolor_update)
    {
        tweenToColor(printerConfig.testColor); // Variable Test Color
        printLogs("LED Test ON", printerConfig.testColor);
        LogSerial.print(F("["));
        LogSerial.print(millis());
        LogSerial.print(F("]"));
        LogSerial.println(F(" ** Test Color Mode **"));
        printerConfig.testcolor_update = false;
        return;
    }

    // From here the BBLP status sets the colors
    if (printerConfig.debuging == true)
    {
        /*         LogSerial.println(F("Updating LEDs"));

                LogSerial.println(printerVariables.stage);
                LogSerial.println(printerVariables.gcodeState);
                LogSerial.println(printerVariables.printerledstate);
                LogSerial.println(printerVariables.hmsstate);
                LogSerial.println(printerVariables.parsedHMSlevel); */

        char ledDbgStr[128];
        snprintf(ledDbgStr, sizeof(ledDbgStr),
                 "[LED] Stage:%d gcodeState:%s printerLedState:%s HMSErr:%s ParsedHMS:%s",
                 printerVariables.stage,
                 printerVariables.gcodeState.c_str(),
                 printerVariables.printerledstate ? "true" : "false",
                 printerVariables.hmsstate ? "true" : "false",
                 printerVariables.parsedHMSlevel.c_str());
        LogSerial.println(ledDbgStr);
    }

    // Initial Boot
    if (printerVariables.initalisedLEDs == false)
    {
        printerVariables.initalisedLEDs = true;     // Run once per boot
        printerConfig.inactivityStartms = millis(); // restart idle timer
        printerConfig.isIdleOFFActive = false;
        printerVariables.waitingForDoor = false;
        printerConfig.finish_check = false;
        printerVariables.lastdoorClosems = millis();
        LogSerial.println(F("Initial Boot"));
        return;
    }

    if (printerConfig.testcolorEnabled || printerConfig.maintMode || printerConfig.debugwifi || printerConfig.discoMode)
    {
        // Skip trying to set a color as it's in one of the override states
        if (printerConfig.discoMode)
        {
            brightTargetShown = false; // RGBCycle() draws over the others from now on
            setLedSource("RGB Cycle Mode");
        }
        else if (printerConfig.maintMode)
            setLedSource("Maintenance Mode");
        else if (printerConfig.testcolorEnabled)
            setLedSource("LED Test ON");
        return;
    }

    // TOGGLE LIGHTS via DOOR
    // If door is closed twice in 6 seconds, it will flip the state of the lights
    /*     if (printerVariables.doorSwitchTriggered == true)
        {
            if (printerConfig.debugingchange)
            {
                LogSerial.print(F("Door closed twice within 6 seconds - Toggling LEDs to "));
            }
            if (currentWarm == 0 && currentCold == 0)
            {
                tweenToColor(0, 0, 0, 255, 255); // WHITE
                if (printerConfig.debuging || printerConfig.debugingchange)
                {
                    LogSerial.println(F("ON"));
                }
                printerConfig.isIdleOFFActive = false;
            }
            else
            {
                tweenToColor(0, 0, 0, 0, 0); // OFF
                // Shortcut to idle state - note: light will go back on immediately if there is an MQTT change of any sort
                printerConfig.isIdleOFFActive = true;
                printerConfig.inactivityStartms = (millis() - printerConfig.inactivityTimeOut);
                if (printerConfig.debuging || printerConfig.debugingchange)
                {
                    LogSerial.println(F("OFF"));
                }
            }
            printerVariables.doorSwitchTriggered = false;
            return;
        } */
    if (printerVariables.doorSwitchTriggered == true)
    {
        bool ledsAreOff = (currentWarm == 0 && currentCold == 0);
        bool chamberLightIsOff = (printerVariables.printerledstate == false);

        if (printerConfig.debugingchange)
        {
            LogSerial.print(F("Door closed twice within 2 seconds - "));

            if (ledsAreOff)
                LogSerial.print(F("Turning LEDs ON"));
            else
                LogSerial.print(F("Turning LEDs OFF"));

            if (printerConfig.controlChamberLight)
            {
                if (ledsAreOff && chamberLightIsOff)
                    LogSerial.println(F(" + Chamber Light ON"));
                else if (!ledsAreOff)
                    LogSerial.println(F(" + Chamber Light OFF"));
                else
                    LogSerial.println();
            }
            else
            {
                LogSerial.println();
            }
        }

        if (ledsAreOff)
        {
            tweenToColor(0, 0, 0, 255, 255); // WHITE
            printerConfig.isIdleOFFActive = false;

            if (printerConfig.controlChamberLight && chamberLightIsOff)
            {
                controlChamberLight(true); // Turn ON chamber light only if off
            }
        }
        else
        {
            tweenToColor(0, 0, 0, 0, 0); // OFF
            printerConfig.isIdleOFFActive = true;
            printerConfig.inactivityStartms = millis() - printerConfig.inactivityTimeOut;

            if (printerConfig.controlChamberLight)
            {
                controlChamberLight(false); // Always OFF on manual off
            }
        }

        setLedSource(ledsAreOff ? "Door double-close - ON" : "Door double-close - OFF");
        printerVariables.doorSwitchTriggered = false;
        return;
    }

    // RED -- RED -- RED -- RED

    // allow errordetection to turn ledstrip red
    if (printerConfig.errordetection == true)
    {

        // Fillament runout
        if (printerVariables.stage == 6 || printerVariables.overridestage == 6)
        {
            tweenToColor(printerConfig.filamentRunoutRGB); // Customisable - Default is RED
            printLogs("Stage 6, FILAMENT RUNOUT", printerConfig.filamentRunoutRGB);
            return;
        };

        // Front Cover Removed
        if (printerVariables.stage == 17 || printerVariables.overridestage == 17)
        {
            tweenToColor(printerConfig.frontCoverRGB); // Customisable - Default is RED
            printLogs("Stage 17, FRONT COVER REMOVED", printerConfig.frontCoverRGB);
            return;
        };

        // Nozzle Temp fail
        if (printerVariables.stage == 20 || printerVariables.overridestage == 20)
        {
            tweenToColor(printerConfig.nozzleTempRGB); // Customisable - Default is RED
            printLogs("Stage 20, NOZZLE TEMP FAIL", printerConfig.nozzleTempRGB);
            return;
        };

        // Bed Temp Fail
        if (printerVariables.stage == 21 || printerVariables.overridestage == 21)
        {
            tweenToColor(printerConfig.bedTempRGB); // Customisable - Default is RED
            printLogs("Stage 21, BED TEMP FAIL", printerConfig.bedTempRGB);
            return;
        };
        // SERIOUS HMS state
        if (printerVariables.parsedHMSlevel == "Serious")
        {
            tweenToColor(printerConfig.hmsSeriousRGB); // Customisable - Default is RED
            LogSerial.print(F("HMS SERIOUS Severity      "));
            LogSerial.print(F("Error Code: "));
            LogSerial.printf("%016llX\n", printerVariables.parsedHMScode);
            printLogs("PROBLEM", printerConfig.hmsSeriousRGB);
            return;
        };

        // FATAL HMS state
        if (printerVariables.parsedHMSlevel == "Fatal")
        {
            tweenToColor(printerConfig.hmsFatalRGB); // Customisable - Default is RED
            LogSerial.print(F("HMS FATAL Severity      "));
            LogSerial.print(F("Error Code: "));
            LogSerial.printf("%016llX\n", printerVariables.parsedHMScode);
            printLogs("PROBLEM", printerConfig.hmsFatalRGB);
            return;
        };
    };

    // BLUE -- BLUE -- BLUE -- BLUE

    // Pause (by user or via Gcode)
    if ((printerVariables.stage == 16 || printerVariables.stage == 30) || printerVariables.gcodeState == "PAUSE")
    {
        tweenToColor(printerConfig.pauseRGB); // Customisable - Default is BLUE
        printLogs("Stage 16, gcodeState pause, PAUSED", printerConfig.pauseRGB);
        return;
    }

    // First Layer Error PAUSED
    if (printerVariables.stage == 34)
    {
        tweenToColor(printerConfig.firstlayerRGB); // Customisable - Default is BLUE
        printLogs("Stage 34, FIRST LAYER ERROR, PAUSED", printerConfig.firstlayerRGB);
        return;
    }

    // Nozzle Clog PAUSED
    if (printerVariables.stage == 35)
    {
        tweenToColor(printerConfig.nozzleclogRGB); // Customisable - Default is BLUE
        printLogs("Stage 35, NOZZLE CLOG, PAUSED", printerConfig.nozzleclogRGB);
        return;
    }

    // OFF -- OFF -- OFF -- OFF

    // Printer offline / MQTT disconnected for longer than the configured grace time.
    // Instead of getting stuck on the last colour we either dim a low-power
    // "connecting" indicator (default) or turn fully off, while the MQTT task keeps
    // retrying the connection in the background until it succeeds.
    if (printerVariables.online == false &&
        (millis() - printerVariables.disconnectMQTTms) >= ((unsigned long)printerConfig.offlineDimAfterSec * 1000UL))
    {
        if (printerConfig.offlineDimEnabled)
        {
            applyOfflineDim();
            setLedSource("Printer offline - dimmed");
            if (!offlineDimLogged) { printLogs("Printer offline - dimmed", printerConfig.offlineDimBrightness, 0, 0, 0, 0); offlineDimLogged = true; }
        }
        else
        {
            tweenToColor(0, 0, 0, 0, 0); // OFF (legacy behaviour)
            setLedSource("Printer offline");
            if (!offlineDimLogged) { printLogs("Printer offline", 0, 0, 0, 0, 0); offlineDimLogged = true; }
        }
        return;
    }

    // replicate printer behaviour OFF
    if (printerConfig.replicatestate && printerConfig.replicate_update && printerVariables.printerledstate == false)
    {
        tweenToColor(0, 0, 0, 0, 0); // OFF
        printLogs("LED Replication OFF", 0, 0, 0, 0, 0);
        printerConfig.replicate_update = false;
        return;
    }

    // Cleaning nozzle
    if (printerVariables.stage == 14)
    {
        tweenToColor(printerConfig.stage14Color); // Customisable - Default is OFF
        printLogs("Stage 14, CLEANING NOZZLE", printerConfig.stage14Color);
        return;
    }

    // Auto Bed Leveling
    if (printerVariables.stage == 1)
    {
        tweenToColor(printerConfig.stage1Color); // Customisable - Default is OFF
        printLogs("Stage 1, BED LEVELING", printerConfig.stage1Color);
        return;
    }

    // Calibrating Extrusion
    if (printerVariables.stage == 8)
    {
        tweenToColor(printerConfig.stage8Color); // Customisable - Default is OFF
        printLogs("Stage 8, CALIBRATING EXTRUSION", printerConfig.stage8Color);
        return;
    }

    // Scaning surface
    if (printerVariables.stage == 9)
    {
        tweenToColor(printerConfig.stage9Color); // Customisable - Default is OFF
        printLogs("Stage 9, SCANNING BED SURFACE", printerConfig.stage9Color);
        return;
    }

    // Inspecting First Layer
    if (printerVariables.stage == 10 || printerVariables.overridestage == 10)
    {
        tweenToColor(printerConfig.stage10Color); // Customisable - Default is OFF
        printLogs("Stage 10 / HMS 0C00, FIRST LAYER INSPECTION", printerConfig.stage10Color);
        return;
    }

    // Calibrating  MicroLidar
    if (printerVariables.stage == 12)
    {
        tweenToColor(printerConfig.stage10Color);
        printLogs("Stage 12, CALIBRATING MICRO LIDAR", printerConfig.stage10Color);
        return;
    }

    // Idle Timeout (Has to be enabled)
    if ((printerVariables.stage == -1 || printerVariables.stage == 255) && !((printerConfig.finishExit && printerVariables.waitingForDoor) || (printerConfig.finishExit == false && ((millis() - printerConfig.finishStartms) < printerConfig.finishTimeOut))) && (millis() - printerConfig.inactivityStartms) > printerConfig.inactivityTimeOut && printerConfig.isIdleOFFActive == false && printerConfig.inactivityEnabled)
    {
        tweenToColor(0, 0, 0, 0, 0); // OFF
        controlChamberLight(false);  // Turn off chamber light via MQTT
        printerConfig.isIdleOFFActive = true;
        setLedSource("Idle Timeout - OFF");
        if (printerConfig.debuging || printerConfig.debugingchange)
        {
            LogSerial.print(F("Idle Timeout ["));
            LogSerial.print((int)(printerConfig.inactivityTimeOut / 60000));
            LogSerial.println(F(" mins] - Turning LEDs OFF"));
        };
        return;
    }

    // ON -- ON -- ON -- ON

    // Preheating Bed
    if (printerVariables.stage == 2)
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage 2, PREHEATING BED", printerConfig.runningColor);
        return;
    }

    // Printing or Resume after Pausing
    if (printerVariables.stage == 0 && printerVariables.gcodeState == "RUNNING")
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage 0, PRINTING - gcodeState RUNNING", printerConfig.runningColor);
        return;
    }

    // for IDLE - P1 uses 255, X1 uses -1
    if ((printerVariables.stage == -1 || printerVariables.stage == 255) && !((printerConfig.finishExit && printerVariables.waitingForDoor) || (printerConfig.finishExit == false && ((millis() - printerConfig.finishStartms) < printerConfig.finishTimeOut))) && (millis() - printerConfig.inactivityStartms < printerConfig.inactivityTimeOut))
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage -1/255, IDLE", printerConfig.runningColor);
        return;
    }

    // User Cancelled Print
    if (printerVariables.gcodeState == "FAILED")
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage -1/255, FAILED", printerConfig.runningColor);
        return;
    }
    // Print file just sent
    if (printerVariables.gcodeState == "PREPARE")
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage -1/255, PREPARE", printerConfig.runningColor);
        return;
    }

    // Homing ToolHead
    if (printerVariables.stage == 13)
    {
        // No color change assigned
        LogSerial.println(F("STAGE 13, HOMING TOOL HEAD"));
        return;
    }

    // Offline
    if (printerVariables.gcodeState == "OFFLINE" || printerVariables.stage == -2)
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("Stage -1/255, OFFLINE", printerConfig.runningColor);
        return;
    }

    // GREEN -- GREEN -- GREEN -- GREEN

    // Sets to green when print finishes AND user wants Finish Indication enabled
    if (printerVariables.finished == true && printerConfig.finishindication == true)
    {
        tweenToColor(printerConfig.finishColor); // Customisable - Default is GREEN
        printLogs("Finished print", printerConfig.finishColor);
        printerVariables.finished = false;
        return;
    }

    // replicate printer behaviour ON
    if (printerConfig.replicatestate && printerConfig.replicate_update && printerVariables.printerledstate && !((printerConfig.finishExit && printerVariables.waitingForDoor) || (printerConfig.finishExit == false && ((millis() - printerConfig.finishStartms) < printerConfig.finishTimeOut))))
    {
        tweenToColor(printerConfig.runningColor); // Customisable - Default is WHITE
        printLogs("LED Replication ON", printerConfig.runningColor);
        printerConfig.replicate_update = false;
        return;
    }

    // Ensure doorSwitchTriggered is processed immediately
    if (printerVariables.doorSwitchTriggered)
    {
        updateleds();
    }
}

// Run the LED logic after something else had the strip (the master enable was off, or Home
// Assistant had it). Where the logic keeps the colour it last chose instead of drawing one
// (maintenance mode, the colour test, a finished print waiting for the door), draw that
// colour again, because the strip has been showing something else since.
void redrawLeds()
{
    unsigned long draws = ledDrawCount;
    updateleds();
    if (ledDrawCount == draws && !printerConfig.discoMode) // RGB cycle draws itself
    {
        tweenToColor(brightTargetRed, brightTargetGreen, brightTargetBlue, brightTargetWarm, brightTargetCold);
        setLedSource(brightTargetSource);
    }
}

// Redraw the strip after an LED setting changed (web page or Home Assistant): restart the
// idle timer and let the one-shot behaviours (replicate, maintenance, RGB cycle, test colour)
// draw again with the new settings.
void refreshLeds()
{
    printerConfig.inactivityStartms = millis();
    printerConfig.isIdleOFFActive = false;
    printerConfig.replicate_update = true;
    printerConfig.maintMode_update = true;
    printerConfig.discoMode_update = true;
    printerConfig.testcolor_update = true;
    redrawLeds();
}

// Redraw the colour the printer logic last chose at the current brightness, without
// re-running that logic, so a brightness change doesn't wake LEDs the idle timer turned off.
void reapplyBrightness()
{
    if (!brightTargetShown || printerConfig.discoMode)
        return; // Home Assistant, offline dim or RGB cycle is drawing the strip
    tweenToColor(brightTargetRed, brightTargetGreen, brightTargetBlue, brightTargetWarm, brightTargetCold);
}

// What a door interaction does, for printers without a door sensor: end the finish
// indication and restart the idle timer, so the strip goes back to its normal colour.
void wakeLeds()
{
    printerVariables.waitingForDoor = false;
    printerConfig.finish_check = false;
    printerConfig.finishStartms = millis() - (unsigned long)printerConfig.finishTimeOut - 1; // finish timer already over
    printerConfig.inactivityStartms = millis();
    printerConfig.isIdleOFFActive = false;
    updateleds();
}

void setupLeds()
{
    LogSerial.println(F("Updating from setupleds"));

    ledcSetup(redChannel, pwmFreq, pwmResolution);
    ledcSetup(greenChannel, pwmFreq, pwmResolution);
    ledcSetup(blueChannel, pwmFreq, pwmResolution);
    ledcSetup(warmChannel, pwmFreq, pwmResolution);
    ledcSetup(coldChannel, pwmFreq, pwmResolution);

    ledcAttachPin(redPin, redChannel);
    ledcAttachPin(greenPin, greenChannel);
    ledcAttachPin(bluePin, blueChannel);
    ledcAttachPin(warmPin, warmChannel);
    ledcAttachPin(coldPin, coldChannel);
}

void ledsloop()
{
    RGBCycle();

    // While the printer is offline no MQTT messages arrive to trigger updateleds(),
    // so periodically re-evaluate ONLY in printer-driven modes - this is what lets
    // the offline-dim timeout kick in. We skip this when the strip is master-disabled
    // or HA-controlled, since those are event-driven and would otherwise spam the log.
    static unsigned long lastForcedEval = 0;
    if ((millis() - lastForcedEval) > 2000 &&
        printerVariables.online == false &&
        haVariables.masterEnable &&
        printerConfig.ledControlMode != LED_MODE_HA &&
        !(printerConfig.ledControlMode == LED_MODE_HYBRID && haVariables.overrideActive))
    {
        lastForcedEval = millis();
        updateleds();
    }
    if ((millis() - lastUpdatems) > 30000 && (printerConfig.maintMode || printerConfig.testcolorEnabled || printerConfig.discoMode || printerConfig.debugwifi))
    {
        LogSerial.print(F("["));
        LogSerial.print(millis());
        LogSerial.print(F("]"));
        if (printerConfig.maintMode)
            LogSerial.println(F(" Maintenance Mode - next update in 30 seconds"));
        if (printerConfig.testcolorEnabled)
            LogSerial.println(F(" Test Color - next update in 30 seconds"));
        if (printerConfig.discoMode)
            LogSerial.println(F(" RGB Cycle Mode - next update in 30 seconds"));
        if (printerConfig.debugwifi)
            LogSerial.println(F(" Wifi Debug Mode - next update in 30 seconds"));
        lastUpdatems = millis();
    }

    // Turn off GREEN if... finished and Door OPENED or CLOSED in last 5 secs AND user wants Finish Indication enabled
    // Can't use Boolean finished as printer defaults back to IDLE via MQTT
    if ((printerVariables.waitingForDoor && printerConfig.finishindication && printerConfig.finishExit && ((millis() - printerVariables.lastdoorClosems) < 6000 || (millis() - printerVariables.lastdoorOpenms) < 6000)))
    {
        if (printerConfig.debuging || printerConfig.debugingchange)
        {
            LogSerial.println(F("Updating from finishloop after Door interaction - Starting IDLE timer"));
        }
        printerVariables.waitingForDoor = false;
        printerConfig.inactivityStartms = millis();
        printerConfig.isIdleOFFActive = false;
        updateleds();
    }

    if ((printerConfig.finish_check && printerConfig.finishindication && printerConfig.finishExit == false && ((millis() - printerConfig.finishStartms) > printerConfig.finishTimeOut)))
    {
        if (printerConfig.debuging || printerConfig.debugingchange)
        {
            LogSerial.println(F("Updating from finishloop after Finish timer expired - Starting IDLE timer"));
        }
        printerConfig.finish_check = false;
        printerConfig.inactivityStartms = millis();
        printerConfig.isIdleOFFActive = false;
        updateleds();
        controlChamberLight(false); // Turn off chamber light via MQTT
    }

    // Need an trigger action to run updateleds() so lights turn off
    // There is no change in the printer STATE, just monitoring the timer and triggering when over a threshhold
    if (printerConfig.inactivityEnabled && (millis() - printerConfig.inactivityStartms) > printerConfig.inactivityTimeOut && printerVariables.finished == false && printerConfig.isIdleOFFActive == false)
    {
        // Opening or Closing the Door will turn LEDs back on and restart the timer.
        updateleds();
    }
    // disabled, need testing for future use
    /*     // Periodic fallback update to ensure MQTT timeout or other updates are evaluated
        static unsigned long lastPeriodicUpdate = 0;
        if (millis() - lastPeriodicUpdate > 10000) { // every 10 seconds
            updateleds();
            lastPeriodicUpdate = millis();
        } */

        // Auto turn off chamber light after timeout when door is closed
if (printerVariables.chamberLightLocked &&
    !printerVariables.doorOpen &&
    (millis() - printerConfig.inactivityStartms > printerConfig.inactivityTimeOut))
{
    controlChamberLight(false);
    printerVariables.chamberLightLocked = false;

    if (printerConfig.debugingchange)
        LogSerial.println(F("[LED] Timeout – Chamber light OFF and lock released"));
}
    delay(10);
}

#endif