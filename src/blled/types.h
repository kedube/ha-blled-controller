#ifndef _TYPES
#define _TYPES

// Home Assistant colour-temperature range (mireds). 153 = ~6500K (cold),
// 500 = ~2000K (warm). Used to map HA color_temp to the warm/cold white channels.
#define HA_MIRED_MIN 153
#define HA_MIRED_MAX 500

#include <esp_heap_caps.h>

// Memory that ordinary allocations (malloc, String, JSON, TLS buffers) can use.
// ESP.getFreeHeap() and ESP.getMaxAllocHeap() also count the ~50 kB of leftover instruction
// RAM, which only takes 32-bit accesses, so they overstate it.
static inline uint32_t freeMemory() { return heap_caps_get_free_size(MALLOC_CAP_8BIT); }
static inline uint32_t minFreeMemory() { return heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT); }
static inline uint32_t largestFreeBlock() { return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); }

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct COLORStruct {
        short r;
        short g;
        short b;
        short ww;
        short cw;
        char RGBhex[8];
    } COLOR;


    typedef struct PrinterVaraiblesStruct{
        String parsedHMSlevel = "";
        uint64_t parsedHMScode = 0;            //8 bytes per code stored
        String gcodeState = "FINISH";           //Initialised to Finish so the logic doesn't 
                                                //assume a Print has just finished and needs 
                                                //to wait for a door interaction to continue
        int stage = 0;
        int overridestage = 999;
        bool printerledstate = true;
        bool hmsstate = false;
        bool online = false;
        bool finished = false;
        bool initalisedLEDs = false;
        //Time since
        unsigned long disconnectMQTTms = 0;        

        //PrinterType
        bool isP1Printer = false;               //Is this a P1 Printer without lidar or door switch
        //Door Monitoring
        bool useDoorSwtich = true;              //DoorSwitch to be used for Actions?
        bool doorOpen = false;                  // Current State of Door
        bool doorSwitchTriggered = false;       // Has door been closed twice within 6 seconds?
        bool waitingForDoor = false;            // Are we waiting for the door to be actuated?
        unsigned long lastdoorClosems = 0;      // Last time door was opened
        unsigned long lastdoorOpenms = 0;       // Last time door was closed
        bool chamberLightLocked = false;  // blocks replicate while true
        bool ledWasForcedByDoor = false;
    } PrinterVariables;
    PrinterVariables printerVariables;

    typedef struct SecurityVariables{
                // Security
        char HTTPUser[40] = "";             //http basic auth username
        char HTTPPass[40] = "";              //http basic auth password
    }SecurityVariables;
    SecurityVariables securityVariables;



    typedef struct GlobalVariablesStruct{
        char SSID[32];
        char APPW[64];
        String FWVersion = STRVERSION;
        String Host = "BLLED";
        bool started = false;
    } GlobalVariables;

    GlobalVariables globalVariables;

    typedef struct PrinterConfigStruct
    {
        char printerIP[16];             //BBLP IP Address - used for MQTT reports
        char accessCode[9];             //BBLP Access Code - used for MQTT reports
        char serialNumber[16];          //BBLP Serial Number - used for MQTT reports

        char BSSID[18];                 //Nominated AP to connect to (Useful if multiple accesspoints with same name)
        int brightness = 20;            //Brightness of LEDS - Default to 20% in case user use LED's that draw too much power for their PS
        bool rescanWiFiNetwork = false; //Scans available WiFi networks for strongest signal
        // LED Behaviour (Choose One)
        bool maintMode = false;         //White lights on, even if printer is unpowered
        bool maintMode_update = true;
        bool discoMode = false;         //Cycles through RGB colors slowly for 'pretty' timelapse movie
        bool discoMode_update = true;
        bool replicatestate = true;     //LED will be on if the BBPL Light is on
        bool replicate_update = true;     //LED will be on if the BBPL Light is on
        COLOR runningColor;             //Running Color (Default if no issues)
        bool testcolorEnabled = false;
        bool testcolor_update= true;    //When updateleds() is run, should the TEST LEDS be set?
        COLOR testColor;                //Test Color
        bool debugwifi = false;         //Changes LED to a color range that represents WiFi signal Strength        
        // Options
        bool finishindication = true;   //Enable / Disable
        COLOR finishColor;              //Set Finish Color
        bool finishExit = true;         //True = use Door / False = use Timer
        bool finish_check = false;    //When updateleds() is run, should the TEST LEDS be set?
        unsigned long finishStartms = 0;    // Time the finish countdown is measured from
        int finishTimeOut = 600000;     //300000 = 5 mins
        bool controlChamberLight = false;                //control chamber light

        //Inactivity Timout
        bool inactivityEnabled = true;
        bool isIdleOFFActive = false;       // Are the lights out due to inactivity Timeout?
        unsigned long inactivityStartms = 0;    // Time the inactivity countdown is measured from
        int inactivityTimeOut = 3600000;  // 1800000 = 30mins / 600000 = 10mins / 60000 = 1mins 
        // Debugging
        bool debuging = false;          //Debugging for all interactions through functions
        bool debugingchange = true;     //Default debugging level - to shows onChange
        bool mqttdebug = false;         //Writes each packet from BBLP to the serial log
        //Custom Colors for events using lidar
        COLOR stage14Color;
        COLOR stage1Color;
        COLOR stage8Color;
        COLOR stage9Color;
        COLOR stage10Color;
        // Customise LED Colors
        bool errordetection = true;     //Utilises Error Colors when BBLP give an error
        COLOR wifiRGB;                  
        COLOR pauseRGB;
        COLOR firstlayerRGB;
        COLOR nozzleclogRGB;
        COLOR hmsSeriousRGB;
        COLOR hmsFatalRGB;
        COLOR filamentRunoutRGB;
        COLOR frontCoverRGB;
        COLOR nozzleTempRGB;
        COLOR bedTempRGB;
        // HMS Error Handling
        String hmsIgnoreList; // comma-separated list of HMS_XXXX_XXXX_XXXX_XXXX codes to ignore

        // === Home Assistant integration & operating mode ===
        // Operating mode: how the LED strip is driven
        //   0 = LED_MODE_PRINTER  -> driven only by the printer (original behaviour)
        //   1 = LED_MODE_HA       -> driven only by Home Assistant (printer state ignored)
        //   2 = LED_MODE_HYBRID   -> printer drives the strip, but Home Assistant can override
        int ledControlMode = 2;          // default Hybrid

        bool haEnabled = false;          // Enable the Home Assistant MQTT connection
        char haMqttHost[40] = "";        // Home Assistant MQTT broker host / IP
        int  haMqttPort = 1883;          // Home Assistant MQTT broker port
        char haMqttUser[40] = "";        // Home Assistant MQTT username (optional)
        char haMqttPass[64] = "";        // Home Assistant MQTT password (optional)
        bool haMqttTls = false;          // Connect to the broker over TLS (usually port 8883)
        String haMqttCaCert;             // CA certificate (PEM) to verify the broker; empty = don't verify

        // Offline (printer unreachable) behaviour
        // Instead of getting "stuck" on the last colour, dim the strip after a timeout
        // while the firmware keeps retrying the printer connection in the background.
        bool offlineDimEnabled = true;   // dim the strip when the printer is unreachable
        int  offlineDimAfterSec = 60;    // seconds offline before dimming
        int  offlineDimBrightness = 5;   // dim brightness in % (0 = fully off)

        // WiFi RF tuning — configurable to reduce electrical noise from power rail excitation
        // Stored value for 8.5 dBm is 8; for 19.5 dBm is 19 (falls to default in switch).
        int  wifiTxPower = 11;           // dBm: 2, 5, 8(=8.5), 11, 13, 15, 17, 19(=19.5)
        bool wifiSleepEnabled = true;    // WiFi modem sleep (rhythmic vs. continuous noise)

        // Subsystem isolation — disable individually to locate noise source.
        // Changes take effect after reboot. All default true → safe for existing configs.
        bool wifiRadioEnabled = true;        // master WiFi switch; false = WiFi.mode(WIFI_OFF)
        bool diagEnableMqtt = true;          // printer MQTT (TLS, port 8883)
        bool diagEnableHa = true;            // Home Assistant MQTT
        bool diagEnableSsdp = true;          // SSDP responder (passive, listens for UPnP queries)
        bool diagEnableBblScan = true;       // BBL active UDP multicast scanner (tx every 10 s)

    } PrinterConfig;

    PrinterConfig printerConfig;

    // Runtime state for the Home Assistant integration (not all persisted).
    typedef struct HAVariablesStruct
    {
        bool masterEnable = true;       // HA "enable" switch. When false the strip is forced OFF.
        bool overrideActive = false;    // Hybrid: HA has taken control until the printer state changes again
        bool lightOn = false;           // HA light desired on/off state
        short r = 255;                  // HA light colour
        short g = 255;
        short b = 255;
        int brightness = 100;           // HA light brightness, 0-100 (%)
        int transitionMs = 500;         // fade duration for HA colour/brightness changes (ms)
        int colorMode = 1;              // HA desired colour mode: 0 = rgb, 1 = color_temp (white)
        int colorTempMireds = 326;      // HA desired colour temperature (mireds, ~3000K)

        // Reported (actual) strip state mirrored to HA, so the light entity always
        // reflects what is physically lit - whether the printer or HA set it.
        bool reportedOn = false;
        short reportedR = 0;
        short reportedG = 0;
        short reportedB = 0;
        int reportedBrightness = 0;     // 0-255 (actual PWM level)
        bool reportedIsTemp = false;    // true = strip is showing white (report as color_temp)
        int reportedMireds = 326;       // actual colour temperature when reportedIsTemp

        // Connection / publishing bookkeeping (volatile)
        bool connected = false;         // HA broker currently connected
        int  haLastConnectState = -1;   // PubSubClient state after last failed connect (4=bad creds, 5=unauth)
        bool discoverySent = false;     // discovery configs published since last connect
        bool stateDirty = true;         // a state change needs to be published to HA
        unsigned long lastReconnectMs = 0;
        unsigned long lastAvailabilityPublishMs = 0; // for periodic availability heartbeat
        volatile bool discoveryRepublishPending = false; // set by web handler, serviced by haTask
    } HAVariables;

    HAVariables haVariables;

    // Operating mode constants (kept as plain ints for easy JSON persistence)
    enum LedControlMode
    {
        LED_MODE_PRINTER = 0,
        LED_MODE_HA = 1,
        LED_MODE_HYBRID = 2
    };


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif