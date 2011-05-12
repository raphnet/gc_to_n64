#ifndef _eeprom_h__
#define _eeprom_h__

#define EEPROM_MAGIC_SIZE		7
#define EEPROM_APPDATA_SIZE		512

struct eeprom_data_struct {
	unsigned char magic[EEPROM_MAGIC_SIZE]; /* 'TenkiCfg' */
	unsigned char defmap;
	unsigned char appdata[EEPROM_APPDATA_SIZE];
};

extern struct eeprom_data_struct g_eeprom_data;
void eeprom_commit(void);
void eeprom_init(void);

#endif // _eeprom_h__

