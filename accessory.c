#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <homekit/types.h>

#include "accessory.h"

#define LOG_PRINT(fmt,args...)      printf(("%s,%s,LINE%d: " fmt "\n"),__FILE__,__func__,__LINE__, ##args)
#define ACCESSORY_NAME              ("LG AC")
#define ACCESSORY_SN                ("AKB74955614_001")      //SERIAL_NUMBER
#define ACCESSORY_MANUFACTURER      ("LG")
#define ACCESSORY_MODEL             ("AKB74955614")
#define ACCESSORY_FIRMWARE_REVISION ("1.0.0")
#define ACCESSORY_HARDWARE_REVISION ("20250504")

homekit_characteristic_t ac_active         = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t ac_light         = HOMEKIT_CHARACTERISTIC_(ON, 1);
homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 27);

//OFF=0, COOL=3
homekit_characteristic_t current_heater_cooler_state    = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 0,
        .min_value = (float[]) {0},
        .max_value = (float[]) {3},
        .valid_values={.count=2, .values = (uint8_t[]) {0, 3}}
        );

//COOL=2
homekit_characteristic_t target_heater_cooler_state     = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 2,
        .min_value = (float[]) {2},
        .max_value = (float[]) {2},
        .valid_values={.count=1, .values = (uint8_t[]) {2}}
        );

homekit_characteristic_t cooling_threshold_temperature = HOMEKIT_CHARACTERISTIC_(COOLING_THRESHOLD_TEMPERATURE, 27,
        .min_value = (float[]) {18.0},
        .max_value = (float[]) {30.0},
        .min_step = (float[]) {1.0},
        );

void accessory_identify(homekit_value_t _value) {
    LOG_PRINT("accessory identify\n");        
}

homekit_accessory_t *accessories[] ={
        HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_air_conditioner, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, ACCESSORY_MANUFACTURER),
                        HOMEKIT_CHARACTERISTIC(MODEL, ACCESSORY_MODEL),
                        HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, ACCESSORY_SN),
                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, ACCESSORY_FIRMWARE_REVISION),
                        HOMEKIT_CHARACTERISTIC(HARDWARE_REVISION, ACCESSORY_HARDWARE_REVISION),
                        NULL
                }),
                HOMEKIT_SERVICE(HEATER_COOLER, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                //Required Characteristics:
                        &ac_active,
                        &current_temperature,
                        &current_heater_cooler_state,
                        &target_heater_cooler_state,
                        &cooling_threshold_temperature,
                        NULL
                }),
                HOMEKIT_SERVICE(LIGHTBULB, .primary=false, .characteristics=(homekit_characteristic_t*[]){
                //Required Characteristics:
                        &ac_light,
                        NULL
                }),
                NULL
        }),
        NULL
};


homekit_server_config_t config = {
		.accessories = accessories,
		.password = "111-11-111"
};