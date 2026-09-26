#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

// Hardware Drivers
#include "LCD_Driver.h"
#include "LCD_Touch.h"
#include "LCD_GUI.h"
#include "DEV_Config.h"
#include "icm20948.h"

// FatFs SD Card Libraries
#include "ff.h"
#include "sd_card.h"
#include "f_util.h"
#include "hw_config.h"

#define PATH_MAX_LEN 256
#define MAX_FILE_WRITE 100

// The single "KEY" button on the Pico-Eval-Board is wired to GPIO 2
static constexpr uint KEY_GPIO = 2;

enum class AppMode {
    Idle,
    Imu,
    Drawing
};
static AppMode current_mode = AppMode::Idle;

// Shared Synchronization & Data Objects
static mutex_t touch_mutex;
static TP_DATA tp_data;

// FatFs Globals
static FATFS fs;
static sd_card_t *g_sd = NULL;
static const char *g_drive = NULL;

// Global IMU data structures
static IMU_ST_ANGLES_DATA stAngles;
static IMU_ST_SENSOR_DATA stGyroRawData;
static IMU_ST_SENSOR_DATA stAccelRawData;
static IMU_ST_SENSOR_DATA stMagnRawData;
static uint64_t t_prev = 0;

// ------------------------- Utility / FatFs -------------------------------
/** 
 * Join a drive prefix and a relative path into a full path.
 * @param out The output buffer for the full path.
 * @param out_sz The size of the output buffer.
 * @param drive The drive prefix.
 * @param rel The relative path.
 */
static void join_path(char *out, size_t out_sz, const char *drive, const char *rel) {
    if (rel && rel[0] == '/') rel++;
    if (drive && drive[strlen(drive) - 1] == '/')
        snprintf(out, out_sz, "%s%s", drive, rel ? rel : "");
    else
        snprintf(out, out_sz, "%s/%s", drive, rel ? rel : "");
}

/** 
 * Initialize the SD-card hardware and mounts its file system.
 * @return true if successful, false otherwise
 */
static bool sd_init_and_mount(void) {
    if (!sd_init_driver()) {
        printf("sd_init_driver() failed\n");
        return false;
    }

    g_sd = sd_get_by_num(0);
    if (!g_sd) return false;

    g_drive = sd_get_drive_prefix(g_sd); //FatFs drive prefix (e.g., "0:/")
    if (!g_drive) return false;

    FRESULT fr = f_mount(&fs, g_drive, 1);
    if (fr == FR_NO_FILESYSTEM) {
        BYTE work[4096];
        MKFS_PARM opt = { FM_FAT | FM_SFD, 0, 0, 0, 0 };
        fr = f_mkfs(g_drive, &opt, work, sizeof work);
        if (fr == FR_OK) {
            fr = f_mount(&fs, g_drive, 1);
        }
    }

    return (fr == FR_OK);
}

// ------------------------- CORE 1 SD WRITER -----------------------------
void core1_entry() {
    printf("Core 1 initialized: Waiting for SD write triggers...\n");

    if (!sd_init_and_mount()) {
        printf("Core 1: SD Mount Failed!\n");
        multicore_fifo_push_blocking(WRITE_FAILED_FLAG);
        while (1) tight_loop_contents();
    }

    int count = 0;
    while (count < MAX_FILE_WRITE) {
        uint32_t msg = multicore_fifo_pop_blocking();
        if (msg != DATA_READY_FLAG) continue; //Waiting for core0 to signal data is ready

        char path[PATH_MAX_LEN];
        char name[64];
        snprintf(name, sizeof(name), "drawing_capture_%d.txt", count); //Combines wantedpath+count into name
        join_path(path, sizeof(path), g_drive, name); //Combines gdrive+name into path

        FIL f; //opens file
        FRESULT fr = f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr != FR_OK) {
            multicore_fifo_push_blocking(WRITE_FAILED_FLAG);
            continue;
        }

        // Lock the touch data while writing to SD card
        mutex_enter_blocking(&touch_mutex);

        if (tp_data.data != NULL && tp_data.data_len > 0) {
            char *ascii_buffer = (char *)malloc(tp_data.data_len);
            if (ascii_buffer != NULL) {
                for (size_t i = 0; i < tp_data.data_len; i++) {
                    ascii_buffer[i] = tp_data.data[i] ? '1' : '0';
                }

                UINT bw = 0;
                f_write(&f, ascii_buffer, (UINT)tp_data.data_len, &bw);
                f_sync(&f);
                free(ascii_buffer);
            }
        }

        mutex_exit(&touch_mutex);
        f_close(&f);

        count++;
        printf("Core 1: Saved drawing to %s\n", path);
        multicore_fifo_push_blocking(TASK_COMPLETE_FLAG);
    }

    f_unmount(g_drive);
    while (1) tight_loop_contents();
}

// ------------------------- CORE 0 CONTROLS ------------------------------

bool KEY_pressed(void) {
    return (gpio_get(KEY_GPIO) == 0);
}    

static void select_next_mode(void) {
    switch (current_mode) {
        case AppMode::Idle:
            current_mode = AppMode::Imu;
            printf("\n>>> Mode: IMU <<<\n");
            break;
        case AppMode::Imu:
            current_mode = AppMode::Drawing;
            printf("\n>>> Mode: Drawing <<<\n");
            // Clear LCD and initialize canvas when entering Drawing Mode
            LCD_Clear(WHITE);
            TP_Dialog();
            break;
        case AppMode::Drawing:
            current_mode = AppMode::Idle;
            printf("\n>>> Mode: Idle <<<\n");
            break;
    }
}

static void run_idle_mode(void) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

static void run_imu_mode(void) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);

    uint64_t t_now = time_us_64();
    uint64_t dt_us = (t_now - t_prev);
    t_prev = t_now;
    float hz = (dt_us > 0) ? (1000000.0f / (float)dt_us) : 0.0f;

    printf("Roll: %6.2f | Pitch: %6.2f | Yaw: %6.2f | Rate: %4.1f Hz\r",
           stAngles.fRoll, stAngles.fPitch, stAngles.fYaw, hz);
}

static void run_drawing_mode(void) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    
    // Process touch panel active drawing
    LCD_SetBackLight(1000);
    TP_DrawBoard(); // Handles all the touch screen stuff, detecting position and drawing on it

    // Check if Core 1 sent background task status updates
    if (multicore_fifo_rvalid()) {
        uint32_t msg = multicore_fifo_pop_blocking();
        if (msg == TASK_COMPLETE_FLAG) {
            printf("\nDrawing successfully written to SD card!\n");
        } else if (msg == WRITE_FAILED_FLAG) {
            printf("\nSD Card write failed!\n");
        }
    }
}

// ------------------------------ Main -------------------------------------

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("CYW43 init failed!\n");
        return -1;
    }

    // Configure Key Button
    gpio_init(KEY_GPIO);
    gpio_set_dir(KEY_GPIO, GPIO_IN);
    gpio_pull_up(KEY_GPIO);

    // Initialize Hardware Peripherals
    System_Init();
    mutex_init(&touch_mutex);

    LCD_SCAN_DIR lcd_scan_dir = SCAN_DIR_DFT;
    LCD_Init(lcd_scan_dir, 1000);
    TP_Init(lcd_scan_dir, &tp_data, &touch_mutex);
    TP_GetAdFac();

    IMU_EN_SENSOR_TYPE enMotionSensorType;
    imuInit(&enMotionSensorType);

    // Launch Core 1 for FatFs operations
    multicore_launch_core1(core1_entry);

    t_prev = time_us_64();
    bool last_button_state = true;
    uint32_t last_debounce_time = 0;

    while (true) {
        bool current_button_state = KEY_pressed();
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (current_button_state && !last_button_state && (now - last_debounce_time > 200)) {
            select_next_mode();
            last_debounce_time = now;
        }
        last_button_state = current_button_state;

        switch (current_mode) {
            case AppMode::Idle:
                run_idle_mode();
                break;
            case AppMode::Imu:
                run_imu_mode();
                break;
            case AppMode::Drawing:
                run_drawing_mode();
                break;
        }

        sleep_ms(1);
    }

    return 0;
}