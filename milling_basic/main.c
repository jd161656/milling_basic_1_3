/**************************************************************
 * main.c
 * milling_basic
 * Styled terminal UI version
 *************************************************************/

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "mmhal.h"

// Access motor speed timing from mmhal.c
extern volatile int mmhal_low_delay_us;

typedef enum
{
    MODE_MANUAL = 0,
    MODE_COMMAND
} ui_mode_t;

typedef enum
{
    UNITS_MM = 0,
    UNITS_INCH
} units_t;

typedef enum
{
    POSITION_ABSOLUTE = 0,
    POSITION_RELATIVE
} positioning_t;

typedef struct
{
    float x, y, z;
    float home_x, home_y, home_z;
    uint16_t spindle_pwm;
    bool spindle_on;
    ui_mode_t ui_mode;
    units_t units;
    positioning_t positioning;
} machine_state_t;

static machine_state_t machine = {
    .x = 0.0f, .y = 0.0f, .z = 0.0f,
    .home_x = 0.0f, .home_y = 0.0f, .home_z = 0.0f,
    .spindle_pwm = 0,
    .spindle_on = false,
    .ui_mode = MODE_MANUAL,
    .units = UNITS_MM,
    .positioning = POSITION_ABSOLUTE
};

#define STEP_MM       1.0f
#define STEPS_PER_MM  20
#define SPINDLE_STEP  16
#define CMD_BUF_LEN   80

static bool manual_mode = true;

/* ============================================================
   ANSI STYLING
   ============================================================ */

#define ESC "\033"

#define ANSI_RESET       ESC "[0m"
#define ANSI_BOLD        ESC "[1m"

#define FG_BLACK         ESC "[30m"
#define FG_RED           ESC "[31m"
#define FG_GREEN         ESC "[32m"
#define FG_YELLOW        ESC "[33m"
#define FG_BLUE          ESC "[34m"
#define FG_MAGENTA       ESC "[35m"
#define FG_CYAN          ESC "[36m"
#define FG_WHITE         ESC "[37m"

#define BG_BLACK         ESC "[40m"
#define BG_RED           ESC "[41m"
#define BG_GREEN         ESC "[42m"
#define BG_YELLOW        ESC "[43m"
#define BG_BLUE          ESC "[44m"
#define BG_MAGENTA       ESC "[45m"
#define BG_CYAN          ESC "[46m"
#define BG_WHITE         ESC "[47m"

static void ui_clear(void)
{
    printf(ESC "[2J" ESC "[H");
}

static void ui_goto(int row, int col)
{
    printf(ESC "[%d;%dH", row, col);
}

static void ui_clear_line(int row)
{
    ui_goto(row, 1);
    printf(ESC "[2K");
}

static const char *mode_str(void)
{
    return manual_mode ? "MANUAL" : "COMMAND";
}

static const char *units_str(void)
{
    return (machine.units == UNITS_MM) ? "MM" : "INCH";
}

static const char *positioning_str(void)
{
    return (machine.positioning == POSITION_ABSOLUTE) ? "ABSOLUTE" : "RELATIVE";
}

static void ui_draw_box(int top, int left, int width, int height)
{
    ui_goto(top, left);
    printf("┌");
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┐");

    for (int r = top + 1; r < top + height - 1; r++)
    {
        ui_goto(r, left);
        printf("│");
        ui_goto(r, left + width - 1);
        printf("│");
    }

    ui_goto(top + height - 1, left);
    printf("└");
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┘");
}

static void ui_title(int row, int col, const char *title)
{
    ui_goto(row, col);
    printf(ANSI_BOLD FG_CYAN "%s" ANSI_RESET, title);
}

static void ui_draw_static(void)
{
    ui_clear();

    ui_goto(1, 3);
    printf(ANSI_BOLD FG_CYAN "CC2511 CNC MILL CONTROL PANEL" ANSI_RESET);

    ui_draw_box(2, 2, 38, 8);    // machine status
    ui_draw_box(2, 42, 36, 8);   // spindle / state
    ui_draw_box(11, 2, 76, 7);   // controls (made 1 line taller)
    ui_draw_box(19, 2, 76, 3);   // command line
    ui_draw_box(23, 2, 76, 3);   // message

    ui_title(2, 4, " MACHINE ");
    ui_title(2, 44, " STATE ");
    ui_title(11, 4, " CONTROLS ");
    ui_title(19, 4, " COMMAND INPUT ");
    ui_title(23, 4, " MESSAGE ");

    ui_goto(4, 4);  printf("Mode        :");
    ui_goto(5, 4);  printf("Units       :");
    ui_goto(6, 4);  printf("Positioning :");
    ui_goto(8, 4);  printf("Position    : X=       Y=       Z=");
    ui_goto(9, 4);  printf("Home        : X=       Y=       Z=");

    ui_goto(4, 44); printf("Spindle     :");
    ui_goto(5, 44); printf("PWM         :");
    ui_goto(7, 44); printf("UI Notes    :");
    ui_goto(8, 44); printf("Manual = single-key control");
    ui_goto(9, 44); printf("Command = full line + Enter");

    ui_goto(13, 4);
    printf(ANSI_BOLD FG_GREEN "Manual movement:" ANSI_RESET " a/d = X-/X+   s/w = Y-/Y+   f/r = Z-/Z+");

    ui_goto(14, 4);
    printf(ANSI_BOLD FG_YELLOW "Spindle:" ANSI_RESET " [ = slower   ] = faster   o = ON   p = OFF");

    ui_goto(15, 4);
    printf(ANSI_BOLD FG_CYAN "Motor speed:" ANSI_RESET " 1 = very slow   2 = slow   3 = medium");

    ui_goto(16, 4);
    printf(ANSI_BOLD FG_CYAN "Speed trim:" ANSI_RESET " - = slower   + = faster");

    ui_goto(17, 4);
    printf(ANSI_BOLD FG_MAGENTA "Modes:" ANSI_RESET " m = command mode   M2 or M02 = manual mode");

    ui_goto(20, 4);
    printf(FG_WHITE ">" ANSI_RESET " ");

    fflush(stdout);
}

static void ui_update_mode(void)
{
    ui_goto(4, 18);
    if (manual_mode)
    {
        printf(ANSI_BOLD FG_BLACK BG_GREEN " %-10s " ANSI_RESET, mode_str());
    }
    else
    {
        printf(ANSI_BOLD FG_BLACK BG_YELLOW " %-10s " ANSI_RESET, mode_str());
    }
    fflush(stdout);
}

static void ui_update_units(void)
{
    ui_goto(5, 18);
    printf(ANSI_BOLD FG_BLUE "%-10s" ANSI_RESET, units_str());
    fflush(stdout);
}

static void ui_update_positioning(void)
{
    ui_goto(6, 18);
    printf(ANSI_BOLD FG_MAGENTA "%-10s" ANSI_RESET, positioning_str());
    fflush(stdout);
}

static void ui_update_position(void)
{
    ui_goto(8, 19); printf(FG_GREEN "%6.2f" ANSI_RESET, machine.x);
    ui_goto(8, 29); printf(FG_GREEN "%6.2f" ANSI_RESET, machine.y);
    ui_goto(8, 39); printf(FG_GREEN "%6.2f" ANSI_RESET, machine.z);
    fflush(stdout);
}

static void ui_update_home(void)
{
    ui_goto(9, 19); printf(FG_CYAN "%6.2f" ANSI_RESET, machine.home_x);
    ui_goto(9, 29); printf(FG_CYAN "%6.2f" ANSI_RESET, machine.home_y);
    ui_goto(9, 39); printf(FG_CYAN "%6.2f" ANSI_RESET, machine.home_z);
    fflush(stdout);
}

static void ui_update_spindle(void)
{
    ui_goto(4, 58);
    if (machine.spindle_on)
    {
        printf(ANSI_BOLD FG_BLACK BG_GREEN " %-10s " ANSI_RESET, "ON");
    }
    else
    {
        printf(ANSI_BOLD FG_BLACK BG_RED " %-10s " ANSI_RESET, "OFF");
    }

    ui_goto(5, 58);
    printf(FG_YELLOW "%3u     " ANSI_RESET, machine.spindle_pwm);
    fflush(stdout);
}

static void ui_set_message(const char *msg)
{
    ui_goto(24, 4);
    printf(ESC "[2K");
    ui_goto(24, 4);
    printf(FG_WHITE "%-68s" ANSI_RESET, msg);
    fflush(stdout);
}

static void ui_update_command_buffer(const char *buf)
{
    ui_goto(20, 6);
    printf(ESC "[2K");
    ui_goto(20, 6);
    printf(FG_WHITE "%s" ANSI_RESET, buf);
    fflush(stdout);
}

static void ui_refresh_all(void)
{
    ui_update_mode();
    ui_update_units();
    ui_update_positioning();
    ui_update_position();
    ui_update_home();
    ui_update_spindle();
    ui_update_command_buffer("");
    ui_set_message("System ready");
}

/* ============================================================
   MACHINE HELPERS
   ============================================================ */

static float to_mm(float value)
{
    if (machine.units == UNITS_INCH)
    {
        return value * 25.4f;
    }
    return value;
}

static void spindle_apply(uint16_t pwm)
{
    if (pwm > 255)
    {
        pwm = 255;
    }

    machine.spindle_pwm = pwm;
    machine.spindle_on = (pwm > 0);
    mmhal_set_spindle_pwm(pwm);
    ui_update_spindle();
}

static void jog_x(float mm)
{
    int dir = (mm >= 0.0f) ? 1 : -1;
    int steps = (int)((mm >= 0.0f ? mm : -mm) * STEPS_PER_MM);

    for (int i = 0; i < steps; i++)
    {
        mmhal_step_motors(dir, 0, 0);
    }

    machine.x += mm;
    ui_update_position();
}

static void jog_y(float mm)
{
    int dir = (mm >= 0.0f) ? 1 : -1;
    int steps = (int)((mm >= 0.0f ? mm : -mm) * STEPS_PER_MM);

    for (int i = 0; i < steps; i++)
    {
        mmhal_step_motors(0, dir, 0);
    }

    machine.y += mm;
    ui_update_position();
}

static void jog_z(float mm)
{
    int dir = (mm >= 0.0f) ? 1 : -1;
    int steps = (int)((mm >= 0.0f ? mm : -mm) * STEPS_PER_MM);

    for (int i = 0; i < steps; i++)
    {
        mmhal_step_motors(0, 0, dir);
    }

    machine.z += mm;
    ui_update_position();
}

static void move_to(int target_x, int target_y, int target_z)
{
    int dx = target_x - machine.x;
    int dy = target_y - machine.y;
    int dz = target_z - machine.z;

    if (0 <= dx <= 400) jog_x(dx);
    if (0 <= dy <= 268) jog_y(dy);
    if (0 <= dz <= 81) jog_z(dz);
}

static bool try_parse_value(const char *line, char key, float *out_value)
{
    const char *p = line;

    while (*p != '\0')
    {
        if (toupper((unsigned char)*p) == key)
        {
            p++;
            *out_value = strtof(p, NULL);
            return true;
        }
        p++;
    }

    return false;
}

/* ============================================================
   INPUT HANDLERS
   ============================================================ */

static void handle_manual_key(char c)
{
    switch (c)
    {
        case 'a':
            jog_x(-STEP_MM);
            ui_set_message("Jogged X-");
            break;

        case 'd':
            jog_x(STEP_MM);
            ui_set_message("Jogged X+");
            break;

        case 'w':
            jog_y(STEP_MM);
            ui_set_message("Jogged Y+");
            break;

        case 's':
            jog_y(-STEP_MM);
            ui_set_message("Jogged Y-");
            break;

        case 'r':
            jog_z(STEP_MM);
            ui_set_message("Jogged Z+");
            break;

        case 'f':
            jog_z(-STEP_MM);
            ui_set_message("Jogged Z-");
            break;

        case '[':
            if (machine.spindle_pwm >= SPINDLE_STEP)
                spindle_apply(machine.spindle_pwm - SPINDLE_STEP);
            else
                spindle_apply(0);
            ui_set_message("Spindle PWM decreased");
            break;

        case ']':
            if (machine.spindle_pwm <= 255 - SPINDLE_STEP)
                spindle_apply(machine.spindle_pwm + SPINDLE_STEP);
            else
                spindle_apply(255);
            ui_set_message("Spindle PWM increased");
            break;

        case 'o':
            spindle_apply(180);
            ui_set_message("Spindle ON");
            break;

        case 'p':
            spindle_apply(0);
            ui_set_message("Spindle OFF");
            break;

        case '1':
            mmhal_low_delay_us = 3000;   // very slow / safest
            ui_set_message("Motor speed set to VERY SLOW");
            break;

        case '2':
            mmhal_low_delay_us = 2500;   // slow / reliable
            ui_set_message("Motor speed set to SLOW");
            break;

        case '3':
            mmhal_low_delay_us = 1800;   // medium
            ui_set_message("Motor speed set to MEDIUM");
            break;

        case '-':
            if (mmhal_low_delay_us < 10000)
            {
                mmhal_low_delay_us += 200;   // bigger delay = slower
            }
            ui_set_message("Motor slowed down");
            break;

        case '+':
            if (mmhal_low_delay_us > 400)
            {
                mmhal_low_delay_us -= 200;   // smaller delay = faster
            }
            ui_set_message("Motor sped up");
            break;

        case 'm':
            manual_mode = false;
            machine.ui_mode = MODE_COMMAND;
            ui_update_mode();
            ui_update_command_buffer("");
            ui_set_message("Switched to command mode");
            break;

        // case 'h':
        // case '?':
        //     ui_set_message("Manual: a/d x, s/w y, f/r z, 1/2/3 speed, -/+ trim, [/] spindle, o/p on/off, m command");
        //     break;

        default:
            break;
    }
}

static void handle_command_line(char *line)
{
    float x_val, y_val, z_val, p_val, s_val;
    bool has_x = false, has_y = false, has_z = false;

    while (isspace((unsigned char)*line)) line++;

    for (char *p = line; *p; p++)
    {
        *p = (char)toupper((unsigned char)*p);
    }

    if (strcmp(line, "M2") == 0 || strcmp(line, "M02") == 0)
    {
        manual_mode = true;
        machine.ui_mode = MODE_MANUAL;
        ui_update_mode();
        ui_set_message("Returned to manual mode");
        return;
    }

    if (strcmp(line, "G90") == 0)
    {
        machine.positioning = POSITION_ABSOLUTE;
        ui_update_positioning();
        ui_set_message("Positioning set to ABSOLUTE");
        return;
    }

    if (strcmp(line, "G91") == 0)
    {
        machine.positioning = POSITION_RELATIVE;
        ui_update_positioning();
        ui_set_message("Positioning set to RELATIVE");
        return;
    }

    if (strcmp(line, "G20") == 0)
    {
        machine.units = UNITS_INCH;
        ui_update_units();
        ui_set_message("Units set to INCH");
        return;
    }

    if (strcmp(line, "G21") == 0)
    {
        machine.units = UNITS_MM;
        ui_update_units();
        ui_set_message("Units set to MM");
        return;
    }

    if (strcmp(line, "G28") == 0)
    {
        move_to(machine.home_x, machine.home_y, machine.home_z);
        ui_set_message("Moved to home");
        return;
    }

    if (strcmp(line, "G28.1") == 0)
    {
        machine.home_x = machine.x;
        machine.home_y = machine.y;
        machine.home_z = machine.z;
        ui_update_home();
        ui_set_message("Stored current position as home");
        return;
    }

    if (strcmp(line, "M5") == 0)
    {
        spindle_apply(0);
        ui_set_message("Spindle OFF");
        return;
    }

    if (strncmp(line, "M3", 2) == 0)
    {
        if (try_parse_value(line, 'S', &s_val))
        {
            if (s_val < 0.0f) s_val = 0.0f;
            if (s_val > 255.0f) s_val = 255.0f;
            spindle_apply((uint16_t)s_val);
        }
        else
        {
            spindle_apply(180);
        }

        ui_set_message("Spindle ON");
        return;
    }

    if (strncmp(line, "G4", 2) == 0)
    {
        if (try_parse_value(line, 'P', &p_val))
        {
            if (p_val < 0.0f) p_val = 0.0f;
            ui_set_message("Dwelling...");
            sleep_ms((uint32_t)p_val);
            ui_set_message("Dwell complete");
        }
        else
        {
            ui_set_message("G4 needs P value");
        }
        return;
    }

    if (strncmp(line, "G0", 2) == 0 || strncmp(line, "G1", 2) == 0)
    {
        float target_x = machine.x;
        float target_y = machine.y;
        float target_z = machine.z;

        has_x = try_parse_value(line, 'X', &x_val);
        has_y = try_parse_value(line, 'Y', &y_val);
        has_z = try_parse_value(line, 'Z', &z_val);

        if (machine.positioning == POSITION_ABSOLUTE)
        {
            if (has_x) target_x = to_mm(x_val);
            if (has_y) target_y = to_mm(y_val);
            if (has_z) target_z = to_mm(z_val);
        }
        else
        {
            if (has_x) target_x += to_mm(x_val);
            if (has_y) target_y += to_mm(y_val);
            if (has_z) target_z += to_mm(z_val);
        }

        move_to(target_x, target_y, target_z);
        ui_set_message("Motion command complete");
        return;
    }

    if (strcmp(line, "H") == 0 || strcmp(line, "?") == 0)
    {
        ui_set_message("Cmds: G0/G1 G4 G28 G28.1 G20/G21 G90/G91 M3/M5 M2");
        return;
    }

    ui_set_message("Unknown command");
}

/* ============================================================
   MAIN
   ============================================================ */

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);
    mmhal_init();

    ui_draw_static();
    ui_refresh_all();

    char cmd_buf[CMD_BUF_LEN];
    int cmd_len = 0;

    while (true)
    {
        int ch = getchar_timeout_us(0);

        if (ch == PICO_ERROR_TIMEOUT)
        {
            continue;
        }

        if (manual_mode)
        {
            handle_manual_key((char)ch);
        }
        else
        {
            char c = (char)ch;

            if (c == '\r' || c == '\n')
            {
                if (cmd_len > 0)
                {
                    cmd_buf[cmd_len] = '\0';
                    handle_command_line(cmd_buf);
                    cmd_len = 0;
                    ui_update_command_buffer("");
                }
            }
            else if ((c == '\b' || c == 127) && cmd_len > 0)
            {
                cmd_len--;
                cmd_buf[cmd_len] = '\0';
                ui_update_command_buffer(cmd_buf);
            }
            else if (isprint((unsigned char)c) && cmd_len < (CMD_BUF_LEN - 1))
            {
                cmd_buf[cmd_len++] = c;
                cmd_buf[cmd_len] = '\0';
                ui_update_command_buffer(cmd_buf);
            }
        }
    }
}