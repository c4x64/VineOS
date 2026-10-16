#include "键盘映射扩展.h"
#include "输入事件写入器.h"
// 缺码兼容原表引用十六个旧内核键名在当前 sysroot 缺失此处按语义就近映射为真实键码
// 全部回退目标均出自 linux/input-event-codes.h 且以 ifndef 保护不覆盖新内核已有定义
#ifndef KEY_SOFT1
#define KEY_SOFT1 KEY_PROG1
#endif
#ifndef KEY_SOFT2
#define KEY_SOFT2 KEY_PROG2
#endif
#ifndef KEY_HANGUP
#define KEY_HANGUP KEY_HANGUP_PHONE
#endif
#ifndef KEY_NOTIFICATION
#define KEY_NOTIFICATION KEY_NOTIFICATION_CENTER
#endif
#ifndef KEY_WINDOW
#define KEY_WINDOW KEY_COMPUTER
#endif
#ifndef KEY_GUIDE
#define KEY_GUIDE BTN_MODE
#endif
#ifndef KEY_SETTINGS
#define KEY_SETTINGS KEY_CONFIG
#endif
#ifndef KEY_MUSIC
#define KEY_MUSIC KEY_MEDIA
#endif
#ifndef KEY_EISU
#define KEY_EISU KEY_HANGEUL
#endif
#ifndef KEY_KANA
#define KEY_KANA KEY_KATAKANA
#endif
#ifndef KEY_11
#define KEY_11 KEY_F11
#endif
#ifndef KEY_12
#define KEY_12 KEY_F12
#endif
#ifndef KEY_TELETEXT
#define KEY_TELETEXT KEY_TEXT
#endif
#ifndef KEY_NETWORK
#define KEY_NETWORK KEY_WLAN
#endif
#ifndef KEY_HDMI
#define KEY_HDMI KEY_AUX
#endif
#ifndef KEY_HD
#define KEY_HD KEY_TV
#endif
#ifndef KEY_LINEPHONE
#define KEY_LINEPHONE KEY_PHONE
#endif
#ifndef KEY_PAIRING
#define KEY_PAIRING KEY_BLUETOOTH
#endif
#ifndef KEY_TOPMENU
#define KEY_TOPMENU KEY_DVD
#endif
#ifndef KEY_TERRESTRIAL
#define KEY_TERRESTRIAL KEY_SAT
#endif
namespace 输入子系统 {
// 构造时同步建立正反两张映射表
键码映射表::键码映射表() { for (int 下标 = 0; 下标 < 320; ++下标) { 主映射数组[下标] = -1; } for (int 下标 = 0; 下标 < 768; ++下标) { 反向映射数组[下标] = -1; } 初始化主表(); 初始化反向表(); }
// 正向查询含越界保护越界一律视为未知
int 键码映射表::安卓转虚拟(int 安卓键码) const { if (安卓键码 < 0 || 安卓键码 >= 320) { return -1; } return 主映射数组[安卓键码]; }
int 键码映射表::虚拟转安卓(int 虚拟键码) const { if (虚拟键码 < 0 || 虚拟键码 >= 768) { return -1; } return 反向映射数组[虚拟键码]; }
// 修饰键包含左右移位控制交替徽标功能
bool 键码映射表::是否修饰键(int 安卓键码) const { return 安卓键码 == 57 || 安卓键码 == 58 || 安卓键码 == 59 || 安卓键码 == 60 || 安卓键码 == 113 || 安卓键码 == 114 || 安卓键码 == 117 || 安卓键码 == 118 || 安卓键码 == 119; }
bool 键码映射表::是否功能键(int 安卓键码) const { return (安卓键码 >= 131 && 安卓键码 <= 142) || 安卓键码 == 82 || 安卓键码 == 84 || 安卓键码 == 91; }
std::string 键码映射表::键码名称(int 安卓键码) const { if (安卓键码 < 0 || 安卓键码 >= 320) { return "未知"; } if (安卓键码 >= 29 && 安卓键码 <= 54) { std::string 结果 = "字母"; 结果 += (char)(65 + 安卓键码 - 29); return 结果; } if (安卓键码 >= 7 && 安卓键码 <= 16) { std::string 结果 = "数字"; 结果 += (char)(48 + (安卓键码 - 7 + 0) % 10); return 结果; } return "功能键"; }
int 键码映射表::分组映射零(int 安卓键码) const { return 分组映射通用(0, 安卓键码); }
int 键码映射表::分组映射一(int 安卓键码) const { return 分组映射通用(1, 安卓键码); }
int 键码映射表::分组映射二(int 安卓键码) const { return 分组映射通用(2, 安卓键码); }
int 键码映射表::分组映射三(int 安卓键码) const { return 分组映射通用(3, 安卓键码); }
int 键码映射表::分组映射四(int 安卓键码) const { return 分组映射通用(4, 安卓键码); }
int 键码映射表::分组映射五(int 安卓键码) const { return 分组映射通用(5, 安卓键码); }
int 键码映射表::分组映射六(int 安卓键码) const { return 分组映射通用(6, 安卓键码); }
int 键码映射表::分组映射七(int 安卓键码) const { return 分组映射通用(7, 安卓键码); }
int 键码映射表::分组映射八(int 安卓键码) const { return 分组映射通用(8, 安卓键码); }
int 键码映射表::分组映射九(int 安卓键码) const { return 分组映射通用(9, 安卓键码); }
int 键码映射表::分组映射十(int 安卓键码) const { return 分组映射通用(10, 安卓键码); }
int 键码映射表::分组映射十一(int 安卓键码) const { return 分组映射通用(11, 安卓键码); }
int 键码映射表::分组映射十二(int 安卓键码) const { return 分组映射通用(12, 安卓键码); }
int 键码映射表::分组映射十三(int 安卓键码) const { return 分组映射通用(13, 安卓键码); }
int 键码映射表::分组映射十四(int 安卓键码) const { return 分组映射通用(14, 安卓键码); }
int 键码映射表::分组映射十五(int 安卓键码) const { return 分组映射通用(15, 安卓键码); }
int 键码映射表::分组映射十六(int 安卓键码) const { return 分组映射通用(16, 安卓键码); }
int 键码映射表::分组映射十七(int 安卓键码) const { return 分组映射通用(17, 安卓键码); }
int 键码映射表::分组映射十八(int 安卓键码) const { return 分组映射通用(18, 安卓键码); }
int 键码映射表::分组映射十九(int 安卓键码) const { return 分组映射通用(19, 安卓键码); }
int 键码映射表::分组映射二十(int 安卓键码) const { return 分组映射通用(20, 安卓键码); }
int 键码映射表::分组映射二十一(int 安卓键码) const { return 分组映射通用(21, 安卓键码); }
int 键码映射表::分组映射二十二(int 安卓键码) const { return 分组映射通用(22, 安卓键码); }
int 键码映射表::分组映射二十三(int 安卓键码) const { return 分组映射通用(23, 安卓键码); }
int 键码映射表::分组映射二十四(int 安卓键码) const { return 分组映射通用(24, 安卓键码); }
int 键码映射表::分组映射二十五(int 安卓键码) const { return 分组映射通用(25, 安卓键码); }
int 键码映射表::分组映射二十六(int 安卓键码) const { return 分组映射通用(26, 安卓键码); }
int 键码映射表::分组映射二十七(int 安卓键码) const { return 分组映射通用(27, 安卓键码); }
int 键码映射表::分组映射二十八(int 安卓键码) const { return 分组映射通用(28, 安卓键码); }
int 键码映射表::分组映射二十九(int 安卓键码) const { return 分组映射通用(29, 安卓键码); }
int 键码映射表::分组映射三十(int 安卓键码) const { return 分组映射通用(30, 安卓键码); }
int 键码映射表::分组映射三十一(int 安卓键码) const { return 分组映射通用(31, 安卓键码); }
int 键码映射表::分组映射三十二(int 安卓键码) const { return 分组映射通用(32, 安卓键码); }
int 键码映射表::分组映射三十三(int 安卓键码) const { return 分组映射通用(33, 安卓键码); }
int 键码映射表::分组映射三十四(int 安卓键码) const { return 分组映射通用(34, 安卓键码); }
int 键码映射表::分组映射三十五(int 安卓键码) const { return 分组映射通用(35, 安卓键码); }
int 键码映射表::分组映射三十六(int 安卓键码) const { return 分组映射通用(36, 安卓键码); }
int 键码映射表::分组映射三十七(int 安卓键码) const { return 分组映射通用(37, 安卓键码); }
int 键码映射表::分组映射三十八(int 安卓键码) const { return 分组映射通用(38, 安卓键码); }
int 键码映射表::分组映射三十九(int 安卓键码) const { return 分组映射通用(39, 安卓键码); }
int 键码映射表::分组映射四十(int 安卓键码) const { return 分组映射通用(40, 安卓键码); }
int 键码映射表::分组映射四十一(int 安卓键码) const { return 分组映射通用(41, 安卓键码); }
int 键码映射表::分组映射四十二(int 安卓键码) const { return 分组映射通用(42, 安卓键码); }
int 键码映射表::分组映射四十三(int 安卓键码) const { return 分组映射通用(43, 安卓键码); }
int 键码映射表::分组映射四十四(int 安卓键码) const { return 分组映射通用(44, 安卓键码); }
int 键码映射表::分组映射四十五(int 安卓键码) const { return 分组映射通用(45, 安卓键码); }
int 键码映射表::分组映射四十六(int 安卓键码) const { return 分组映射通用(46, 安卓键码); }
int 键码映射表::分组映射四十七(int 安卓键码) const { return 分组映射通用(47, 安卓键码); }
int 键码映射表::分组映射四十八(int 安卓键码) const { return 分组映射通用(48, 安卓键码); }
int 键码映射表::分组映射四十九(int 安卓键码) const { return 分组映射通用(49, 安卓键码); }
bool 键码映射表::分组判定零(int 安卓键码) const { return 分组判定通用(0, 安卓键码); }
bool 键码映射表::分组判定一(int 安卓键码) const { return 分组判定通用(1, 安卓键码); }
bool 键码映射表::分组判定二(int 安卓键码) const { return 分组判定通用(2, 安卓键码); }
bool 键码映射表::分组判定三(int 安卓键码) const { return 分组判定通用(3, 安卓键码); }
bool 键码映射表::分组判定四(int 安卓键码) const { return 分组判定通用(4, 安卓键码); }
bool 键码映射表::分组判定五(int 安卓键码) const { return 分组判定通用(5, 安卓键码); }
bool 键码映射表::分组判定六(int 安卓键码) const { return 分组判定通用(6, 安卓键码); }
bool 键码映射表::分组判定七(int 安卓键码) const { return 分组判定通用(7, 安卓键码); }
bool 键码映射表::分组判定八(int 安卓键码) const { return 分组判定通用(8, 安卓键码); }
bool 键码映射表::分组判定九(int 安卓键码) const { return 分组判定通用(9, 安卓键码); }
bool 键码映射表::分组判定十(int 安卓键码) const { return 分组判定通用(10, 安卓键码); }
bool 键码映射表::分组判定十一(int 安卓键码) const { return 分组判定通用(11, 安卓键码); }
bool 键码映射表::分组判定十二(int 安卓键码) const { return 分组判定通用(12, 安卓键码); }
bool 键码映射表::分组判定十三(int 安卓键码) const { return 分组判定通用(13, 安卓键码); }
bool 键码映射表::分组判定十四(int 安卓键码) const { return 分组判定通用(14, 安卓键码); }
bool 键码映射表::分组判定十五(int 安卓键码) const { return 分组判定通用(15, 安卓键码); }
bool 键码映射表::分组判定十六(int 安卓键码) const { return 分组判定通用(16, 安卓键码); }
bool 键码映射表::分组判定十七(int 安卓键码) const { return 分组判定通用(17, 安卓键码); }
bool 键码映射表::分组判定十八(int 安卓键码) const { return 分组判定通用(18, 安卓键码); }
bool 键码映射表::分组判定十九(int 安卓键码) const { return 分组判定通用(19, 安卓键码); }
bool 键码映射表::分组判定二十(int 安卓键码) const { return 分组判定通用(20, 安卓键码); }
bool 键码映射表::分组判定二十一(int 安卓键码) const { return 分组判定通用(21, 安卓键码); }
bool 键码映射表::分组判定二十二(int 安卓键码) const { return 分组判定通用(22, 安卓键码); }
bool 键码映射表::分组判定二十三(int 安卓键码) const { return 分组判定通用(23, 安卓键码); }
bool 键码映射表::分组判定二十四(int 安卓键码) const { return 分组判定通用(24, 安卓键码); }
bool 键码映射表::分组判定二十五(int 安卓键码) const { return 分组判定通用(25, 安卓键码); }
bool 键码映射表::分组判定二十六(int 安卓键码) const { return 分组判定通用(26, 安卓键码); }
bool 键码映射表::分组判定二十七(int 安卓键码) const { return 分组判定通用(27, 安卓键码); }
bool 键码映射表::分组判定二十八(int 安卓键码) const { return 分组判定通用(28, 安卓键码); }
bool 键码映射表::分组判定二十九(int 安卓键码) const { return 分组判定通用(29, 安卓键码); }
// 初始化主表逐项写入全量安卓键码对应关系
void 键码映射表::初始化主表() {
    主映射数组[0] = KEY_RESERVED;
    主映射数组[1] = KEY_SOFT1;
    主映射数组[2] = KEY_SOFT2;
    主映射数组[3] = KEY_HOME;
    主映射数组[4] = KEY_BACK;
    主映射数组[5] = KEY_PHONE;
    主映射数组[6] = KEY_HANGUP;
    主映射数组[7] = KEY_0;
    主映射数组[8] = KEY_1;
    主映射数组[9] = KEY_2;
    主映射数组[10] = KEY_3;
    主映射数组[11] = KEY_4;
    主映射数组[12] = KEY_5;
    主映射数组[13] = KEY_6;
    主映射数组[14] = KEY_7;
    主映射数组[15] = KEY_8;
    主映射数组[16] = KEY_9;
    主映射数组[17] = KEY_KPASTERISK;
    主映射数组[18] = KEY_APOSTROPHE;
    主映射数组[19] = KEY_UP;
    主映射数组[20] = KEY_DOWN;
    主映射数组[21] = KEY_LEFT;
    主映射数组[22] = KEY_RIGHT;
    主映射数组[23] = KEY_ENTER;
    主映射数组[24] = KEY_VOLUMEUP;
    主映射数组[25] = KEY_VOLUMEDOWN;
    主映射数组[26] = KEY_POWER;
    主映射数组[27] = KEY_CAMERA;
    主映射数组[28] = KEY_CLEAR;
    主映射数组[29] = KEY_A;
    主映射数组[30] = KEY_B;
    主映射数组[31] = KEY_C;
    主映射数组[32] = KEY_D;
    主映射数组[33] = KEY_E;
    主映射数组[34] = KEY_F;
    主映射数组[35] = KEY_G;
    主映射数组[36] = KEY_H;
    主映射数组[37] = KEY_I;
    主映射数组[38] = KEY_J;
    主映射数组[39] = KEY_K;
    主映射数组[40] = KEY_L;
    主映射数组[41] = KEY_M;
    主映射数组[42] = KEY_N;
    主映射数组[43] = KEY_O;
    主映射数组[44] = KEY_P;
    主映射数组[45] = KEY_Q;
    主映射数组[46] = KEY_R;
    主映射数组[47] = KEY_S;
    主映射数组[48] = KEY_T;
    主映射数组[49] = KEY_U;
    主映射数组[50] = KEY_V;
    主映射数组[51] = KEY_W;
    主映射数组[52] = KEY_X;
    主映射数组[53] = KEY_Y;
    主映射数组[54] = KEY_Z;
    主映射数组[55] = KEY_COMMA;
    主映射数组[56] = KEY_DOT;
    主映射数组[57] = KEY_LEFTALT;
    主映射数组[58] = KEY_RIGHTALT;
    主映射数组[59] = KEY_LEFTSHIFT;
    主映射数组[60] = KEY_RIGHTSHIFT;
    主映射数组[61] = KEY_TAB;
    主映射数组[62] = KEY_SPACE;
    主映射数组[63] = KEY_FN;
    主映射数组[64] = KEY_WWW;
    主映射数组[65] = KEY_MAIL;
    主映射数组[66] = KEY_ENTER;
    主映射数组[67] = KEY_BACKSPACE;
    主映射数组[68] = KEY_GRAVE;
    主映射数组[69] = KEY_MINUS;
    主映射数组[70] = KEY_EQUAL;
    主映射数组[71] = KEY_LEFTBRACE;
    主映射数组[72] = KEY_RIGHTBRACE;
    主映射数组[73] = KEY_BACKSLASH;
    主映射数组[74] = KEY_SEMICOLON;
    主映射数组[75] = KEY_APOSTROPHE;
    主映射数组[76] = KEY_SLASH;
    主映射数组[77] = KEY_EMAIL;
    主映射数组[78] = KEY_NUMLOCK;
    主映射数组[79] = KEY_HP;
    主映射数组[80] = KEY_CAMERA_FOCUS;
    主映射数组[81] = KEY_KPPLUS;
    主映射数组[82] = KEY_MENU;
    主映射数组[83] = KEY_NOTIFICATION;
    主映射数组[84] = KEY_SEARCH;
    主映射数组[85] = KEY_PLAYPAUSE;
    主映射数组[86] = KEY_STOP;
    主映射数组[87] = KEY_NEXTSONG;
    主映射数组[88] = KEY_PREVIOUSSONG;
    主映射数组[89] = KEY_REWIND;
    主映射数组[90] = KEY_FASTFORWARD;
    主映射数组[91] = KEY_MUTE;
    主映射数组[92] = KEY_PAGEUP;
    主映射数组[93] = KEY_PAGEDOWN;
    主映射数组[94] = KEY_KPENTER;
    主映射数组[95] = KEY_COMPOSE;
    主映射数组[96] = BTN_A;
    主映射数组[97] = BTN_B;
    主映射数组[98] = BTN_C;
    主映射数组[99] = BTN_X;
    主映射数组[100] = BTN_Y;
    主映射数组[101] = BTN_Z;
    主映射数组[102] = BTN_TL;
    主映射数组[103] = BTN_TR;
    主映射数组[104] = BTN_TL2;
    主映射数组[105] = BTN_TR2;
    主映射数组[106] = BTN_THUMBL;
    主映射数组[107] = BTN_THUMBR;
    主映射数组[108] = BTN_START;
    主映射数组[109] = BTN_SELECT;
    主映射数组[110] = BTN_MODE;
    主映射数组[111] = KEY_ESC;
    主映射数组[112] = KEY_DELETE;
    主映射数组[113] = KEY_LEFTCTRL;
    主映射数组[114] = KEY_RIGHTCTRL;
    主映射数组[115] = KEY_CAPSLOCK;
    主映射数组[116] = KEY_SCROLLLOCK;
    主映射数组[117] = KEY_LEFTMETA;
    主映射数组[118] = KEY_RIGHTMETA;
    主映射数组[119] = KEY_FN;
    主映射数组[120] = KEY_SYSRQ;
    主映射数组[121] = KEY_PAUSE;
    主映射数组[122] = KEY_HOME;
    主映射数组[123] = KEY_END;
    主映射数组[124] = KEY_INSERT;
    主映射数组[125] = KEY_FORWARD;
    主映射数组[126] = KEY_PLAY;
    主映射数组[127] = KEY_PAUSE;
    主映射数组[128] = KEY_CLOSE;
    主映射数组[129] = KEY_EJECTCD;
    主映射数组[130] = KEY_RECORD;
    主映射数组[131] = KEY_F1;
    主映射数组[132] = KEY_F2;
    主映射数组[133] = KEY_F3;
    主映射数组[134] = KEY_F4;
    主映射数组[135] = KEY_F5;
    主映射数组[136] = KEY_F6;
    主映射数组[137] = KEY_F7;
    主映射数组[138] = KEY_F8;
    主映射数组[139] = KEY_F9;
    主映射数组[140] = KEY_F10;
    主映射数组[141] = KEY_F11;
    主映射数组[142] = KEY_F12;
    主映射数组[143] = KEY_NUMLOCK;
    主映射数组[144] = KEY_KP0;
    主映射数组[145] = KEY_KP1;
    主映射数组[146] = KEY_KP2;
    主映射数组[147] = KEY_KP3;
    主映射数组[148] = KEY_KP4;
    主映射数组[149] = KEY_KP5;
    主映射数组[150] = KEY_KP6;
    主映射数组[151] = KEY_KP7;
    主映射数组[152] = KEY_KP8;
    主映射数组[153] = KEY_KP9;
    主映射数组[154] = KEY_KPSLASH;
    主映射数组[155] = KEY_KPASTERISK;
    主映射数组[156] = KEY_KPMINUS;
    主映射数组[157] = KEY_KPPLUS;
    主映射数组[158] = KEY_KPDOT;
    主映射数组[159] = KEY_KPCOMMA;
    主映射数组[160] = KEY_KPENTER;
    主映射数组[161] = KEY_KPEQUAL;
    主映射数组[162] = KEY_KPLEFTPAREN;
    主映射数组[163] = KEY_KPRIGHTPAREN;
    主映射数组[164] = KEY_MUTE;
    主映射数组[165] = KEY_INFO;
    主映射数组[166] = KEY_CHANNELUP;
    主映射数组[167] = KEY_CHANNELDOWN;
    主映射数组[168] = KEY_ZOOMIN;
    主映射数组[169] = KEY_ZOOMOUT;
    主映射数组[170] = KEY_TV;
    主映射数组[171] = KEY_WINDOW;
    主映射数组[172] = KEY_GUIDE;
    主映射数组[173] = KEY_DVD;
    主映射数组[174] = KEY_BOOKMARKS;
    主映射数组[175] = KEY_SUBTITLE;
    主映射数组[176] = KEY_SETTINGS;
    主映射数组[177] = KEY_TV;
    主映射数组[178] = KEY_VIDEO;
    主映射数组[179] = KEY_POWER;
    主映射数组[180] = KEY_VIDEO;
    主映射数组[181] = KEY_POWER;
    主映射数组[182] = KEY_AUDIO;
    主映射数组[183] = KEY_RED;
    主映射数组[184] = KEY_GREEN;
    主映射数组[185] = KEY_YELLOW;
    主映射数组[186] = KEY_BLUE;
    主映射数组[187] = KEY_APPSELECT;
    主映射数组[188] = BTN_0;
    主映射数组[189] = BTN_1;
    主映射数组[190] = BTN_2;
    主映射数组[191] = BTN_3;
    主映射数组[192] = BTN_4;
    主映射数组[193] = BTN_5;
    主映射数组[194] = BTN_6;
    主映射数组[195] = BTN_7;
    主映射数组[196] = BTN_8;
    主映射数组[197] = BTN_9;
    主映射数组[198] = KEY_PROG1;
    主映射数组[199] = KEY_PROG2;
    主映射数组[200] = KEY_PROG3;
    主映射数组[201] = KEY_PROG4;
    主映射数组[202] = KEY_LANGUAGE;
    主映射数组[203] = KEY_MODE;
    主映射数组[204] = KEY_3D_MODE;
    主映射数组[205] = KEY_ADDRESSBOOK;
    主映射数组[206] = KEY_CALENDAR;
    主映射数组[207] = KEY_MUSIC;
    主映射数组[208] = KEY_CALC;
    主映射数组[209] = KEY_ZENKAKUHANKAKU;
    主映射数组[210] = KEY_EISU;
    主映射数组[211] = KEY_MUHENKAN;
    主映射数组[212] = KEY_HENKAN;
    主映射数组[213] = KEY_KATAKANAHIRAGANA;
    主映射数组[214] = KEY_YEN;
    主映射数组[215] = KEY_RO;
    主映射数组[216] = KEY_KANA;
    主映射数组[217] = KEY_ASSISTANT;
    主映射数组[218] = KEY_BRIGHTNESSDOWN;
    主映射数组[219] = KEY_BRIGHTNESSUP;
    主映射数组[220] = KEY_MEDIA;
    主映射数组[221] = KEY_SLEEP;
    主映射数组[222] = KEY_WAKEUP;
    主映射数组[223] = KEY_BLUETOOTH;
    主映射数组[224] = KEY_MENU;
    主映射数组[225] = KEY_11;
    主映射数组[226] = KEY_12;
    主映射数组[227] = KEY_LAST;
    主映射数组[228] = KEY_DATA;
    主映射数组[229] = KEY_ASSISTANT;
    主映射数组[230] = KEY_RADIO;
    主映射数组[231] = KEY_TELETEXT;
    主映射数组[232] = KEY_DIGITS;
    主映射数组[233] = KEY_TV;
    主映射数组[234] = KEY_TV;
    主映射数组[235] = KEY_SAT;
    主映射数组[236] = KEY_SAT;
    主映射数组[237] = KEY_SAT;
    主映射数组[238] = KEY_SAT;
    主映射数组[239] = KEY_NETWORK;
    主映射数组[240] = KEY_TUNER;
    主映射数组[241] = KEY_HDMI;
    主映射数组[242] = KEY_HDMI;
    主映射数组[243] = KEY_HDMI;
    主映射数组[244] = KEY_HDMI;
    主映射数组[245] = KEY_VIDEO;
    主映射数组[246] = KEY_VIDEO;
    主映射数组[247] = KEY_VIDEO;
    主映射数组[248] = KEY_VIDEO;
    主映射数组[249] = KEY_VIDEO;
    主映射数组[250] = KEY_TUNER;
    主映射数组[251] = KEY_PC;
    主映射数组[252] = KEY_TV;
    主映射数组[253] = KEY_HD;
    主映射数组[254] = KEY_DIGITS;
    主映射数组[255] = KEY_LINEPHONE;
    主映射数组[256] = KEY_KP0;
    主映射数组[257] = KEY_KP1;
    主映射数组[258] = KEY_KP2;
    主映射数组[259] = KEY_KP3;
    主映射数组[260] = KEY_KP4;
    主映射数组[261] = KEY_KP5;
    主映射数组[262] = KEY_KP6;
    主映射数组[263] = KEY_KP7;
    主映射数组[264] = KEY_KP8;
    主映射数组[265] = KEY_KP9;
    主映射数组[266] = KEY_KPSLASH;
    主映射数组[267] = KEY_KPASTERISK;
    主映射数组[268] = KEY_KPMINUS;
    主映射数组[269] = KEY_KPPLUS;
    主映射数组[270] = KEY_KPDOT;
    主映射数组[271] = KEY_COMMA;
    主映射数组[272] = KEY_ENTER;
    主映射数组[273] = KEY_EQUAL;
    主映射数组[274] = KEY_LEFTBRACE;
    主映射数组[275] = KEY_RIGHTBRACE;
    主映射数组[276] = KEY_SLEEP;
    主映射数组[277] = KEY_WAKEUP;
    主映射数组[278] = KEY_PAIRING;
    主映射数组[279] = KEY_TOPMENU;
    主映射数组[280] = KEY_DIGITS;
    主映射数组[281] = KEY_DIGITS;
    主映射数组[282] = KEY_LAST;
    主映射数组[283] = KEY_DATA;
    主映射数组[284] = KEY_ASSISTANT;
    主映射数组[285] = KEY_RADIO;
    主映射数组[286] = KEY_TELETEXT;
    主映射数组[287] = KEY_DIGITS;
    主映射数组[288] = KEY_TERRESTRIAL;
    主映射数组[289] = KEY_SAT;
    主映射数组[290] = KEY_SAT;
    主映射数组[291] = KEY_SAT;
    主映射数组[292] = KEY_SAT;
    主映射数组[293] = KEY_NETWORK;
    主映射数组[294] = KEY_TUNER;
    主映射数组[295] = KEY_HDMI;
    主映射数组[296] = KEY_HDMI;
    主映射数组[297] = KEY_HDMI;
    主映射数组[298] = KEY_HDMI;
    主映射数组[299] = KEY_VIDEO;
    主映射数组[300] = KEY_VIDEO;
    主映射数组[301] = KEY_VIDEO;
    主映射数组[302] = KEY_VIDEO;
    主映射数组[303] = KEY_VIDEO;
    主映射数组[304] = KEY_TUNER;
    主映射数组[305] = KEY_PC;
    主映射数组[306] = KEY_TV;
    主映射数组[307] = KEY_HD;
    主映射数组[308] = KEY_DIGITS;
    主映射数组[309] = KEY_LINEPHONE;
    主映射数组[310] = KEY_MACRO;
    主映射数组[311] = KEY_MACRO;
    主映射数组[312] = KEY_MACRO;
    主映射数组[313] = KEY_MACRO;
    主映射数组[314] = KEY_MACRO;
    主映射数组[315] = KEY_MACRO;
    主映射数组[316] = KEY_MACRO;
    主映射数组[317] = KEY_MACRO;
    主映射数组[318] = KEY_MACRO;
    主映射数组[319] = KEY_MACRO;
}
void 键码映射表::初始化反向表() { for (int 安卓码 = 0; 安卓码 < 320; ++安卓码) { int 虚拟码 = 主映射数组[安卓码]; if (虚拟码 >= 0 && 虚拟码 < 768 && 反向映射数组[虚拟码] < 0) { 反向映射数组[虚拟码] = 安卓码; } } }
修饰键追踪器::修饰键追踪器() : 左移位状态(false), 右移位状态(false), 左控制状态(false), 右控制状态(false), 左交替状态(false), 右交替状态(false), 徽标状态(false), 功能状态(false) { 清空状态(); }
void 修饰键追踪器::按下键(int 安卓键码) { if (安卓键码 == 59) { 左移位状态 = true; } else if (安卓键码 == 60) { 右移位状态 = true; } else if (安卓键码 == 113) { 左控制状态 = true; } else if (安卓键码 == 114) { 右控制状态 = true; } else if (安卓键码 == 57) { 左交替状态 = true; } else if (安卓键码 == 58) { 右交替状态 = true; } else if (安卓键码 == 117 || 安卓键码 == 118) { 徽标状态 = true; } else if (安卓键码 == 119) { 功能状态 = true; } }
void 修饰键追踪器::抬起键(int 安卓键码) { if (安卓键码 == 59) { 左移位状态 = false; } else if (安卓键码 == 60) { 右移位状态 = false; } else if (安卓键码 == 113) { 左控制状态 = false; } else if (安卓键码 == 114) { 右控制状态 = false; } else if (安卓键码 == 57) { 左交替状态 = false; } else if (安卓键码 == 58) { 右交替状态 = false; } else if (安卓键码 == 117 || 安卓键码 == 118) { 徽标状态 = false; } else if (安卓键码 == 119) { 功能状态 = false; } }
bool 修饰键追踪器::左移按下() const { return 左移位状态; }
bool 修饰键追踪器::右移按下() const { return 右移位状态; }
bool 修饰键追踪器::左控按下() const { return 左控制状态; }
bool 修饰键追踪器::右控按下() const { return 右控制状态; }
bool 修饰键追踪器::左交替按下() const { return 左交替状态; }
bool 修饰键追踪器::右交替按下() const { return 右交替状态; }
bool 修饰键追踪器::任一移位按下() const { return 左移位状态 || 右移位状态; }
bool 修饰键追踪器::任一控制按下() const { return 左控制状态 || 右控制状态; }
int 修饰键追踪器::当前组合掩码() const { int 掩码 = 0; if (左移位状态 || 右移位状态) { 掩码 |= 1; } if (左控制状态 || 右控制状态) { 掩码 |= 2; } if (左交替状态 || 右交替状态) { 掩码 |= 4; } if (徽标状态) { 掩码 |= 8; } if (功能状态) { 掩码 |= 16; } return 掩码; }
void 修饰键追踪器::清空状态() { 左移位状态 = false; 右移位状态 = false; 左控制状态 = false; 右控制状态 = false; 左交替状态 = false; 右交替状态 = false; 徽标状态 = false; 功能状态 = false; }
bool 修饰键追踪器::组合判定零() const { return 组合判定通用(0); }
bool 修饰键追踪器::组合判定一() const { return 组合判定通用(1); }
bool 修饰键追踪器::组合判定二() const { return 组合判定通用(2); }
bool 修饰键追踪器::组合判定三() const { return 组合判定通用(3); }
bool 修饰键追踪器::组合判定四() const { return 组合判定通用(4); }
bool 修饰键追踪器::组合判定五() const { return 组合判定通用(5); }
bool 修饰键追踪器::组合判定六() const { return 组合判定通用(6); }
bool 修饰键追踪器::组合判定七() const { return 组合判定通用(7); }
bool 修饰键追踪器::组合判定八() const { return 组合判定通用(8); }
bool 修饰键追踪器::组合判定九() const { return 组合判定通用(9); }
bool 修饰键追踪器::组合判定十() const { return 组合判定通用(10); }
bool 修饰键追踪器::组合判定十一() const { return 组合判定通用(11); }
bool 修饰键追踪器::组合判定十二() const { return 组合判定通用(12); }
bool 修饰键追踪器::组合判定十三() const { return 组合判定通用(13); }
bool 修饰键追踪器::组合判定十四() const { return 组合判定通用(14); }
bool 修饰键追踪器::组合判定十五() const { return 组合判定通用(15); }
bool 修饰键追踪器::组合判定十六() const { return 组合判定通用(16); }
bool 修饰键追踪器::组合判定十七() const { return 组合判定通用(17); }
bool 修饰键追踪器::组合判定十八() const { return 组合判定通用(18); }
bool 修饰键追踪器::组合判定十九() const { return 组合判定通用(19); }
bool 修饰键追踪器::组合判定二十() const { return 组合判定通用(20); }
bool 修饰键追踪器::组合判定二十一() const { return 组合判定通用(21); }
bool 修饰键追踪器::组合判定二十二() const { return 组合判定通用(22); }
bool 修饰键追踪器::组合判定二十三() const { return 组合判定通用(23); }
bool 修饰键追踪器::组合判定二十四() const { return 组合判定通用(24); }
bool 修饰键追踪器::组合判定二十五() const { return 组合判定通用(25); }
bool 修饰键追踪器::组合判定二十六() const { return 组合判定通用(26); }
bool 修饰键追踪器::组合判定二十七() const { return 组合判定通用(27); }
bool 修饰键追踪器::组合判定二十八() const { return 组合判定通用(28); }
bool 修饰键追踪器::组合判定二十九() const { return 组合判定通用(29); }
void 修饰键追踪器::更新组合零(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合一(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合二(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合三(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合四(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合五(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合六(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合七(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合八(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合九(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十一(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十二(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十三(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十四(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十五(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十六(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十七(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十八(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
void 修饰键追踪器::更新组合十九(int 安卓键码, bool 是否按下) { 更新组合通用(安卓键码, 是否按下); }
按键去抖器::按键去抖器(uint64_t 去抖微秒) : 去抖阈值(去抖微秒) { for (int 下标 = 0; 下标 < 320; ++下标) { 上次时刻表[下标] = 0; 上次状态表[下标] = false; } if (去抖阈值 < 5000) { 去抖阈值 = 5000; } }
bool 按键去抖器::过滤按键零(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键一(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键三(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键四(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键五(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键六(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键七(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键八(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键九(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十一(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十二(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十三(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十四(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十五(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十六(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十七(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十八(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键十九(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十一(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十二(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十三(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十四(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十五(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十六(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十七(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十八(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::过滤按键二十九(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 过滤按键通用(安卓键码, 是否按下, 当前时刻); }
bool 按键去抖器::应否放行(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { if (安卓键码 < 0 || 安卓键码 >= 320) { return false; } uint64_t 上次 = 上次时刻表[安卓键码]; if (是否按下 == 上次状态表[安卓键码] && 当前时刻 - 上次 < 去抖阈值) { return false; } 上次时刻表[安卓键码] = 当前时刻; 上次状态表[安卓键码] = 是否按下; return true; }
void 按键去抖器::清空记录() { for (int 下标 = 0; 下标 < 320; ++下标) { 上次时刻表[下标] = 0; 上次状态表[下标] = false; } }
void 按键去抖器::设置阈值(uint64_t 去抖微秒) { 去抖阈值 = 去抖微秒; if (去抖阈值 < 1000) { 去抖阈值 = 1000; } }
uint64_t 按键去抖器::上次时刻零(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻一(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻二(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻三(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻四(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻五(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻六(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻七(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻八(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻九(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻十(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻十一(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻十二(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻十三(int 安卓键码) const { return 上次时刻通用(安卓键码); }
uint64_t 按键去抖器::上次时刻十四(int 安卓键码) const { return 上次时刻通用(安卓键码); }
组合键识别器::组合键识别器() { static_assert(sizeof(int) >= 4, "位宽"); }
int 组合键识别器::识别组合零(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(0, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合一(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(1, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(2, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(3, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合四(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(4, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合五(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(5, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合六(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(6, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合七(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(7, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合八(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(8, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合九(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(9, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(10, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十一(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(11, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十二(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(12, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十三(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(13, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十四(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(14, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十五(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(15, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十六(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(16, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十七(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(17, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十八(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(18, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合十九(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(19, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(20, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十一(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(21, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十二(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(22, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十三(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(23, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十四(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(24, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十五(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(25, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十六(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(26, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十七(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(27, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十八(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(28, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合二十九(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(29, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(30, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十一(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(31, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十二(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(32, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十三(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(33, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十四(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(34, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十五(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(35, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十六(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(36, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十七(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(37, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十八(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(38, 第一键, 第二键, 第三键); }
int 组合键识别器::识别组合三十九(int 第一键, int 第二键, int 第三键) const { return 识别组合通用(39, 第一键, 第二键, 第三键); }
bool 组合键识别器::是否复制(int 修饰掩码, int 主键) const { return (修饰掩码 & 2) && 主键 == 33; }
bool 组合键识别器::是否粘贴(int 修饰掩码, int 主键) const { return (修饰掩码 & 2) && 主键 == 50; }
bool 组合键识别器::是否剪切(int 修饰掩码, int 主键) const { return (修饰掩码 & 2) && 主键 == 52; }
bool 组合键识别器::是否全选(int 修饰掩码, int 主键) const { return (修饰掩码 & 2) && 主键 == 29; }
bool 组合键识别器::是否撤销(int 修饰掩码, int 主键) const { return (修饰掩码 & 2) && 主键 == 54; }
int 组合键识别器::快捷功能码(int 修饰掩码, int 主键) const { if (是否复制(修饰掩码, 主键)) { return 1; } if (是否粘贴(修饰掩码, 主键)) { return 2; } if (是否剪切(修饰掩码, 主键)) { return 3; } if (是否全选(修饰掩码, 主键)) { return 4; } if (是否撤销(修饰掩码, 主键)) { return 5; } return 0; }
int 组合键识别器::扩展识别零(int 修饰掩码, int 主键) const { return 扩展识别通用(0, 修饰掩码, 主键); }
int 组合键识别器::扩展识别一(int 修饰掩码, int 主键) const { return 扩展识别通用(1, 修饰掩码, 主键); }
int 组合键识别器::扩展识别二(int 修饰掩码, int 主键) const { return 扩展识别通用(2, 修饰掩码, 主键); }
int 组合键识别器::扩展识别三(int 修饰掩码, int 主键) const { return 扩展识别通用(3, 修饰掩码, 主键); }
int 组合键识别器::扩展识别四(int 修饰掩码, int 主键) const { return 扩展识别通用(4, 修饰掩码, 主键); }
int 组合键识别器::扩展识别五(int 修饰掩码, int 主键) const { return 扩展识别通用(5, 修饰掩码, 主键); }
int 组合键识别器::扩展识别六(int 修饰掩码, int 主键) const { return 扩展识别通用(6, 修饰掩码, 主键); }
int 组合键识别器::扩展识别七(int 修饰掩码, int 主键) const { return 扩展识别通用(7, 修饰掩码, 主键); }
int 组合键识别器::扩展识别八(int 修饰掩码, int 主键) const { return 扩展识别通用(8, 修饰掩码, 主键); }
int 组合键识别器::扩展识别九(int 修饰掩码, int 主键) const { return 扩展识别通用(9, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十(int 修饰掩码, int 主键) const { return 扩展识别通用(10, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十一(int 修饰掩码, int 主键) const { return 扩展识别通用(11, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十二(int 修饰掩码, int 主键) const { return 扩展识别通用(12, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十三(int 修饰掩码, int 主键) const { return 扩展识别通用(13, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十四(int 修饰掩码, int 主键) const { return 扩展识别通用(14, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十五(int 修饰掩码, int 主键) const { return 扩展识别通用(15, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十六(int 修饰掩码, int 主键) const { return 扩展识别通用(16, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十七(int 修饰掩码, int 主键) const { return 扩展识别通用(17, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十八(int 修饰掩码, int 主键) const { return 扩展识别通用(18, 修饰掩码, 主键); }
int 组合键识别器::扩展识别十九(int 修饰掩码, int 主键) const { return 扩展识别通用(19, 修饰掩码, 主键); }
自动连发器::自动连发器() : 初次延迟值(400000), 重复间隔值(50000) { for (int 下标 = 0; 下标 < 320; ++下标) { 按下时刻表[下标] = 0; 按下状态表[下标] = false; } }
bool 自动连发器::是否触发连发零(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发一(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发三(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发四(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发五(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发六(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发七(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发八(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发九(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十一(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十二(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十三(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十四(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十五(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十六(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十七(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十八(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发十九(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二十(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二十一(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二十二(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二十三(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
bool 自动连发器::是否触发连发二十四(int 安卓键码, uint64_t 当前时刻) const { return 是否触发连发通用(安卓键码, 当前时刻); }
void 自动连发器::记录按下(int 安卓键码, uint64_t 当前时刻) { if (安卓键码 < 0 || 安卓键码 >= 320) { return; } 按下时刻表[安卓键码] = 当前时刻; 按下状态表[安卓键码] = true; }
void 自动连发器::记录抬起(int 安卓键码) { if (安卓键码 < 0 || 安卓键码 >= 320) { return; } 按下状态表[安卓键码] = false; }
void 自动连发器::设置初次延迟(uint64_t 延迟微秒) { 初次延迟值 = 延迟微秒; }
void 自动连发器::设置重复间隔(uint64_t 间隔微秒) { 重复间隔值 = 间隔微秒; }
int 自动连发器::待发次数零(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数一(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数二(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数三(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数四(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数五(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数六(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数七(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数八(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数九(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数十(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数十一(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数十二(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数十三(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
int 自动连发器::待发次数十四(int 安卓键码, uint64_t 当前时刻) const { return 待发次数通用(安卓键码, 当前时刻); }
// 分组映射统一核心主表已含全量对应关系直接查表替代五十组开关分支消除分支预测失败
int 键码映射表::分组映射通用(int 组号, int 安卓键码) const { (void)组号; return 安卓转虚拟(安卓键码); }
// 分组判定统一核心有效映射即放行原三十组取模抽奖修复为诚实判定
bool 键码映射表::分组判定通用(int 组号, int 安卓键码) const { (void)组号; int 映射值 = 安卓转虚拟(安卓键码); return 映射值 >= 0; }
// 组合判定统一核心按位直查原三十组极性漂移修复为位存在即真
bool 修饰键追踪器::组合判定通用(int 位号) const { int 掩码 = 当前组合掩码(); int 位 = ((位号 % 5) + 5) % 5; return ((掩码 >> 位) & 1) != 0; }
void 修饰键追踪器::更新组合通用(int 安卓键码, bool 是否按下) { if (是否按下) { 按下键(安卓键码); } else { 抬起键(安卓键码); } }
// 去抖统一核心全区间记录时间差原三十组区间门限漂移修复为统一阈值单次比较
bool 按键去抖器::过滤按键通用(int 安卓键码, bool 是否按下, uint64_t 当前时刻) { return 应否放行(安卓键码, 是否按下, 当前时刻); }
uint64_t 按键去抖器::上次时刻通用(int 安卓键码) const { if (安卓键码 < 0 || 安卓键码 >= 320) { return 0; } return 上次时刻表[安卓键码]; }
// 组合识别统一核心公式保留组号语义原四十组拷贝收敛为单次算术判定
int 组合键识别器::识别组合通用(int 组号, int 第一键, int 第二键, int 第三键) const { int 归一组 = 组号 < 0 ? 0 : (组号 > 39 ? 39 : 组号); int 累加 = 第一键 * 3 + 第二键 * 5 + 第三键 * 7 + 归一组; if (累加 % 11 == 0) { return 归一组; } if (第一键 == 113 && 第二键 == 29 + 归一组) { return 100 + 归一组; } return -1; }
int 组合键识别器::扩展识别通用(int 组号, int 修饰掩码, int 主键) const { int 归一组 = 组号 < 0 ? 0 : (组号 > 19 ? 19 : 组号); int 除数 = 2 + 归一组; int 组合 = 修饰掩码 * 1000 + 主键; if (除数 != 0 && 组合 % 除数 == 0) { return 归一组; } return 快捷功能码(修饰掩码, 主键); }
// 连发统一核心阈值归零修复逐级虚增单次时间比较替代重复分支
bool 自动连发器::是否触发连发通用(int 安卓键码, uint64_t 当前时刻) const { if (安卓键码 < 0 || 安卓键码 >= 320) { return false; } if (!按下状态表[安卓键码]) { return false; } uint64_t 已按 = 当前时刻 > 按下时刻表[安卓键码] ? 当前时刻 - 按下时刻表[安卓键码] : 0; return 已按 > 初次延迟值; }
int 自动连发器::待发次数通用(int 安卓键码, uint64_t 当前时刻) const { if (安卓键码 < 0 || 安卓键码 >= 320 || !按下状态表[安卓键码]) { return 0; } uint64_t 已按 = 当前时刻 > 按下时刻表[安卓键码] ? 当前时刻 - 按下时刻表[安卓键码] : 0; if (已按 <= 初次延迟值 || 重复间隔值 == 0) { return 0; } return (int)((已按 - 初次延迟值) / 重复间隔值); }
} // namespace 输入子系统
