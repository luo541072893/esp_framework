// Config.h
#ifdef __cplusplus

#ifndef _CONFIG_h
#define _CONFIG_h

#include <Ticker.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <pb.h>
#include "Arduino.h"
#include "Common.h"
#include "GlobalConfig.pb.h"

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifndef GLOBAL_CFG_VERSION
#define GLOBAL_CFG_VERSION 1 // 1 - 999
#endif

#ifdef ESP8266
#define LOG_SIZE 512
#define WEB_LOG_SIZE 4096  // Max number of characters in weblog
#else
#define LOG_SIZE 1024
#define WEB_LOG_SIZE 8192  // Max number of characters in weblog
#endif

#define MQTT_FULLTOPIC "%module%/%hostname%/%prefix%/" // MQTT 主题格式

#define OTA_URL "http://10.0.0.50/esp/%module%.bin"


#define BOOT_LOOP_OFFSET 5 // 开始恢复默认值之前的引导循环数 (0 = disable, 1..200 = 循环次数)

enum ModuleFunctions
{
    FUNC_PRE_INIT,
    FUNC_INIT,
    FUNC_LOOP,
    FUNC_EVERY_50_MSECOND,
    FUNC_EVERY_100_MSECOND,
    FUNC_EVERY_200_MSECOND,
    FUNC_EVERY_250_MSECOND,
    FUNC_EVERY_SECOND,
    FUNC_COMMAND,
    FUNC_SAVE_BEFORE_RESTART,
    FUNC_WEB,
};

extern char UID[16];
extern char tmpData[LOG_SIZE];
extern GlobalConfigMessage globalConfig;
extern uint32_t perSecond;
extern Ticker *tickerPerSecond;
extern bool (*module_func_ptr[10])(uint8_t);
extern uint8_t module_func_present;
void addModule(bool (*call)(uint8_t));
bool callModule(uint8_t function);

class Config
{
private:
    static uint16_t nowCrc;
    static bool isDelay;
    static uint8_t countdown;

public:
    static uint8_t operationFlag; // 0每秒
    static uint8_t statusFlag;    // 0：wifi状态 1：MQTT状态 2：Lan状态
    static uint16_t crc16(uint8_t *ptr, uint16_t len);

    static void readConfig();
    static void resetConfig();
    static bool saveConfig(bool isEverySecond = false);
    static void delaySaveConfig(uint8_t second);

    static void moduleReadConfig(uint16_t version, uint16_t size, const pb_field_t fields[], void *dest_struct);
    static bool moduleSaveConfig(uint16_t version, uint16_t size, const pb_field_t fields[], const void *src_struct);

    static void perSecondDo();
    static bool callModule(uint8_t function);
};
#endif
#endif
