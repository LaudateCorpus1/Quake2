#include "client.h"
#include "qmenu.h"

// dmflags->integer flags
//!! NOTE: all DF_XXX... values used in engine for dmflags edit menu only
// should remove this consts (place into a separate script for dmflags editor !!)
#define	DF_NO_HEALTH		0x00000001
#define	DF_NO_ITEMS			0x00000002
#define	DF_WEAPONS_STAY		0x00000004
#define	DF_NO_FALLING		0x00000008
#define	DF_INSTANT_ITEMS	0x00000010
#define	DF_SAME_LEVEL		0x00000020
#define DF_SKINTEAMS		0x00000040
#define DF_MODELTEAMS		0x00000080
#define DF_NO_FRIENDLY_FIRE	0x00000100
#define	DF_SPAWN_FARTHEST	0x00000200
#define DF_FORCE_RESPAWN	0x00000400
#define DF_NO_ARMOR			0x00000800
#define DF_ALLOW_EXIT		0x00001000
#define DF_INFINITE_AMMO	0x00002000
#define DF_QUAD_DROP		0x00004000
#define DF_FIXED_FOV		0x00008000
// XATRIX
#define	DF_QUADFIRE_DROP	0x00010000
// ROGUE
#define DF_NO_MINES			0x00020000
#define DF_NO_STACK_DOUBLE	0x00040000
#define DF_NO_NUKES			0x00080000
#define DF_NO_SPHERES		0x00100000


#define CHAR_WIDTH	8
#define CHAR_HEIGHT	8


const char *menu_in_sound   = "misc/menu1.wav";
const char *menu_move_sound = "misc/menu2.wav";
const char *menu_out_sound  = "misc/menu3.wav";


static const char *yesno_names[] = {
	"no", "yes", NULL
};


// forward declarations
static void Menu_DMOptions_f();

/*---------- Support Routines ------------*/

void M_ForceMenuOn()
{
	if (!m_current) M_Menu_Main_f();
}


//=============================================================================

static void M_Print(int cx, int cy, const char *str)
{
	while (*str)
	{
		RE_DrawChar(cx + ((viddef.width - 320)/2), cy + ((viddef.height - 240)/2), *str++, C_RED);
		cx += CHAR_WIDTH;
	}
}

static void DrawCharacter(int cx, int cy, int num)
{
	RE_DrawChar(cx + ((viddef.width - 320)/2), cy + ((viddef.height - 240)/2), num);
}

static void M_DrawTextBox(int x, int y, int width, int lines)
{
	int		n;

	// draw left side
	int cx = x;
	int cy = y;
	DrawCharacter(cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += CHAR_HEIGHT;
		DrawCharacter(cx, cy, 4);
	}
	DrawCharacter(cx, cy+CHAR_HEIGHT, 7);

	// draw middle
	cx += CHAR_WIDTH;
	while (width > 0)
	{
		cy = y;
		DrawCharacter(cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += CHAR_HEIGHT;
			DrawCharacter(cx, cy, 5);
		}
		DrawCharacter(cx, cy+CHAR_HEIGHT, 8);
		width -= 1;
		cx += CHAR_WIDTH;
	}

	// draw right side
	cy = y;
	DrawCharacter(cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += CHAR_HEIGHT;
		DrawCharacter(cx, cy, 6);
	}
	DrawCharacter(cx, cy+CHAR_WIDTH, 9);
}


/*
=======================================================================

MAIN MENU

=======================================================================
*/

struct menuMain_t : menuFramework_t
{
#define	MAIN_ITEMS	5			// == ARRAY_COUNT(menuMain_t::menuNames)
#define NUM_CURSOR_FRAMES 15

	static const char *menuNames[MAIN_ITEMS];
	void Draw()
	{
		int		i;

		// compute menu size
		int		widest = -1;
		int		totalheight = 0;
		for (i = 0; i < MAIN_ITEMS; i++)
		{
			CBasicImage *img = RE_RegisterPic(va("pics/m_main_%s", menuNames[i]));
			if (!img) continue;		//?? error
			if (img->width > widest) widest = img->width;
			totalheight += img->height + 12;
		}

		int ystart = (viddef.height - totalheight) / 2;
		int xoffset = (viddef.width - widest + 70) / 2;

		// draw items
		for (i = 0; i < MAIN_ITEMS; i++)
		{
			//?? can use img->Draw()
			RE_DrawPic(xoffset, ystart + i * 40 + 13, va("pics/m_main_%s%s", menuNames[i], i != cursor ? "" : "_sel"));
		}
		// draw animated cursor
#if 0
		RE_DrawPic(xoffset - 25, ystart + cursor * 40 + 11,
			va("pics/m_cursor%d", (unsigned)appRound(cls.realtime / 100) % NUM_CURSOR_FRAMES));
#else
		RE_DrawPic(xoffset - 25, ystart + cursor * 40 + 11, "pics/m_cursor");
#endif

		CBasicImage *img = RE_RegisterPic("pics/m_main_plaque");
		if (img)
		{
			//!! can use img->Draw()
			// NOTE: size of this image used for 2 image placement
			RE_DrawPic(xoffset - 30 - img->width, ystart, "pics/m_main_plaque");					// "QUAKE2" vertical text
			RE_DrawPic(xoffset - 30 - img->width, ystart + img->height + 5, "pics/m_main_logo");	// id logo
		}
	}

	const char * KeyDown(int key)
	{
		const char *sound = menu_move_sound;

		switch (key)
		{
		case K_ESCAPE:
		case K_MOUSE2:
			Pop();
			break;

		case K_KP_DOWNARROW:
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			if (++cursor >= MAIN_ITEMS)
				cursor = 0;
			return sound;

		case K_KP_UPARROW:
		case K_UPARROW:
		case K_MWHEELUP:
			if (--cursor < 0)
				cursor = MAIN_ITEMS - 1;
			return sound;

		case K_HOME:
			cursor = 0;
			return sound;

		case K_END:
			cursor = MAIN_ITEMS - 1;
			return sound;

		case K_KP_ENTER:
		case K_ENTER:
		case K_MOUSE1:
			Cbuf_AddText(va("menu_%s\n", menuNames[cursor]));
			return menu_in_sound;
		}

		return NULL;
	}

	bool Init()
	{
		M_ForceMenuOff();		// pop all opened menus
		return true;
	}
};
static menuMain_t mainMenu;

// static members
const char *menuMain_t::menuNames[] =
{
	"game",
	"multiplayer",
	"options",
	"video",
	"quit"
};

void M_Menu_Main_f()
{
	mainMenu.Push();
}

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/

struct multiplayerMenu_t : menuFramework_t
{
	menuAction_t	join_network_server_action;
	menuAction_t	start_network_server_action;
	menuAction_t	player_setup_action;
	menuAction_t	disconnect_action;

	bool Init()
	{
		x = viddef.width / 2 - 64;
		nitems = 0;
		banner = "multiplayer";

		MENU_ACTION(join_network_server_action,0,"join network server",NULL,"menu_joinserver")
		join_network_server_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&join_network_server_action);

		MENU_ACTION(start_network_server_action,10,"start network server",NULL,"menu_startserver")
		start_network_server_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&start_network_server_action);

		MENU_ACTION(disconnect_action,20,"disconnect",NULL,"disconnect;menu_main")
		disconnect_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&disconnect_action);

		MENU_ACTION(player_setup_action,30,"player setup",NULL,"menu_playerconfig")
		player_setup_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&player_setup_action);

		SetStatusBar(NULL);
		Center();
		return true;
	}
};
static multiplayerMenu_t multiplayerMenu;

static void Menu_Multiplayer_f()
{
	multiplayerMenu.Push();
}


/*
=======================================================================

KEYS MENU

=======================================================================
*/

static void SeekLine(const char *&s)
{
	char c1 = *s;							// remember line delimiter (CR or LF)
	if (c1 != '\n' && c1 != '\r') return;	// it was not line delimiter!
	while (true)
	{
		char c2 = *(++s);
		if (c1 == c2) return;				// next end of line
		if (c2 != '\n' && c2 != '\r') return; // next line text
	}
}

static void AppendToken(char **dest, const char *&src)
{
	char *tok;
	tok = COM_Parse(src);
	strcpy(*dest, tok);
	*dest += strlen(tok) + 1;
}


struct keysMenu_t : menuFramework_t
{
#define MAX_KEYS_MENU		128
#define MAX_KEYS_BUF		16384
#define BIND_LINE_HEIGHT	10
	menuAction_t keys_item[MAX_KEYS_MENU];
	static char *bindnames[MAX_KEYS_MENU][2];		// static, because accessed from static DrawKeyBindingFunc()
	char	bindmenubuf[MAX_KEYS_BUF];

	static void KeyCursorDrawFunc(menuFramework_t *menu)
	{
		RE_DrawChar(menu->x, menu->y + menu->cursor * BIND_LINE_HEIGHT,
			cls.key_dest == key_bindingMenu ? '=' : 12 + ((cls.realtime >> 8) & 1));
	}

	static void DrawKeyBindingFunc(void *self)
	{
		int		keys[2];
		menuAction_t *a = (menuAction_t *) self;
		TString<256> Text;

		int numKeys = Key_FindBinding(bindnames[a->localData][0], ARRAY_ARG(keys));

		if (!numKeys)
			Text = S_RED"(not bound)";
		else
		{
			if (numKeys == 1)
				Text = Key_KeynumToString(keys[0]);
			else
				Text.sprintf("%s or %s%s", Key_KeynumToString(keys[0]), Key_KeynumToString(keys[1]), numKeys > 2 ? " ..." : "");
		}
		DrawString(a->x + a->parent->x + 16, a->y + a->parent->y, Text);
	}

	bool Init()
	{
		int		i;

		//!! change parser
		// load "binds.lst" (file always present - have inline file)
		unsigned length;
		char *buffer = (char*) GFileSystem->LoadFile("binds.lst", &length);
		// parse this file
		const char *s = buffer;
		int numbinds = 0;
		for (i = 0; i < length; i++)
			if (s[i] == '\n') numbinds++;
		char *d = bindmenubuf;
		s = buffer;
		for (i = 0; i < numbinds; i++)
		{
			bindnames[i][0] = "";
			bindnames[i][1] = "";
			char c;
			while ((c = *s) == ' ') s++;		// skip whitespace
			if (c == '\r' || c == '\n')
			{
				SeekLine(s);
				continue;	// empty line
			}
			bindnames[i][0] = d;				// remember place for command name
			AppendToken(&d, s);
			while ((c = *s) == ' ') s++;
			if (c == '\r' || c == '\n')
			{	// EOLN => command = menu string
				bindnames[i][1] = bindnames[i][0];
				SeekLine(s);
				continue;
			}
			bindnames[i][1] = d;
			AppendToken(&d, s);
			while (*s != '\n') s++;				// was '\r'
			SeekLine(s);
		}

		// unload "binds.lst"
		delete buffer;

		// build menu
		int y = 0;

		x = viddef.width / 2;
		nitems = 0;
		cursordraw = KeyCursorDrawFunc;

		for (i = 0; i < numbinds; i++)
		{
			if (*bindnames[i][0])
			{
				keys_item[i].type = MTYPE_ACTION;
				keys_item[i].flags = QMF_GRAYED;
			}
			else
			{
				keys_item[i].type = MTYPE_SEPARATOR;
				keys_item[i].flags = QMF_CENTER;
			}
			keys_item[i].x = 0;
			keys_item[i].y = y; y += BIND_LINE_HEIGHT;
			keys_item[i].ownerdraw = DrawKeyBindingFunc;
			keys_item[i].localData = i;
			keys_item[i].name = bindnames[i][1];
			AddItem(&keys_item[i]);
		}

		SetStatusBar("enter to change, backspace to clear");
		Center();
		return true;
	}

	void UnbindCommand(char *command)
	{
		int		keys[256];

		int numKeys = Key_FindBinding(command, ARRAY_ARG(keys));
		for (int i = 0; i < numKeys; i++)
		{
			Key_SetBinding(keys[i], NULL);
//			Com_DPrintf("unbind %s\n",Key_KeynumToString(keys[i]));
		}
	}

	void KeyBindingFunc(void *self)
	{
		menuAction_t *a = (menuAction_t *) self;
		if (Key_FindBinding(bindnames[a->localData][0], NULL, 0) > 1)
			UnbindCommand(bindnames[a->localData][0]);
		cls.key_dest = key_bindingMenu;		// hook keyboard
		SetStatusBar("press a key or button for this action");
	}

	const char * KeyDown(int key)
	{
		menuAction_t *item = (menuAction_t *) ItemAtCursor();
		if (cls.key_dest == key_bindingMenu)
		{
			if (key != K_ESCAPE && key != '`')
				Key_SetBinding(key, bindnames[item->localData][0]);

			SetStatusBar("enter to change, backspace to clear");
			cls.key_dest = key_menu;		// unhook keyboard
			return menu_out_sound;
		}

		switch (key)
		{
		case K_KP_ENTER:
		case K_ENTER:
		case K_MOUSE1:
			KeyBindingFunc(item);
			return menu_in_sound;
		case K_BACKSPACE:		// delete bindings
		case K_DEL:
		case K_KP_DEL:
			UnbindCommand(bindnames[item->localData][0]);
			return menu_out_sound;
		default:
			return menuFramework_t::KeyDown(key);
		}
	}
};
static keysMenu_t keysMenu;

// static members
char *keysMenu_t::bindnames[MAX_KEYS_MENU][2];


static void Menu_Keys_f()
{
	keysMenu.Push();
}


/*
=======================================================================

CONTROLS MENU

=======================================================================
*/
//static cvar_t *win_noalttab;
extern cvar_t *in_joystick;

#define MAX_CROSSHAIRS 256
static const char *crosshair_names[MAX_CROSSHAIRS + 1];	// reserve last item for NULL
static const char *crosshair_color_names[9] = {"", "", "", "", "", "", "", "", NULL};
static int crosshairs_count;


struct optionsMenu_t : menuFramework_t
{
	menuAction_t	defaults_action;
	menuAction_t	customize_options_action;
	static menuSlider_t	sensitivity_slider;
	static menuList_t	invertmouse_box;
	static menuList_t	alwaysrun_box;
	static menuList_t	crosshair_box;
	static menuList_t	crosshair_color_box;
	static menuSlider_t	sfxvolume_slider;
	static menuList_t	joystick_box;
	static menuList_t	cdvolume_box;
	static menuList_t	s_khz;
	static menuList_t	s_8bit;
	static menuList_t	s_reverse;
	static menuList_t	compatibility_list;		//?? win32-only

	static void CrosshairFunc( void *unused )
	{
		Cvar_SetInteger("crosshair", crosshair_box.curvalue);
		Cvar_SetInteger("crosshairColor", crosshair_color_box.curvalue);
	}

	static void JoystickFunc(void *unused)
	{
		Cvar_SetInteger("in_joystick", joystick_box.curvalue);
	}

	static void AlwaysRunFunc(void *unused)
	{
		Cvar_SetInteger("cl_run", alwaysrun_box.curvalue);
	}

	static void MouseSpeedFunc(void *unused)
	{
		Cvar_SetValue("sensitivity", sensitivity_slider.curvalue / 2.0f);
	}

	static void InvertMouseFunc(void *unused)
	{
		Cvar_SetValue("m_invert", !m_invert->value);
	}

	static void UpdateVolumeFunc(void *unused)
	{
		Cvar_SetValue("s_volume", sfxvolume_slider.curvalue / 10);
	}

	static void UpdateCDVolumeFunc(void *unused)
	{
		Cvar_SetInteger("cd_nocd", !cdvolume_box.curvalue);
	}

	static void SetMenuItemValues()
	{
		sfxvolume_slider.curvalue	= Cvar_VariableValue("s_volume") * 10;
		cdvolume_box.curvalue 		= !Cvar_VariableInt("cd_nocd");
		s_8bit.curvalue				= Cvar_VariableInt("s_loadas8bit");
		int i = Cvar_VariableInt("s_khz");
		s_khz.curvalue				= i == 11 ? 0 : (i == 22 ? 1 : 2);
		s_reverse.curvalue			= Cvar_VariableInt("s_reverse_stereo");

		sensitivity_slider.curvalue	= sensitivity->value * 2;

		cl_run->Clamp(0, 1);
		alwaysrun_box.curvalue		= cl_run->integer;

		invertmouse_box.curvalue	= m_invert->integer;

		crosshair->Clamp(0, crosshairs_count);
		crosshair_box.curvalue		= crosshair->integer;

		crosshairColor->Clamp(0, 7);
		crosshair_color_box.curvalue = crosshairColor->integer;

		in_joystick->Clamp(0, 1);
		joystick_box.curvalue		= in_joystick->integer;
	}

	static void ResetDefaultsFunc(void *unused)
	{
		//?? should use cvar->resetString of all linked vars instead ?
		Cbuf_AddText("exec default.cfg\n");
		Cbuf_Execute();

		SetMenuItemValues();
	}

	static void UpdateSoundFunc(void *unused)
	{
		const int khz[] = {11, 22, 44};

		Cvar_SetInteger("s_loadas8bit", s_8bit.curvalue);
		Cvar_SetInteger("s_khz", khz[s_khz.curvalue]);
		Cvar_SetInteger("s_reverse_stereo", s_reverse.curvalue);

		Cvar_SetInteger("s_primary", compatibility_list.curvalue);

		M_DrawTextBox(8, 120 - 48, 36, 3 );
		M_Print(16 + 16, 120 - 48 + 8,  "Restarting the sound system. This");
		M_Print(16 + 16, 120 - 48 + 16, "could take up to a minute, so");
		M_Print(16 + 16, 120 - 48 + 24, "please be patient.");

		// the text box won't show up unless we do a buffer swap
		RE_EndFrame();

		CL_Snd_Restart_f();
	}

	static void ScanCrosshairs()
	{
		crosshair_names[0] = S_RED"(none)";
		int i;
		for (i = 1; i <= MAX_CROSSHAIRS; i++)	// item [0] is "none"
		{
			if (!ImageExists(va("pics/ch%d", i)))
			{
				crosshair_names[i] = NULL;
				break;
			}
			crosshair_names[i] = "";
		}
		crosshair_names[i] = NULL;
		crosshairs_count = i - 1;
	}

	bool Init()
	{
		static const char *cd_music_items[] = {
			"disabled", "enabled", NULL
		};

		static const char *s_khz_items[] = {
			"11 khz", "22 khz", "44 khz", NULL
		};

		static const char *compatibility_items[] = {
			"max compatibility", "max performance", NULL
		};

		/*------- configure controls menu and menu items -------*/

		x = viddef.width / 2;
		y = viddef.height / 2 - 58;
		nitems = 0;
		banner = "options";

		int y = 0;

		MENU_SLIDER(sfxvolume_slider,y,"effects volume",UpdateVolumeFunc,0,10)
		AddItem(&sfxvolume_slider);
		MENU_SPIN(cdvolume_box,y+=10,"CD music",UpdateCDVolumeFunc,cd_music_items)
		AddItem(&cdvolume_box);
		MENU_SPIN(s_khz,y+=10,"sound quality",UpdateSoundFunc,s_khz_items)
		AddItem(&s_khz);
		MENU_SPIN(s_8bit,y+=10,"8 bit sound",UpdateSoundFunc,yesno_names)
		AddItem(&s_8bit);
		MENU_SPIN(s_reverse,y+=10,"reverse stereo",UpdateSoundFunc,yesno_names)
		AddItem(&s_reverse);
		MENU_SPIN(compatibility_list,y+=10,"sound compatibility",UpdateSoundFunc,compatibility_items)
		compatibility_list.curvalue = Cvar_VariableInt("s_primary");
		AddItem(&compatibility_list);
		y += 10;
		MENU_SLIDER(sensitivity_slider,y+=10,"mouse speed",MouseSpeedFunc,2,22)
		AddItem(&sensitivity_slider);
		MENU_SPIN(invertmouse_box,y+=10,"invert mouse",InvertMouseFunc,yesno_names)
		AddItem(&invertmouse_box);
		MENU_SPIN(alwaysrun_box,y+=10,"always run",AlwaysRunFunc,yesno_names)
		AddItem(&alwaysrun_box);
		y += 10;
		MENU_SPIN(crosshair_box,y+=10,"crosshair shape",CrosshairFunc,crosshair_names)
		AddItem(&crosshair_box);
		MENU_SPIN(crosshair_color_box,y+=10,"crosshair color",CrosshairFunc,crosshair_color_names)
		AddItem(&crosshair_color_box);
		y += 10;
		MENU_SPIN(joystick_box,y+=10,"use joystick",JoystickFunc,yesno_names)
		AddItem(&joystick_box);
		y += 10;
		MENU_ACTION(customize_options_action,y+=10,"customize controls",NULL,"menu_keys")
		AddItem(&customize_options_action);
		MENU_ACTION(defaults_action,y+=10,S_RED"reset defaults",ResetDefaultsFunc,NULL)
		AddItem(&defaults_action);

		ScanCrosshairs();
		SetMenuItemValues();
		return true;
	}

	void Draw()
	{
		menuFramework_t::Draw();
		// draw crosshair
		if (!crosshair->integer) return;
		RE_DrawPic(viddef.width / 2 + 32, crosshair_box.y + y + 10, va("pics/ch%d", crosshair->integer),
			ANCHOR_CENTER, crosshairColor->integer);
	}
};
static optionsMenu_t optionsMenu;

menuSlider_t optionsMenu_t::sensitivity_slider;
menuList_t	optionsMenu_t::invertmouse_box;
menuList_t	optionsMenu_t::alwaysrun_box;
menuList_t	optionsMenu_t::crosshair_box;
menuList_t	optionsMenu_t::crosshair_color_box;
menuSlider_t optionsMenu_t::sfxvolume_slider;
menuList_t	optionsMenu_t::joystick_box;
menuList_t	optionsMenu_t::cdvolume_box;
menuList_t	optionsMenu_t::s_khz;
menuList_t	optionsMenu_t::s_8bit;
menuList_t	optionsMenu_t::s_reverse;
menuList_t	optionsMenu_t::compatibility_list;

static void Menu_Options_f()
{
	optionsMenu.Push();
}

/*
=============================================================================

END GAME MENU

=============================================================================
*/

struct creditsMenu_t : menuFramework_t
{
	int credits_start_time;
	char *credits[256];		// start of each line; ugly !!
	char *creditsBuffer;

	void Draw()
	{
		int y = viddef.height - (cls.realtime - credits_start_time) / 40;
		for (int i = 0; credits[i] && y < viddef.height; y += CHAR_HEIGHT+2, i++)
		{
			if (y <= -CHAR_HEIGHT)
				continue;

			int color = C_WHITE;
			const char *s = credits[i];
			if (s[0] == '+')
			{
				color = C_GREEN;
				s++;
			}

			int x = (viddef.width - strlen(s) * CHAR_WIDTH) / 2;
			while (*s)
			{
				RE_DrawChar(x, y, *s, color);
				x += CHAR_WIDTH;
				s++;
			}
		}

		if (y < 0)
			credits_start_time = cls.realtime;
	}

	const char * KeyDown(int key)
	{
		switch (key)
		{
		case K_ESCAPE:
		case K_MOUSE1:
		case K_MOUSE2:
			if (creditsBuffer) delete creditsBuffer;
			Pop();
			break;
		}
		return menu_out_sound;
	}

	bool Init()
	{
		unsigned count;
		creditsBuffer = (char*) GFileSystem->LoadFile("credits", &count);
		// file always present - have inline file
		char *p = creditsBuffer;
		int n;
		for (n = 0; n < ARRAY_COUNT(credits)-1; n++)
		{
			credits[n] = p;
			while (*p != '\r' && *p != '\n')
			{
				p++;
				if (--count == 0) break;
			}
			if (*p == '\r')
			{
				*p++ = 0;
				if (--count == 0) break;
			}
			*p++ = 0;
			if (--count == 0) break;
		}
		credits[++n] = NULL;

		credits_start_time = cls.realtime;
		return true;
	}
};
static creditsMenu_t creditsMenu;

static void Menu_Credits_f()
{
	creditsMenu.Push();
}

/*
=============================================================================

GAME MENU

=============================================================================
*/

struct gameMenu_t : menuFramework_t
{
	menuAction_t	easy_game_action;
	menuAction_t	medium_game_action;
	menuAction_t	hard_game_action;
	menuAction_t	load_game_action;
	menuAction_t	save_game_action;
	menuAction_t	credits_action;

	static void StartGame(const char *skill)
	{
		// disable updates and start the cinematic going
		cl.servercount = -1;
		M_ForceMenuOff();
		Cvar_ForceSet("skill", skill);
		Cvar_ForceSet("deathmatch", "0");
		Cvar_ForceSet("coop", "0");

		Cvar_SetInteger("gamerules", 0);		// ROGUE

		Cbuf_AddText("killserver\nwait\nnewgame\n");
		cls.key_dest = key_game;
	}

	static void EasyGameFunc(void *data)
	{
		StartGame("0");
	}

	static void MediumGameFunc(void *data)
	{
		StartGame("1");
	}

	static void HardGameFunc(void *data)
	{
		StartGame("2");
	}

	bool Init()
	{
		x = viddef.width / 2;
		nitems = 0;
		banner = "game";

		MENU_ACTION(easy_game_action,0,"easy",EasyGameFunc,NULL)
		easy_game_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&easy_game_action);
		MENU_ACTION(medium_game_action,10,"medium",MediumGameFunc,NULL)
		medium_game_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&medium_game_action);
		MENU_ACTION(hard_game_action,20,"hard",HardGameFunc,NULL)
		hard_game_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&hard_game_action);
		MENU_ACTION(load_game_action,40,"load game",NULL,"menu_loadgame")
		load_game_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&load_game_action);
		MENU_ACTION(save_game_action,50,"save game",NULL,"menu_savegame")
		save_game_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&save_game_action);
		MENU_ACTION(credits_action,60,"credits",NULL,"menu_credits")
		credits_action.flags  = QMF_LEFT_JUSTIFY;
		AddItem(&credits_action);

		Center();
		return true;
	}
};
static gameMenu_t gameMenu;

static void Menu_Game_f()
{
	gameMenu.Push();
}

/*
=============================================================================

LOADGAME/SAVEGAME MENU

=============================================================================
*/

#define	MAX_SAVEGAMES	15

struct savegameMenu_t : menuFramework_t
{
	static bool saveMenu;
	menuAction_t actions[MAX_SAVEGAMES];

	char	m_savestrings[MAX_SAVEGAMES][32+1];	// reserve 1 byte for 0 (for overflowed names)
	static bool m_savevalid[MAX_SAVEGAMES];
	bool	m_shotvalid[MAX_SAVEGAMES];

	void Create_Savestrings()
	{
		memset(m_savevalid, 0, sizeof(m_savevalid));
		memset(m_shotvalid, 0, sizeof(m_shotvalid));
		for (int i = 0; i < MAX_SAVEGAMES; i++)
		{
			CFile *File = GFileSystem->OpenFile(va("./%s/" SAVEGAME_DIRECTORY "/save%d/server." SAVEGAME_VARS_EXTENSION, FS_Gamedir(), i));
			if (!File)
				strcpy(m_savestrings[i], "<EMPTY>");
			else
			{
				File->Read(m_savestrings[i], sizeof(m_savestrings[i])-1);
				delete File;
				m_savevalid[i] = true;
				if (CBasicImage *img = RE_RegisterPic(va(SAVEGAME_DIRECTORY "/save%d/shot", i)))
				{
					m_shotvalid[i] = true;
					// force renderer to refresh image
					img->Reload();
				}
			}
		}
	}

	void DrawSavegameShot(int index, int y)
	{
		int		x, w, h;

		if (saveMenu) index++;
		if (viddef.width >= 320)
		{
			x = viddef.width / 2 + 20;
			w = viddef.width / 4 + 20;
			h = w * 3 / 4;
		}
		else
		{
			x = viddef.width / 10;
			w = viddef.width * 8 / 10;
			h = w * 3 / 4;
			y = (viddef.height - h) / 2;
		}
		RE_Fill(x - 3, y - 3, w + 6, h + 6, RGB(0,0,0));

		if (!m_shotvalid[index]) return;
		RE_DrawDetailedPic(x, y, w, h, va(SAVEGAME_DIRECTORY "/save%d/shot", index));
	}

	static void LoadGameCallback(void *self)
	{
		menuAction_t *a = (menuAction_t *) self;

		if (m_savevalid[a->localData])
			Cbuf_AddText(va("load save%d\n",  a->localData));
		M_ForceMenuOff();
	}

	static void SaveGameCallback(void *self)
	{
		menuAction_t *a = (menuAction_t *) self;

		Cbuf_AddText(va("save save%d\n", a->localData));
		M_ForceMenuOff();
	}

	bool Init()
	{
		if (saveMenu && !Com_ServerState()) return false;	// not playing a game

		x = viddef.width / 2 - 120 - (viddef.width - 320) / 6;
		y = viddef.height / 2 - 58;
		nitems = 0;
		banner = saveMenu ? "save_game" : "load_game";

		Create_Savestrings();

		int d = 0;
		int i = 0;
		if (saveMenu) d++;			// don't include autosave slot
		for ( ; d < MAX_SAVEGAMES; d++, i++)
		{
			actions[i].name		= m_savestrings[d];
			actions[i].flags	= QMF_LEFT_JUSTIFY;
			actions[i].localData = d;
			actions[i].callback	= saveMenu ? SaveGameCallback : LoadGameCallback;

			actions[i].x = 0;
			actions[i].y = i * 10;
			if (i > 0 && !saveMenu)	// separate from autosave
				actions[i].y += 10;

			actions[i].type = MTYPE_ACTION;
//			if (!m_savevalid[i] && !saveMenu) -- incorrect drawing
//				actions[i].type = MTYPE_SEPARATOR;
			AddItem(&actions[i]);
		}
		return true;
	}

	void Draw()
	{
		menuFramework_t::Draw();
		DrawSavegameShot(cursor, y);
	}
};
static savegameMenu_t savegameMenu;

// static members
bool savegameMenu_t::saveMenu;
bool savegameMenu_t::m_savevalid[MAX_SAVEGAMES];

static void Menu_LoadGame_f()
{
	if (savegameMenu.saveMenu) savegameMenu.cursor++;	// save->load
	savegameMenu.saveMenu = false;
	savegameMenu.Push();
}

static void Menu_SaveGame_f()
{
	if (!savegameMenu.saveMenu) savegameMenu.cursor--;	// load->save
	savegameMenu.saveMenu = true;
	savegameMenu.Push();
}


/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/

struct joinserverMenu_t : menuFramework_t
{
#define MAX_LOCAL_SERVERS 8
	menuSeparator_t	server_title;
	menuAction_t		search_action;
	menuAction_t		address_book_action;
	static menuAction_t	server_actions[MAX_LOCAL_SERVERS];

	static int		num_servers;
#define	NO_SERVER_STRING	"<no server>"

	// user readable information
	static char local_server_names[MAX_LOCAL_SERVERS][80];
	// network address
	static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

	static void JoinServerFunc(void *self)
	{
		int index = (menuAction_t *) self - server_actions;

		if (!stricmp(local_server_names[index], NO_SERVER_STRING))
			return;

		if (index >= num_servers) return;

		Cbuf_AddText(va("connect %s\n", NET_AdrToString(&local_server_netadr[index])));
		M_ForceMenuOff();
	}

	static void NullCursorDraw(void *self)
	{}

	static void SearchLocalGames()
	{
		num_servers = 0;
		for (int i = 0; i < MAX_LOCAL_SERVERS; i++)
			strcpy(local_server_names[i], NO_SERVER_STRING);

		M_DrawTextBox(8, 120 - 48, 36, 3);
		M_Print(16 + 16, 120 - 48 + 8,  "Searching for local servers, this");
		M_Print(16 + 16, 120 - 48 + 16, "could take up to a minute, so");
		M_Print(16 + 16, 120 - 48 + 24, "please be patient.");

		// the text box won't show up unless we do a buffer swap
		RE_EndFrame();

		// send out info packets
		CL_PingServers_f();
	}

	bool Init()
	{
		x = viddef.width / 2 - 120;
		nitems = 0;
		banner = "join_server";

		server_title.type = MTYPE_SEPARATOR;
		server_title.name = "connect to...";
		server_title.x    = 80;
		server_title.y	  = 0;
		AddItem(&server_title);

		int y = 0;
		for (int i = 0; i < MAX_LOCAL_SERVERS; i++)
		{
			strcpy(local_server_names[i], NO_SERVER_STRING);
			MENU_ACTION(server_actions[i],y+=10,local_server_names[i],JoinServerFunc,NULL)
			server_actions[i].flags = QMF_LEFT_JUSTIFY;
			server_actions[i].statusbar = "press ENTER to connect";
			AddItem(&server_actions[i]);
		}

		MENU_ACTION(address_book_action,y+=20,"address book",NULL,"menu_addressbook")
		address_book_action.flags = QMF_LEFT_JUSTIFY;
		AddItem(&address_book_action);

		MENU_ACTION(search_action,y+=10,"search for servers",SearchLocalGames,NULL)
		search_action.flags = QMF_LEFT_JUSTIFY;
		search_action.statusbar = "search for servers";
		AddItem(&search_action);

		Center();
		SearchLocalGames();
		return true;
	}
};
static joinserverMenu_t joinserverMenu;

static void Menu_JoinServer_f()
{
	joinserverMenu.Push();
}

// static members
menuAction_t	joinserverMenu_t::server_actions[MAX_LOCAL_SERVERS];
int	joinserverMenu_t::num_servers;
char joinserverMenu_t::local_server_names[MAX_LOCAL_SERVERS][80];
netadr_t joinserverMenu_t::local_server_netadr[MAX_LOCAL_SERVERS];


void M_AddToServerList(netadr_t adr, const char *info)
{
	if (joinserverMenu.num_servers == MAX_LOCAL_SERVERS) return;

	while (*info == ' ') info++;

	// ignore if duplicated
	for (int i = 0; i < joinserverMenu.num_servers; i++)
		if (!strcmp(info, joinserverMenu.local_server_names[i]))
			return;

	joinserverMenu.local_server_netadr[joinserverMenu.num_servers] = adr;
	appStrncpyz(joinserverMenu.local_server_names[joinserverMenu.num_servers], info, sizeof(joinserverMenu_t::local_server_names[0]));
	joinserverMenu.num_servers++;
}


/*
=============================================================================

START SERVER MENU

=============================================================================
*/

struct startserverMenu_t : menuFramework_t
{
	static TList<CStringItem> mapNames;
	static CMemoryChain *mapNamesChain;
	static int	numMaps;

	menuAction_t	start_action;
	static menuAction_t	dmoptions_action;
	menuAction_t	dmbrowse_action;
	static menuField_t	timelimit_field;
	static menuField_t	fraglimit_field;
	static menuField_t	maxclients_field;
	static menuField_t	hostname_field;
	static menuList2_t	startmap_list;
	static menuList_t	rules_box;

	static void DMOptionsFunc(void *self)
	{
		if (rules_box.curvalue == 1) return;		// coop game
		Menu_DMOptions_f();
	}

	static void RulesChangeFunc(void *self)
	{
		if (rules_box.curvalue == 0)				// deathmatch
		{
			maxclients_field.statusbar = NULL;
			dmoptions_action.statusbar = NULL;
		}
		else if (rules_box.curvalue == 1)			// coop
		{
			maxclients_field.statusbar = "4 maximum for cooperative";
			if (atoi(maxclients_field.buffer) > 4)
				strcpy(maxclients_field.buffer, "4" );
			dmoptions_action.statusbar = "N/A for cooperative";
		}
		else if (!stricmp(FS_Gamedir(), "rogue"))
		{
			if (rules_box.curvalue == 2)			// tag
			{
				maxclients_field.statusbar = NULL;
				dmoptions_action.statusbar = NULL;
			}
/*
			else if (rules_box.curvalue == 3)		// deathball
			{
				maxclients_field.statusbar = NULL;
				dmoptions_action.statusbar = NULL;
			}
*/
		}
	}

	static void StartServerFunc(const char *map)
	{
		int		v;

		//?? low limit is 0, upper limit - unlimited ?
		v = atoi(maxclients_field.buffer);
		Cvar_SetInteger("maxclients", max(v, 0));

		v = atoi(timelimit_field.buffer);
		Cvar_SetInteger("timelimit", max(v, 0));

		v = atoi(fraglimit_field.buffer);
		Cvar_SetInteger("fraglimit", max(v, 0));

//		Cvar_SetValue("deathmatch", !rules_box.curvalue);
//		Cvar_SetValue("coop", rules_box.curvalue);

		if (!stricmp(FS_Gamedir(), "rogue") && rules_box.curvalue >= 2)
		{
			Cvar_SetInteger("deathmatch", 1);		// deathmatch is always true for rogue games, right?
			Cvar_SetInteger("coop", 0);			// FIXME - this might need to depend on which game we're running
			Cvar_SetInteger("gamerules", rules_box.curvalue);
		}
		else
		{
			Cvar_SetInteger("deathmatch", !rules_box.curvalue);
			Cvar_SetInteger("coop", rules_box.curvalue);
			Cvar_SetInteger("gamerules", 0);
		}

		const char *spot = NULL;
		if (rules_box.curvalue == 1)		// ROGUE
		{
			if (!stricmp(map, "bunk1")  ||
				!stricmp(map, "mintro") ||
				!stricmp(map, "fact1"))
				spot = "start";
			else if (!stricmp(map, "power1"))
				spot = "pstart";
			else if (!stricmp(map, "biggun"))
				spot = "bstart";
			else if (!stricmp(map, "hangar1") || !stricmp(map, "city1"))
				spot = "unitstart";
			else if (!stricmp(map, "boss1"))
				spot = "bosstart";
		}

		if (spot)
		{
			if (Com_ServerState()) Cbuf_AddText("disconnect\n");
			Cbuf_AddText(va("gamemap \"*%s$%s\"\n", map, spot));
		}
		else
			Cbuf_AddText(va("map %s\n", map));

		if (mapNamesChain)
		{
			delete mapNamesChain;
			mapNamesChain = NULL;
		}
		M_ForceMenuOff();
	}

	static void StartServerActionFunc(void *self)
	{
		CStringItem *item = &mapNames[startmap_list.curvalue];
		if (item) StartServerFunc(strchr(item->name, '\n') + 1);
	}

	bool Init()
	{
		static const char *dm_coop_names[] =
		{
			"deathmatch",
			"cooperative",
			NULL
		};
		static const char *dm_coop_names_rogue[] =
		{
			"deathmatch",
			"cooperative",
			"tag",
//			"deathball",
			NULL
		};

		char	*buffer;
		// load the list of map names
		TString<64> Filename; Filename.sprintf("./%s/maps.lst", FS_Gamedir());
		CFile *File = GFileSystem->OpenFile(Filename, FS_OS);	// try OS file (quake fs OS file have lower priority, than pak)
		if (!File) File = GFileSystem->OpenFile(Filename);		// open any file from this mod dir
		if (!File) File = GFileSystem->OpenFile("maps.lst");	// open any file from any dir

		if (!File)
		{
			appWPrintf("Couldn't find maps.lst\n");
			buffer = NULL;
		}
		else
		{
			unsigned len = File->GetSize();
			buffer = new char [len+1];
			File->Read(buffer, len);
			delete File;
		}

		numMaps = 0;
		if (buffer)
		{
			mapNamesChain = new CMemoryChain;
			mapNames.Reset();

			const char *s = buffer;
			while (s)
			{
				char	name[256];

				appStrncpylwr(name, COM_Parse(s, true), sizeof(name));
				if (!name[0]) break;
				/*CStringItem *item =*/ mapNames.CreateAndInsert(va("%s\n%s", COM_Parse(s, false), name), mapNamesChain);

				numMaps++;
			}

			delete buffer;
		}

		// initialize the menu stuff
		x = viddef.width / 2;
		nitems = 0;

		startmap_list.type = MTYPE_SPINCONTROL2;
		startmap_list.x	= 0;
		startmap_list.y	= 0;
		startmap_list.name = "initial map";
		startmap_list.itemnames = mapNames.First();
		AddItem(&startmap_list);

		rules_box.type = MTYPE_SPINCONTROL;
		rules_box.x	= 0;
		rules_box.y	= 20;
		rules_box.name = "rules";
		if (!stricmp(FS_Gamedir(), "rogue"))
			rules_box.itemnames = dm_coop_names_rogue;
		else
			rules_box.itemnames = dm_coop_names;
		if (Cvar_VariableInt("coop"))
			rules_box.curvalue = 1;
		else
			rules_box.curvalue = 0;
		rules_box.callback = RulesChangeFunc;
		AddItem(&rules_box);

		timelimit_field.type = MTYPE_FIELD;
		timelimit_field.name = "time limit";
		timelimit_field.flags = QMF_NUMBERSONLY;
		timelimit_field.x	= 0;
		timelimit_field.y	= 36;
		timelimit_field.statusbar = "0 = no limit";
		timelimit_field.length = 3;
		timelimit_field.visible_length = 3;
		strcpy(timelimit_field.buffer, Cvar_VariableString("timelimit"));
		AddItem(&timelimit_field);

		fraglimit_field.type = MTYPE_FIELD;
		fraglimit_field.name = "frag limit";
		fraglimit_field.flags = QMF_NUMBERSONLY;
		fraglimit_field.x	= 0;
		fraglimit_field.y	= 54;
		fraglimit_field.statusbar = "0 = no limit";
		fraglimit_field.length = 3;
		fraglimit_field.visible_length = 3;
		strcpy(fraglimit_field.buffer, Cvar_VariableString("fraglimit"));
		AddItem(&fraglimit_field);

		/*
		** maxclients determines the maximum number of players that can join
		** the game.  If maxclients is only "1" then we should default the menu
		** option to 8 players, otherwise use whatever its current value is.
		** Clamping will be done when the server is actually started.
		*/
		maxclients_field.type = MTYPE_FIELD;
		maxclients_field.name = "max players";
		maxclients_field.flags = QMF_NUMBERSONLY;
		maxclients_field.x	= 0;
		maxclients_field.y	= 72;
		maxclients_field.statusbar = NULL;
		maxclients_field.length = 3;
		maxclients_field.visible_length = 3;
		if (Cvar_VariableInt("maxclients") == 1)
			strcpy(maxclients_field.buffer, "8");
		else
			strcpy(maxclients_field.buffer, Cvar_VariableString("maxclients"));
		AddItem(&maxclients_field);

		hostname_field.type = MTYPE_FIELD;
		hostname_field.name = "hostname";
		hostname_field.flags = 0;
		hostname_field.x	= 0;
		hostname_field.y	= 90;
		hostname_field.statusbar = NULL;
		hostname_field.length = 12;
		hostname_field.visible_length = 12;
		strcpy(hostname_field.buffer, Cvar_VariableString("hostname"));
		AddItem(&hostname_field);

		MENU_ACTION(dmoptions_action,108,"deathmatch flags",DMOptionsFunc,NULL)
		dmoptions_action.flags= QMF_LEFT_JUSTIFY;
		dmoptions_action.x	= 24;
		dmoptions_action.statusbar = NULL;
		AddItem(&dmoptions_action);

		MENU_ACTION(dmbrowse_action,118,"browse maps",NULL,"menu_dmbrowse")
		dmbrowse_action.flags = QMF_LEFT_JUSTIFY;
		dmbrowse_action.x = 24;
		dmbrowse_action.statusbar = NULL;
		AddItem(&dmbrowse_action);

		MENU_ACTION(start_action,138,"begin",StartServerActionFunc,NULL)
		start_action.flags= QMF_LEFT_JUSTIFY;
		start_action.x	= 24;
		AddItem(&start_action);

		Center();

		// call this now to set proper inital state
		RulesChangeFunc(NULL);
		return true;
	}

	const char * KeyDown(int key)
	{
		if (key == K_ESCAPE || key == K_MOUSE2)
		{
			if (mapNamesChain)
			{
				delete mapNamesChain;
				mapNamesChain = NULL;
			}
			numMaps = 0;
		}

		return menuFramework_t::KeyDown(key);
	}
};
static startserverMenu_t startserverMenu;

TList<CStringItem> startserverMenu_t::mapNames;
CMemoryChain *startserverMenu_t::mapNamesChain;
int	startserverMenu_t::numMaps;
menuAction_t startserverMenu_t::dmoptions_action;
menuField_t	startserverMenu_t::timelimit_field;
menuField_t	startserverMenu_t::fraglimit_field;
menuField_t	startserverMenu_t::maxclients_field;
menuField_t	startserverMenu_t::hostname_field;
menuList2_t	startserverMenu_t::startmap_list;
menuList_t	startserverMenu_t::rules_box;


static void Menu_StartServer_f()
{
	startserverMenu.Push();
}

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/

struct dmoptionsMenu_t : menuFramework_t
{
	static TString<128>	DMoptionsStatusbar;

	static menuList_t	friendlyfire_box;
	static menuList_t	falls_box;
	static menuList_t	weapons_stay_box;
	static menuList_t	instant_powerups_box;
	static menuList_t	powerups_box;
	static menuList_t	health_box;
	static menuList_t	spawn_farthest_box;
	static menuList_t	teamplay_box;
	static menuList_t	samelevel_box;
	static menuList_t	force_respawn_box;
	static menuList_t	armor_box;
	static menuList_t	allow_exit_box;
	static menuList_t	infinite_ammo_box;
	static menuList_t	fixed_fov_box;
	static menuList_t	quad_drop_box;

	//ROGUE
	static menuList_t	no_mines_box;
	static menuList_t	no_nukes_box;
	static menuList_t	stack_double_box;
	static menuList_t	no_spheres_box;

	static void DMFlagCallback(void *self)
	{
		menuList_t *f;
		int		flags, bit, nobit;

		f = (menuList_t *)self;
		flags = Cvar_VariableInt("dmflags");
		bit = nobit = 0;

		if (f == &friendlyfire_box)
			nobit = DF_NO_FRIENDLY_FIRE;
		else if (f == &falls_box)
			nobit = DF_NO_FALLING;
		else if (f == &weapons_stay_box)
			bit = DF_WEAPONS_STAY;
		else if (f == &instant_powerups_box)
			bit = DF_INSTANT_ITEMS;
		else if (f == &allow_exit_box)
			bit = DF_ALLOW_EXIT;
		else if (f == &powerups_box)
			nobit = DF_NO_ITEMS;
		else if (f == &health_box)
			nobit = DF_NO_HEALTH;
		else if (f == &spawn_farthest_box)
			bit = DF_SPAWN_FARTHEST;
		else if (f == &teamplay_box)
		{
			if (f->curvalue == 1)
			{
				flags = flags | DF_SKINTEAMS & ~DF_MODELTEAMS;
			}
			else if (f->curvalue == 2)
			{
				flags = flags | DF_MODELTEAMS & ~DF_SKINTEAMS;
			}
			else
			{
				flags &= ~(DF_MODELTEAMS|DF_SKINTEAMS);
			}
		}
		else if (f == &samelevel_box)
			bit = DF_SAME_LEVEL;
		else if (f == &force_respawn_box)
			bit = DF_FORCE_RESPAWN;
		else if (f == &armor_box)
			nobit = DF_NO_ARMOR;
		else if (f == &infinite_ammo_box)
			bit = DF_INFINITE_AMMO;
		else if (f == &fixed_fov_box)
			bit = DF_FIXED_FOV;
		else if (f == &quad_drop_box)
			bit = DF_QUAD_DROP;
		else if (!stricmp(FS_Gamedir(), "rogue"))
		{
			if (f == &no_mines_box)
				bit = DF_NO_MINES;
			else if (f == &no_nukes_box)
				bit = DF_NO_NUKES;
			else if (f == &stack_double_box)
				bit = DF_NO_STACK_DOUBLE;
			else if (f == &no_spheres_box)
				bit = DF_NO_SPHERES;
		}

		if (f)
		{
			if (f->curvalue == 0)
				flags = flags & ~bit | nobit;
			else
				flags = flags & ~nobit | bit;
		}

		Cvar_SetInteger("dmflags", flags);

		DMoptionsStatusbar.sprintf("dmflags = %d", flags);
	}

	bool Init()
	{
		static const char *teamplay_names[] =
		{
			"disabled", "by skin", "by model", 0
		};
		int dmflags = Cvar_VariableInt("dmflags");

		x = viddef.width / 2;
		nitems = 0;

		int y = 0;
		MENU_SPIN(falls_box,y,"falling damage",DMFlagCallback,yesno_names)
		falls_box.curvalue = (dmflags & DF_NO_FALLING) == 0;
		AddItem(&falls_box);
		MENU_SPIN(weapons_stay_box,y+=10,"weapons stay",DMFlagCallback,yesno_names)
		weapons_stay_box.curvalue = (dmflags & DF_WEAPONS_STAY) != 0;
		AddItem(&weapons_stay_box);
		MENU_SPIN(instant_powerups_box,y+=10,"instant powerups",DMFlagCallback,yesno_names)
		instant_powerups_box.curvalue = (dmflags & DF_INSTANT_ITEMS) != 0;
		AddItem(&instant_powerups_box);
		MENU_SPIN(powerups_box,y+=10,"allow powerups",DMFlagCallback,yesno_names)
		powerups_box.curvalue = (dmflags & DF_NO_ITEMS) == 0;
		AddItem(&powerups_box);
		MENU_SPIN(health_box,y+=10,"allow health",DMFlagCallback,yesno_names)
		health_box.curvalue = (dmflags & DF_NO_HEALTH) == 0;
		AddItem(&health_box);
		MENU_SPIN(armor_box,y+=10,"allow armor",DMFlagCallback,yesno_names)
		armor_box.curvalue = (dmflags & DF_NO_ARMOR) == 0;
		AddItem(&armor_box);
		MENU_SPIN(spawn_farthest_box,y+=10,"spawn farthest",DMFlagCallback,yesno_names)
		spawn_farthest_box.curvalue = (dmflags & DF_SPAWN_FARTHEST) != 0;
		AddItem(&spawn_farthest_box);
		MENU_SPIN(samelevel_box,y+=10,"same map",DMFlagCallback,yesno_names)
		samelevel_box.curvalue = (dmflags & DF_SAME_LEVEL) != 0;
		AddItem(&samelevel_box);
		MENU_SPIN(force_respawn_box,y+=10,"force respawn",DMFlagCallback,yesno_names)
		force_respawn_box.curvalue = (dmflags & DF_FORCE_RESPAWN) != 0;
		AddItem(&force_respawn_box);
		MENU_SPIN(teamplay_box,y+=10,"teamplay",DMFlagCallback,yesno_names)
		teamplay_box.itemnames = teamplay_names;
		AddItem(&teamplay_box);
		MENU_SPIN(allow_exit_box,y+=10,"allow exit",DMFlagCallback,yesno_names)
		allow_exit_box.curvalue = (dmflags & DF_ALLOW_EXIT) != 0;
		AddItem(&allow_exit_box);
		MENU_SPIN(infinite_ammo_box,y+=10,"infinite ammo",DMFlagCallback,yesno_names)
		infinite_ammo_box.curvalue = (dmflags & DF_INFINITE_AMMO) != 0;
		AddItem(&infinite_ammo_box);
		MENU_SPIN(fixed_fov_box,y+=10,"fixed FOV",DMFlagCallback,yesno_names)
		fixed_fov_box.curvalue = (dmflags & DF_FIXED_FOV) != 0;
		AddItem(&fixed_fov_box);
		MENU_SPIN(quad_drop_box,y+=10,"quad drop",DMFlagCallback,yesno_names)
		quad_drop_box.curvalue = (dmflags & DF_QUAD_DROP) != 0;
		AddItem(&quad_drop_box);
		MENU_SPIN(friendlyfire_box,y+=10,"friendly fire",DMFlagCallback,yesno_names)
		friendlyfire_box.curvalue = (dmflags & DF_NO_FRIENDLY_FIRE) == 0;
		AddItem(&friendlyfire_box);

		if (!stricmp(FS_Gamedir(), "rogue"))
		{
			MENU_SPIN(no_mines_box,y+=10,"remove mines",DMFlagCallback,yesno_names)
			no_mines_box.curvalue = (dmflags & DF_NO_MINES) != 0;
			AddItem(&no_mines_box);
			MENU_SPIN(no_nukes_box,y+=10,"remove nukes",DMFlagCallback,yesno_names)
			no_nukes_box.curvalue = (dmflags & DF_NO_NUKES) != 0;
			AddItem(&no_nukes_box);
			MENU_SPIN(stack_double_box,y+=10,"2x/4x stacking off",DMFlagCallback,yesno_names)
			stack_double_box.curvalue = (dmflags & DF_NO_STACK_DOUBLE) != 0;
			AddItem(&stack_double_box);
			MENU_SPIN(no_spheres_box,y+=10,"remove spheres",DMFlagCallback,yesno_names)
			no_spheres_box.curvalue = (dmflags & DF_NO_SPHERES) != 0;
			AddItem(&no_spheres_box);
		}

		Center();

		// set the original dmflags statusbar
		DMFlagCallback(0);
		SetStatusBar(DMoptionsStatusbar);
		return true;
	}
};
static dmoptionsMenu_t dmoptionsMenu;

// static members
TString<128> dmoptionsMenu_t::DMoptionsStatusbar;
menuList_t	dmoptionsMenu_t::friendlyfire_box;
menuList_t	dmoptionsMenu_t::falls_box;
menuList_t	dmoptionsMenu_t::weapons_stay_box;
menuList_t	dmoptionsMenu_t::instant_powerups_box;
menuList_t	dmoptionsMenu_t::powerups_box;
menuList_t	dmoptionsMenu_t::health_box;
menuList_t	dmoptionsMenu_t::spawn_farthest_box;
menuList_t	dmoptionsMenu_t::teamplay_box;
menuList_t	dmoptionsMenu_t::samelevel_box;
menuList_t	dmoptionsMenu_t::force_respawn_box;
menuList_t	dmoptionsMenu_t::armor_box;
menuList_t	dmoptionsMenu_t::allow_exit_box;
menuList_t	dmoptionsMenu_t::infinite_ammo_box;
menuList_t	dmoptionsMenu_t::fixed_fov_box;
menuList_t	dmoptionsMenu_t::quad_drop_box;
menuList_t	dmoptionsMenu_t::no_mines_box;
menuList_t	dmoptionsMenu_t::no_nukes_box;
menuList_t	dmoptionsMenu_t::stack_double_box;
menuList_t	dmoptionsMenu_t::no_spheres_box;


static void Menu_DMOptions_f()
{
	dmoptionsMenu.Push();
}

/*
=============================================================================

DOWNLOADOPTIONS MENU

=============================================================================
*/

struct downloadOptionsMenu_t : menuFramework_t
{
	menuSeparator_t	download_title;
	static menuList_t	allow_download_box;
	static menuList_t	allow_download_maps_box;
	static menuList_t	allow_download_models_box;
	static menuList_t	allow_download_players_box;
	static menuList_t	allow_download_sounds_box;

	static void DownloadCallback(void *self)
	{
		const char *name;
		menuList_t *f = (menuList_t *) self;

		if (f == &allow_download_box)
			name = "allow_download";
		else if (f == &allow_download_maps_box)
			name = "allow_download_maps";
		else if (f == &allow_download_models_box)
			name = "allow_download_models";
		else if (f == &allow_download_players_box)
			name = "allow_download_players";
		else if (f == &allow_download_sounds_box)
			name = "allow_download_sounds";
		else
			return;
		Cvar_SetInteger(name, f->curvalue);
	}

	bool Init()
	{
		x = viddef.width / 2;
		nitems = 0;

		int y = 0;
		download_title.type = MTYPE_SEPARATOR;
		download_title.name = "Download Options";
		download_title.x	= 48;
		download_title.y	= y;
		AddItem( &download_title);

		MENU_SPIN(allow_download_box, y+=20,"allow downloading", DownloadCallback, yesno_names);
		allow_download_box.curvalue = Cvar_VariableInt("allow_download");
		AddItem( &allow_download_box);
		MENU_SPIN(allow_download_maps_box, y+=20,"maps", DownloadCallback, yesno_names);
		allow_download_maps_box.curvalue = Cvar_VariableInt("allow_download_maps");
		AddItem( &allow_download_maps_box);
		MENU_SPIN(allow_download_players_box, y+=20,"player models/skins",DownloadCallback, yesno_names);
		allow_download_players_box.curvalue = Cvar_VariableInt("allow_download_players");
		AddItem( &allow_download_players_box);
		MENU_SPIN(allow_download_models_box, y+=20,"models", DownloadCallback, yesno_names)
		allow_download_models_box.curvalue = Cvar_VariableInt("allow_download_models");
		AddItem( &allow_download_models_box);
		MENU_SPIN(allow_download_sounds_box, y+=20,"sounds", DownloadCallback, yesno_names)
		allow_download_sounds_box.curvalue = Cvar_VariableInt("allow_download_sounds");
		AddItem( &allow_download_sounds_box);

		Center();
		return true;
	}
};
static downloadOptionsMenu_t downloadOptionsMenu;

// static members
menuList_t	downloadOptionsMenu_t::allow_download_box;
menuList_t	downloadOptionsMenu_t::allow_download_maps_box;
menuList_t	downloadOptionsMenu_t::allow_download_models_box;
menuList_t	downloadOptionsMenu_t::allow_download_players_box;
menuList_t	downloadOptionsMenu_t::allow_download_sounds_box;


static void Menu_DownloadOptions_f()
{
	downloadOptionsMenu.Push();
}

/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/

struct addressBookMenu_t : menuFramework_t
{
	menuField_t	addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

	bool Init()
	{
		x = viddef.width / 2 - 142;
		y = viddef.height / 2 - 58;
		nitems = 0;
		banner = "addressbook";

		for (int i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++)
		{
			addressbook_fields[i].type = MTYPE_FIELD;
			addressbook_fields[i].name = 0;
			addressbook_fields[i].callback = 0;
			addressbook_fields[i].x	= 0;
			addressbook_fields[i].y	= i * 18 + 0;
			addressbook_fields[i].localData = i;
			addressbook_fields[i].cursor = 0;
			addressbook_fields[i].length = 60;
			addressbook_fields[i].visible_length = 30;

			strcpy(addressbook_fields[i].buffer, Cvar_VariableString(va("adr%d", i)));

			AddItem(&addressbook_fields[i]);
		}
		return true;
	}

	const char * KeyDown(int key)
	{
		if (key == K_ESCAPE || key == K_MOUSE2)
			for (int i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++)
				Cvar_Set(va("adr%d", i), addressbook_fields[i].buffer);
		return menuFramework_t::KeyDown(key);
	}
};
static addressBookMenu_t addressBookMenu;

static void Menu_AddressBook_f()
{
	addressBookMenu.Push();
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/

#define MAX_DISPLAYNAME 16

static const int rate_tbl[] = {
	2500, 3200, 5000, 10000, 25000, 0
};

static const char *rate_names[] = {
	"28.8 Modem", "33.6 Modem", "Single ISDN", "Dual ISDN/Cable", "T1/LAN", "User defined", NULL
};


struct playerConfigMenu_t : menuFramework_t
{
	static menuField_t	name_field;
	static menuList2_t	model_box;
	static menuList2_t	skin_box;
	static menuList_t	rtype_box;
	static menuList_t	rcolor_box;
	static menuList_t	handedness_box;
	static menuList_t	rate_box;
	menuSeparator_t	skin_title;
	menuSeparator_t	model_title;
	menuSeparator_t	rtype_title;
	menuSeparator_t	rcolor_title;
	menuSeparator_t	hand_title;
	menuSeparator_t	rate_title;
	menuAction_t	download_action;

	static unsigned modelChangeTime;
	static clientInfo_t ci;
	static centity_t cent;

	static void HandednessCallback(void *)
	{
		Cvar_SetInteger("hand", handedness_box.curvalue);
	}

	static void RTypeCallback(void *)
	{
		Cvar_SetInteger("railtype", rtype_box.curvalue);
	}

	static void RColorCallback(void *)
	{
		Cvar_SetInteger("railcolor", rcolor_box.curvalue);
	}

	static void RateCallback(void *)
	{
		if (rate_box.curvalue != ARRAY_COUNT(rate_tbl) - 1)
			Cvar_SetInteger("rate", rate_tbl[rate_box.curvalue]);
	}

	static void ModelCallback(void *)
	{
		playerModelInfo_t *info = &pmiList[model_box.curvalue];
		skin_box.itemnames = info->skins.First();
		skin_box.curvalue  = 0;
		modelChangeTime = appMilliseconds();
		ci.isValidModel = false;
	}

	static void SkinCallback(void *)
	{
		ci.isValidModel = false;		// reload clientInfo_t for new skin/icon
	}

	bool Init()
	{
		extern cvar_t *name, *skin;
		char	currentModel[MAX_QPATH], currentSkin[MAX_QPATH], *path;

		static const char *handedness[] = {"right", "left", "center", NULL};
		static const char *colorNames[] = {"default", "red", "green", "yellow", "blue", "magenta", "cyan", "white", NULL};
		static const char *railTypes[]  = {"original", "spiral", "rings", "beam", NULL};

		memset(&ci, 0, sizeof(ci));
		memset(&cent, 0, sizeof(cent));

		if (!ScanPlayerModels())
		{
			multiplayerMenu.SetStatusBar(S_YELLOW"No valid player models found");
			return false;
		}

		hand->Clamp(0, 2);

		strcpy(currentModel, skin->string);
		if (path = strchr(currentModel, '/'))
		{
			strcpy(currentSkin, path + 1);
			path[0] = 0;
		}
		else
		{	// bad skin info - set default
			strcpy(currentModel, "male");
			strcpy(currentSkin, "grunt");
		}

		// find current model/skin index
		playerModelInfo_t *info;
		int currentModelIndex, currentSkinIndex;
		for (info = pmiList.First(), currentModelIndex = 0; info; info = pmiList.Next(info), currentModelIndex++)
			if (!stricmp(info->name, currentModel))
			{	// we have found model - find skin
				currentSkinIndex = info->skins.IndexOf(currentSkin);
				if (currentSkinIndex < 0)	// not found
					currentSkinIndex = 0;
				break;
			}
		if (!info)
		{	// model not found
			info = pmiList.First();
			currentModelIndex = 0;
			currentSkinIndex = 0;
		}

		x = viddef.width * 65 / 320;
		y = viddef.height * 23 / 240;
		nitems = 0;

		name_field.type = MTYPE_FIELD;
		name_field.name = "name";
		name_field.x	= 0;
		name_field.y	= viddef.height * 20 / 240 - 20;
		name_field.length		= 20;
		name_field.visible_length = 20;
		strcpy(name_field.buffer, name->string);
		name_field.cursor = strlen(name->string);
		AddItem(&name_field);

		int y = viddef.height * 68 / 240;

		// model
		model_title.type = MTYPE_SEPARATOR;
		model_title.name = "model";
		model_title.x	= -8;
		model_title.y	= y+=10;
		AddItem(&model_title);

		model_box.type = MTYPE_SPINCONTROL2;
		model_box.x	= -56;
		model_box.y	= y+=10;
		model_box.callback = ModelCallback;
		model_box.cursor_offset = -48;
		model_box.curvalue = currentModelIndex;
		model_box.itemnames = pmiList.First();
		AddItem(&model_box);

		// skin
		skin_title.type = MTYPE_SEPARATOR;
		skin_title.name = "skin";
		skin_title.x   = -16;
		skin_title.y	= y+=14;
		AddItem(&skin_title);

		skin_box.type	= MTYPE_SPINCONTROL2;
		skin_box.x		= -56;
		skin_box.y		= y+=10;
		skin_box.callback = SkinCallback;
		skin_box.cursor_offset = -48;
		skin_box.curvalue = currentSkinIndex;
		skin_box.itemnames = info->skins.First();
		AddItem(&skin_box);

		// rail type
		rtype_title.type = MTYPE_SEPARATOR;
		rtype_title.name = "rail type";
		rtype_title.x	= 24;
		rtype_title.y	= y+=14;
		AddItem(&rtype_title);

		rtype_box.type = MTYPE_SPINCONTROL;
		rtype_box.x = -56;
		rtype_box.y = y+=10;
		rtype_box.cursor_offset = -48;
		rtype_box.callback = RTypeCallback;
		rtype_box.curvalue = Cvar_VariableInt("railtype");
		rtype_box.itemnames = railTypes;
		AddItem(&rtype_box);

		// rail color
		rcolor_title.type = MTYPE_SEPARATOR;
		rcolor_title.name = "rail color";
		rcolor_title.x	= 32;
		rcolor_title.y	= y+=14;
		AddItem(&rcolor_title);

		rcolor_box.type = MTYPE_SPINCONTROL;
		rcolor_box.x = -56;
		rcolor_box.y = y+=10;
		rcolor_box.cursor_offset = -48;
		rcolor_box.callback = RColorCallback;
		rcolor_box.curvalue = Cvar_VariableInt("railcolor");
		rcolor_box.itemnames = colorNames;
		AddItem(&rcolor_box);

		// handedness
		hand_title.type = MTYPE_SEPARATOR;
		hand_title.name = "handedness";
		hand_title.x	= 32;
		hand_title.y	= y+=14;
		AddItem(&hand_title);

		handedness_box.type = MTYPE_SPINCONTROL;
		handedness_box.x = -56;
		handedness_box.y = y+=10;
		handedness_box.cursor_offset = -48;
		handedness_box.callback = HandednessCallback;
		handedness_box.curvalue = Cvar_VariableInt("hand");
		handedness_box.itemnames = handedness;
		AddItem(&handedness_box);

		// rate
		int i;
		for (i = 0; i < ARRAY_COUNT(rate_tbl) - 1; i++)
			if (Cvar_VariableInt("rate") == rate_tbl[i])
				break;

		rate_title.type = MTYPE_SEPARATOR;
		rate_title.name = "connect speed";
		rate_title.x	= 56;
		rate_title.y	= y+=14;
		AddItem(&rate_title);

		rate_box.type	= MTYPE_SPINCONTROL;
		rate_box.x		= -56;
		rate_box.y		= y+=10;
		rate_box.cursor_offset = -48;
		rate_box.callback = RateCallback;
		rate_box.curvalue = i;
		rate_box.itemnames = rate_names;
		AddItem(&rate_box);

		download_action.type = MTYPE_ACTION;
		download_action.name = "download options";
		download_action.flags= QMF_LEFT_JUSTIFY;
		download_action.x = -24;
		download_action.y = y+=14;
		download_action.command = "menu_downloadoptions";
		AddItem(&download_action);

		SetStatusBar(NULL);
		return true;
	}

// stand: 0-39; run: 40-45
#define FIRST_FRAME	0
#define LAST_FRAME	39

#define MODEL_DELAY	300

	void Draw()
	{
		static dlight_t dl[] = {
			{{30, 100, 100}, {1, 1, 1}, 350},
			{{90, -100, 10}, {0.4, 0.2, 0.2}, 200}
		};
//		sscanf(Cvar_VariableString("dl0"), "%f %f %f %f", VECTOR_ARG(&dl[0].origin), &dl[0].intensity);
//		sscanf(Cvar_VariableString("dl1"), "%f %f %f %f", VECTOR_ARG(&dl[1].origin), &dl[1].intensity);

		bool showModels = appMilliseconds() - modelChangeTime > MODEL_DELAY;

		playerModelInfo_t *model = &pmiList[model_box.curvalue];
		if (!model) return;
		CStringItem *skin = &model->skins[skin_box.curvalue];
		if (!skin) return;

		refdef_t refdef;
		memset(&refdef, 0, sizeof(refdef));
		refdef.x      = viddef.width / 2;
		refdef.y      = viddef.height * 6 / 25;
		refdef.width  = viddef.height * 16 / 30 + 16;
		refdef.height = viddef.height * 20 / 30;
		refdef.fov_x  = refdef.height * 55 / viddef.height;
		refdef.fov_y  = CalcFov(refdef.fov_x, refdef.width, refdef.height);
		refdef.time   = cls.realtime / 1000.0f;

		if (showModels && !ci.isValidModel)
		{
			// load new model
			CL_LoadClientinfo(ci, va("menu\\%s/%s", model->name, skin->name), false);
			ci.fixedAll = true;		// do not rotate model parts one around another
		}

		if (ci.isValidModel)
		{
			clEntityState_t st;
			st.SetAnim(LEGS_IDLE, TORSO_STAND);
			cent.prev    = st;
			cent.current = st;
			// setup base entity
			// note: we will setup both frame numbers(for q2 models) and animation type(for q3 models)
			entity_t base;
			memset(&base, 0, sizeof(base));
			base.pos.origin[0] = 90;
			base.pos.origin[2] = -6;
			base.frame = (cls.realtime + 99) / 100 % (LAST_FRAME-FIRST_FRAME+1) + FIRST_FRAME;
			base.oldframe = base.frame - 1;
			if (base.oldframe < FIRST_FRAME)
				base.oldframe = LAST_FRAME;
			base.backlerp = 1.0f - (cls.realtime % 100) / 100.0f;
			if (base.backlerp == 1.0f)
				base.backlerp = 0.0;
			base.angles[1] = cls.realtime / 20 % 360;
			base.pos.axis.FromEuler(base.angles);
			// create entities
			entity_t e[16];
			memset(&e, 0, sizeof(e));
			// setup for renderer
			int numEnts = ParsePlayerEntity(cent, ci, &st, base, ARRAY_ARG(e), 0);
//			refdef.zonebits     = NULL;
			refdef.num_entities = numEnts;
			refdef.entities     = e;
//			refdef.lightstyles  = NULL;
			refdef.dlights      = dl;
			refdef.num_dlights  = ARRAY_COUNT(dl);
		}
		refdef.rdflags = RDF_NOWORLDMODEL;

		menuFramework_t::Draw();
		RE_Fill(refdef.x-4, refdef.y-4, refdef.width+8, refdef.height+8, RGBA(0,0,0,0.6));
		RE_RenderFrame(&refdef);
		RE_DrawDetailedPic(x - 40, refdef.y, viddef.height * 32 / 240, viddef.height * 32 / 240, ci.IconName);
	}

	const char * KeyDown(int key)
	{
		if (key == K_ESCAPE || key == K_MOUSE2)
		{
			// check colorizing of player name
			int color = C_WHITE;
			const char *s = name_field.buffer;
			while (char c = *s++)
			{
				if (c == COLOR_ESCAPE)
				{
					int col = *s - '0';
					if (col >= 0 && col <= 7)
					{
						color = col;
						s++;
					}
				}
			}
			// final color must be white; "i" points to string end
			if (color != C_WHITE)
				Cvar_Set("name", va("%s"S_WHITE, name_field.buffer));
			else
				Cvar_Set("name", name_field.buffer);

			playerModelInfo_t *model = &pmiList[model_box.curvalue];
			CStringItem *skin = &model->skins[skin_box.curvalue];
			// set "skin" variable
			Cvar_Set("skin", va("%s/%s", model->name, skin->name));

			// free directory scan data
			FreePlayerModelsInfo();
		}
		return menuFramework_t::KeyDown(key);
	}
};
static playerConfigMenu_t playerConfigMenu;

// static members
menuField_t	playerConfigMenu_t::name_field;
menuList2_t	playerConfigMenu_t::model_box;
menuList2_t	playerConfigMenu_t::skin_box;
menuList_t	playerConfigMenu_t::rtype_box;
menuList_t	playerConfigMenu_t::rcolor_box;
menuList_t	playerConfigMenu_t::handedness_box;
menuList_t	playerConfigMenu_t::rate_box;
unsigned playerConfigMenu_t::modelChangeTime;
clientInfo_t playerConfigMenu_t::ci;
centity_t playerConfigMenu_t::cent;


static void Menu_PlayerConfig_f()
{
	playerConfigMenu.Push();
}


/*
=======================================================================

BROWSE FOR MAP

=======================================================================
*/

struct thumbParams_t
{
	unsigned short w, h;	// thumbnail sizes
	byte	cx, cy;			// number of thumbnail on screen
	short	x0, y0, dx, dy; // delta for coputing thumbnail coordinates on screen (Xo+Ix*Dx, Yo+Iy*Dy)
	int		count;			// number of maps
	int		top;
	int		current;
};

struct thumbLayout_t
{
	unsigned short width;	// max screen width
	unsigned short w, h;	// thumb sizes
};


static const thumbLayout_t thumbLayout[] =
{
	{0,    128,  96},
	{640,  192, 144},
	{1024, 256, 192},
	{1600, 320, 240},
};


struct dmbrowseMenu_t : menuFramework_t
{
#define THUMBNAIL_BORDER	4
#define THUMBNAIL_TEXT		(CHAR_HEIGHT + 2)
#define MAX_BROWSE_MAPS		1024
	static const char	*browse_map_names[MAX_BROWSE_MAPS];
	static CFileList	*browse_list;
	static thumbParams_t thumbs;

	/*
	 * Read screenshots list and refine it with maps that doesn't exists. Do not bother
	 * about freeing screenshots from imagelist - this will be done automatically when a
	 * new level started (NOT IMPLEMENTED NOW !!) (registration sequence is less than new level sequence by 1 ...)
	 */
	bool Init()
	{
		// free previous levelshots list
		if (browse_list)
		{
			delete browse_list;
			browse_list = NULL;
		}

		int oldcount = thumbs.count;
		thumbs.count = 0;

		int i;

		for (i = 0; i < ARRAY_COUNT(thumbLayout); i++)
		{
			if (viddef.width < thumbLayout[i].width)
				break;
			thumbs.w = thumbLayout[i].w;
			thumbs.h = thumbLayout[i].h;
		}

		thumbs.dx = thumbs.w + THUMBNAIL_BORDER * 4;
		thumbs.dy = thumbs.h + THUMBNAIL_BORDER * 4 + THUMBNAIL_TEXT;
		// compute suitable thumbnail grid size
		for (thumbs.cx = 5; thumbs.cx >= 2; thumbs.cx--)
		{
			thumbs.x0 = (viddef.width - thumbs.cx * thumbs.dx + thumbs.dx - thumbs.w) / 2;	// (x-cx*dx)/2 + (dx-w)/2
			if (thumbs.x0 >= 0) break;		// size is ok
		}
		for (thumbs.cy = 4; thumbs.cy >= 2; thumbs.cy--)
		{
			thumbs.y0 = (viddef.height - thumbs.cy * thumbs.dy + thumbs.dy - thumbs.h) / 2 - CHAR_HEIGHT;
			if (thumbs.y0 >= 0) break;		// size is ok
		}

		const char *path = NULL;
		browse_list = new CFileList;
		while (path = FS_NextPath(path))
		{
			GFileSystem->List(va("%s/levelshots/*.pcx,*.tga,*.jpg", path), FS_FILE|FS_NOEXT, browse_list);	// images only
			if (*browse_list) break;
		}
		for (TListIterator<CFileItem> item = *browse_list; item; ++item)
		{
			// screenshot found - check map presence
			if (!GFileSystem->FileExists(va("maps/%s.bsp", item->name)))
				continue;

			browse_map_names[thumbs.count++] = item->name;
			if (thumbs.count == MAX_BROWSE_MAPS)
			{
				appWPrintf("Maximum number of levelshots reached\n");
				break;
			}
		}

		if (thumbs.count != oldcount)
			thumbs.current = thumbs.top = 0;

		if (thumbs.count)
			return true;

		// no maps found - free list
		delete browse_list;
		browse_list = NULL;

		startserverMenu.SetStatusBar(S_YELLOW"No levelshots found");
		return false;
	}

	const char * KeyDown(int key)
	{
		int	page = thumbs.cx * thumbs.cy;

		switch (key)
		{
		case K_ESCAPE:
		case K_MOUSE2:
			m_current->Pop();
			return menu_out_sound;

		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			thumbs.current += thumbs.cx;
			break;

		case K_UPARROW:
		case K_KP_UPARROW:
			thumbs.current -= thumbs.cx;
			break;

		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			thumbs.current++;
			break;

		case K_LEFTARROW:
		case K_KP_LEFTARROW:
			thumbs.current--;
			break;

		case K_PGUP:
		case K_KP_PGUP:
		case K_MWHEELUP:
			thumbs.current -= page;
			thumbs.top -= page;
			break;

		case K_PGDN:
		case K_KP_PGDN:
		case K_MWHEELDOWN:
			thumbs.current += page;
			thumbs.top += page;
			break;

		case K_HOME:
			thumbs.current = 0;
			break;

		case K_END:
			thumbs.current = MAX_BROWSE_MAPS;
			break;

		case K_KP_ENTER:
		case K_ENTER:
		case K_MOUSE1:
			startserverMenu.StartServerFunc(browse_map_names[thumbs.current]);
			return NULL;
		}

		thumbs.current = bound(thumbs.current, 0, thumbs.count-1);

		// clamp thumbs.top
		if (thumbs.top + page > thumbs.count)
			thumbs.top = ((thumbs.count + thumbs.cx - 1) / thumbs.cx - thumbs.cy) * thumbs.cx;
		if (thumbs.top < 0)
			thumbs.top = 0;

		// scroll thumbnails
		if (thumbs.current < thumbs.top)
			thumbs.top = thumbs.current - thumbs.current % thumbs.cx;
		else if (thumbs.current >= thumbs.top + page)
			thumbs.top = (thumbs.current / thumbs.cx - thumbs.cy + 1) * thumbs.cx;

		return menu_move_sound;
	}

	static void DrawThumbnail(int x, int y, int w, int h, const char *name, bool selected)
	{
		char	name2[256];

		RE_Fill(x - THUMBNAIL_BORDER, y - THUMBNAIL_BORDER, w + THUMBNAIL_BORDER * 2,
				h + THUMBNAIL_BORDER * 2 + THUMBNAIL_TEXT, selected ? RGB(0,0.1,0.2) : RGB(0,0,0) /* blue/black */);

		RE_DrawStretchPic(x, y, w, h, va("levelshots/%s", name));
		int max_width = w / CHAR_WIDTH;
		strcpy(name2, name);
		int text_width = strlen(name2);
		if (text_width > max_width)
		{
			strcpy(&name2[max_width - 3], "...");		// make long names ends with "..."
			text_width = max_width;
		}
		DrawString(x + (w-CHAR_WIDTH*text_width) / 2, y + h + (THUMBNAIL_TEXT + THUMBNAIL_BORDER - 8) / 2, name2);
	}

	static void DrawScroller(int x0, int y0, int w)
	{
		for (int x = x0; x < x0 + w; x += CHAR_WIDTH)
			RE_DrawChar(x, y0, 0, C_GREEN);
	}

	void Draw ()
	{
		for (int y = 0; y < thumbs.cy; y++)
			for (int x = 0; x < thumbs.cx; x++)
			{
				int i = thumbs.top + y * thumbs.cx + x;
				if (i >= thumbs.count) continue;

				DrawThumbnail(thumbs.x0 + x * thumbs.dx,
							   thumbs.y0 + y * thumbs.dy,
							   thumbs.w, thumbs.h, browse_map_names[i], i == thumbs.current);
			}
		// draw scrollers
		int w = thumbs.w * thumbs.cx;
		int x = (viddef.width - w) / 2;
		if (thumbs.top > 0)
			DrawScroller(x, thumbs.y0 - CHAR_HEIGHT - THUMBNAIL_BORDER, w);
		if (thumbs.top + thumbs.cx * thumbs.cy < thumbs.count)
			DrawScroller(x, thumbs.y0 + thumbs.cy * thumbs.dy - THUMBNAIL_BORDER * 2, w);
	}
};
static dmbrowseMenu_t dmbrowseMenu;

// static members
const char *dmbrowseMenu_t::browse_map_names[MAX_BROWSE_MAPS];
CFileList *dmbrowseMenu_t::browse_list;
thumbParams_t dmbrowseMenu_t::thumbs;


static void Menu_DMBrowse_f()
{
	dmbrowseMenu.Push();
}


/*
=======================================================================

VIDEO MENU

=======================================================================
*/

#define MIN_GAMMA		10		// 0.5
#define MAX_GAMMA		30		// 2.0

#define MIN_CONTRAST	10		// 0.5
#define MAX_CONTRAST	30		// 1.5

// cvars

//?? ADD r_ignorehwgamma, gl_ext_texture_compression
extern cvar_t *scr_viewsize;
//static cvar_t *gl_driver;
static cvar_t *gl_mode;
static cvar_t *gl_picmip;
static cvar_t *gl_bitdepth;
static cvar_t *gl_textureBits;
static cvar_t *gl_vertexLight;
static cvar_t *gl_overbright;
static cvar_t *r_q3map_overbright;


struct videoMenu_t : menuFramework_t
{
	/*--- Common menu items ---*/
	static menuList_t	mode_list;			// video mode
	static menuSlider_t	screensize_slider;	// screen size
	static menuSlider_t	brightness_slider;	// gamma
	static menuSlider_t	contrast_slider;	// contrast
	static menuSlider_t	saturation;			// saturation
	static menuList_t  	fs_box;				// fullscreen
	static menuAction_t	cancel_action;

	/*---- OpenGL renderer ----*/
	static menuList_t  	finish_box;			// gl_finish
	static menuList_t	colorDepth;			// gl_bitdepth
	static menuList_t	overbright;			// gl_overbright
	// quality/speed
	static menuList_t	lightStyle;			// gl_vertexLight
	static menuList_t	fastSky;			// gl_fastSky
	static menuList_t	q3mapOverbright;	// r_q3map_overbright
	// textures
	static menuSlider_t	tq_slider;			// gl_picmip
	static menuList_t	textureBits;		// gl_textureBits
	static menuList_t	textureFilter;		// gl_texturemode

	static float old_gamma, old_contrast, old_bright;

	static void ScreenSizeCallback(void *s)
	{
		menuSlider_t *slider = (menuSlider_t *) s;
		Cvar_SetValue("scr_viewsize", slider->curvalue * 10);
	}

	static void BrightnessCallback(void *s)
	{
		menuSlider_t *slider = (menuSlider_t *) s;
		Cvar_SetValue("r_gamma", slider->curvalue / (MAX_GAMMA - MIN_GAMMA));
	}

	static void ContrastCallback(void *s)
	{
		menuSlider_t *slider = (menuSlider_t *) s;
		Cvar_SetValue("r_contrast", slider->curvalue / (MAX_CONTRAST - MIN_CONTRAST));
		Cvar_SetValue("r_brightness", slider->curvalue / (MAX_CONTRAST - MIN_CONTRAST));
	}

	static void ApplyChanges(void *)
	{
		bool modeChanded = false;

		Cvar_SetInteger("r_fullscreen", fs_box.curvalue);
		Cvar_SetValue("r_saturation", saturation.curvalue / 10.0);
		Cvar_SetInteger("gl_picmip", 3 - appRound(tq_slider.curvalue));
		Cvar_SetInteger("gl_finish", finish_box.curvalue);
		Cvar_SetInteger("gl_mode", mode_list.curvalue);
		Cvar_SetInteger("gl_fastsky", fastSky.curvalue);
		Cvar_SetInteger("r_q3map_overbright", q3mapOverbright.curvalue);
		Cvar_SetInteger("gl_bitdepth", colorDepth.curvalue * 16);
		Cvar_SetInteger("gl_overbright", overbright.curvalue);
		Cvar_SetInteger("gl_textureBits", textureBits.curvalue * 16);
		Cvar_SetInteger("gl_vertexLight", lightStyle.curvalue);
		Cvar_Set("gl_texturemode", textureFilter.curvalue ? "trilinear" : "bilinear");

#define CHECK_UPDATE(var)		\
	if (var->modified)			\
	{							\
		var->modified = false;	\
		modeChanded = true;		\
	}

		CHECK_UPDATE(r_saturation);
		CHECK_UPDATE(r_gamma);

		CHECK_UPDATE(gl_mode);
		CHECK_UPDATE(gl_picmip);
		CHECK_UPDATE(gl_vertexLight);
		CHECK_UPDATE(gl_textureBits);
		CHECK_UPDATE(gl_overbright);
		CHECK_UPDATE(gl_bitdepth);

		CHECK_UPDATE(r_q3map_overbright);	// note: affects Q3A maps only

		if (modeChanded)
		{
			Vid_Restart ();
			M_ForceMenuOff();
		}
		else
			m_current->Pop();
	}

	static void CancelChanges(void *)
	{
		Cvar_SetValue("r_gamma", old_gamma);
		Cvar_SetValue("r_contrast", old_contrast);
		Cvar_SetValue("r_brightness", old_bright);
		m_current->Pop();
	}

	bool Init ()
	{
		static const char* resolutions[32];

		static const char* overbrightNames[] = {
			"no",			"yes",			"auto",			NULL
		};
		static const char* bits[] = {
			"default",		"16 bit",		"32 bit",		NULL
		};
		static const char* lighting[] = {
			"lightmap",		"vertex",		NULL
		};
		static const char* filters[] = {
			"bilinear",		"trilinear",	NULL
		};

		// Init video modes
		if (resolutions[0] == NULL)
		{
			for (int mode = 0; /* empty */; mode++)
			{
				int w, h;
				if (!Vid_GetModeInfo(&w, &h, mode))
					break;
				assert(mode < ARRAY_COUNT(resolutions)-1);
				char buf[32];
				appSprintf(ARRAY_ARG(buf), "[%4dx%4d]", w, h);
				resolutions[mode] = appStrdup(buf); // not releasing string
				resolutions[mode+1] = NULL;
			}
		}

		// init cvars
CVAR_BEGIN(vars)
//		CVAR_GET(gl_driver, opengl32, CVAR_ARCHIVE),
//		CVAR_VAR(scr_viewsize, 100, CVAR_ARCHIVE),
		CVAR_VAR(gl_mode, 3, CVAR_ARCHIVE),
		CVAR_VAR(gl_picmip, 0, CVAR_ARCHIVE),
		CVAR_VAR(gl_bitdepth, 0, CVAR_ARCHIVE),
		CVAR_VAR(gl_textureBits, 0, CVAR_ARCHIVE),
		CVAR_VAR(gl_vertexLight, 0, CVAR_ARCHIVE),
		CVAR_VAR(gl_overbright, 2, CVAR_ARCHIVE),
		CVAR_VAR(r_q3map_overbright, 1, CVAR_ARCHIVE)
CVAR_END
		Cvar_GetVars(ARRAY_ARG(vars));

		mode_list.curvalue = Cvar_VariableInt("gl_mode");

		x = viddef.width / 2;
		y = viddef.height / 2 - 58;
		nitems = 0;
		banner = "video";

		// save current gamma (for cancel action)
		old_gamma    = r_gamma->value;
		old_contrast = r_contrast->value;
		old_bright   = r_brightness->value;

		int y = 0;
		MENU_SPIN(mode_list, y+=10, "video mode", NULL, resolutions);
		AddItem(&mode_list);
		MENU_SLIDER(screensize_slider, y+=10, "screen size", ScreenSizeCallback, 4, 10);
		screensize_slider.curvalue = scr_viewsize->value / 10;
		AddItem(&screensize_slider);
		MENU_SLIDER(brightness_slider, y+=10, "brightness", BrightnessCallback, MIN_GAMMA, MAX_GAMMA);
		r_gamma->Clamp(0.5, 3);
		brightness_slider.curvalue = r_gamma->value * (MAX_GAMMA - MIN_GAMMA);
		AddItem(&brightness_slider);
		MENU_SLIDER(contrast_slider, y+=10, "contrast", ContrastCallback, MIN_CONTRAST, MAX_CONTRAST);
		r_contrast->Clamp(0.5, 1.5);
		contrast_slider.curvalue = r_contrast->value * (MAX_CONTRAST - MIN_CONTRAST);
		AddItem(&contrast_slider);
		MENU_SLIDER(saturation, y+=10, "saturation", NULL, 0, 20);
		r_saturation->Clamp(0, 2);
		saturation.curvalue = r_saturation->value * 10;
		AddItem(&saturation);
		MENU_SPIN(fs_box, y+=10, "fullscreen", NULL, yesno_names);
		fs_box.curvalue = r_fullscreen->integer;
		AddItem(&fs_box);

		y += 10;
		MENU_SPIN(colorDepth, y+=10, "color depth", NULL, bits);
		colorDepth.curvalue = gl_bitdepth->integer >> 4;
		AddItem(&colorDepth);
		MENU_SPIN(overbright, y+=10, "gamma overbright", NULL, overbrightNames);
		overbright.curvalue = gl_overbright->integer;
		AddItem(&overbright);
		MENU_SPIN(lightStyle, y+=10, "lighting", NULL, lighting);
		lightStyle.curvalue = gl_vertexLight->integer;
		AddItem(&lightStyle);
		MENU_SPIN(fastSky, y+=10, "fast sky", NULL, yesno_names);
		fastSky.curvalue = Cvar_VariableInt("gl_fastsky");
		AddItem(&fastSky);
		MENU_SPIN(q3mapOverbright, y+=10, "overbright Q3A maps", NULL, yesno_names);
		q3mapOverbright.curvalue = Cvar_VariableInt("r_q3map_overbright");
		AddItem(&q3mapOverbright);
		MENU_SLIDER(tq_slider, y+=10, "texture detail", NULL, 0, 3);
		tq_slider.curvalue = 3 - gl_picmip->integer;
		AddItem(&tq_slider);
		MENU_SPIN(textureBits, y+=10, "texture quality", NULL, bits);
		textureBits.curvalue = gl_textureBits->integer >> 4;
		AddItem(&textureBits);
		MENU_SPIN(textureFilter, y+=10, "texture filter", NULL, filters);
		textureFilter.curvalue = stricmp(Cvar_VariableString("gl_texturemode"), "bilinear") ? 1 : 0;
		AddItem(&textureFilter);
		MENU_SPIN(finish_box, y+=10, "sync every frame", NULL, yesno_names);
		finish_box.curvalue = Cvar_VariableInt("gl_finish");
		AddItem(&finish_box);

		y += 10;
		MENU_ACTION(cancel_action, y+=10, "cancel", CancelChanges,NULL);
		AddItem(&cancel_action);

		return true;
	}

	const char *KeyDown(int key)
	{
		switch (key)
		{
		case K_ESCAPE:
		case K_MOUSE2:
			ApplyChanges(NULL);
			return NULL;
		case K_KP_ENTER:
		case K_ENTER:
			if (!SelectItem()) ApplyChanges(NULL);
			return menu_out_sound;
		}
		return menuFramework_t::KeyDown(key);
	}
};
static videoMenu_t videoMenu;

// static members
menuList_t	videoMenu_t::mode_list;
menuSlider_t	videoMenu_t::screensize_slider;
menuSlider_t	videoMenu_t::brightness_slider;
menuSlider_t	videoMenu_t::contrast_slider;
menuSlider_t	videoMenu_t::saturation;
menuList_t  videoMenu_t::fs_box;
menuAction_t	videoMenu_t::cancel_action;
menuList_t  videoMenu_t::finish_box;
menuList_t	videoMenu_t::colorDepth;
menuList_t	videoMenu_t::overbright;
menuList_t	videoMenu_t::lightStyle;
menuList_t	videoMenu_t::fastSky;
menuList_t	videoMenu_t::q3mapOverbright;
menuSlider_t	videoMenu_t::tq_slider;
menuList_t	videoMenu_t::textureBits;
menuList_t	videoMenu_t::textureFilter;
float videoMenu_t::old_gamma, videoMenu_t::old_contrast, videoMenu_t::old_bright;


static void Menu_Video_f()
{
	videoMenu.Push();
}


/*
=======================================================================

QUIT MENU

=======================================================================
*/

struct quitMenu_t : menuFramework_t
{
	const char * KeyDown(int key)
	{
		switch (key)
		{
		case K_ESCAPE:
		case K_MOUSE2:
		case 'n':
		case 'N':
			Pop ();
			break;

		case 'Y':
		case 'y':
		case K_MOUSE1:
			GIsRequestingExit = true;
			break;

		default:
			break;
		}

		return NULL;
	}

	void Draw ()
	{
		CBasicImage *img = RE_RegisterPic("pics/quit");
		if (!img) return;
		//!! can use img->Draw()
		if (img->width >= 320 && img->width * 3 / 4 == img->height)		// this pic is for fullscreen in mode 320x240
			RE_DrawDetailedPic(0, 0, viddef.width, viddef.height, "pics/quit");
		else
			RE_DrawPic(viddef.width / 2, viddef.height / 2, "pics/quit", ANCHOR_CENTER);
	}
};
static quitMenu_t quitMenu;

static void Menu_Quit_f()
{
	quitMenu.Push();
}



#if 0
-- currently broken (changed model skin system)
/*
=======================================================================

TEST MENU

=======================================================================
*/
#define REGISTER_TEST		1

void attach (const entity_t &e1, entity_t &e2, const char *tag)
{
	CCoords lerped;		// get position modifier
	e1.model->LerpTag(e1.frame, e1.oldframe, 0, tag, lerped);
	e1.pos.UnTransformCoords(lerped, e2.pos);
}

struct testMenu_t : menuFramework_t
{
	const char * KeyDown(int key)
	{
		switch (key)
		{
		case K_ESCAPE:
			Pop ();
			break;
		}

		return NULL;
	}

	void Draw ()
	{
		entity_t e[4];
		static dlight_t	dl[] = {
			{{30, 100, 100}, {1, 1, 1}, 350},
			{{90, -100, 10}, {0.4, 0.2, 0.2}, 200}
		};

		refdef_t refdef;
		memset(&refdef, 0, sizeof(refdef));
		refdef.x = viddef.width / 2;
		refdef.y = viddef.height * 6 / 25;
		refdef.width = viddef.width * 4 / 10 + 16;
		refdef.height = viddef.width * 5 / 10;
		refdef.fov_x = refdef.width * 90 / viddef.width;
		refdef.fov_y = CalcFov(refdef.fov_x, refdef.width, refdef.height);
		refdef.time = cls.realtime / 1000.0f;

		memset(&e, 0, sizeof(e));

		const char *model_name = "visor";
//		const char *model_name = "crash";

		// add legs model
		e[0].angles[1] = cls.realtime / 20 % 360;	// rotate
		e[0].pos.origin[0] = 130;
		e[0].pos.axis.FromEuler(e[0].angles);		// compute legs axis
		e[0].model = RE_RegisterModel(va("models/players/%s/lower.md3", model_name));
		e[0].skin = RE_RegisterSkin(va("models/players/%s/blue", model_name));
		e[0].frame = e[0].oldframe = 220-63;	// legs frame

		// add torso model
		e[1].model = RE_RegisterModel(va("models/players/%s/upper.md3", model_name));
		e[1].skin = e[0].skin;
		e[1].frame = e[1].oldframe = 151;		// torso frame
		attach (e[0], e[1], "tag_torso");

		// add head model
		e[2].model = RE_RegisterModel(va("models/players/%s/head.md3", model_name));
		e[2].skin = e[0].skin;
		e[2].frame = e[2].oldframe = 0;
		attach (e[1], e[2], "tag_head");

		e[3].model = RE_RegisterModel("models/weapons/g_rail/tris.md2");
		e[3].skin = NULL;
		e[3].frame = e[3].oldframe = 0;
		attach (e[1], e[3], "tag_weapon");

		refdef.num_entities = 4;
		refdef.entities = e;
		refdef.dlights = dl;
		refdef.num_dlights = 1;//ARRAY_COUNT(dl);
		refdef.rdflags = RDF_NOWORLDMODEL;

		RE_Fill(refdef.x-4, refdef.y-4, refdef.width+8, refdef.height+8, RGBA(0,0,0,0.6));
		RE_RenderFrame(&refdef);
	}
};
static testMenu_t testMenu;

static void Menu_Test_f ()
{
	testMenu.Push();
}


#endif


/*-----------------------------------------------------------------------------
	Menu registration
-----------------------------------------------------------------------------*/

void M_Init()
{
	RegisterCommand("menu_main", M_Menu_Main_f);
	RegisterCommand("menu_game", Menu_Game_f);
		RegisterCommand("menu_loadgame", Menu_LoadGame_f);
		RegisterCommand("menu_savegame", Menu_SaveGame_f);
		RegisterCommand("menu_joinserver", Menu_JoinServer_f);
			RegisterCommand("menu_addressbook", Menu_AddressBook_f);
		RegisterCommand("menu_startserver", Menu_StartServer_f);
			RegisterCommand("menu_dmoptions", Menu_DMOptions_f);
			RegisterCommand("menu_dmbrowse", Menu_DMBrowse_f);
		RegisterCommand("menu_playerconfig", Menu_PlayerConfig_f);
			RegisterCommand("menu_downloadoptions", Menu_DownloadOptions_f);
		RegisterCommand("menu_credits", Menu_Credits_f );
	RegisterCommand("menu_multiplayer", Menu_Multiplayer_f );
	RegisterCommand("menu_video", Menu_Video_f);
	RegisterCommand("menu_options", Menu_Options_f);
		RegisterCommand("menu_keys", Menu_Keys_f);
	RegisterCommand("menu_quit", Menu_Quit_f);
#if REGISTER_TEST
	RegisterCommand("menu_test", Menu_Test_f);
#endif
}
