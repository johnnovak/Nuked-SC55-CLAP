#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clap/plugin.h"

/// @page LCD
/// @brief read access to the emulated front-panel LCD
///
/// Gives the host the state of the emulated LCD controller, so it can draw
/// the SC-55's display itself. The state is the controller's character
/// memory; the host needs the controller's character font to draw it.
///
/// The plugin only tracks the LCD for hosts that query this extension, and
/// starts doing so at the next activate(): query it between init() and
/// activate(), or the state stays empty until the plugin is activated again.

static CLAP_CONSTEXPR const char NUKED_SC55_EXT_LCD[] = "com.johnnovak.nuked-sc55.lcd/1";

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nuked_sc55_lcd_state {
	// False while the emulated MCU keeps the display switched off.
	bool enabled;

	// Display data RAM, as character codes: row 0 at [0..39], row 1 at
	// [40..79]. Codes 0-15 select the CG RAM characters (code & 7).
	uint8_t ddram[80];

	// Character generator RAM: 8 characters of 8 rows each, 5 dots per row
	// in bits 4..0 (bit 4 is the leftmost dot).
	uint8_t cgram[64];
} nuked_sc55_lcd_state_t;

typedef struct nuked_sc55_plugin_lcd {
	// Copies the current LCD state into *state. Returns false if there is
	// no emulator instance (the plugin failed to initialise). [thread-safe]
	bool(CLAP_ABI* get_state)(const clap_plugin_t* plugin,
	                          nuked_sc55_lcd_state_t* state);
} nuked_sc55_plugin_lcd_t;

#ifdef __cplusplus
}
#endif
