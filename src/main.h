#ifndef _H_MAIN_
#define _H_MAIN_

#include "phobia/pm.h"
#include "phobia/libm.h"

#include "libc.h"
#include "ntc.h"
#include "tel.h"

typedef struct {

	/* PPM interface (PWM).
	 * */
	int			ppm_reg_ID;
	float			ppm_in_cached;
	float			ppm_in_range[3];
	float			ppm_control_range[3];

	/* STEP/DIR interface.
	 * */
	int			step_reg_ID;
	int			step_baseEP;
	int			step_accuEP;
	float			step_const_ld_EP;

	/* Analog interface.
	 * */
	float			analog_const_GU;
	int			analog_ENABLED;
	int			analog_reg_ID;
	float			analog_in_ANG[3];
	float			analog_in_BRK[2];
	float			analog_in_lost[2];
	float			analog_control_ANG[3];
	float			analog_control_BRK;

	/* Startup control.
	 * */
	int			startup_locked;
	float			startup_in_range[2];

	/* Timeout control.
	 * */
	int			timeout_BASE;
	int			timeout_TIME;
	int			timeout_revol_cached;
	float			timeout_in_cached;
	float			timeout_in_tol;
	float			timeout_shutdown;

	/* CPU load.
	 * */
	int			lc_flag;
	int			lc_tick;
	int			lc_idle;

	/* NTC constants.
	 * */
	ntc_t			ntc_PCB;
	ntc_t			ntc_EXT;

	/* Thermal info.
	 * */
	float			temp_PCB;
	float			temp_EXT;
	float			temp_INT;

	/* Heat control.
	 * */
	float			heat_PCB;
	float			heat_PCB_derated_1;
	float			heat_EXT;
	float			heat_EXT_derated_1;
	float			heat_PCB_FAN;
	float			heat_recovery_gap;

	/* Servo drive.
	 * */
	float			servo_span_mm[2];
	float			servo_uniform_mmps;
	int			servo_mice_role;

	/* FT constants.
	 * */
	int			FT_grab_hz;

	/* HX711 (load cell amplifier).
	 * */
	float			hx711_kg;
	float			hx711_gain[2];
}
application_t;

extern application_t		ap;
extern pmc_t			pm;
extern tel_t			ti;

extern int flash_block_load();
extern int flash_block_relocate();
extern int pm_wait_for_IDLE();

float ADC_get_analog_ANG();
float ADC_get_analog_BRK();

#endif /* _H_MAIN_ */

