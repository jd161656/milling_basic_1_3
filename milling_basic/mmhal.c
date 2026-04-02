#include "mmhal.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#ifndef CNC_VERSION
#define CNC_VERSION 2
#endif

// Public pin arrays
const int step_pins[] = { XSTEP_PIN, YSTEP_PIN, ZSTEP_PIN };
const int dir_pins[]  = { XDIR_PIN,  YDIR_PIN,  ZDIR_PIN  };

// Axis direction correction for different machine versions
#if CNC_VERSION == 1
static const int stepper_multipliers[] = { -1, 1, -1 };
#elif CNC_VERSION == 2
static const int stepper_multipliers[] = { 1, -1, 1 };
#else
#error "Invalid CNC_VERSION"
#endif

// Stepper timing
// HIGH delay = pulse width
// LOW delay  = motor speed control
// Bigger LOW delay = slower / safer motor motion
volatile int mmhal_high_delay_us = 3;
volatile int mmhal_low_delay_us  = 2500;   // safer default than 1000

static uint spindle_slice;
static uint spindle_channel;

static void init_output_pin(uint pin, bool initial_value)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, initial_value ? 1 : 0);
}

void mmhal_init(void)
{
    // Step and direction pins
    for (int i = 0; i < DIMCOUNT; i++)
    {
        init_output_pin(step_pins[i], false);
        init_output_pin(dir_pins[i], false);
    }

    // Microstep control pins
    init_output_pin(X_MODE0_PIN, false);
    init_output_pin(X_MODE1_PIN, false);
    init_output_pin(X_MODE2_PIN, false);

    init_output_pin(Y_MODE0_PIN, false);
    init_output_pin(Y_MODE1_PIN, false);
    init_output_pin(Y_MODE2_PIN, false);

    // Enable outputs (assume active-low enable)
    init_output_pin(ENABLE_PIN, false);

    // Optional push input
    gpio_init(PUSH1_PIN);
    gpio_set_dir(PUSH1_PIN, GPIO_IN);
    gpio_pull_up(PUSH1_PIN);

    // Fault inputs
    gpio_init(XFLT_PIN);
    gpio_set_dir(XFLT_PIN, GPIO_IN);
    gpio_pull_up(XFLT_PIN);

    gpio_init(YFLT_PIN);
    gpio_set_dir(YFLT_PIN, GPIO_IN);
    gpio_pull_up(YFLT_PIN);

    gpio_init(ZFLT_PIN);
    gpio_set_dir(ZFLT_PIN, GPIO_IN);
    gpio_pull_up(ZFLT_PIN);

    // Spindle PWM
    gpio_set_function(SPINDLE_PIN, GPIO_FUNC_PWM);
    spindle_slice   = pwm_gpio_to_slice_num(SPINDLE_PIN);
    spindle_channel = pwm_gpio_to_channel(SPINDLE_PIN);

    pwm_set_wrap(spindle_slice, 255);
    pwm_set_chan_level(spindle_slice, spindle_channel, 0);
    pwm_set_enabled(spindle_slice, true);

    // Default microstepping
    mmhal_set_microstepping(0, MMHAL_MS_MODE_1);
    mmhal_set_microstepping(1, MMHAL_MS_MODE_1);
}

void mmhal_set_spindle_pwm(uint16_t pwm_level)
{
    if (pwm_level > 255)
    {
        pwm_level = 255;
    }

    pwm_set_chan_level(spindle_slice, spindle_channel, pwm_level);
}

void mmhal_set_microstepping(int x_or_y, mmhal_microstep_mode_t mode)
{
    int m0, m1, m2;

    if (x_or_y == 0)
    {
        m0 = X_MODE0_PIN;
        m1 = X_MODE1_PIN;
        m2 = X_MODE2_PIN;
    }
    else
    {
        m0 = Y_MODE0_PIN;
        m1 = Y_MODE1_PIN;
        m2 = Y_MODE2_PIN;
    }

    // DRV8825 truth table
    switch (mode)
    {
        case MMHAL_MS_MODE_1:
            gpio_put(m0, 0); gpio_put(m1, 0); gpio_put(m2, 0);
            break;
        case MMHAL_MS_MODE_2:
            gpio_put(m0, 1); gpio_put(m1, 0); gpio_put(m2, 0);
            break;
        case MMHAL_MS_MODE_4:
            gpio_put(m0, 0); gpio_put(m1, 1); gpio_put(m2, 0);
            break;
        case MMHAL_MS_MODE_8:
            gpio_put(m0, 1); gpio_put(m1, 1); gpio_put(m2, 0);
            break;
        case MMHAL_MS_MODE_16:
            gpio_put(m0, 0); gpio_put(m1, 0); gpio_put(m2, 1);
            break;
        case MMHAL_MS_MODE_32:
            gpio_put(m0, 1); gpio_put(m1, 0); gpio_put(m2, 1);
            break;
        default:
            gpio_put(m0, 0); gpio_put(m1, 0); gpio_put(m2, 0);
            break;
    }
}

static void mmhal_step_motors_impl(const int dirs[DIMCOUNT])
{
    // Set direction pins first
    for (int i = 0; i < DIMCOUNT; i++)
    {
        if (dirs[i] == 0)
        {
            continue;
        }

        int actual_dir = dirs[i] * stepper_multipliers[i];
        gpio_put(dir_pins[i], (actual_dir > 0) ? 1 : 0);
    }

    // Allow DIR to settle
    sleep_us(5);

    // Raise step pins for active axes
    for (int i = 0; i < DIMCOUNT; i++)
    {
        if (dirs[i] != 0)
        {
            gpio_put(step_pins[i], 1);
        }
    }

    // Step pulse width
    sleep_us(mmhal_high_delay_us);

    // Lower step pins
    for (int i = 0; i < DIMCOUNT; i++)
    {
        if (dirs[i] != 0)
        {
            gpio_put(step_pins[i], 0);
        }
    }

    // Motor speed control
    sleep_us(mmhal_low_delay_us);
}

void mmhal_step_motors(int x_dir, int y_dir, int z_dir)
{
    int dirs[DIMCOUNT] = { x_dir, y_dir, z_dir };
    mmhal_step_motors_impl(dirs);
}