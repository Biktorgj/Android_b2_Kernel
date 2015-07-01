/*
 * LCD panel property definitions.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Joongmock Shin <jmock.shin@samsung.com>
 * Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef LCD_PROPERTY_H
#define LCD_PROPERTY_H

/* definition of flip */
enum lcd_property_flip {
	LCD_PROPERTY_FLIP_NONE = (0 << 0),
	LCD_PROPERTY_FLIP_VERTICAL = (1 << 0),
	LCD_PROPERTY_FLIP_HORIZONTAL = (1 << 1),
};

enum lcd_esd_detected {
	 ESD_DETECTED,
	 ESD_NONE,
};

/*
 * A structure for lcd property.
 *
 * @flip: flip information for each lcd.
 * @dynamic_refresh: enable/disable dynamic refresh.
 * @esd_det: LCD ESD detection gpio.
 * @reset_low: LCD reset low function.
 * @init_backlight: backlight control function for Adonis.
 */
struct lcd_property {
	enum lcd_property_flip flip;
	bool	dynamic_refresh;
#ifdef CONFIG_LCD_ESD
	int det_gpio;
#endif
	void (*reset_low)(void);
	void (*init_backlight)(void);
	void (*backlight_ctrl)(int, int puls_count);
	bool enable_mdnie_pwm;
};

#endif /* LCD_PROPERTY_H */
