#include <string.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "emmit_funcs.h"

//struct that holds keypress enum array
typedef struct _emmit_KEYPRESS{
    int* key_enums;
    unsigned int key_enums_size;
    unsigned int key_inv; //should the keypress be repeated, but in its inverted form (sending keypress on and keypress off signals for the key)
}EMMIT_KEYPRESS;

//function that writes bits to the fd to simulate a keypress
static void emit(int fd, int type, int code, int val){
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    write(fd, &ie, sizeof(ie));
}
//convert input string (for ex KEY_W) to code
int app_emmit_convert_to_enum(const char* in_string){
    if(strcmp(in_string, "KEY_ESC") == 0)return 1;
    if(strcmp(in_string, "KEY_1") == 0)return 2;
    if(strcmp(in_string, "KEY_2") == 0)return 3;
    if(strcmp(in_string, "KEY_3") == 0)return 4;
    if(strcmp(in_string, "KEY_4") == 0)return 5;
    if(strcmp(in_string, "KEY_5") == 0)return 6;
    if(strcmp(in_string, "KEY_6") == 0)return 7;
    if(strcmp(in_string, "KEY_7") == 0)return 8;
    if(strcmp(in_string, "KEY_8") == 0)return 9;
    if(strcmp(in_string, "KEY_9") == 0)return 10;
    if(strcmp(in_string, "KEY_0") == 0)return 11;
    if(strcmp(in_string, "KEY_MINUS") == 0)return 12;
    if(strcmp(in_string, "KEY_EQUAL") == 0)return 13;
    if(strcmp(in_string, "KEY_BACKSPACE") == 0)return 14;
    if(strcmp(in_string, "KEY_TAB") == 0)return 15;
    if(strcmp(in_string, "KEY_Q") == 0)return 16;
    if(strcmp(in_string, "KEY_W") == 0)return 17;
    if(strcmp(in_string, "KEY_E") == 0)return 18;
    if(strcmp(in_string, "KEY_R") == 0)return 19;
    if(strcmp(in_string, "KEY_T") == 0)return 20;
    if(strcmp(in_string, "KEY_Y") == 0)return 21;
    if(strcmp(in_string, "KEY_U") == 0)return 22;
    if(strcmp(in_string, "KEY_I") == 0)return 23;
    if(strcmp(in_string, "KEY_O") == 0)return 24;
    if(strcmp(in_string, "KEY_P") == 0)return 25;
    if(strcmp(in_string, "KEY_LEFTBRACE") == 0)return 26;
    if(strcmp(in_string, "KEY_RIGHTBRACE") == 0)return 27;
    if(strcmp(in_string, "KEY_ENTER") == 0)return 28;
    if(strcmp(in_string, "KEY_LEFTCTRL") == 0)return 29;
    if(strcmp(in_string, "KEY_A") == 0)return 30;
    if(strcmp(in_string, "KEY_S") == 0)return 31;
    if(strcmp(in_string, "KEY_D") == 0)return 32;
    if(strcmp(in_string, "KEY_F") == 0)return 33;
    if(strcmp(in_string, "KEY_G") == 0)return 34;
    if(strcmp(in_string, "KEY_H") == 0)return 35;
    if(strcmp(in_string, "KEY_J") == 0)return 36;
    if(strcmp(in_string, "KEY_K") == 0)return 37;
    if(strcmp(in_string, "KEY_L") == 0)return 38;
    if(strcmp(in_string, "KEY_SEMICOLON") == 0)return 39;
    if(strcmp(in_string, "KEY_APOSTROPHE") == 0)return 40;
    if(strcmp(in_string, "KEY_GRAVE") == 0)return 41;
    if(strcmp(in_string, "KEY_LEFTSHIFT") == 0)return 42;
    if(strcmp(in_string, "KEY_BACKSLASH") == 0)return 43;
    if(strcmp(in_string, "KEY_Z") == 0)return 44;
    if(strcmp(in_string, "KEY_X") == 0)return 45;
    if(strcmp(in_string, "KEY_C") == 0)return 46;
    if(strcmp(in_string, "KEY_V") == 0)return 47;
    if(strcmp(in_string, "KEY_B") == 0)return 48;
    if(strcmp(in_string, "KEY_N") == 0)return 49;
    if(strcmp(in_string, "KEY_M") == 0)return 50;
    if(strcmp(in_string, "KEY_COMMA") == 0)return 51;
    if(strcmp(in_string, "KEY_DOT") == 0)return 52;
    if(strcmp(in_string, "KEY_SLASH") == 0)return 53;
    if(strcmp(in_string, "KEY_RIGHTSHIFT") == 0)return 54;
    if(strcmp(in_string, "KEY_KPASTERISK") == 0)return 55;
    if(strcmp(in_string, "KEY_LEFTALT") == 0)return 56;
    if(strcmp(in_string, "KEY_SPACE") == 0)return 57;
    if(strcmp(in_string, "KEY_CAPSLOCK") == 0)return 58;
    if(strcmp(in_string, "KEY_F1") == 0)return 59;
    if(strcmp(in_string, "KEY_F2") == 0)return 60;
    if(strcmp(in_string, "KEY_F3") == 0)return 61;
    if(strcmp(in_string, "KEY_F4") == 0)return 62;
    if(strcmp(in_string, "KEY_F5") == 0)return 63;
    if(strcmp(in_string, "KEY_F6") == 0)return 64;
    if(strcmp(in_string, "KEY_F7") == 0)return 65;
    if(strcmp(in_string, "KEY_F8") == 0)return 66;
    if(strcmp(in_string, "KEY_F9") == 0)return 67;
    if(strcmp(in_string, "KEY_F10") == 0)return 68;
    if(strcmp(in_string, "KEY_NUMLOCK") == 0)return 69;
    if(strcmp(in_string, "KEY_SCROLLOCK") == 0)return 70;
    if(strcmp(in_string, "KEY_KP7") == 0)return 71;
    if(strcmp(in_string, "KEY_KP8") == 0)return 72;
    if(strcmp(in_string, "KEY_KP9") == 0)return 73;
    if(strcmp(in_string, "KEY_KPMINUS") == 0)return 74;
    if(strcmp(in_string, "KEY_KP4") == 0)return 75;
    if(strcmp(in_string, "KEY_KP5") == 0)return 76;
    if(strcmp(in_string, "KEY_KP6") == 0)return 77;
    if(strcmp(in_string, "KEY_KPPLUS") == 0)return 78;
    if(strcmp(in_string, "KEY_KP1") == 0)return 79;
    if(strcmp(in_string, "KEY_KP2") == 0)return 80;
    if(strcmp(in_string, "KEY_KP3") == 0)return 81;
    if(strcmp(in_string, "KEY_KP0") == 0)return 82;
    if(strcmp(in_string, "KEY_KPDOT") == 0)return 83;
    
    if(strcmp(in_string, "KEY_ZENKAKUHANKAKU") == 0)return 85;
    if(strcmp(in_string, "KEY_102ND") == 0)return 86;
    if(strcmp(in_string, "KEY_F11") == 0)return 87;
    if(strcmp(in_string, "KEY_F12") == 0)return 88;
    if(strcmp(in_string, "KEY_RO") == 0)return 89;
    if(strcmp(in_string, "KEY_KATAKANA") == 0)return 90;
    if(strcmp(in_string, "KEY_HIRAGANA") == 0)return 91;
    if(strcmp(in_string, "KEY_HENKAN") == 0)return 92;
    if(strcmp(in_string, "KEY_KATAKANAHIRAGANA") == 0)return 93;
    if(strcmp(in_string, "KEY_MUHENKAN") == 0)return 94;
    if(strcmp(in_string, "KEY_KPJPCOMMA") == 0)return 95;
    if(strcmp(in_string, "KEY_KPENTER") == 0)return 96;
    if(strcmp(in_string, "KEY_RIGHTCTRL") == 0)return 97;
    if(strcmp(in_string, "KEY_KPSLASH") == 0)return 98;
    if(strcmp(in_string, "KEY_SYSRQ") == 0)return 99;
    if(strcmp(in_string, "KEY_RIGHTALT") == 0)return 100;
    if(strcmp(in_string, "KEY_LINEFEED") == 0)return 101;
    if(strcmp(in_string, "KEY_HOME") == 0)return 102;
    if(strcmp(in_string, "KEY_UP") == 0)return 103;
    if(strcmp(in_string, "KEY_PAGEUP") == 0)return 104;
    if(strcmp(in_string, "KEY_LEFT") == 0)return 105;
    if(strcmp(in_string, "KEY_RIGHT") == 0)return 106;
    if(strcmp(in_string, "KEY_END") == 0)return 107;
    if(strcmp(in_string, "KEY_DOWN") == 0)return 108;
    if(strcmp(in_string, "KEY_PAGEDOWN") == 0)return 109;
    if(strcmp(in_string, "KEY_INSERT") == 0)return 110;
    if(strcmp(in_string, "KEY_DELETE") == 0)return 111;
    if(strcmp(in_string, "KEY_MACRO") == 0)return 112;
    if(strcmp(in_string, "KEY_MUTE") == 0)return 113;
    if(strcmp(in_string, "KEY_VOLUMEDOWN") == 0)return 114;
    if(strcmp(in_string, "KEY_VOLUMEUP") == 0)return 115;
    if(strcmp(in_string, "KEY_POWER") == 0)return 116;
    if(strcmp(in_string, "KEY_KPEQUAL") == 0)return 117;
    if(strcmp(in_string, "KEY_KPPLUSMINUS") == 0)return 118;
    if(strcmp(in_string, "KEY_PAUSE") == 0)return 119;
    if(strcmp(in_string, "KEY_SCALE") == 0)return 120;
    
    if(strcmp(in_string, "KEY_KPCOMMA") == 0)return 121;
    if(strcmp(in_string, "KEY_HANGEUL") == 0)return 122;
    if(strcmp(in_string, "KEY_HANGUEL") == 0)return 122;
    if(strcmp(in_string, "KEY_HANJA") == 0)return 123;
    if(strcmp(in_string, "KEY_YEN") == 0)return 124;
    if(strcmp(in_string, "KEY_LEFTMETA") == 0)return 125;
    if(strcmp(in_string, "KEY_RIGHTMETA") == 0)return 126;
    if(strcmp(in_string, "KEY_COMPOSE") == 0)return 127;
    
    if(strcmp(in_string, "KEY_STOP") == 0)return 128;
    if(strcmp(in_string, "KEY_AGAIN") == 0)return 129;
    if(strcmp(in_string, "KEY_PROPS") == 0)return 130;
    if(strcmp(in_string, "KEY_UNDO") == 0)return 131;
    if(strcmp(in_string, "KEY_FRONT") == 0)return 132;
    if(strcmp(in_string, "KEY_COPY") == 0)return 133;
    if(strcmp(in_string, "KEY_OPEN") == 0)return 134;
    if(strcmp(in_string, "KEY_PASTE") == 0)return 135;
    if(strcmp(in_string, "KEY_FIND") == 0)return 136;
    if(strcmp(in_string, "KEY_CUT") == 0)return 137;
    if(strcmp(in_string, "KEY_HELP") == 0)return 138;
    if(strcmp(in_string, "KEY_MENU") == 0)return 139;
    if(strcmp(in_string, "KEY_CALC") == 0)return 140;
    if(strcmp(in_string, "KEY_SETUP") == 0)return 141;
    if(strcmp(in_string, "KEY_SLEEP") == 0)return 142;
    if(strcmp(in_string, "KEY_WAKEUP") == 0)return 143;
    if(strcmp(in_string, "KEY_FILE") == 0)return 144;
    if(strcmp(in_string, "KEY_SENDFILE") == 0)return 145;
    if(strcmp(in_string, "KEY_DELETEFILE") == 0)return 146;
    if(strcmp(in_string, "KEY_XFER") == 0)return 147;
    if(strcmp(in_string, "KEY_PROG1") == 0)return 148;
    if(strcmp(in_string, "KEY_PROG2") == 0)return 149;
    if(strcmp(in_string, "KEY_WWW") == 0)return 150;
    if(strcmp(in_string, "KEY_MSDOS") == 0)return 151;
    if(strcmp(in_string, "KEY_COFFEE") == 0)return 152;
    if(strcmp(in_string, "KEY_SCREENLOCK") == 0)return 152;
    if(strcmp(in_string, "KEY_ROTATE_DISPLAY") == 0)return 153;
    if(strcmp(in_string, "KEY_DIRECTION") == 0)return 153;
    if(strcmp(in_string, "KEY_CYCLEWINDOWS") == 0)return 154;
    if(strcmp(in_string, "KEY_MAIL") == 0)return 155;
    if(strcmp(in_string, "KEY_BOOKMARKS") == 0)return 156;
    if(strcmp(in_string, "KEY_COMPUTER") == 0)return 157;
    if(strcmp(in_string, "KEY_BACK") == 0)return 158;
    if(strcmp(in_string, "KEY_FORWARD") == 0)return 159;
    if(strcmp(in_string, "KEY_CLOSECD") == 0)return 160;
    if(strcmp(in_string, "KEY_EJECTCD") == 0)return 161;
    if(strcmp(in_string, "KEY_EJECTCLOSECD") == 0)return 162;
    if(strcmp(in_string, "KEY_NEXTSONG") == 0)return 163;
    if(strcmp(in_string, "KEY_PLAYPAUSE") == 0)return 164;
    if(strcmp(in_string, "KEY_PREVIOUSSONG") == 0)return 165;
    if(strcmp(in_string, "KEY_STOPCD") == 0)return 166;
    if(strcmp(in_string, "KEY_RECORD") == 0)return 167;
    if(strcmp(in_string, "KEY_REWIND") == 0)return 168;
    if(strcmp(in_string, "KEY_PHONE") == 0)return 169;
    if(strcmp(in_string, "KEY_ISO") == 0)return 170;
    if(strcmp(in_string, "KEY_CONFIG") == 0)return 171;
    if(strcmp(in_string, "KEY_HOMEPAGE") == 0)return 172;
    if(strcmp(in_string, "KEY_REFRESH") == 0)return 173;
    if(strcmp(in_string, "KEY_EXIT") == 0)return 174;
    if(strcmp(in_string, "KEY_MOVE") == 0)return 175;
    if(strcmp(in_string, "KEY_EDIT") == 0)return 176;
    if(strcmp(in_string, "KEY_SCROLLUP") == 0)return 177;
    if(strcmp(in_string, "KEY_SCROLLDOWN") == 0)return 178;
    if(strcmp(in_string, "KEY_KPLEFTPAREN") == 0)return 179;
    if(strcmp(in_string, "KEY_KPRIGHTPAREN") == 0)return 180;
    if(strcmp(in_string, "KEY_NEW") == 0)return 181;
    if(strcmp(in_string, "KEY_REDO") == 0)return 182;
    
    if(strcmp(in_string, "KEY_F13") == 0)return 183;
    if(strcmp(in_string, "KEY_F14") == 0)return 184;
    if(strcmp(in_string, "KEY_F15") == 0)return 185;
    if(strcmp(in_string, "KEY_F16") == 0)return 186;
    if(strcmp(in_string, "KEY_F17") == 0)return 187;
    if(strcmp(in_string, "KEY_F18") == 0)return 188;
    if(strcmp(in_string, "KEY_F19") == 0)return 189;
    if(strcmp(in_string, "KEY_F20") == 0)return 190;
    if(strcmp(in_string, "KEY_F21") == 0)return 191;
    if(strcmp(in_string, "KEY_F22") == 0)return 192;
    if(strcmp(in_string, "KEY_F23") == 0)return 193;
    if(strcmp(in_string, "KEY_F24") == 0)return 194;
    
    if(strcmp(in_string, "KEY_PLAYCD") == 0)return 200;
    if(strcmp(in_string, "KEY_PAUSECD") == 0)return 201;
    if(strcmp(in_string, "KEY_PROG3") == 0)return 202;
    if(strcmp(in_string, "KEY_PROG4") == 0)return 203;
    if(strcmp(in_string, "KEY_ALL_APPLICATIONS") == 0)return 204;
    if(strcmp(in_string, "KEY_DASHBOARD") == 0)return 204;
    if(strcmp(in_string, "KEY_SUSPEND") == 0)return 205;
    if(strcmp(in_string, "KEY_CLOSE") == 0)return 206;
    if(strcmp(in_string, "KEY_PLAY") == 0)return 207;
    if(strcmp(in_string, "KEY_FASTFORWARD") == 0)return 208;
    if(strcmp(in_string, "KEY_BASSBOOST") == 0)return 209;
    if(strcmp(in_string, "KEY_PRINT") == 0)return 210;
    if(strcmp(in_string, "KEY_HP") == 0)return 211;
    if(strcmp(in_string, "KEY_CAMERA") == 0)return 212;
    if(strcmp(in_string, "KEY_SOUND") == 0)return 213;
    if(strcmp(in_string, "KEY_QUESTION") == 0)return 214;
    if(strcmp(in_string, "KEY_EMAIL") == 0)return 215;
    if(strcmp(in_string, "KEY_CHAT") == 0)return 216;
    if(strcmp(in_string, "KEY_SEARCH") == 0)return 217;
    if(strcmp(in_string, "KEY_CONNECT") == 0)return 218;
    if(strcmp(in_string, "KEY_FINANCE") == 0)return 219;
    if(strcmp(in_string, "KEY_SPORT") == 0)return 220;
    if(strcmp(in_string, "KEY_SHOP") == 0)return 221;
    if(strcmp(in_string, "KEY_ALTERASE") == 0)return 222;
    if(strcmp(in_string, "KEY_CANCEL") == 0)return 223;
    if(strcmp(in_string, "KEY_BRIGHTNESSDOWN") == 0)return 224;
    if(strcmp(in_string, "KEY_BRIGHTNESSUP") == 0)return 225;
    if(strcmp(in_string, "KEY_MEDIA") == 0)return 226;
    
    if(strcmp(in_string, "KEY_SWITCHVIDEOMODE") == 0)return 227;
    
    if(strcmp(in_string, "KEY_KBDILLUMTOGGLE") == 0)return 228;
    if(strcmp(in_string, "KEY_KBDILLUMDOWN") == 0)return 229;
    if(strcmp(in_string, "KEY_KBDILLUMUP") == 0)return 230;
    
    if(strcmp(in_string, "KEY_SEND") == 0)return 231;
    if(strcmp(in_string, "KEY_REPLY") == 0)return 232;
    if(strcmp(in_string, "KEY_FORWARDMAIL") == 0)return 233;
    if(strcmp(in_string, "KEY_SAVE") == 0)return 234;
    if(strcmp(in_string, "KEY_DOCUMENTS") == 0)return 235;
    
    if(strcmp(in_string, "KEY_BATTERY") == 0)return 236;
    
    if(strcmp(in_string, "KEY_BLUETOOTH") == 0)return 237;
    if(strcmp(in_string, "KEY_WLAN") == 0)return 238;
    if(strcmp(in_string, "KEY_UWB") == 0)return 239;
    
    if(strcmp(in_string, "KEY_UNKOWN") == 0)return 240;
    
    if(strcmp(in_string, "KEY_VIDEO_NEXT") == 0)return 241;
    if(strcmp(in_string, "KEY_VIDEO_PREV") == 0)return 242;
    if(strcmp(in_string, "KEY_BRIGHTNESS_CYCLE") == 0)return 243;
    if(strcmp(in_string, "KEY_BRIGHTNESS_AUTO") == 0)return 244;
    
    if(strcmp(in_string, "KEY_BRIGHTNESS_ZERO") == 0)return 244;
    if(strcmp(in_string, "KEY_DISPLAY_OFF") == 0)return 245;
    
    if(strcmp(in_string, "KEY_WWAN") == 0)return 246;
    if(strcmp(in_string, "KEY_WIMAX") == 0)return 246;
    if(strcmp(in_string, "KEY_RFKILL") == 0)return 247;
    
    if(strcmp(in_string, "KEY_MICMUTE") == 0)return 248;
    
    if(strcmp(in_string, "BTN_MISC") == 0)return 0x100;
    if(strcmp(in_string, "BTN_0") == 0)return 0x100;
    if(strcmp(in_string, "BTN_1") == 0)return 0x101;
    if(strcmp(in_string, "BTN_2") == 0)return 0x102;
    if(strcmp(in_string, "BTN_3") == 0)return 0x103;
    if(strcmp(in_string, "BTN_4") == 0)return 0x104;
    if(strcmp(in_string, "BTN_5") == 0)return 0x105;
    if(strcmp(in_string, "BTN_6") == 0)return 0x106;
    if(strcmp(in_string, "BTN_7") == 0)return 0x107;
    if(strcmp(in_string, "BTN_8") == 0)return 0x108;
    if(strcmp(in_string, "BTN_9") == 0)return 0x109;
    
    if(strcmp(in_string, "BTN_MOUSE") == 0)return 0x110;
    if(strcmp(in_string, "BTN_LEFT") == 0)return 0x110;
    if(strcmp(in_string, "BTN_RIGHT") == 0)return 0x111;
    if(strcmp(in_string, "BTN_MIDDLE") == 0)return 0x112;
    if(strcmp(in_string, "BTN_SIDE") == 0)return 0x113;
    if(strcmp(in_string, "BTN_EXTRA") == 0)return 0x114;
    if(strcmp(in_string, "BTN_FORWARD") == 0)return 0x115;
    if(strcmp(in_string, "BTN_BACK") == 0)return 0x116;
    if(strcmp(in_string, "BTN_TASK") == 0)return 0x117;

    if(strcmp(in_string, "BTN_JOYSTICK") == 0)return 0x120;
    if(strcmp(in_string, "BTN_TRIGGER") == 0)return 0x120;
    if(strcmp(in_string, "BTN_THUMB") == 0)return 0x121;
    if(strcmp(in_string, "BTN_THUMB2") == 0)return 0x122;
    if(strcmp(in_string, "BTN_TOP") == 0)return 0x123;
    if(strcmp(in_string, "BTN_TOP2") == 0)return 0x124;
    if(strcmp(in_string, "BTN_PINKIE") == 0)return 0x125;
    if(strcmp(in_string, "BTN_BASE") == 0)return 0x126;
    if(strcmp(in_string, "BTN_BASE2") == 0)return 0x127;
    if(strcmp(in_string, "BTN_BASE3") == 0)return 0x128;
    if(strcmp(in_string, "BTN_BASE4") == 0)return 0x129;
    if(strcmp(in_string, "BTN_BASE5") == 0)return 0x12a;
    if(strcmp(in_string, "BTN_BASE6") == 0)return 0x12b;
    if(strcmp(in_string, "BTN_DEAD") == 0)return 0x12f;

    if(strcmp(in_string, "BTN_GAMEPAD") == 0)return 0x130;
    if(strcmp(in_string, "BTN_SOUTH") == 0)return 0x130;
    if(strcmp(in_string, "BTN_A") == 0)return 0x130;
    if(strcmp(in_string, "BTN_EAST") == 0)return 0x131;
    if(strcmp(in_string, "BTN_B") == 0)return 0x131;
    if(strcmp(in_string, "BTN_C") == 0)return 0x132;
    if(strcmp(in_string, "BTN_NORTH") == 0)return 0x133;
    if(strcmp(in_string, "BTN_X") == 0)return 0x133;
    if(strcmp(in_string, "BTN_WEST") == 0)return 0x134;
    if(strcmp(in_string, "BTN_Y") == 0)return 0x134;
    if(strcmp(in_string, "BTN_Z") == 0)return 0x135;
    if(strcmp(in_string, "BTN_TL") == 0)return 0x136;
    if(strcmp(in_string, "BTN_TR") == 0)return 0x137;
    if(strcmp(in_string, "BTN_TL2") == 0)return 0x138;
    if(strcmp(in_string, "BTN_TR2") == 0)return 0x139;
    if(strcmp(in_string, "BTN_SELECT") == 0)return 0x13a;
    if(strcmp(in_string, "BTN_START") == 0)return 0x13b;
    if(strcmp(in_string, "BTN_MODE") == 0)return 0x13c;
    if(strcmp(in_string, "BTN_THUMBL") == 0)return 0x13d;
    if(strcmp(in_string, "BTN_THUMBR") == 0)return 0x13e;

    if(strcmp(in_string, "BTN_DIGI") == 0)return 0x140;
    if(strcmp(in_string, "BTN_TOOL_PEN") == 0)return 0x140;
    if(strcmp(in_string, "BTN_TOOL_RUBBER") == 0)return 0x141;
    if(strcmp(in_string, "BTN_TOOL_BRUSH") == 0)return 0x142;
    if(strcmp(in_string, "BTN_TOOL_PENCIL") == 0)return 0x143;
    if(strcmp(in_string, "BTN_TOOL_AIRBRUSH") == 0)return 0x144;
    if(strcmp(in_string, "BTN_TOOL_FINGER") == 0)return 0x145;
    if(strcmp(in_string, "BTN_TOOL_MOUSE") == 0)return 0x146;
    if(strcmp(in_string, "BTN_TOOL_LENS") == 0)return 0x147;
    if(strcmp(in_string, "BTN_TOOL_QUINTTAP") == 0)return 0x148;
    if(strcmp(in_string, "BTN_STYLUS3") == 0)return 0x149;
    if(strcmp(in_string, "BTN_TOUCH") == 0)return 0x14a;
    if(strcmp(in_string, "BTN_STYLUS") == 0)return 0x14b;
    if(strcmp(in_string, "BTN_STYLUS2") == 0)return 0x14c;
    if(strcmp(in_string, "BTN_TOOL_DOUBLETAP") == 0)return 0x14d;
    if(strcmp(in_string, "BTN_TOOL_TRIPLETAP") == 0)return 0x14e;
    if(strcmp(in_string, "BTN_TOOL_QUADTAP") == 0)return 0x14f;

    if(strcmp(in_string, "BTN_WHEEL") == 0)return 0x150;
    if(strcmp(in_string, "BTN_GEAR_DOWN") == 0)return 0x150;
    if(strcmp(in_string, "BTN_GEAR_UP") == 0)return 0x151;

    if(strcmp(in_string, "KEY_OK") == 0)return 0x160;
    if(strcmp(in_string, "KEY_SELECT") == 0)return 0x161;
    if(strcmp(in_string, "KEY_GOTO") == 0)return 0x162;
    if(strcmp(in_string, "KEY_CLEAR") == 0)return 0x163;
    if(strcmp(in_string, "KEY_POWER2") == 0)return 0x164;
    if(strcmp(in_string, "KEY_OPTION") == 0)return 0x165;
    if(strcmp(in_string, "KEY_INFO") == 0)return 0x166;
    if(strcmp(in_string, "KEY_TIME") == 0)return 0x167;
    if(strcmp(in_string, "KEY_VENDOR") == 0)return 0x168;
    if(strcmp(in_string, "KEY_ARCHIVE") == 0)return 0x169;
    if(strcmp(in_string, "KEY_PROGRAM") == 0)return 0x16a;
    if(strcmp(in_string, "KEY_CHANNEL") == 0)return 0x16b;
    if(strcmp(in_string, "KEY_FAVORITES") == 0)return 0x16c;
    if(strcmp(in_string, "KEY_EPG") == 0)return 0x16d;
    if(strcmp(in_string, "KEY_PVR") == 0)return 0x16e;
    if(strcmp(in_string, "KEY_MHP") == 0)return 0x16f;
    if(strcmp(in_string, "KEY_LANGUAGE") == 0)return 0x170;
    if(strcmp(in_string, "KEY_TITLE") == 0)return 0x171;
    if(strcmp(in_string, "KEY_SUBTITLE") == 0)return 0x172;
    if(strcmp(in_string, "KEY_ANGLE") == 0)return 0x173;
    if(strcmp(in_string, "KEY_FULL_SCREEN") == 0)return 0x174;
    if(strcmp(in_string, "KEY_ZOOM") == 0)return 0x174;
    if(strcmp(in_string, "KEY_MODE") == 0)return 0x175;
    if(strcmp(in_string, "KEY_KEYBOARD") == 0)return 0x176;
    if(strcmp(in_string, "KEY_ASPECT_RATIO") == 0)return 0x177;
    if(strcmp(in_string, "KEY_SCREEN") == 0)return 0x177;
    if(strcmp(in_string, "KEY_PC") == 0)return 0x178;
    if(strcmp(in_string, "KEY_TV") == 0)return 0x179;
    if(strcmp(in_string, "KEY_TV2") == 0)return 0x17a;
    if(strcmp(in_string, "KEY_VCR") == 0)return 0x17b;
    if(strcmp(in_string, "KEY_VCR2") == 0)return 0x17c;
    if(strcmp(in_string, "KEY_SAT") == 0)return 0x17d;
    if(strcmp(in_string, "KEY_SAT2") == 0)return 0x17e;
    if(strcmp(in_string, "KEY_CD") == 0)return 0x17f;
    if(strcmp(in_string, "KEY_TAPE") == 0)return 0x180;
    if(strcmp(in_string, "KEY_RADIO") == 0)return 0x181;
    if(strcmp(in_string, "KEY_TUNER") == 0)return 0x182;
    if(strcmp(in_string, "KEY_PLAYER") == 0)return 0x183;
    if(strcmp(in_string, "KEY_TEXT") == 0)return 0x184;
    if(strcmp(in_string, "KEY_DVD") == 0)return 0x185;
    if(strcmp(in_string, "KEY_AUX") == 0)return 0x186;
    if(strcmp(in_string, "KEY_MP3") == 0)return 0x187;
    if(strcmp(in_string, "KEY_AUDIO") == 0)return 0x188;
    if(strcmp(in_string, "KEY_VIDEO") == 0)return 0x189;
    if(strcmp(in_string, "KEY_DIRECTORY") == 0)return 0x18a;
    if(strcmp(in_string, "KEY_LIST") == 0)return 0x18b;
    if(strcmp(in_string, "KEY_MEMO") == 0)return 0x18c;
    if(strcmp(in_string, "KEY_CALENDAR") == 0)return 0x18d;
    if(strcmp(in_string, "KEY_RED") == 0)return 0x18e;
    if(strcmp(in_string, "KEY_GREEN") == 0)return 0x18f;
    if(strcmp(in_string, "KEY_YELLOW") == 0)return 0x190;
    if(strcmp(in_string, "KEY_BLUE") == 0)return 0x191;
    if(strcmp(in_string, "KEY_CHANNELUP") == 0)return 0x192;
    if(strcmp(in_string, "KEY_CHANNELDOWN") == 0)return 0x193;
    if(strcmp(in_string, "KEY_FIRST") == 0)return 0x194;
    if(strcmp(in_string, "KEY_LAST") == 0)return 0x195;
    if(strcmp(in_string, "KEY_AB") == 0)return 0x196;
    if(strcmp(in_string, "KEY_NEXT") == 0)return 0x197;
    if(strcmp(in_string, "KEY_RESTART") == 0)return 0x198;
    if(strcmp(in_string, "KEY_SLOW") == 0)return 0x199;
    if(strcmp(in_string, "KEY_SHUFFLE") == 0)return 0x19a;
    if(strcmp(in_string, "KEY_BREAK") == 0)return 0x19b;
    if(strcmp(in_string, "KEY_PREVIOUS") == 0)return 0x19c;
    if(strcmp(in_string, "KEY_DIGITS") == 0)return 0x19d;
    if(strcmp(in_string, "KEY_TEEN") == 0)return 0x19e;
    if(strcmp(in_string, "KEY_TWEN") == 0)return 0x19f;
    if(strcmp(in_string, "KEY_VIDEOPHONE") == 0)return 0x1a0;
    if(strcmp(in_string, "KEY_GAMES") == 0)return 0x1a1;
    if(strcmp(in_string, "KEY_ZOOMIN") == 0)return 0x1a2;
    if(strcmp(in_string, "KEY_ZOOMOUT") == 0)return 0x1a3;
    if(strcmp(in_string, "KEY_ZOOMRESET") == 0)return 0x1a4;
    if(strcmp(in_string, "KEY_WORDPROCESSOR") == 0)return 0x1a5;
    if(strcmp(in_string, "KEY_EDITOR") == 0)return 0x1a6;
    if(strcmp(in_string, "KEY_SPREADSHEET") == 0)return 0x1a7;
    if(strcmp(in_string, "KEY_GRAPHICSEDITOR") == 0)return 0x1a8;
    if(strcmp(in_string, "KEY_PRESENTATION") == 0)return 0x1a9;
    if(strcmp(in_string, "KEY_DATABASE") == 0)return 0x1aa;
    if(strcmp(in_string, "KEY_NEWS") == 0)return 0x1ab;
    if(strcmp(in_string, "KEY_VOICEMAIL") == 0)return 0x1ac;
    if(strcmp(in_string, "KEY_ADDRESSBOOK") == 0)return 0x1ad;
    if(strcmp(in_string, "KEY_MESSENGER") == 0)return 0x1ae;
    if(strcmp(in_string, "KEY_DISPLAYTOGGLE") == 0)return 0x1af;
    if(strcmp(in_string, "KEY_BRIGHTNESS_TOGGLE") == 0)return 0x1af;
    if(strcmp(in_string, "KEY_SPELLCHECK") == 0)return 0x1b0;
    if(strcmp(in_string, "KEY_LOGOFF") == 0)return 0x1b1;

    if(strcmp(in_string, "KEY_DOLLAR") == 0)return 0x1b2;
    if(strcmp(in_string, "KEY_EURO") == 0)return 0x1b3;

    if(strcmp(in_string, "KEY_FRAMEBACK") == 0)return 0x1b4;
    if(strcmp(in_string, "KEY_FRAMEFORWARD") == 0)return 0x1b5;
    if(strcmp(in_string, "KEY_CONTEXT_MENU") == 0)return 0x1b6;
    if(strcmp(in_string, "KEY_MEDIA_REPEAT") == 0)return 0x1b7;
    if(strcmp(in_string, "KEY_10CHANNELSUP") == 0)return 0x1b8;
    if(strcmp(in_string, "KEY_10CHANNELSDOWN") == 0)return 0x1b9;
    if(strcmp(in_string, "KEY_IMAGES") == 0)return 0x1ba;
    if(strcmp(in_string, "KEY_NOTIFICATION_CENTER") == 0)return 0x1bc;
    if(strcmp(in_string, "KEY_PICKUP_PHONE") == 0)return 0x1bd;
    if(strcmp(in_string, "KEY_HANGUP_PHONE") == 0)return 0x1be;

    if(strcmp(in_string, "KEY_DEL_EOL") == 0)return 0x1c0;
    if(strcmp(in_string, "KEY_DEL_EOS") == 0)return 0x1c1;
    if(strcmp(in_string, "KEY_INS_LINE") == 0)return 0x1c2;
    if(strcmp(in_string, "KEY_DEL_LINE") == 0)return 0x1c3;

    if(strcmp(in_string, "KEY_FN") == 0)return 0x1d0;
    if(strcmp(in_string, "KEY_FN_ESC") == 0)return 0x1d1;
    if(strcmp(in_string, "KEY_FN_F1") == 0)return 0x1d2;
    if(strcmp(in_string, "KEY_FN_F2") == 0)return 0x1d3;
    if(strcmp(in_string, "KEY_FN_F3") == 0)return 0x1d4;
    if(strcmp(in_string, "KEY_FN_F4") == 0)return 0x1d5;
    if(strcmp(in_string, "KEY_FN_F5") == 0)return 0x1d6;
    if(strcmp(in_string, "KEY_FN_F6") == 0)return 0x1d7;
    if(strcmp(in_string, "KEY_FN_F7") == 0)return 0x1d8;
    if(strcmp(in_string, "KEY_FN_F8") == 0)return 0x1d9;
    if(strcmp(in_string, "KEY_FN_F9") == 0)return 0x1da;
    if(strcmp(in_string, "KEY_FN_F10") == 0)return 0x1db;
    if(strcmp(in_string, "KEY_FN_F11") == 0)return 0x1dc;
    if(strcmp(in_string, "KEY_FN_F12") == 0)return 0x1dd;
    if(strcmp(in_string, "KEY_FN_1") == 0)return 0x1de;
    if(strcmp(in_string, "KEY_FN_2") == 0)return 0x1df;
    if(strcmp(in_string, "KEY_FN_D") == 0)return 0x1e0;
    if(strcmp(in_string, "KEY_FN_E") == 0)return 0x1e1;
    if(strcmp(in_string, "KEY_FN_F") == 0)return 0x1e2;
    if(strcmp(in_string, "KEY_FN_S") == 0)return 0x1e3;
    if(strcmp(in_string, "KEY_FN_B") == 0)return 0x1e4;
    if(strcmp(in_string, "KEY_FN_RIGHT_SHIFT") == 0)return 0x1e5;

    if(strcmp(in_string, "KEY_BRL_DOT1") == 0)return 0x1f1;
    if(strcmp(in_string, "KEY_BRL_DOT2") == 0)return 0x1f2;
    if(strcmp(in_string, "KEY_BRL_DOT3") == 0)return 0x1f3;
    if(strcmp(in_string, "KEY_BRL_DOT4") == 0)return 0x1f4;
    if(strcmp(in_string, "KEY_BRL_DOT5") == 0)return 0x1f5;
    if(strcmp(in_string, "KEY_BRL_DOT6") == 0)return 0x1f6;
    if(strcmp(in_string, "KEY_BRL_DOT7") == 0)return 0x1f7;
    if(strcmp(in_string, "KEY_BRL_DOT8") == 0)return 0x1f8;
    if(strcmp(in_string, "KEY_BRL_DOT9") == 0)return 0x1f9;
    if(strcmp(in_string, "KEY_BRL_DOT10") == 0)return 0x1fa;
    
    if(strcmp(in_string, "KEY_NUMERIC_0") == 0)return 0x200;
    if(strcmp(in_string, "KEY_NUMERIC_1") == 0)return 0x201;
    if(strcmp(in_string, "KEY_NUMERIC_2") == 0)return 0x202;
    if(strcmp(in_string, "KEY_NUMERIC_3") == 0)return 0x203;
    if(strcmp(in_string, "KEY_NUMERIC_4") == 0)return 0x204;
    if(strcmp(in_string, "KEY_NUMERIC_5") == 0)return 0x205;
    if(strcmp(in_string, "KEY_NUMERIC_6") == 0)return 0x206;
    if(strcmp(in_string, "KEY_NUMERIC_7") == 0)return 0x207;
    if(strcmp(in_string, "KEY_NUMERIC_8") == 0)return 0x208;
    if(strcmp(in_string, "KEY_NUMERIC_9") == 0)return 0x209;
    if(strcmp(in_string, "KEY_NUMERIC_STAR") == 0)return 0x20a;
    if(strcmp(in_string, "KEY_NUMERIC_POUND") == 0)return 0x20b;
    if(strcmp(in_string, "KEY_NUMERIC_A") == 0)return 0x20c;
    if(strcmp(in_string, "KEY_NUMERIC_B") == 0)return 0x20d;
    if(strcmp(in_string, "KEY_NUMERIC_C") == 0)return 0x20e;
    if(strcmp(in_string, "KEY_NUMERIC_D") == 0)return 0x20f;

    if(strcmp(in_string, "KEY_CAMERA_FOCUS") == 0)return 0x210;
    if(strcmp(in_string, "KEY_WPS_BUTTON") == 0)return 0x211;

    if(strcmp(in_string, "KEY_TOUCHPAD_TOGGLE") == 0)return 0x212;
    if(strcmp(in_string, "KEY_TOUCHPAD_ON") == 0)return 0x213;
    if(strcmp(in_string, "KEY_TOUCHPAD_OFF") == 0)return 0x214;

    if(strcmp(in_string, "KEY_CAMERA_ZOOMIN") == 0)return 0x215;
    if(strcmp(in_string, "KEY_CAMERA_ZOOMOUT") == 0)return 0x216;
    if(strcmp(in_string, "KEY_CAMERA_UP") == 0)return 0x217;
    if(strcmp(in_string, "KEY_CAMERA_DOWN") == 0)return 0x218;
    if(strcmp(in_string, "KEY_CAMERA_LEFT") == 0)return 0x219;
    if(strcmp(in_string, "KEY_CAMERA_RIGHT") == 0)return 0x21a;

    if(strcmp(in_string, "KEY_ATTENDANT_ON") == 0)return 0x21b;
    if(strcmp(in_string, "KEY_ATTENDANT_OFF") == 0)return 0x21c;
    if(strcmp(in_string, "KEY_ATTENDANT_TOGGLE") == 0)return 0x21d;
    if(strcmp(in_string, "KEY_LIGHTS_TOGGLE") == 0)return 0x21e;

    if(strcmp(in_string, "BTN_DPAD_UP") == 0)return 0x220;
    if(strcmp(in_string, "BTN_DPAD_DOWN") == 0)return 0x221;
    if(strcmp(in_string, "BTN_DPAD_LEFT") == 0)return 0x222;
    if(strcmp(in_string, "BTN_DPAD_RIGHT") == 0)return 0x223;

    if(strcmp(in_string, "KEY_ALS_TOGGLE") == 0)return 0x230;
    if(strcmp(in_string, "KEY_ROTATE_LOCK_TOGGLE") == 0)return 0x231;
    if(strcmp(in_string, "KEY_REFRESH_RATE_TOGGLE") == 0)return 0x232;

    if(strcmp(in_string, "KEY_BUTTONCONFIG") == 0)return 0x240;
    if(strcmp(in_string, "KEY_TASKMANAGER") == 0)return 0x241;
    if(strcmp(in_string, "KEY_JOURNAL") == 0)return 0x242;
    if(strcmp(in_string, "KEY_CONTROLPANEL") == 0)return 0x243;
    if(strcmp(in_string, "KEY_APPSELECT") == 0)return 0x244;
    if(strcmp(in_string, "KEY_SCREENSAVER") == 0)return 0x245;
    if(strcmp(in_string, "KEY_VOICECOMMAND") == 0)return 0x246;
    if(strcmp(in_string, "KEY_ASSISTANT") == 0)return 0x247;
    if(strcmp(in_string, "KEY_KBD_LAYOUT_NEXT") == 0)return 0x248;
    if(strcmp(in_string, "KEY_EMOJI_PICKER") == 0)return 0x249;
    if(strcmp(in_string, "KEY_DICTATE") == 0)return 0x24a;
    if(strcmp(in_string, "KEY_CAMERA_ACCESS_ENABLE") == 0)return 0x24b;
    if(strcmp(in_string, "KEY_CAMERA_ACCESS_DISABLE") == 0)return 0x24c;
    if(strcmp(in_string, "KEY_CAMERA_ACCESS_TOGGLE") == 0)return 0x24d;
    if(strcmp(in_string, "KEY_ACCESSIBILITY") == 0)return 0x24e;
    if(strcmp(in_string, "KEY_DO_NOT_DISTURB") == 0)return 0x24f;

    if(strcmp(in_string, "KEY_BRIGHTNESS_MIN") == 0)return 0x250;
    if(strcmp(in_string, "KEY_BRIGHTNESS_MAX") == 0)return 0x251;

    if(strcmp(in_string, "KEY_KBDINPUTASSIST_PREV") == 0)return 0x260;
    if(strcmp(in_string, "KEY_KBDINPUTASSIST_NEXT") == 0)return 0x261;
    if(strcmp(in_string, "KEY_KBDINPUTASSIST_PREVGROUP") == 0)return 0x262;
    if(strcmp(in_string, "KEY_KBDINPUTASSIST_NEXTGROUP") == 0)return 0x263;
    if(strcmp(in_string, "KEY_KBDINPUTASSIST_ACCEPT") == 0)return 0x264;
    if(strcmp(in_string, "KEY_KBDINPUTASSIST_CANCEL") == 0)return 0x265;

    if(strcmp(in_string, "KEY_RIGHT_UP") == 0)return 0x266;
    if(strcmp(in_string, "KEY_RIGHT_DOWN") == 0)return 0x267;
    if(strcmp(in_string, "KEY_LEFT_UP") == 0)return 0x268;
    if(strcmp(in_string, "KEY_LEFT_DOWN") == 0)return 0x269;

    if(strcmp(in_string, "KEY_ROOT_MENU") == 0)return 0x26a;
    if(strcmp(in_string, "KEY_MEDIA_TOP_MENU") == 0)return 0x26b;
    if(strcmp(in_string, "KEY_NUMERIC_11") == 0)return 0x26c;
    if(strcmp(in_string, "KEY_NUMERIC_12") == 0)return 0x26d;

    if(strcmp(in_string, "KEY_AUDIO_DESC") == 0)return 0x26e;
    if(strcmp(in_string, "KEY_3D_MODE") == 0)return 0x26f;
    if(strcmp(in_string, "KEY_NEXT_FAVORITE") == 0)return 0x270;
    if(strcmp(in_string, "KEY_STOP_RECORD") == 0)return 0x271;
    if(strcmp(in_string, "KEY_PAUSE_RECORD") == 0)return 0x272;
    if(strcmp(in_string, "KEY_VOD") == 0)return 0x273;
    if(strcmp(in_string, "KEY_UNMUTE") == 0)return 0x274;
    if(strcmp(in_string, "KEY_FASTREVERSE") == 0)return 0x275;
    if(strcmp(in_string, "KEY_SLOWREVERSE") == 0)return 0x276;

    if(strcmp(in_string, "KEY_DATA") == 0)return 0x277;
    if(strcmp(in_string, "KEY_ONSCREEN_KEYBOARD") == 0)return 0x278;
    if(strcmp(in_string, "KEY_PRIVACY_SCREEN_TOGGLE") == 0)return 0x279;

    if(strcmp(in_string, "KEY_SELECTIVE_SCREENSHOT") == 0)return 0x27a;

    if(strcmp(in_string, "KEY_NEXT_ELEMENT") == 0)return 0x27b;
    if(strcmp(in_string, "KEY_PREVIOUS_ELEMENT") == 0)return 0x27c;
    if(strcmp(in_string, "KEY_AUTOPILOT_ENGANGE_TOGGLE") == 0)return 0x27d;
    
    if(strcmp(in_string, "KEY_MARK_WAYPOINT") == 0)return 0x27e;
    if(strcmp(in_string, "KEY_SOS") == 0)return 0x27f;
    if(strcmp(in_string, "KEY_NAV_CHART") == 0)return 0x280;
    if(strcmp(in_string, "KEY_FISHING_CHART") == 0)return 0x281;
    if(strcmp(in_string, "KEY_SINGLE_RANGE_RADAR") == 0)return 0x282;
    if(strcmp(in_string, "KEY_DUAL_RANGE_RADAR") == 0)return 0x283;
    if(strcmp(in_string, "KEY_RADAR_OVERLAY") == 0)return 0x284;
    if(strcmp(in_string, "KEY_TRADITIONAL_SONAR") == 0)return 0x285;
    if(strcmp(in_string, "KEY_CLEARVU_SONAR") == 0)return 0x286;
    if(strcmp(in_string, "KEY_SIDEVU_SONAR") == 0)return 0x287;
    if(strcmp(in_string, "KEY_NAV_INFO") == 0)return 0x288;
    if(strcmp(in_string, "KEY_BRIGHTNESS_MENU") == 0)return 0x289;

    if(strcmp(in_string, "KEY_MACRO1") == 0)return 0x290;
    if(strcmp(in_string, "KEY_MACRO2") == 0)return 0x291;
    if(strcmp(in_string, "KEY_MACRO3") == 0)return 0x292;
    if(strcmp(in_string, "KEY_MACRO4") == 0)return 0x293;
    if(strcmp(in_string, "KEY_MACRO5") == 0)return 0x294;
    if(strcmp(in_string, "KEY_MACRO6") == 0)return 0x295;
    if(strcmp(in_string, "KEY_MACRO7") == 0)return 0x296;
    if(strcmp(in_string, "KEY_MACRO8") == 0)return 0x297;
    if(strcmp(in_string, "KEY_MACRO9") == 0)return 0x298;
    if(strcmp(in_string, "KEY_MACRO10") == 0)return 0x299;
    if(strcmp(in_string, "KEY_MACRO11") == 0)return 0x29a;
    if(strcmp(in_string, "KEY_MACRO12") == 0)return 0x29b;
    if(strcmp(in_string, "KEY_MACRO13") == 0)return 0x29c;
    if(strcmp(in_string, "KEY_MACRO14") == 0)return 0x29d;
    if(strcmp(in_string, "KEY_MACRO15") == 0)return 0x29e;
    if(strcmp(in_string, "KEY_MACRO16") == 0)return 0x29f;
    if(strcmp(in_string, "KEY_MACRO17") == 0)return 0x2a0;
    if(strcmp(in_string, "KEY_MACRO18") == 0)return 0x2a1;
    if(strcmp(in_string, "KEY_MACRO19") == 0)return 0x2a2;
    if(strcmp(in_string, "KEY_MACRO20") == 0)return 0x2a3;
    if(strcmp(in_string, "KEY_MACRO21") == 0)return 0x2a4;
    if(strcmp(in_string, "KEY_MACRO22") == 0)return 0x2a5;
    if(strcmp(in_string, "KEY_MACRO23") == 0)return 0x2a6;
    if(strcmp(in_string, "KEY_MACRO24") == 0)return 0x2a7;
    if(strcmp(in_string, "KEY_MACRO25") == 0)return 0x2a8;
    if(strcmp(in_string, "KEY_MACRO26") == 0)return 0x2a9;
    if(strcmp(in_string, "KEY_MACRO27") == 0)return 0x2aa;
    if(strcmp(in_string, "KEY_MACRO28") == 0)return 0x2ab;
    if(strcmp(in_string, "KEY_MACRO29") == 0)return 0x2ac;
    if(strcmp(in_string, "KEY_MACRO30") == 0)return 0x2ad;

    if(strcmp(in_string, "KEY_MACRO_RECORD_START") == 0)return 0x2b0;
    if(strcmp(in_string, "KEY_MACRO_RECORD_STOP") == 0)return 0x2b1;
    if(strcmp(in_string, "KEY_MACRO_PRESET_CYCLE") == 0)return 0x2b2;
    if(strcmp(in_string, "KEY_MACRO_PRESET1") == 0)return 0x2b3;
    if(strcmp(in_string, "KEY_MACRO_PRESET2") == 0)return 0x2b4;
    if(strcmp(in_string, "KEY_MACRO_PRESET2") == 0)return 0x2b5;

    if(strcmp(in_string, "KEY_KBD_LCD_MENU1") == 0)return 0x2b8;
    if(strcmp(in_string, "KEY_KBD_LCD_MENU2") == 0)return 0x2b9;
    if(strcmp(in_string, "KEY_KBD_LCD_MENU3") == 0)return 0x2ba;
    if(strcmp(in_string, "KEY_KBD_LCD_MENU4") == 0)return 0x2bb;
    if(strcmp(in_string, "KEY_KBD_LCD_MENU5") == 0)return 0x2bc;

    if(strcmp(in_string, "BTN_TRIGGER_HAPPY") == 0)return 0x2c0;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY1") == 0)return 0x2c0;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY2") == 0)return 0x2c1;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY3") == 0)return 0x2c2;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY4") == 0)return 0x2c3;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY5") == 0)return 0x2c4;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY6") == 0)return 0x2c5;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY7") == 0)return 0x2c6;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY8") == 0)return 0x2c7;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY9") == 0)return 0x2c8;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY10") == 0)return 0x2c9;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY11") == 0)return 0x2ca;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY12") == 0)return 0x2cb;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY13") == 0)return 0x2cc;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY14") == 0)return 0x2cd;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY15") == 0)return 0x2ce;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY16") == 0)return 0x2cf;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY17") == 0)return 0x2d0;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY18") == 0)return 0x2d1;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY19") == 0)return 0x2d2;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY20") == 0)return 0x2d3;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY21") == 0)return 0x2d4;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY22") == 0)return 0x2d5;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY23") == 0)return 0x2d6;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY24") == 0)return 0x2d7;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY25") == 0)return 0x2d8;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY26") == 0)return 0x2d9;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY27") == 0)return 0x2da;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY28") == 0)return 0x2db;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY29") == 0)return 0x2dc;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY30") == 0)return 0x2dd;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY31") == 0)return 0x2de;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY32") == 0)return 0x2df;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY33") == 0)return 0x2e0;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY34") == 0)return 0x2e1;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY35") == 0)return 0x2e2;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY36") == 0)return 0x2e3;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY37") == 0)return 0x2e4;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY38") == 0)return 0x2e5;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY39") == 0)return 0x2e6;
    if(strcmp(in_string, "BTN_TRIGGER_HAPPY40") == 0)return 0x2e7;

    if(strcmp(in_string, "KEY_MIN_INTERESTING") == 0)return 113;
    if(strcmp(in_string, "KEY_MAX") == 0)return 0x2ff;
    if(strcmp(in_string, "KEY_CNT") == 0)return (0x2ff + 1);    
       
    return -1;

}

int app_emmit_emmit_keypress(int uinput_fd, APP_EMMIT_KEYPRESS* keypress){
    if(!keypress)return -1;
    EMMIT_KEYPRESS* emmit_press = (EMMIT_KEYPRESS*)keypress;
    if(!emmit_press)return -1;
    
    if(emmit_press->key_enums_size <= 0)return -1;
    if(!(emmit_press->key_enums))return -1;

    for(int i = 0; i < emmit_press->key_enums_size; i++){
	int cur_keybit = emmit_press->key_enums[i];
	emit(uinput_fd, EV_KEY, cur_keybit, 1);
    }
    emit(uinput_fd, EV_SYN, SYN_REPORT, 0);

    if(emmit_press->key_inv == 0)return 0;
    for(int i = 0; i < emmit_press->key_enums_size; i++){
	int cur_keybit = emmit_press->key_enums[i];
	emit(uinput_fd, EV_KEY, cur_keybit, 0);
    }
    emit(uinput_fd, EV_SYN, SYN_REPORT, 0);

    return 0;
}

APP_EMMIT_KEYPRESS* app_emmit_init_keypress(int* key_nums, int key_size, int invert){
    if(!key_nums)return NULL;
    if(key_size <= 0)return NULL;
    EMMIT_KEYPRESS* return_keypress = malloc(sizeof(EMMIT_KEYPRESS));
    if(!return_keypress)return NULL;
    return_keypress->key_enums = malloc(sizeof(int) * key_size);
    if(!(return_keypress->key_enums))return NULL;
    for(int i = 0; i < key_size; i ++){
	return_keypress->key_enums[i] = key_nums[i];
    }
    return_keypress->key_enums_size = key_size;
    return_keypress->key_inv = invert;
    return (APP_EMMIT_KEYPRESS*)return_keypress;
}

int app_emmit_init_input(int* uinput_fd, const char* device_name, int* keybits, int keybit_size){
    if(keybit_size <= 0)return -1;
    struct uinput_setup usetup;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(!fd)return -1;
    //initiate the virtual device for emmiting the keypresses
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for(int i = 0; i < keybit_size; i++){
	int cur_keybit = keybits[i];
	ioctl(fd, UI_SET_KEYBIT, cur_keybit);
    }
    
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; // sample vendor
    usetup.id.product = 0x5678; //sample product
    strcpy(usetup.name, device_name);

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    *uinput_fd = fd;

    return 0;
}

void app_emmit_clean(int uinput_fd){
    if(uinput_fd == 0)return;
    //clean the uinput emmiter
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
}

void app_emmit_clean_keypress(APP_EMMIT_KEYPRESS* keypress){
    if(!keypress)return;
    EMMIT_KEYPRESS* emmit_press = (EMMIT_KEYPRESS*)keypress;
    if(!emmit_press)return;
    if(emmit_press->key_enums)free(emmit_press->key_enums);
    free(emmit_press);
}
