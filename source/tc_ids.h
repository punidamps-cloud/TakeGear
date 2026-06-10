// Take Control — plugin and gadget identifiers.
//
#ifndef TC_IDS_H__
#define TC_IDS_H__

#define TC_VERSION_STRING "1.0.0"

// --- Official plugin IDs (registered range 1068876..1068885) ---
static const maxon::Int32 ID_TC_COMMAND   = 1068876;
static const maxon::Int32 ID_TC_SCENEHOOK = 1068877;
// reserved for future plugins of this suite: 1068878..1068885

// disk level of the scene hook serialization
static const maxon::Int32 TC_DISKLEVEL = 1;

// --- C4D command ids used by Preview playback ---
static const maxon::Int32 CID_PLAY_FORWARD = 12412;
static const maxon::Int32 CID_PLAY_STOP    = 12002;

// --- Dialog gadget IDs ---
enum
{
	GID_TABS = 1000,
	GID_TAB_TIMELINE,
	GID_TAB_SHEET,
	GID_TAB_HELP,

	// toolbar
	GID_BTN_ADDCLIP = 1100,
	GID_BTN_SPLIT,
	GID_BTN_DUP,
	GID_BTN_DELETE,
	GID_BTN_MARKER,
	GID_BTN_SETIN,
	GID_BTN_SETOUT,
	GID_BTN_FOCUS,
	GID_BTN_PREVIEW,
	GID_BTN_STOP,
	GID_BTN_RENDER,
	GID_BTN_RENDERNOW,
	GID_BTN_RANGE,
	GID_BTN_AUDIO,
	GID_BTN_AUDIO_CLEAR,
	GID_BTN_REFRESH,
	GID_BTN_ADDTRACK,
	GID_BTN_DELTRACK,
	GID_CHK_AUTO,
	GID_CHK_FALLBACK,
	GID_CHK_SNAP,
	GID_CHK_HUD,

	// source takes panel
	GID_GRP_TAKES = 1200,
	GID_GRP_TAKES_INNER,
	GID_BTN_NEWTAKE,
	GID_TXT_CURTAKE,
	GID_UA_TAKES,

	// timeline user area
	GID_UA_TIMELINE = 1300,
	GID_GRP_SPLIT, // weighted group: takes panel | timeline

	// clip properties row
	GID_GRP_PROPS = 1400,
	GID_EDIT_CLIPNAME,
	GID_COMBO_TAKE,
	GID_COLOR_CLIP,
	GID_COMBO_STATUS,
	GID_CHK_CAMFOLLOW,
	GID_EDIT_NOTE,
	GID_TXT_CLIPINFO,

	// sheet view
	GID_GRP_SHEET = 1500,
	GID_GRP_SHEET_INNER,
	GID_BTN_SHEET_REFRESH,
	GID_UA_SHEET,

	// help tab
	GID_HELP_TEXT = 1600,
	GID_BTN_DEBUG,
	GID_UA_HELP,

	// dynamic id bases
	GID_TAKEBTN_BASE = 20000,	// one button per take in the source panel
	GID_SHEET_BASE	 = 30000	// sheet rows, stride 10: +0 go, +1 status, +2 focus
};

// context menu ids (timeline user area)
enum
{
	POP_RENAME = 40000,
	POP_CLIPSETTINGS, // modal clip/take settings window
	POP_TAKE_SETTINGS,
	POP_NOTE,
	POP_SPLIT,
	POP_DUP,
	POP_DELETE,
	POP_FOCUS,
	POP_GOTOTAKE,
	POP_CAMFOLLOW,
	POP_ADDMARKER,
	POP_DELMARKER,
	POP_RENMARKER,
	POP_FIT,
	POP_ADDTRACK,
	POP_DELTRACK,
	POP_RENTRACK,
	POP_STATUS_BASE = 40100,			// +status index (0..4)
	POP_COLOR_BASE	= 40200,			// +palette index (clip color)
	POP_TRACKCOLOR_BASE = 40300,	// +palette index (track color)
	POP_TAKE_SETCUR = 40400,			// takes panel context menu
	POP_TAKE_ADDCLIP,
	POP_TAKE_NEWCHILD,
	POP_TAKE_RENAME,
	POP_TAKE_DELETE,
	POP_TAKE_MARK,								// toggle "marked for render"
	POP_TAKE_DUP,									// duplicate take
	POP_TAKE_CLEARCAM,						// camera -> inherit
	POP_TAKE_CLEARRD,							// render settings -> inherit
	POP_RIPPLEDEL = 40450,				// ripple delete (close the gap)
	POP_COPY,
	POP_PASTE,
	POP_CAMWIZARD,								// create takes + clips from scene cameras
	POP_EXPORTCSV,								// export shot list
	POP_AUDIO_LOAD = 40500,				// audio track context menu
	POP_AUDIO_CLEAR,
	POP_AUDIO_RESET,
	POP_AUDIO_TOPLAYHEAD,
	POP_AUDIO_SYNC,
	POP_ADDCLIP_BASE = 41000,			// +take index (add clip for take under cursor)
	POP_TAKECAM_BASE = 42000,			// +scene camera index (assign take camera)
	POP_TAKERD_BASE	 = 43000			// +render data index (assign take render settings)
};

#endif // TC_IDS_H__
