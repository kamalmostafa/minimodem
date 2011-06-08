
static char
baudot[32][3] = {
    // letter, U.S. figs, CCITT No.2 figs (Europe)
    { '*', '*', '*' },	// NUL
    { 'E', '3', '3' },
    { '\n', '\n', '\n' },
    { 'A', '-', '-' },
    { ' ', ' ', ' ' },	// SPACE
    { 'S', '*', '\'' },	// BELL or apostrophe
    { 'I', '8', '8' },
    { 'U', '7', '7' },

    { '\n', '\n', '\n' },
    { 'D', '$', '*' },	// '$' or ENQ
    { 'R', '4', '4' },
    { 'J', '\'', '*' },	// apostrophe or BELL
    { 'N', ',', ',' },
    { 'F', '!', '!' },
    { 'C', ':', ':' },
    { 'K', '(', '(' },

    { 'T', '5', '5' },
    { 'Z', '"', '+' },
    { 'L', ')', ')' },
    { 'W', '2', '2' },
    { 'H', '#', '*' },	// '#' or British pounds symbol	// FIXME
    { 'Y', '6', '6' },
    { 'P', '0', '0' },
    { 'Q', '1', '1' },

    { 'O', '9', '9' },
    { 'B', '?', '?' },
    { 'G', '&', '&' },
    { '*', '*', '*' },	// FIGS
    { 'M', '.', '.' },
    { 'X', '/', '/' },
    { 'V', ';', '=' },
    { '*', '*', '*' },	// LTRS
};

#define BAUDOT_LTRS	0x1F
#define BAUDOT_FIGS	0x1B
#define BAUDOT_SPACE	0x04

