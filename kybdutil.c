/*
 *  Utility module. Given a format string it generates 8-byte USB HID keyboard
 *  reports suitable to send to the host.
 *
 *  Authors:
 *      Collin Mulliner (collin AT mulliner.org)
 *      Quentin Young (qlyoung AT qlyoung.net
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include "kybdutil.h"

/**
 * Key struct. Holds a character, its HID
 * Usage ID, and a bit field that indicates
 * what modifier keys must be pressed to produce it.
 */
struct key_t {
    // literal character or the escape code of a special
    char k;
    // HID keycode for that character
    char c;
    // modifier byte needed to generate the character
    char mod;
};

/** Table of key_t for digital keys */
static struct key_t keys_num[] = {
    {.k = '0', .c = 0x27, .mod=0x00},
    {.k = '1', .c = 0x1e, .mod=0x00},
    {.k = '2', .c = 0x1f, .mod=0x00},
    {.k = '3', .c = 0x20, .mod=0x00},
    {.k = '4', .c = 0x21, .mod=0x00},
    {.k = '5', .c = 0x22, .mod=0x00},
    {.k = '6', .c = 0x23, .mod=0x00},
    {.k = '7', .c = 0x24, .mod=0x00},
    {.k = '8', .c = 0x25, .mod=0x00},
    {.k = '9', .c = 0x26, .mod=0x00},
};

/**
 * Table of key_t for non-alphanumeric keys and
 * escape codes
 */
static struct key_t keys_symbol[] = {
    {.k = '!', .c = 0x1e, .mod=0x20},
    {.k = '@', .c = 0x1f, .mod=0x20},
    {.k = '#', .c = 0x20, .mod=0x20},
    {.k = '$', .c = 0x21, .mod=0x20},
    {.k = '%', .c = 0x22, .mod=0x20},
    {.k = '^', .c = 0x23, .mod=0x20},
    {.k = '&', .c = 0x24, .mod=0x20},
    {.k = '*', .c = 0x25, .mod=0x20},
    {.k = '(', .c = 0x26, .mod=0x20},
    {.k = ')', .c = 0x27, .mod=0x20},
    {.k = '-', .c = 0x2d, .mod=0x00},
    {.k = '_', .c = 0x2d, .mod=0x20},
    {.k = '+', .c = 0x2e, .mod=0x20},
    {.k = '=', .c = 0x2e, .mod=0x00},
    {.k = '[', .c = 0x2f, .mod=0x00},
    {.k = '{', .c = 0x2f, .mod=0x20},
    {.k = ']', .c = 0x30, .mod=0x00},
    {.k = '}', .c = 0x30, .mod=0x20},
    {.k ='\\', .c = 0x31, .mod=0x00},
    {.k = '|', .c = 0x31, .mod=0x20},
    {.k = ';', .c = 0x33, .mod=0x00},
    {.k = ':', .c = 0x33, .mod=0x20},
    {.k ='\'', .c = 0x34, .mod=0x00},
    {.k = '"', .c = 0x34, .mod=0x20},
    {.k = ',', .c = 0x36, .mod=0x00},
    {.k = '<', .c = 0x36, .mod=0x20},
    {.k = '.', .c = 0x37, .mod=0x00},
    {.k = '>', .c = 0x37, .mod=0x20},
    {.k = '/', .c = 0x38, .mod=0x00},
    {.k = '?', .c = 0x38, .mod=0x20},
    {.k = '`', .c = 0x35, .mod=0x00},
    {.k = '~', .c = 0x35, .mod=0x20},
    {.k = ' ', .c = 0x2c, .mod=0x00},
    {.k = 0x0, .c = 0x00, .mod=0x00}
};
static struct key_t keys_escape[] = {
    // escape codes
    {.k = ALT,      .c = 0x00, .mod=0x04}, // alt
    {.k = BACKSPACE,.c = 0x2A, .mod=0x00}, // backspace
    {.k = CONTROL,  .c = 0x00, .mod=0x01}, // ctrl
    {.k = DELETE,   .c = 0x4C, .mod=0x00}, // delete
    {.k = ESCAPE,   .c = 0x29, .mod=0x00}, // esc
    {.k = END,      .c = 0x4D, .mod=0x00}, // end
    {.k = GUI,      .c = 0x00, .mod=0x08}, // gui/win
    {.k = HOME,     .c = 0x4A, .mod=0x00}, // home
    {.k = INSERT,   .c = 0x49, .mod=0x00}, // insert
    {.k = MENU,     .c = 0x76, .mod=0x00}, // menu
    {.k = ENTER,    .c = 0x28, .mod=0x00}, // enter
    {.k = SHIFT,    .c = 0x00, .mod=0x02}, // shift
    {.k = TAB,      .c = 0x2B, .mod=0x00}, // tab
    {.k = LARROW,   .c = 0x50, .mod=0x00}, // left arrow
    {.k = RARROW,   .c = 0x4F, .mod=0x00}, // right arrow
    {.k = UARROW,   .c = 0x52, .mod=0x00}, // up arrow
    {.k = DARROW,   .c = 0x51, .mod=0x00}, // down arrow
    {.k = PAGEUP,   .c = 0x4B, .mod=0x00}, // page up
    {.k = PAGEDOWN, .c = 0x4E, .mod=0x00}, // page down
    {.k = SPACE,    .c = 0x2C, .mod=0x00}, // space (helps with parsing)
    {.k = 0x0, .c = 0x00, .mod=0x00}
};


int make_hid_report(char *report, int numescape, int argc, ...)
{
    // vargs list
    va_list chars;
    va_start(chars, argc);

    // starting index in the report; skip first two bytes
    // as the last 6 hold key data
    int index = 2;


    // fill bytes 2-7 with Usage ID's for characters we were passed
    // if we were passed more than 6 chars, just process the first 6
    argc = argc > 6 ? 6 : argc;
    for (int ic = 0; ic < argc; ic++) {

        // get the input character we're working with
        char input = (char) va_arg(chars, int);

        // if we're still processing escapes,
        if (ic < numescape) {
            // search for the escape in our lookup table
            for (int i = 0; i < sizeof(keys_escape); i++) {
                if (input == keys_escape[i].k) {
                    // set the current report byte to the Usage ID
                    if (keys_escape[i].c != 0) {
                        report[index] = keys_escape[i].c;
                    }
                    // OR in any modifiers needed to produce the character
                    report[0] |= keys_escape[i].mod;
                    index++;
                    break;
                }
                // if we reached the end of the list, break
                if (keys_escape[i].k == 0)
                    break;
            }

        }
        else {
            // handle case where character is a letter
            if (isalpha(input)) {
                // calculate Usage ID; ASCII lowercase letters map directly
                // to their Usage ID's by subtracting 93.
                // See Usage Page 0x07 for this conversion
                report[index] = tolower(input) - ('a' - 4);
                index++;
                // if the character is a capital letter, set bits 1 and 5 of byte 0
                // (modifier byte) to indicate left and right shift keys
                if (isupper(input)) {
                    report[0] = 0x22;
                }
            }
            // handle case where character is a digit
            else if (isdigit(input)) {
                // calculate Usage ID with the table above
                report[index] = keys_num[input - '0'].c;
                index++;
            }
            // if it's printable, assume it's a symbol
            else if (isprint(input)) {
                for (int i = 0; i < sizeof(keys_symbol); i++) {
                    if (input == keys_symbol[i].k) {
                        // set the current report byte to the Usage ID
                        if (keys_symbol[i].c != 0) {
                            report[index] = keys_symbol[i].c;
                        }
                        // OR in any modifiers needed to produce the character
                        report[index] |= keys_symbol[i].mod;
                        index++;
                        break;
                    }
                    // if we reached the end of the list, break
                    if (keys_symbol[i].k == 0)
                        break;
                }
            }
        }

        // if the first data byte is unset and the modifier is also unset
        // then this report is not valid and we should return
        if (report[2] == 0 && report[0] == 0) {
            printf("error for >%c<\n", input);
            return 1;
        }

    }

    return 0;
}

int make_hid_report_arr(char *report, int numspec, int argc, char* chars) {
  // i'm so sorry
  switch (argc) {
    case 1:
      return make_hid_report(report, numspec, argc, chars[0]);
    case 2:
      return make_hid_report(report, numspec, argc, chars[0], chars[1]);
    case 3:
      return make_hid_report(report, numspec, argc, chars[0], chars[1], chars[2]);
    case 4:
      return make_hid_report(report, numspec, argc, chars[0], chars[1], chars[2], chars[3]);
    case 5:
      return make_hid_report(report, numspec, argc, chars[0], chars[1], chars[2], chars[3],
                             chars[4]);
    case 6:
      return make_hid_report(report, numspec, argc, chars[0], chars[1], chars[2], chars[3],
                             chars[4], chars[5]);
  }
  return 1;
}
