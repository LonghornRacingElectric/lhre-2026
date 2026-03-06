#include <stdint.h>

typedef struct {
  char magic[4];
  uint8_t major;
  uint8_t minor;
  uint16_t patch;
  char build_date[12];
} AppVersion_t;

__attribute__((section(".ver_info"), used)) const AppVersion_t g_app_version = {
    .magic = {'V', 'E', 'R', 'S'},
    .major = APP_VERS_MAJOR,
    .minor = APP_VERS_MINOR,
    .patch = APP_VERS_PATCH,
    .build_date = __DATE__,
};
