#ifndef __MMHAL_H__
#define __MMHAL_H__

#include <stdint.h>

///////////////////////////////////////////////////////////////////////
// Pins
///////////////////////////////////////////////////////////////////////

enum
{
    SERIAL_TX_PIN = 0,
    SERIAL_RX_PIN = 1,
    X_MODE0_PIN = 2,
    X_MODE1_PIN = 3,
    X_MODE2_PIN = 4,
    XSTEP_PIN   = 5,
    XDIR_PIN    = 6,
    XFLT_PIN    = 7,
    Y_MODE0_PIN = 8,
    Y_MODE1_PIN = 9,
    Y_MODE2_PIN = 10,
    YSTEP_PIN   = 11,
    YDIR_PIN    = 12,
    YFLT_PIN    = 13,
    ZSTEP_PIN   = 14,
    ZDIR_PIN    = 15,
    ZFLT_PIN    = 16,
    SPINDLE_PIN = 17,
    PUSH1_PIN   = 18,
    GPIO19_PIN  = 19,
    GPIO20_PIN  = 20,
    GPIO21_PIN  = 21,
    ENABLE_PIN  = 22,
    GPIO26_PIN  = 26,
    GPIO27_PIN  = 27,
    GPIO28_PIN  = 28
};

///////////////////////////////////////////////////////////////////////
// Stepper motors
///////////////////////////////////////////////////////////////////////

enum DIMS
{
    XDIM = 0,
    YDIM,
    ZDIM,
    DIMCOUNT
};

extern const int step_pins[];
extern const int dir_pins[];

extern volatile int mmhal_high_delay_us;
extern volatile int mmhal_low_delay_us;

typedef enum
{
    MMHAL_MS_MODE_1 = 0,
    MMHAL_MS_MODE_2,
    MMHAL_MS_MODE_4,
    MMHAL_MS_MODE_8,
    MMHAL_MS_MODE_16,
    MMHAL_MS_MODE_32
} mmhal_microstep_mode_t;

void mmhal_init(void);
void mmhal_step_motors(int x_dir, int y_dir, int z_dir);
void mmhal_set_microstepping(int x_or_y, mmhal_microstep_mode_t mode);
void mmhal_set_spindle_pwm(uint16_t pwm_level);

#endif // __MMHAL_H__