
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "soc/sens_reg.h"

// Macros and defines

// The windsensor is connected to GPI34, which equals
// RTC_GPIO4.
// This should normally be 1, and temporarily be pulled
// to 0 on every 1/3rd of a aenometer rotation.
#define WSPORT 4

// debounce-wait.
#define DEBWAIT 3

// Set port mode to Input (floating / high impedance)
.macro SET_PORT_TO_INPUT rtc_port
	WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TC_REG, RTC_GPIO_ENABLE_W1TC_S + \rtc_port, 1, 1)
.endm

// Read Port-Status of "rtc_port" into register R0
.macro READ_PIN rtc_port
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + \rtc_port, 1)
.endm

.data


// variables, global and local. If you mark a variable with .global,
// then the main program will have access to it.
.bss
.global windsenscounter
windsenscounter:
	.long 0


// program code
.text
.global entry
entry:
	SET_PORT_TO_INPUT WSPORT
	nop
	nop

waitforhigh:
	READ_PIN WSPORT
	and r0, r0, 0x01 // mask all bits but the one we care about
	jump waitforhigh, EQ
	// 'debounce' by just waiting for a little bit and rereading.
	wait DEBWAIT
	READ_PIN WSPORT
	and r0, r0, 0x01 // mask all bits but the one we care about
	jump waitforhigh, EQ

waitforlow:
	// wait for the pin to go to 0
	READ_PIN WSPORT	
	and r0, r0, 0x01
	sub r0, r0, 1
	jump waitforlow, EQ
	// 'debounce' by just waiting for a little bit and rereading.
	wait DEBWAIT
	READ_PIN WSPORT	
	and r0, r0, 0x01
	sub r0, r0, 1
	jump waitforlow, EQ
	
	// So the pin has properly changed to low. Increase the counter.
	move r3, windsenscounter
	ld r2, r3, 0
	add r2, r2, 1
	st r2, r3, 0
	jump waitforhigh

