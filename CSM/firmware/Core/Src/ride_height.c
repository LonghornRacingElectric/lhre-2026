// #include "ride_height.h"
// #include "stdbool.h"
#include "argus_hal_test.h"
// #include "platform/argus_irq.h"
// #include "api/argus_api.h"
// #include "api/argus_def.h"
// #include "platform/argus_print.h"
// #include "platform/board_config.h"
// #include "platform/argus_timer.h"
// #include "platform/argus_s2pi.h"
// #include "argus.h"
// #include "s2pi.h"
// #include "cmsis_os.h"
// #include "argus_timer.h"

#include "ride_height.h"
#include "stdbool.h"
#include "argus.h"                    // ← master header, pulls in everything SDK-related
#include "platform/argus_irq.h"
#include "platform/argus_print.h"
#include "platform/board_config.h"
#include "platform/argus_timer.h"
#include "platform/argus_s2pi.h"
#include "cmsis_os.h"
#include <stdio.h>

/* private variables ===============================================================*/
static volatile uint8_t myDataReadyEvents = 0;
static argus_hnd_t *device;
static float filteredDistance = 0.0f;
static float lastMeasurementTime = 0.0f;
static uint8_t lastSignalQuality = 0;

/* private function declarations ===================================================*/
static argus_hnd_t* ride_height_initialize_device(s2pi_slave_t slave, argus_mode_t mode);
static status_t ride_height_measurement_callback(status_t status, argus_hnd_t *device);
static void HandleError(status_t status, bool stop, char *message);
// static void PrintResults(argus_results_t const * res);

/* private functions ===============================================================*/
// static void PrintResults(argus_results_t const * res)
// {
//   /* Print the recent measurement results:
//    * 1. Time stamp in seconds since the last MCU reset.
//    * 2. Range in mm (converting the Q9.22 value to mm).
//    * 3. Amplitude in LSB (converting the UQ12.4 value to LSB).
//    * 4. Signal Quality in % (100% = good signal).
//    * 5. Status (0: OK, <0: Error, >0: Warning.
//    *
//    * Note: Sending data via UART creates a large delay which might prevent
//    *       the API from reaching the full frame rate. This example sends
//    *       approximately 80 characters per frame at 115200 bps which limits
//    *       the max. frame rate of 144 fps:
//    *       115200 bps / 10 [bauds-per-byte] / 80 [bytes-per-frame] = 144 fps */
//   print("\033[1K");
//   print("%4d.%06d s; Distance: %5d mm;  Quality: %3d",
//         res->TimeStamp.sec,
//         res->TimeStamp.usec,
//         res->Bin.Range / (Q9_22_ONE / 1000),
//         res->Bin.SignalQuality);
// }

static void HandleError(status_t status, bool stop, char *message) {
    (void)message; // Suppress unused parameter warning
    if (status != STATUS_OK) {
        //printf("Error: %s\n", message);
        if (stop) {
            while (1) {
                //printf("Fatal Error: %s\n", message);
            }
        }
    }
}

static status_t ride_height_measurement_callback(status_t status, argus_hnd_t *device) {
    (void)device;
    HandleError(status, false, "Measurement Ready Callback received error!");
    myDataReadyEvents++;
    return STATUS_OK;
}

// static argus_hnd_t* ride_height_initialize_device(s2pi_slave_t slave, argus_mode_t mode)
// {
//   /* The API module handle that contains all data definitions that is
//    * required within the API module for the corresponding hardware device.
//    * Every call to an API function requires the passing of a pointer to this
//    * data structure. */
//     device = Argus_CreateHandle();
//     print("Argus_CreateHandle done\r\n");
//     osDelay(100);
//     HandleError(device ? STATUS_OK : ERROR_FAIL, true, "Argus_CreateHandle failed!");

//   /* Initialize the API with the dedicated default measurement mode.
//    * This implicitly calls the initialization functions
//    * of the underlying API modules.
//    *
//    * The second parameter is stored and passed to all function calls
//    * to the S2PI module. This piece of information can be utilized in
//    * order to determine the addressed SPI slave and enabled the usage
//    * of multiple devices on a single SPI peripheral.
//    *
//    * Also note the #Argus_InitMode alternative that uses a third
//    * parameter to choose the measurement mode: see the #argus_mode_t
//    * enumeration for more information on available measurement modes. */
//     status_t status = Argus_InitMode(device, slave, mode);
//     print("Argus_InitMode done\r\n");
//     osDelay(100);
//     HandleError(status, true, "Argus_Init failed!");

//     status = Argus_SetConfigurationDFMMode(device, DFM_MODE_OFF);
//     print("Argus_InitMode status=%ld\r\n", (int32_t)status);
//     osDelay(100);
//     HandleError(status, true, "Argus_Init failed!");
//     print("DFMMode done\r\n");
//     osDelay(100);
//     HandleError(status, true, "Argus_SetConfigurationDFMMode failed!");

//     status = Argus_SetConfigurationSmartPowerSaveEnabled(device, false);
//     print("SmartPowerSave done\r\n");
//     osDelay(100);
//     HandleError(status, true, "Argus_SetConfigurationSmartPowerSaveEnabled failed!");

//     status = Argus_SetConfigurationFrameTime(device, 10000);
//     print("FrameTime done\r\n");
//     osDelay(100);
//     HandleError(status, true, "Argus_SetConfigurationFrameTime failed!");

//     return device;
// }
static argus_hnd_t* ride_height_initialize_device(s2pi_slave_t slave, argus_mode_t mode)
{
    device = Argus_CreateHandle();
    HandleError(device ? STATUS_OK : ERROR_FAIL, true, "Argus_CreateHandle failed!");

    status_t status = Argus_InitMode(device, slave, mode);
    print("Argus_InitMode status=%ld\r\n", (int32_t)status);
    osDelay(100);
    HandleError(status, true, "Argus_Init failed!");

    // Skip DFM, SmartPowerSave, FrameTime for now — use defaults
    return device;
}

/* public functions ===============================================================*/

void ride_height_init() {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);  // force IMU CS high
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);  // force RH CS high
    osDelay(10);

    Timer_Init();
    print("Timer_Init done\r\n");
    S2PI_Init(SPI_DEFAULT_SLAVE, 5000000);
    print("S2PI_Init done\r\n");

    char buf[64];  // single declaration, reused below

    // Print baud rate
    uint32_t baud = S2PI_GetBaudRate(SPI_DEFAULT_SLAVE);
    snprintf(buf, sizeof(buf), "SPI baud rate: %lu\r\n", baud);
    print(buf);
    osDelay(100);

    // // Run HAL self-test, dont remove this commented out code
    // status_t hal_status = Argus_VerifyHALImplementation(SPI_DEFAULT_SLAVE);
    // snprintf(buf, sizeof(buf), "HAL test status: %ld\r\n", (int32_t)hal_status);
    // print(buf);
    // osDelay(200);

    print("Starting device init\r\n");
    // device = ride_height_initialize_device(SPI_DEFAULT_SLAVE, ARGUS_MODE_SHORT_RANGE);
    device = ride_height_initialize_device(SPI_DEFAULT_SLAVE, ARGUS_MODE_HIGH_PRECISION_SHORT_RANGE);
    print("Device init done\r\n");
    Argus_StartMeasurementTimer(device, &ride_height_measurement_callback);
    print("Measurement timer started\r\n");
}

float ride_height_get_distance_mm() {
    if (!myDataReadyEvents) { // shouldn't myDataReadyEvents be a boolean value? right now its an 8-bit uint value. also with the change we would also have to change the other code that references it
        return filteredDistance; // Return the last filtered distance if no new data is ready
    }

    IRQ_LOCK();
    myDataReadyEvents--; // why do we need an IRQ lock while editing this? there's no code other than the one line so what would be interrupted?
    IRQ_UNLOCK();

    argus_results_t res;
    status_t status = Argus_EvaluateData(device, &res);
    // PrintResults(&res);  // uncomment to debug over USB serial
    HandleError(status, false, "Argus_EvaluateData failed!");

    float distanceRaw = (float) res.Bin.Range / (Q9_22_ONE / 1000.0f);
    uint8_t quality = res.Bin.SignalQuality;
    lastSignalQuality = quality;  // save it for get_quality()
    // // Define physical bounds of suspension travel in mm
    // #define MIN_RIDE_HEIGHT_MM   50.0f   // 5cm minimum
    // #define MAX_RIDE_HEIGHT_MM  100.0f   // 10cm maximum

    // bool inRange = (distanceRaw >= MIN_RIDE_HEIGHT_MM && distanceRaw <= MAX_RIDE_HEIGHT_MM);

    // bool goodQuality;
    // if (inRange) {
    //     // Within expected suspension range — only need minimal quality
    //     goodQuality = (quality >= 5);
    // } else {
    //     // Outside expected range — apply strict quality threshold
    //     // to avoid accepting noise or spurious reflections
    //     goodQuality = (quality >= 80);
    // }

    // if (goodQuality && distanceRaw > 0) {
    //     float distanceMeasurementTime = res.TimeStamp.sec + 
    //                                     res.TimeStamp.usec / 1000000.0f;
    //     float h = distanceMeasurementTime - lastMeasurementTime;
    //     float timeConstant = 0.010f;
    //     float alpha = h / (h + timeConstant);
    //     filteredDistance = distanceRaw * alpha + filteredDistance * (1-alpha);
    //     lastMeasurementTime = distanceMeasurementTime;
    // }

    bool cutoff = false;

    if (distanceRaw == 0) {
        cutoff = true;
    } else if (distanceRaw < 16) {
        cutoff = (quality >= 5*distanceRaw);
    } else {
        cutoff = quality >= 80;
    }

    if (cutoff) {
        if (distanceRaw >= 0) {
            float distanceMeasurementTime = res.TimeStamp.sec + res.TimeStamp.usec / 1000000.0f;
            float h = distanceMeasurementTime - lastMeasurementTime;
            float timeConstant = 0.010f;
            float alpha = h / (h + timeConstant);
            filteredDistance = distanceRaw * alpha + filteredDistance * (1-alpha);
            lastMeasurementTime = distanceMeasurementTime;
        }
      }
    return filteredDistance;
}

uint8_t ride_height_get_quality() {
    return lastSignalQuality;
}