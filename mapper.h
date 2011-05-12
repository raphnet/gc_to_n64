#ifndef _mapper_h__
#define _mapper_h__

#define TYPE_NONE			0 // Do nothing output and array terminator.
#define TYPE_BTN			1
#define TYPE_AXIS			2
#define TYPE_AXIS_TO_BTN	3
#define TYPE_NEG_SLD_TO_BTN	4

struct mapping_controller_unit {
//	char id; // array position is id
	char type;
	char value; // boolean for buttons, signed for axis

	// For input, this is the threshold used when controlling 
	// a button from this axis.
	// 
	// IF positive, value must be >= thres to trigger output button.
	// If negative, value must be <= thres to trigger output button.
	//
	// However, if type is TYPE_NEG_SLD_TO_BTN, value must be <= thres regardless
	// of 'thres' sign. (Usefull for L/R sliders which are not operated
	// from a 'central' resting position.)
	//
	// For outputs, this is the valud to assign to this axis if 
	// triggered from a button.
	char thres;

	// For outputs axis, this value is the default value.
	char def;
};

struct mapping_entry {
	char input;
	char output;
};


#define THRES_25_NEG	-32
#define THRES_25_POS	32
#define THRES_FULL_POS	127
#define THRES_FULL_NEG	-127


void mapper_copy(struct mapping_entry *map, struct mapping_controller_unit *input, struct mapping_controller_unit *output);
char mapper_getButton(struct mapping_controller_unit *input);
void mapper_change_mapping_entry(struct mapping_entry *map, int input, int output);

#endif // _mapper_h__
