#ifndef _eeprom_h__
#define _eeprom_h__

#define EEPROM_MAGIC_SIZE		8
#define EEPROM_APPDATA_SIZE		256

// Atmega8 : 512 byte eeprom
//
// 28 bytes per mapping

#define CONVERSION_MODE_OLD_1v5		1
#define CONVERSION_MODE_V2			2
#define CONVERSION_MODE_EXTENDED	3

#define CONVERSION_MAX			3

struct eeprom_data_struct {
	unsigned char magic[EEPROM_MAGIC_SIZE];
	unsigned char defmap;
	unsigned char deadzone_enabled;
	unsigned char old_v1_5_conversion;
	unsigned char conversion_mode;
	unsigned char appdata[EEPROM_APPDATA_SIZE];
};

extern struct eeprom_data_struct g_eeprom_data;
void eeprom_commit(void);
void eeprom_writeDefaults(void);

/** \return 0 if init ok, 1 if corrupted (and fixed)
 */
int eeprom_init(void);

// These make a change and commit
void cycle_conversion_mode(void);

void toggleDeadzone(void);
void setDefaultMapping(int id);

#endif // _eeprom_h__

