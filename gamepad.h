#ifndef _gamepad_h__
#define _gamepad_h__

typedef struct {
	// size of reports built by buildReport
	int report_size;

	int reportDescriptorSize;
	void *reportDescriptor; // must be in flash

	int deviceDescriptorSize; // if 0, use default
	void *deviceDescriptor; // must be in flash
	
	void (*init)(void);
	char (*update)(char origin);
	char (*changed)(void);
	void (*buildReport)(unsigned char *buf);

	/* Check for the controller */
	char (*probe)(void); /* return true if found */
} Gamepad;

#endif // _gamepad_h__



