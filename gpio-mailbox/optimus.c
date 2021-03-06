#ifdef MINDSPEED
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "fileops.h"
#include "pin.h"

#define UNUSED        __attribute__((unused))

#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

/* This is the driver that allows gpio-mailbox to control the fan speed and the
 * brightness of the two LEDs on Optimus based platforms which include Optimus,
 * Optimus Prime, Sideswipe and Sideswipe Prime.
 *
 * To control the LED brightness and read the status of the reset button, we
 * support two methods to interface with the hardware:
 *
 * (1) Using /dev/mem to directly poke at the memory mapped registers of the
 * PWM and GPIO hardware. This method is used on the Linux 3.2 kernel which
 * lacks PWM, GPIO and LED drivers.
 *
 * (2) Going through Linux' LED and GPIO interfaces which are accessible
 * through sysfs. In other words: Just writing a number between 0 and 200 to
 * /sys/class/leds/blue/brightness and reading "0" or "1" from
 * /sys/class/gpio/gpio6/value. This method is used if we run the Linux 4.1
 * kernel. This newer kernel has proper drivers for the PWM and GPIO hardware.
 * The PWM driver is hooked up to the pwm-leds driver.
 *
 * We pick method 2 if /sys/class/leds/blue/brightness is accessible. If not,
 * we fall back to method 1.
 * */

#define  DEVMEM  "/dev/mem"

/* optimus */
#define REG_PWM_BASE            0x90458000
#define REG_PWM_DIVIDER         (REG_PWM_BASE)
#define REG_PWM_HI(p)           (REG_PWM_BASE+0x08+0x08*(p))
#define REG_PWM_LO(p)           (REG_PWM_HI(p)+0x04)

#define PWM_CLOCK_HZ            250000000                       /* 250 MHz */
#define PWM_DIVIDER_ENABLE_MASK (1<<31)
#define PWM_DIVIDER_VALUE_MASK  ((1<<8)-1)
#define PWM_TIMER_ENABLE_MASK   (1<<31)
#define PWM_TIMER_VALUE_MASK    ((1<<20)-1)
#define PWM_DEFAULT_DIVIDER     PWM_DIVIDER_VALUE_MASK

#define REG_GPIO_BASE           0x90470000
#define REG_GPIO_OUTPUT         (REG_GPIO_BASE+0x00)
#define REG_GPIO_DIRECTION      (REG_GPIO_BASE+0x04)            /* 1 = output */
#define REG_GPIO_INPUT          (REG_GPIO_BASE+0x10)
#define REG_GPIO_SELECT         (REG_GPIO_BASE+0x58)

/* manually maintain these */
#define REG_FIRST               REG_PWM_BASE
#define REG_LAST                REG_GPIO_SELECT
#define REG_LENGTH              (REG_LAST + 0x04 - REG_FIRST)

/* index of gpio pins */
#define GPIO_BUTTON             6
#define GPIO_ACTIVITY           12
#define GPIO_RED                13

/* gpio 12 can be pwm 4, 13 can be 5 */
#define PWM_ACTIVITY            4
#define PWM_RED                 5
#define PWM_LED_HZ              1000    /* 300-1000 is recommended */
#define PWM_DUTY_OFF_PERCENT    50      /* 0 is full bright, 100 is off */

struct PinHandle_s {
  int                           fd;
  volatile unsigned char*       addr;
  const char*                   sys_fan_dir;
  char                          sys_temp1_path[128];
  char                          sys_temp2_path[128];
  char                          sys_fan_path[128];
  char                          sys_rpm_path[128];
};

#define BIT_IS_SET(data, bit)   (((data) & (1u << (bit))) == (1u << (bit)))
#define BIT_SET(data, bit)      ((data) | (1u << (bit)))
#define BIT_CLR(data, bit)      ((data) & ~(1u << (bit)))

const char sys_button_reset_path[] = "/sys/class/gpio/gpio" __stringify(GPIO_BUTTON) "/value";
const char blue_led_brightness[] = "/sys/class/leds/blue/brightness";
const char red_led_brightness[] = "/sys/class/leds/red/brightness";
const char sys_fan_dir1[] =     "/sys/class/hwmon/hwmon0/";
const char sys_fan_dir2[] =     "/sys/class/hwmon/hwmon0/device/";
#define SYS_TEMP1               "temp1_input"
#define SYS_TEMP2               "temp2_input"
#define SYS_FAN                 "pwm1"
#define SYS_RPM                 "fan1_input"

/* helper methods */

// this is for writing to sysfs files
// don't use for writing to a regular file since this is not atomic
static void writeIntToFile(const char* file, int value) {
  FILE* fp = fopen(file, "w");
  if (fp == NULL) {
    perror(file);
    return;
  }
  fprintf(fp, "%d", value);
  fclose(fp);
}

// this is for writing to sysfs files
// don't use for writing to a regular file since this is not atomic
static void writeStringToFile(const char* file, const char* value) {
  FILE* fp = fopen(file, "w");
  if (fp == NULL) {
    perror(file);
    return;
  }
  fprintf(fp, "%s", value);
  fclose(fp);
}

// this is for reading from sysfs files
static int readIntFromFile(const char* file) {
  int value = 0;
  FILE* fp = fopen(file, "r");
  if (fp == NULL) {
    perror(file);
    return 0;
  }
  if (fscanf(fp, "%d", &value) != 1) {
    if (ferror(fp))
      perror(file);
    else
      fprintf(stderr, "Cannot parse integer from %s\n", file);
    return 0;
  }
  fclose(fp);
  return value;
}

/* optimus methods get sensor data */

static uint32_t getRegister(PinHandle handle, unsigned int reg) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + (reg - REG_FIRST));
  if (reg < REG_FIRST || reg > REG_LAST) {
    fprintf(stderr, "getRegister: register 0x%08x is out of range (0x%08x-0x%08x)\n",
      reg, REG_FIRST, REG_LAST);
    return 0;
  }
  return *regaddr;
}

static void setRegister(PinHandle handle, unsigned int reg, uint32_t value) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + (reg - REG_FIRST));
  if (reg < REG_FIRST || reg > REG_LAST) {
    fprintf(stderr, "setRegister: register 0x%08x is out of range (0x%08x-0x%08x)\n",
      reg, REG_FIRST, REG_LAST);
    return;
  }
  *regaddr = value;
}

static int getGPIO(PinHandle handle, int gpio) {
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_GPIO_OUTPUT : REG_GPIO_INPUT;
  uint32_t value = getRegister(handle, reg);
  return BIT_IS_SET(value, gpio);
}

UNUSED static void setGPIO(PinHandle handle, int gpio, int value) {
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_GPIO_OUTPUT : REG_GPIO_INPUT;
  if (!BIT_IS_SET(direction, gpio)) {
    fprintf(stderr, "setGPIO: gpio %d is not an output register, refusing to set\n", gpio);
    return;
  }
  uint32_t val = getRegister(handle, reg);
  uint32_t newVal = value ? BIT_SET(val, gpio) : BIT_CLR(val, gpio);
  setRegister(handle, reg, newVal);
}

static int getPWMValue(PinHandle handle, UNUSED int gpio, int pwm) {
  uint32_t divider = getRegister(handle, REG_PWM_DIVIDER);      /* shared among all PWM */
  uint32_t lo = getRegister(handle, REG_PWM_LO(pwm));
  uint32_t hi = getRegister(handle, REG_PWM_HI(pwm));
  uint32_t hi_enabled = hi & PWM_TIMER_ENABLE_MASK;
  hi &= ~PWM_TIMER_ENABLE_MASK;
  int is_on = (divider & PWM_DIVIDER_ENABLE_MASK) &&
              hi_enabled &&
              lo < hi;        /* technically true, but maybe not visible */
  return is_on;
}

static void setPWMValue(PinHandle handle, int gpio, int pwm, int value) {
  static uint32_t warn_divider = 0xffffffff;
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  if (!BIT_IS_SET(direction, gpio)) {
    fprintf(stderr, "setPWMValue: gpio %d is not an output register, refusing to set\n", gpio);
    return;
  }
  uint32_t select = getRegister(handle, REG_GPIO_SELECT);
  uint32_t mode = (select >> (2*gpio)) & 0x3;
  if (mode != 0x1) {
    fprintf(stderr, "setPWMValue: setting gpio %d to PWM mode\n", gpio);
    select &= ~(0x3 << (2*gpio));
    select |= (0x1 << (2*gpio));
    setRegister(handle, REG_GPIO_SELECT, select);
  }
  uint32_t divider = getRegister(handle, REG_PWM_DIVIDER);      /* shared among all PWM */
  if (! (divider & PWM_DIVIDER_ENABLE_MASK)) {                  /* not enabled */
    fprintf(stderr, "setPWMValue: divider not enabled, enabling\n");
    divider = PWM_DIVIDER_ENABLE_MASK | PWM_DEFAULT_DIVIDER;
    setRegister(handle, REG_PWM_DIVIDER, divider);
  }
  divider &= PWM_DIVIDER_VALUE_MASK;
  divider++;    /* divider reg is 0-based */
  uint32_t timer = PWM_CLOCK_HZ / divider / PWM_LED_HZ;
  if (timer < 1) {
    timer = 1;
    if (warn_divider != divider) {
      fprintf(stderr, "setPWMValue: PWM_LED_HZ too large, LED will be %d Hz\n",
        PWM_CLOCK_HZ/divider/timer);
      warn_divider = divider;
    }
  } else if (timer > PWM_TIMER_VALUE_MASK+1) {
    timer = PWM_TIMER_VALUE_MASK+1;
    if (warn_divider != divider) {
      fprintf(stderr, "setPWMValue: divider too small, LED will be %d Hz\n",
        PWM_CLOCK_HZ/divider/timer);
      warn_divider = divider;
    }
  }
  /* brighter as duty approaches 0, dimmer as it approaches timer */
  uint32_t duty = timer * (value ? PWM_DUTY_OFF_PERCENT : 100) / 100;
  if (duty < 1) {
    duty = 1;
  }
  if (duty > timer) {
    duty = timer;
  }
  setRegister(handle, REG_PWM_LO(pwm), duty-1);                                 /* duty reg is 0-based */
  setRegister(handle, REG_PWM_HI(pwm), (timer-1) | PWM_TIMER_ENABLE_MASK);      /* timer reg is 0-based */
}

/* should return RPS, not RPM */
static int getFan(PinHandle handle) {
  static int rpm_failed = 0;
  if (!rpm_failed) {
    int val = read_file_long(handle->sys_rpm_path);
    if (val >= 0) return (val+30)/60;   // rounded
    rpm_failed = 1;     // old bootloader doesn't enable tachometer
  }
  return 0;
}

static void setFan(PinHandle handle, int percent) {
  int val = percent * 255 / 100;
  if (val < 0) val = 0;
  if (val > 255) val = 255;
  writeIntToFile(handle->sys_fan_path, val);
}

static int getTemp1(PinHandle handle) {
  return read_file_long(handle->sys_temp1_path);
}

static int getTemp2(PinHandle handle) {
  return read_file_long(handle->sys_temp2_path);
}

/* API implementation */

PinHandle PinCreate(void) {
  char buf[128];
  PinHandle handle = (PinHandle) calloc(1, sizeof (*handle));
  if (handle == NULL) {
    perror("calloc(PinHandle)");
    return NULL;
  }
  handle->fd = -1;
  handle->addr = NULL;

  if (access(blue_led_brightness, F_OK)) {
    /* The sysfs file /sys/class/leds/blue/brightness is not available. Assume
     * we are running on the old kernel that lacks PWM and GPIO drivers. Open
     * /dev/mem so that we can poke at the hw registers directly. */
    handle->fd = open(DEVMEM, O_RDWR);
    if (handle->fd < 0) {
      perror(DEVMEM);
      PinDestroy(handle);
      return NULL;
    }
    handle->addr = mmap(NULL, REG_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED,
                        handle->fd, REG_FIRST);
    if (handle->addr == MAP_FAILED) {
      perror("mmap");
      PinDestroy(handle);
      return NULL;
    }
  } else {
    /* The sysfs file /sys/class/leds/blue/brightness is available. We can use
     * the Linux GPIO and LED abstractions which are accessible through sysfs.
     * */
    writeIntToFile("/sys/class/gpio/export", GPIO_BUTTON);
    writeStringToFile("/sys/class/gpio/gpio" __stringify(GPIO_BUTTON) "/direction", "in");
    writeIntToFile("/sys/class/gpio/gpio" __stringify(GPIO_BUTTON) "/active_low", 1);
  }
  snprintf(buf, sizeof(buf), "%s%s", sys_fan_dir1, SYS_TEMP1);
  if (!access(buf, F_OK))
          handle->sys_fan_dir = sys_fan_dir1;
  else
          handle->sys_fan_dir = sys_fan_dir2;
  snprintf(handle->sys_temp1_path, sizeof(handle->sys_temp1_path), "%s%s",
                  handle->sys_fan_dir, SYS_TEMP1);
  snprintf(handle->sys_temp2_path, sizeof(handle->sys_temp2_path), "%s%s",
                  handle->sys_fan_dir, SYS_TEMP2);
  snprintf(handle->sys_fan_path, sizeof(handle->sys_fan_path), "%s%s",
                  handle->sys_fan_dir, SYS_FAN);
  snprintf(handle->sys_rpm_path, sizeof(handle->sys_rpm_path), "%s%s",
                  handle->sys_fan_dir, SYS_RPM);
  return handle;
}

void PinDestroy(PinHandle handle) {
  if (handle != NULL) {
    if (handle->fd > 0) {
      close(handle->fd);
      handle->fd = -1;
    }
    if (handle->addr != NULL) {
      munmap((void*) handle->addr, REG_LENGTH);
      handle->addr = NULL;
    }
    free(handle);
  }
  return;
}

int PinIsPresent(UNUSED PinHandle handle, PinId id) {
  switch (id) {
    case PIN_LED_RED:
    case PIN_LED_ACTIVITY:
    case PIN_BUTTON_RESET:
    case PIN_TEMP_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_MVOLTS_CPU:
    case PIN_FAN_CHASSIS:
      return 1;

    /* no default here so we can be sure we get all the cases */
    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_NONE:
    case PIN_MAX:
      break;
  }
  return 0;
}

PinStatus PinValue(PinHandle handle, PinId id, int* valueP) {
  switch (id) {
    case PIN_LED_RED:
      *valueP = getPWMValue(handle, GPIO_RED, PWM_RED);
      break;

    case PIN_LED_ACTIVITY:
      *valueP = getPWMValue(handle, GPIO_ACTIVITY, PWM_ACTIVITY);
      break;

    case PIN_BUTTON_RESET:
      if (handle->fd < 0)
        *valueP = !!readIntFromFile(sys_button_reset_path);
      else
        *valueP = !getGPIO(handle, GPIO_BUTTON);  /* inverted */
      break;

    case PIN_TEMP_CPU:
      /* optimus has temp2 sensor placed close to SOC */
      *valueP = getTemp2(handle);
      break;

    case PIN_TEMP_EXTERNAL:
      /* temp1 is the lm96063 internal sensor, which is "external" to the SOC */
      *valueP = getTemp1(handle);
      break;

    case PIN_FAN_CHASSIS:
      *valueP = getFan(handle);
      break;

    case PIN_MVOLTS_CPU:
      *valueP = 1000;   /* TODO(edjames) */
      break;

    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_NONE:
    case PIN_MAX:
      *valueP = 0;
      return PIN_ERROR;
  }
  return PIN_OKAY;
}

PinStatus PinSetValue(PinHandle handle, PinId id, int value) {
  switch (id) {
    case PIN_LED_RED:
      if (handle->fd < 0)
        writeIntToFile(red_led_brightness, value);
      else
        setPWMValue(handle, GPIO_RED, PWM_RED, value);
      break;

    case PIN_LED_ACTIVITY:
      if (handle->fd < 0)
        writeIntToFile(blue_led_brightness, value);
      else
        setPWMValue(handle, GPIO_ACTIVITY, PWM_ACTIVITY, value);
      break;

    case PIN_FAN_CHASSIS:
      setFan(handle, value);
      break;

    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_BUTTON_RESET:
    case PIN_TEMP_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_MVOLTS_CPU:
    case PIN_NONE:
    case PIN_MAX:
      return PIN_ERROR;
  }
  return PIN_OKAY;
}
#endif /* MINDSPEED */
