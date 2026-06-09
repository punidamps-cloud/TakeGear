// Take Control — main dialog implementation.
#include "tc_dialog.h"
#include "tc_settings.h"
#include "tc_thumbs.h"

#include "customgui_bitmapbutton.h"

#include <algorithm>

namespace tc
{

// ================================================================== toolbar icons

// tiny pixel canvas helpers for programmatic 24x24 toolbar icons
namespace icon
{

static const Int32 SZ = 24;

struct Canvas
{
	BaseBitmap* bmp = nullptr;
	Vector bg;

	Bool Init()
	{
		bmp = BaseBitmap::Alloc();
		if (!bmp || bmp->Init(SZ, SZ, 24) != IMAGERESULT::OK)
			return false;
		bg = GetGuiWorldColor(COLOR_BG);
		Fill(bg);
		return true;
	}
	void Px(Int32 x, Int32 y, const Vector& c)
	{
		if (x < 0 || y < 0 || x >= SZ || y >= SZ)
			return;
		bmp->SetPixel(x, y, (Int32)(c.x * 255.0), (Int32)(c.y * 255.0), (Int32)(c.z * 255.0));
	}
	void Fill(const Vector& c)
	{
		for (Int32 y = 0; y < SZ; ++y)
			for (Int32 x = 0; x < SZ; ++x)
				Px(x, y, c);
	}
	void Rect(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const Vector& c)
	{
		for (Int32 y = y1; y <= y2; ++y)
			for (Int32 x = x1; x <= x2; ++x)
				Px(x, y, c);
	}
	void Frame(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const Vector& c)
	{
		Rect(x1, y1, x2, y1, c);
		Rect(x1, y2, x2, y2, c);
		Rect(x1, y1, x1, y2, c);
		Rect(x2, y1, x2, y2, c);
	}
	void Line(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const Vector& c)
	{
		const Int32 dx = (x2 > x1) ? x2 - x1 : x1 - x2;
		const Int32 dy = (y2 > y1) ? y2 - y1 : y1 - y2;
		const Int32 sx = (x1 < x2) ? 1 : -1;
		const Int32 sy = (y1 < y2) ? 1 : -1;
		Int32 err = dx - dy, x = x1, y = y1;
		while (true)
		{
			Px(x, y, c);
			if (x == x2 && y == y2)
				break;
			const Int32 e2 = 2 * err;
			if (e2 > -dy)
			{
				err -= dy;
				x += sx;
			}
			if (e2 < dx)
			{
				err += dx;
				y += sy;
			}
		}
	}
	void TriRight(Int32 x, Int32 cy, Int32 size, const Vector& c)
	{
		for (Int32 i = 0; i <= size; ++i)
			Rect(x + i, cy - (size - i), x + i, cy + (size - i), c);
	}
};

static const Vector FG(0.83, 0.83, 0.86);
static const Vector ACC(1.00, 0.62, 0.12);

// builds one icon by id; the returned bitmap is owned by the caller (SetImage copies)
// style: thin monochrome outlines (FG), at most one accent element per icon
static BaseBitmap* Build(Int32 gid)
{
	Canvas c;
	if (!c.Init())
		return nullptr;
	switch (gid)
	{
		case GID_BTN_ADDCLIP: // clip outline + accent plus
			c.Frame(3, 9, 16, 17, FG);
			c.Rect(17, 6, 21, 7, ACC);
			c.Rect(18, 3, 19, 10, ACC);
			break;
		case GID_BTN_SPLIT: // clip cut by an accent blade
			c.Frame(3, 8, 20, 16, FG);
			c.Rect(11, 5, 12, 19, ACC);
			break;
		case GID_BTN_DUP: // two outlined clips
			c.Frame(7, 5, 20, 13, FG);
			c.Rect(4, 10, 17, 18, c.bg);
			c.Frame(4, 10, 17, 18, FG);
			break;
		case GID_BTN_DELETE: // trash: lid + tapered body + two slots
			c.Rect(5, 6, 18, 7, FG);
			c.Rect(10, 4, 13, 5, FG);
			c.Frame(7, 9, 16, 19, FG);
			c.Rect(10, 11, 10, 17, FG);
			c.Rect(13, 11, 13, 17, FG);
			break;
		case GID_BTN_MARKER: // accent flag
			c.Rect(7, 4, 8, 20, FG);
			c.Rect(9, 4, 16, 9, ACC);
			break;
		case GID_BTN_SETIN: // bar + bold arrow right
			c.Rect(5, 5, 6, 19, ACC);
			c.Rect(9, 11, 13, 13, FG);
			c.TriRight(14, 12, 4, FG);
			break;
		case GID_BTN_SETOUT: // bold arrow right + bar
			c.Rect(17, 5, 18, 19, ACC);
			c.Rect(6, 11, 10, 13, FG);
			c.TriRight(11, 12, 4, FG);
			break;
		case GID_BTN_FOCUS: // four corners + accent dot
			c.Rect(4, 4, 8, 5, FG);
			c.Rect(4, 4, 5, 8, FG);
			c.Rect(15, 4, 19, 5, FG);
			c.Rect(18, 4, 19, 8, FG);
			c.Rect(4, 18, 8, 19, FG);
			c.Rect(4, 15, 5, 19, FG);
			c.Rect(15, 18, 19, 19, FG);
			c.Rect(18, 15, 19, 19, FG);
			c.Rect(10, 10, 13, 13, ACC);
			break;
		case GID_BTN_PREVIEW: // play triangle
			c.TriRight(8, 12, 6, FG);
			break;
		case GID_BTN_RENDER: // clapperboard outline
			c.Frame(4, 9, 19, 18, FG);
			c.Rect(4, 6, 19, 8, FG);
			c.Line(7, 6, 9, 8, c.bg);
			c.Line(12, 6, 14, 8, c.bg);
			c.Line(17, 6, 19, 8, c.bg);
			break;
		case GID_BTN_RENDERNOW: // clapperboard + accent play
			c.Frame(4, 9, 19, 18, FG);
			c.Rect(4, 6, 19, 8, FG);
			c.TriRight(10, 13, 3, ACC);
			break;
		case GID_BTN_RANGE: // brackets
			c.Rect(5, 5, 8, 6, FG);
			c.Rect(5, 5, 6, 18, FG);
			c.Rect(5, 17, 8, 18, FG);
			c.Rect(15, 5, 18, 6, FG);
			c.Rect(17, 5, 18, 18, FG);
			c.Rect(15, 17, 18, 18, FG);
			c.Rect(10, 11, 13, 12, ACC);
			break;
		case GID_BTN_AUDIO: // solid speaker + wave arc
			c.Rect(4, 10, 6, 14, FG);
			for (Int32 i = 0; i < 5; ++i)
				c.Rect(7 + i, 10 - i, 7 + i, 14 + i, FG); // filled cone
			for (Float a = -0.9; a <= 0.9; a += 0.08)
				c.Px(14 + (Int32)(5.0 * maxon::Cos(a) + 0.5), 12 + (Int32)(5.0 * maxon::Sin(a) + 0.5), FG);
			break;
		case GID_BTN_AUDIO_CLEAR: // solid speaker + accent cross
			c.Rect(4, 10, 6, 14, FG);
			for (Int32 i = 0; i < 5; ++i)
				c.Rect(7 + i, 10 - i, 7 + i, 14 + i, FG);
			c.Rect(15, 9, 16, 10, ACC);
			c.Rect(19, 9, 20, 10, ACC);
			c.Rect(17, 11, 18, 13, ACC);
			c.Rect(15, 14, 16, 15, ACC);
			c.Rect(19, 14, 20, 15, ACC);
			break;
		case GID_BTN_ADDTRACK: // rows + accent plus
			c.Rect(4, 7, 14, 8, FG);
			c.Rect(4, 12, 14, 13, FG);
			c.Rect(4, 17, 14, 18, FG);
			c.Rect(17, 11, 21, 12, ACC);
			c.Rect(18, 9, 19, 14, ACC);
			break;
		case GID_BTN_DELTRACK: // rows + accent minus
			c.Rect(4, 7, 14, 8, FG);
			c.Rect(4, 12, 14, 13, FG);
			c.Rect(4, 17, 14, 18, FG);
			c.Rect(17, 11, 21, 12, ACC);
			break;
		case GID_BTN_REFRESH: // thick 300° arc + solid arrow head
			for (Float a = 0.7; a <= 5.9; a += 0.04)
			{
				const Float ca = maxon::Cos(a), sa = maxon::Sin(a);
				c.Px(12 + (Int32)(7.0 * ca + 0.5), 12 + (Int32)(7.0 * sa + 0.5), FG);
				c.Px(12 + (Int32)(6.0 * ca + 0.5), 12 + (Int32)(6.0 * sa + 0.5), FG);
				c.Px(12 + (Int32)(6.5 * ca + 0.5), 12 + (Int32)(6.5 * sa + 0.5), FG);
			}
			// arrow head at the arc start (pointing right-down)
			c.Rect(16, 4, 21, 5, FG);
			c.Rect(20, 4, 21, 9, FG);
			c.Px(19, 6, FG);
			c.Px(18, 7, FG);
			break;
		default:
			break;
	}
	return c.bmp;
}

} // namespace icon

// ================================================================== takes area

// scanline rounded rectangle (same look as the timeline primitives)
static void UAFillRoundRect(GeUserArea& ua, Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, const Vector& col)
{
	if (x2 < x1 || y2 < y1)
		return;
	Int32 rr = r;
	if (rr * 2 > y2 - y1 + 1)
		rr = (y2 - y1 + 1) / 2;
	if (rr * 2 > x2 - x1 + 1)
		rr = (x2 - x1 + 1) / 2;
	ua.DrawSetPen(col);
	if (rr <= 0)
	{
		ua.DrawRectangle(x1, y1, x2, y2);
		return;
	}
	ua.DrawRectangle(x1, y1 + rr, x2, y2 - rr);
	for (Int32 i = 0; i < rr; ++i)
	{
		const Float dy = (Float)(rr - i) - 0.5;
		const Float dx = maxon::Sqrt((Float)(rr * rr) - dy * dy);
		const Int32 inset = rr - (Int32)(dx + 0.5);
		ua.DrawRectangle(x1 + inset, y1 + i, x2 - inset, y1 + i);
		ua.DrawRectangle(x1 + inset, y2 - i, x2 - inset, y2 - i);
	}
}

Bool TCTakesArea::GetMinSize(Int32& w, Int32& h)
{
	// keep this small so the splitter can collapse the panel
	w = 70;
	h = 120;
	return true;
}

Int32 TCTakesArea::RowAt(Int32 my) const
{
	if (my < HEAD_H)
		return NOTOK;
	const Int32 r = (my - HEAD_H + scrollY) / ROW_H;
	if (r < 0 || r >= (Int32)rows.size())
		return NOTOK;
	return r;
}

BaseTake* TCTakesArea::TakeByIndex(BaseDocument* doc, Int32 idx) const
{
	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);
	if (idx < 0 || idx >= (Int32)takes.size())
		return nullptr;
	return takes[idx].first;
}

void TCTakesArea::Rebuild()
{
	rows.clear();
	BaseDocument* doc = GetActiveDocument();
	if (!doc)
		return;
	TakeData* td = doc->GetTakeData();
	BaseTake* cur = td ? td->GetCurrentTake() : nullptr;
	TCModel* m = TCGetModel(doc);

	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);
	for (Int32 i = 0; i < (Int32)takes.size(); ++i)
	{
		Row r;
		r.name = takes[i].first->GetName();
		r.depth = takes[i].second;
		r.checked = takes[i].first->IsChecked();
		r.hasKids = takes[i].first->GetDown() != nullptr;
		r.isCurrent = (takes[i].first == cur);
		if (td)
		{
			BaseObject* cam = takes[i].first->GetCamera(td);
			if (cam)
				r.cam = cam->GetName();
		}

		// color: first clip using this take, else palette
		r.color = TCPaletteColor(i % TC_PALETTE_COUNT);
		if (takes[i].second == 0)
			r.color = Vector(0.35, 0.75, 0.40); // Main
		if (m)
		{
			for (const auto& c : m->clips)
			{
				if (c.takeName == r.name)
				{
					r.color = c.color;
					break;
				}
			}
		}
		rows.push_back(r);
	}
}

void TCTakesArea::DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg)
{
	OffScreenOn();
	DrawSetFont(FONT_STANDARD);
	const Int32 fh = DrawGetFontHeight();
	const Int32 w = GetWidth();
	const Int32 h = GetHeight();

	DrawSetPen(Vector(0.094, 0.094, 0.098));
	DrawRectangle(0, 0, w, h);

	// header
	DrawSetPen(Vector(0.137, 0.137, 0.145));
	DrawRectangle(0, 0, w, HEAD_H - 1);
	DrawSetTextCol(Vector(0.82, 0.82, 0.84), COLOR_TRANS);
	DrawText(String("SOURCE TAKES"), 10, (HEAD_H - fh) / 2);

	const Int32 maxScroll = ((Int32)rows.size() * ROW_H) - (h - HEAD_H);
	if (scrollY > (maxScroll > 0 ? maxScroll : 0))
		scrollY = (maxScroll > 0 ? maxScroll : 0);

	for (Int32 i = 0; i < (Int32)rows.size(); ++i)
	{
		const Int32 top = HEAD_H + i * ROW_H - scrollY;
		if (top + ROW_H < HEAD_H || top > h)
			continue;
		const Row& r = rows[i];

		if (r.isCurrent)
		{
			UAFillRoundRect(*this, 2, top, w - 3, top + ROW_H - 1, 5, Vector(0.165, 0.165, 0.176));
			UAFillRoundRect(*this, 0, top + 1, 2, top + ROW_H - 2, 1, Vector(1.0, 0.62, 0.12));
		}
		else if (i == hoverRow)
		{
			UAFillRoundRect(*this, 2, top, w - 3, top + ROW_H - 1, 5, Vector(0.145, 0.145, 0.152));
		}
		else if (i & 1)
		{
			DrawSetPen(Vector(0.104, 0.104, 0.108));
			DrawRectangle(0, top, w, top + ROW_H - 1);
		}

		const Int32 ix = 8 + r.depth * 14;

		// chevron for parents
		if (r.hasKids)
		{
			DrawSetPen(Vector(0.55, 0.55, 0.58));
			const Int32 cy = top + ROW_H / 2;
			DrawLine(ix - 1, cy - 2, ix + 3, cy + 2);
			DrawLine(ix + 3, cy + 2, ix + 7, cy - 2);
		}

		// color square (rounded)
		UAFillRoundRect(*this, ix + 10, top + 5, ix + 19, top + 14, 3, r.color);

		// name
		SetClippingRegion(ix + 24, top, w - ix - 44, ROW_H);
		DrawSetTextCol(r.isCurrent ? Vector(0.98) : Vector(0.72, 0.72, 0.75), COLOR_TRANS);
		DrawText(r.name, ix + 25, top + (ROW_H - fh) / 2);
		ClearClippingRegion();
		SetClippingRegion(0, 0, w, h);

		// directly assigned camera, right-aligned and dimmed
		if (r.cam.IsPopulated())
		{
			const Int32 camw = DrawGetTextWidth(r.cam);
			const Int32 cx = w - 18 - camw;
			const Int32 nameEnd = ix + 25 + DrawGetTextWidth(r.name);
			if (cx > nameEnd + 8)
			{
				DrawSetTextCol(Vector(0.45, 0.55, 0.65), COLOR_TRANS);
				DrawText(r.cam, cx, top + (ROW_H - fh) / 2);
			}
		}

		// marked-for-render tag
		if (r.checked)
		{
			DrawSetTextCol(Vector(0.42, 0.62, 0.95), COLOR_TRANS);
			DrawText(String("R"), w - 14, top + (ROW_H - fh) / 2);
		}
	}
}

Int32 TCTakesArea::Message(const BaseContainer& msg, BaseContainer& result)
{
	switch (msg.GetId())
	{
		case BFM_CURSORINFO_REMOVE:
			if (hoverRow != NOTOK)
			{
				hoverRow = NOTOK;
				Redraw();
			}
			break;
		case BFM_GETCURSORINFO:
		{
			BaseContainer state;
			if (GetInputState(BFM_INPUT_MOUSE, BFM_INPUT_MOUSELEFT, state))
			{
				Int32 mx = state.GetInt32(BFM_INPUT_X);
				Int32 my = state.GetInt32(BFM_INPUT_Y);
				Global2Local(&mx, &my);
				const Int32 r = RowAt(my);
				if (r != hoverRow)
				{
					hoverRow = r;
					Redraw();
				}
				result.SetInt32(RESULT_CURSOR, (r != NOTOK) ? MOUSE_POINT_HAND : MOUSE_NORMAL);
				return true;
			}
			break;
		}
		default:
			break;
	}
	return GeUserArea::Message(msg, result);
}

Bool TCTakesArea::InputEvent(const BaseContainer& msg)
{
	BaseDocument* doc = GetActiveDocument();
	if (!doc)
		return false;
	const Int32 device = msg.GetInt32(BFM_INPUT_DEVICE);
	const Int32 channel = msg.GetInt32(BFM_INPUT_CHANNEL);
	if (device != BFM_INPUT_MOUSE)
		return false;

	const Int32 screenX = msg.GetInt32(BFM_INPUT_X);
	const Int32 screenY = msg.GetInt32(BFM_INPUT_Y);
	Int32 mx = screenX, my = screenY;
	Global2Local(&mx, &my);
	const Bool dbl = msg.GetBool(BFM_INPUT_DOUBLECLICK);

	if (channel == BFM_INPUT_MOUSEWHEEL)
	{
		const Float val = msg.GetFloat(BFM_INPUT_VALUE);
		scrollY += (val > 0) ? -ROW_H * 2 : ROW_H * 2;
		if (scrollY < 0)
			scrollY = 0;
		Redraw();
		return true;
	}

	const Int32 row = RowAt(my);
	if (row == NOTOK)
		return true;

	TakeData* td = doc->GetTakeData();
	if (!td)
		return true;

	if (channel == BFM_INPUT_MOUSELEFT)
	{
		BaseTake* take = TakeByIndex(doc, row);
		if (!take)
			return true;
		if (dbl)
		{
			String name = take->GetName();
			if (RenameDialog(&name))
				TCRenameTake(doc, take, name); // also rebinds clips in the sequencer
			Rebuild();
			Redraw();
			return true;
		}

		// click = set current; moving the mouse first = drag the take to the timeline
		Bool dragged = false;
		{
			BaseContainer state;
			while (GetInputState(BFM_INPUT_MOUSE, BFM_INPUT_MOUSELEFT, state))
			{
				if (state.GetInt32(BFM_INPUT_VALUE) == 0)
					break; // released: it was a click
				Int32 gx = state.GetInt32(BFM_INPUT_X);
				Int32 gy = state.GetInt32(BFM_INPUT_Y);
				Global2Local(&gx, &gy);
				const Int32 moved = ((gx > mx) ? gx - mx : mx - gx) + ((gy > my) ? gy - my : my - gy);
				if (moved > 4)
				{
					dragged = true;
					break;
				}
			}
		}

		if (dragged)
		{
			// hand the take over to Cinema's drag&drop (received by the timeline area)
			static AtomArray* dragArray = nullptr;
			if (!dragArray)
				dragArray = AtomArray::Alloc();
			if (dragArray)
			{
				dragArray->Flush();
				dragArray->Append(take);
				HandleMouseDrag(msg, DRAGTYPE_ATOMARRAY, dragArray, 0);
			}
		}
		else
		{
			td->SetCurrentTake(take);
			EventAdd();
		}
		Rebuild();
		Redraw();
		return true;
	}

	if (channel == BFM_INPUT_MOUSERIGHT)
	{
		BaseTake* take = TakeByIndex(doc, row);
		if (!take)
			return true;

		// current take assignments (for checkmarks in the submenus)
		std::vector<BaseObject*> cams;
		TCCollectCameras(doc, cams);
		std::vector<RenderData*> rds;
		TCCollectRenderData(doc, rds);
		BaseObject* curCam = take->GetCamera(td);
		RenderData* curRd = take->GetRenderData(td);

		BaseContainer menu;
		menu.InsData(POP_TAKE_SETTINGS, String("Take Settings..."));
		menu.InsData(0, String(""));
		menu.InsData(POP_TAKE_SETCUR, String("Set As Current"));
		menu.InsData(POP_TAKE_ADDCLIP, String("Add Clip At Playhead"));
		menu.InsData(0, String(""));

		// --- detailed take settings
		BaseContainer camMenu;
		camMenu.InsData(1, String("Camera"));
		camMenu.InsData(POP_TAKE_CLEARCAM, String("(inherit)") + (curCam ? String("") : String("  (x)")));
		for (Int32 i = 0; i < (Int32)cams.size(); ++i)
			camMenu.InsData(POP_TAKECAM_BASE + i, String(cams[i]->GetName()) + ((cams[i] == curCam) ? String("  (x)") : String("")));
		menu.InsData(0, camMenu);

		BaseContainer rdMenu;
		rdMenu.InsData(1, String("Render Settings"));
		rdMenu.InsData(POP_TAKE_CLEARRD, String("(inherit)") + (curRd ? String("") : String("  (x)")));
		for (Int32 i = 0; i < (Int32)rds.size(); ++i)
			rdMenu.InsData(POP_TAKERD_BASE + i, String(rds[i]->GetName()) + ((rds[i] == curRd) ? String("  (x)") : String("")));
		menu.InsData(0, rdMenu);

		menu.InsData(POP_TAKE_MARK, String(take->IsChecked() ? "Marked For Render: ON" : "Marked For Render: OFF"));
		menu.InsData(0, String(""));
		menu.InsData(POP_TAKE_NEWCHILD, String("New Child Take"));
		menu.InsData(POP_TAKE_DUP, String("Duplicate Take"));
		menu.InsData(POP_TAKE_RENAME, String("Rename..."));
		menu.InsData(POP_TAKE_DELETE, String("Delete Take"));

		const Int32 res = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, menu);

		switch (res)
		{
			case POP_TAKE_SETTINGS:
				TCOpenSettings(NOTOK, take->GetName());
				break;
			case POP_TAKE_SETCUR:
				td->SetCurrentTake(take);
				EventAdd();
				break;
			case POP_TAKE_ADDCLIP:
				if (timeline)
					timeline->AddClipForTake(take->GetName());
				break;
			case POP_TAKE_CLEARCAM:
				take->SetCamera(td, nullptr);
				EventAdd();
				break;
			case POP_TAKE_CLEARRD:
				take->SetRenderData(td, nullptr);
				EventAdd();
				break;
			case POP_TAKE_MARK:
				take->SetChecked(!take->IsChecked());
				EventAdd();
				break;
			case POP_TAKE_NEWCHILD:
				td->AddTake(String(), take, nullptr);
				EventAdd();
				break;
			case POP_TAKE_DUP:
			{
				BaseTake* dup = td->AddTake(String(take->GetName()) + String(" copy"), take->GetUp(), take);
				if (dup)
					EventAdd();
				break;
			}
			case POP_TAKE_RENAME:
			{
				String name = take->GetName();
				if (RenameDialog(&name))
					TCRenameTake(doc, take, name); // also rebinds clips in the sequencer
				break;
			}
			case POP_TAKE_DELETE:
				if (!take->IsMain() && QuestionDialog(String("Delete take '") + String(take->GetName()) + String("'?")))
				{
					td->DeleteTake(take);
					EventAdd();
				}
				break;
			default:
				if (res >= POP_TAKECAM_BASE && res < POP_TAKECAM_BASE + (Int32)cams.size())
				{
					take->SetCamera(td, cams[res - POP_TAKECAM_BASE]);
					EventAdd();
				}
				else if (res >= POP_TAKERD_BASE && res < POP_TAKERD_BASE + (Int32)rds.size())
				{
					take->SetRenderData(td, rds[res - POP_TAKERD_BASE]);
					EventAdd();
				}
				break;
		}
		Rebuild();
		Redraw();
		return true;
	}
	return true;
}

// ================================================================== sheet area

// shared dark-theme colors for the custom tabs
static const Vector SH_BG(0.094, 0.094, 0.098);
static const Vector SH_TITLE(0.137, 0.137, 0.145);
static const Vector SH_ROW_A(0.118, 0.118, 0.122);
static const Vector SH_ROW_B(0.104, 0.104, 0.108);
static const Vector SH_TEXT(0.82, 0.82, 0.84);
static const Vector SH_DIM(0.46, 0.46, 0.50);
static const Vector SH_ACCENT(1.00, 0.62, 0.12);

// column anchors of the sheet table
static const Int32 SHC_SWATCH = 10;
static const Int32 SHC_NAME = 30;
static const Int32 SHC_TAKE = 200;
static const Int32 SHC_CAM = 360;
static const Int32 SHC_RANGE = 500;
static const Int32 SHC_LEN = 600;
static const Int32 SHC_STATUS = 650;
static const Int32 SHC_RENDER = 745;
static const Int32 SHC_NOTE = 860;

Bool TCSheetArea::GetMinSize(Int32& w, Int32& h)
{
	w = 400;
	h = 120;
	return true;
}

Int32 TCSheetArea::RowAt(Int32 my) const
{
	const Int32 top = TITLE_H + COLHEAD_H;
	if (my < top)
		return NOTOK;
	const Int32 r = (my - top + scrollY) / ROW_H;
	if (r < 0 || r >= (Int32)rows.size())
		return NOTOK;
	return r;
}

void TCSheetArea::Rebuild()
{
	rows.clear();
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!doc || !m)
		return;
	TakeData* td = doc->GetTakeData();

	std::vector<const TCClip*> ordered;
	for (const auto& c : m->clips)
		ordered.push_back(&c);
	std::sort(ordered.begin(), ordered.end(), [](const TCClip* a, const TCClip* b) { return a->start < b->start; });

	for (const TCClip* c : ordered)
	{
		Row r;
		r.clipId = c->id;
		r.color = c->color;
		r.name = c->DisplayName();
		r.take = c->takeName;
		r.status = (c->status >= 0 && c->status < TC_STATUS_COUNT) ? c->status : 0;
		r.range = IStr(c->start) + String(" - ") + IStr(c->end - 1);
		r.len = IStr(c->Length());
		r.render = TCRenderStatusText(*c);
		r.note = c->note;

		BaseTake* take = TCFindTakeByName(doc, c->takeName);
		r.missing = (take == nullptr);
		if (take && td)
		{
			BaseObject* camObj = take->GetCamera(td);
			if (!camObj)
			{
				BaseTake* rt = nullptr;
				camObj = take->GetEffectiveCamera(td, rt);
			}
			if (camObj)
				r.cam = camObj->GetName();
		}
		rows.push_back(r);
	}
}

void TCSheetArea::DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg)
{
	OffScreenOn();
	DrawSetFont(FONT_STANDARD);
	const Int32 fh = DrawGetFontHeight();
	const Int32 w = GetWidth();
	const Int32 h = GetHeight();

	DrawSetPen(SH_BG);
	DrawRectangle(0, 0, w, h);

	// title bar
	DrawSetPen(SH_TITLE);
	DrawRectangle(0, 0, w, TITLE_H - 1);
	DrawSetTextCol(SH_TEXT, COLOR_TRANS);
	DrawText(String("SHEET"), 10, (TITLE_H - fh) / 2);
	DrawSetTextCol(SH_DIM, COLOR_TRANS);
	DrawText(String("click = go to clip | double-click = settings | RMB = menu"), 90, (TITLE_H - fh) / 2);
	{
		const String cnt = IStr((Int64)rows.size()) + String(" clips");
		DrawText(cnt, w - DrawGetTextWidth(cnt) - 10, (TITLE_H - fh) / 2);
	}

	// column headers
	DrawSetPen(Vector(0.110, 0.110, 0.114));
	DrawRectangle(0, TITLE_H, w, TITLE_H + COLHEAD_H - 1);
	DrawSetTextCol(SH_DIM, COLOR_TRANS);
	const Int32 hy = TITLE_H + (COLHEAD_H - fh) / 2;
	DrawText(String("CLIP"), SHC_NAME, hy);
	DrawText(String("TAKE"), SHC_TAKE, hy);
	DrawText(String("CAMERA"), SHC_CAM, hy);
	DrawText(String("RANGE"), SHC_RANGE, hy);
	DrawText(String("LEN"), SHC_LEN, hy);
	DrawText(String("STATUS"), SHC_STATUS, hy);
	DrawText(String("RENDER"), SHC_RENDER, hy);
	DrawText(String("NOTE"), SHC_NOTE, hy);

	const Int32 top = TITLE_H + COLHEAD_H;
	const Int32 maxScroll = ((Int32)rows.size() * ROW_H) - (h - top);
	if (scrollY > (maxScroll > 0 ? maxScroll : 0))
		scrollY = (maxScroll > 0 ? maxScroll : 0);

	const Int32 selId = timeline ? timeline->selId : NOTOK;

	for (Int32 i = 0; i < (Int32)rows.size(); ++i)
	{
		const Int32 ry = top + i * ROW_H - scrollY;
		if (ry + ROW_H < top || ry > h)
			continue;
		const Row& r = rows[i];

		// zebra background; selected clip row gets a highlight + accent spine
		if (r.clipId == selId)
		{
			UAFillRoundRect(*this, 2, ry, w - 3, ry + ROW_H - 2, 5, Vector(0.165, 0.165, 0.176));
			UAFillRoundRect(*this, 0, ry + 1, 2, ry + ROW_H - 3, 1, SH_ACCENT);
		}
		else if (i == hoverRow)
		{
			UAFillRoundRect(*this, 2, ry, w - 3, ry + ROW_H - 2, 5, Vector(0.148, 0.148, 0.155));
		}
		else
		{
			DrawSetPen((i & 1) ? SH_ROW_B : SH_ROW_A);
			DrawRectangle(0, ry, w, ry + ROW_H - 2);
		}

		const Int32 ty = ry + (ROW_H - fh) / 2;

		// color swatch
		UAFillRoundRect(*this, SHC_SWATCH, ry + 6, SHC_SWATCH + 12, ry + ROW_H - 8, 3, r.color);

		auto cell = [&](const String& s, Int32 colX, Int32 maxX, const Vector& col) {
			if (!s.IsPopulated())
				return;
			SetClippingRegion(colX, ry, maxX - colX - 8, ROW_H);
			DrawSetTextCol(col, COLOR_TRANS);
			DrawText(s, colX, ty);
			ClearClippingRegion();
			SetClippingRegion(0, 0, w, h);
		};
		cell(r.name, SHC_NAME, SHC_TAKE, SH_TEXT);
		cell(r.take + (r.missing ? String("  (missing!)") : String()), SHC_TAKE, SHC_CAM, r.missing ? Vector(0.90, 0.40, 0.35) : SH_DIM);
		cell(r.cam.IsPopulated() ? r.cam : String("—"), SHC_CAM, SHC_RANGE, Vector(0.45, 0.55, 0.65));
		cell(r.range, SHC_RANGE, SHC_LEN, SH_TEXT);
		cell(r.len, SHC_LEN, SHC_STATUS, SH_DIM);

		// status pill
		{
			const String st(TCStatusNames()[r.status]);
			const Int32 tw = DrawGetTextWidth(st);
			const Vector bg = (r.status == 1) ? Vector(0.18, 0.42, 0.85) : TCStatusColor(r.status);
			UAFillRoundRect(*this, SHC_STATUS, ry + 4, SHC_STATUS + tw + 12, ry + ROW_H - 6, (ROW_H - 10) / 2, bg);
			DrawSetTextCol(Vector(0.97), COLOR_TRANS);
			DrawText(st, SHC_STATUS + 6, ty);
		}

		// render status (accent when in progress, green-ish when done)
		{
			Vector rc = SH_DIM;
			if (r.render == String("done"))
				rc = Vector(0.35, 0.75, 0.40);
			else if (!(r.render == String("—")))
				rc = SH_ACCENT;
			cell(r.render, SHC_RENDER, SHC_NOTE, rc);
		}
		cell(r.note, SHC_NOTE, w, SH_DIM);
	}

	if (rows.empty())
	{
		DrawSetTextCol(SH_DIM, COLOR_TRANS);
		DrawText(String("No clips yet — add clips on the Timeline tab"), 30, top + 20);
	}
}

Int32 TCSheetArea::Message(const BaseContainer& msg, BaseContainer& result)
{
	switch (msg.GetId())
	{
		case BFM_CURSORINFO_REMOVE:
			if (hoverRow != NOTOK)
			{
				hoverRow = NOTOK;
				Redraw();
			}
			break;
		case BFM_GETCURSORINFO:
		{
			BaseContainer state;
			if (GetInputState(BFM_INPUT_MOUSE, BFM_INPUT_MOUSELEFT, state))
			{
				Int32 mx = state.GetInt32(BFM_INPUT_X);
				Int32 my = state.GetInt32(BFM_INPUT_Y);
				Global2Local(&mx, &my);
				const Int32 r = RowAt(my);
				if (r != hoverRow)
				{
					hoverRow = r;
					Redraw();
				}
				result.SetInt32(RESULT_CURSOR, (r != NOTOK) ? MOUSE_POINT_HAND : MOUSE_NORMAL);
				return true;
			}
			break;
		}
		default:
			break;
	}
	return GeUserArea::Message(msg, result);
}

Bool TCSheetArea::InputEvent(const BaseContainer& msg)
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!doc || !m)
		return false;
	if (msg.GetInt32(BFM_INPUT_DEVICE) != BFM_INPUT_MOUSE)
		return false;
	const Int32 channel = msg.GetInt32(BFM_INPUT_CHANNEL);

	Int32 mx = msg.GetInt32(BFM_INPUT_X);
	Int32 my = msg.GetInt32(BFM_INPUT_Y);
	Global2Local(&mx, &my);
	const Bool dbl = msg.GetBool(BFM_INPUT_DOUBLECLICK);

	if (channel == BFM_INPUT_MOUSEWHEEL)
	{
		const Float val = msg.GetFloat(BFM_INPUT_VALUE);
		scrollY += (val > 0) ? -ROW_H * 2 : ROW_H * 2;
		if (scrollY < 0)
			scrollY = 0;
		Redraw();
		return true;
	}

	const Int32 row = RowAt(my);
	if (row == NOTOK)
		return true;
	TCClip* c = m->FindClip(rows[row].clipId);
	if (!c)
		return true;

	if (channel == BFM_INPUT_MOUSELEFT)
	{
		if (timeline)
			timeline->SelectOnly(c->id);
		if (dbl)
		{
			if (TCOpenSettings(c->id, c->takeName))
			{
				Rebuild();
				EventAdd();
			}
		}
		else
		{
			TCSetDocFrame(doc, c->start);
		}
		if (timeline)
			timeline->Redraw();
		Redraw();
		return true;
	}

	if (channel == BFM_INPUT_MOUSERIGHT)
	{
		BaseContainer menu;
		menu.InsData(1, String("Go To Clip Start"));
		menu.InsData(2, String("Settings..."));
		menu.InsData(3, String((c->renderState == 2) ? "Render Done: ON" : "Render Done: OFF"));
		const Int32 res = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, menu);
		if (res == 1)
		{
			if (timeline)
				timeline->SelectOnly(c->id);
			TCSetDocFrame(doc, c->start);
		}
		else if (res == 2)
		{
			TCOpenSettings(c->id, c->takeName);
		}
		else if (res == 3)
		{
			TCAddUndo(doc);
			c->renderState = (c->renderState == 2) ? 0 : 2;
		}
		Rebuild();
		Redraw();
		if (timeline)
			timeline->Redraw();
		return true;
	}
	return true;
}

// ================================================================== help area

Bool TCHelpArea::GetMinSize(Int32& w, Int32& h)
{
	w = 400;
	h = 120;
	return true;
}

namespace help
{

struct Line
{
	Int32 kind; // 0 = section header, 1 = key+description, 2 = plain text, 3 = spacer
	const Char* key;
	const Char* text;
};

static const Line LINES[] = {
	{ 0, nullptr, "MOUSE — TIMELINE" },
	{ 1, "LMB ruler", "scrub (takes switch live)" },
	{ 1, "LMB clip", "move clip; vertical = change track; edges = trim" },
	{ 1, "Cam handle", "drag the bottom strip of a clip = offset camera keys in time" },
	{ 1, "Ctrl+drag clip", "duplicate and drag the copy" },
	{ 1, "Double-click", "rename clip / track / take / marker" },
	{ 1, "RMB click", "context menu (clip, track, audio, takes)" },
	{ 1, "RMB drag", "zoom around the cursor" },
	{ 1, "MMB / Alt+drag", "pan" },
	{ 1, "Wheel", "zoom  |  Shift = pan  |  Ctrl = track height" },
	{ 1, "Header edge", "drag a track boundary in the header = resize tracks" },
	{ 1, "Drag take", "drop a take from SOURCE TAKES onto a track = new clip" },
	{ 3, nullptr, nullptr },
	{ 0, nullptr, "KEYS (timeline focused)" },
	{ 1, "Space", "play / stop" },
	{ 1, "A", "add clip for current take at playhead" },
	{ 1, "S", "split clip at playhead" },
	{ 1, "D", "duplicate selected clip" },
	{ 1, "F", "focus mode on selected clip" },
	{ 1, "M", "add marker at playhead" },
	{ 1, "I / O", "set IN / OUT at playhead" },
	{ 1, "H / Home", "fit view" },
	{ 1, "Left / Right", "nudge clip 1 frame (Shift = 10)" },
	{ 1, "Delete", "delete selected clip" },
	{ 3, nullptr, nullptr },
	{ 0, nullptr, "TITLE BAR" },
	{ 1, "AUTO", "switch takes automatically on scrub / playback" },
	{ 1, "MAIN", "fall back to the Main take outside any clip" },
	{ 1, "SNAP", "snap clip edges to clips, markers, IN/OUT, playhead" },
	{ 1, "HUD", "colored viewport border + active take name" },
	{ 3, nullptr, nullptr },
	{ 0, nullptr, "RENDER" },
	{ 1, "Render Edit", "per clip: Render Setting with the clip range, take marked -> Render All Marked Takes" },
	{ 1, "Render Now", "per clip: isolated take document into the native Render Queue (TC_Render folder)" },
	{ 1, "Sheet", "live render status per clip: queued / rendering n/m / done" },
	{ 3, nullptr, nullptr },
	{ 0, nullptr, "AUDIO" },
	{ 2, nullptr, "Load a PCM WAV via the toolbar or RMB on the REF AUDIO track. The waveform is draggable" },
	{ 2, nullptr, "(offset) and audible during playback via the managed 'TC Audio' sound track." },
	{ 3, nullptr, nullptr },
	{ 0, nullptr, "NOTES" },
	{ 2, nullptr, "Clips reference takes by name; renaming via Take Control rebinds clips automatically." },
	{ 2, nullptr, "Plugin IDs in this build are development IDs — replace before distribution." },
};

} // namespace help

void TCHelpArea::DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg)
{
	OffScreenOn();
	DrawSetFont(FONT_STANDARD);
	const Int32 fh = DrawGetFontHeight();
	const Int32 w = GetWidth();
	const Int32 h = GetHeight();
	const Int32 LH = fh + 7;

	DrawSetPen(SH_BG);
	DrawRectangle(0, 0, w, h);

	// title bar
	DrawSetPen(SH_TITLE);
	DrawRectangle(0, 0, w, 23);
	DrawSetTextCol(SH_TEXT, COLOR_TRANS);
	DrawText(String("HELP"), 10, (24 - fh) / 2);
	DrawSetTextCol(SH_DIM, COLOR_TRANS);
	DrawText(String("TakeGear — timeline-driven Take switcher"), 70, (24 - fh) / 2);

	const Int32 n = (Int32)(sizeof(help::LINES) / sizeof(help::LINES[0]));
	const Int32 contentH = n * LH + 40;
	const Int32 maxScroll = contentH - (h - 24);
	if (scrollY > (maxScroll > 0 ? maxScroll : 0))
		scrollY = (maxScroll > 0 ? maxScroll : 0);

	Int32 y = 36 - scrollY;
	for (Int32 i = 0; i < n; ++i)
	{
		const help::Line& L = help::LINES[i];
		if (y > h)
			break;
		if (y + LH > 24)
		{
			switch (L.kind)
			{
				case 0:
					DrawSetTextCol(SH_ACCENT, COLOR_TRANS);
					DrawText(String(L.text), 14, y);
					DrawSetPen(Vector(0.20, 0.20, 0.21));
					DrawRectangle(14 + DrawGetTextWidth(String(L.text)) + 10, y + fh / 2, w - 20, y + fh / 2);
					break;
				case 1:
				{
					const String key(L.key);
					const Int32 kw = DrawGetTextWidth(key);
					UAFillRoundRect(*this, 24, y - 2, 24 + kw + 12, y + fh + 2, (fh + 4) / 2, Vector(0.185, 0.185, 0.195));
					DrawSetTextCol(SH_TEXT, COLOR_TRANS);
					DrawText(key, 30, y);
					DrawSetTextCol(SH_DIM, COLOR_TRANS);
					DrawText(String(L.text), 175, y);
					break;
				}
				case 2:
					DrawSetTextCol(SH_DIM, COLOR_TRANS);
					DrawText(String(L.text), 24, y);
					break;
				default:
					break;
			}
		}
		y += (L.kind == 3) ? LH / 2 : LH;
	}
}

Bool TCHelpArea::InputEvent(const BaseContainer& msg)
{
	if (msg.GetInt32(BFM_INPUT_DEVICE) != BFM_INPUT_MOUSE)
		return false;
	if (msg.GetInt32(BFM_INPUT_CHANNEL) == BFM_INPUT_MOUSEWHEEL)
	{
		const Float val = msg.GetFloat(BFM_INPUT_VALUE);
		scrollY += (val > 0) ? -40 : 40;
		if (scrollY < 0)
			scrollY = 0;
		Redraw();
		return true;
	}
	return false;
}

// ================================================================== dialog

Bool TCDialog::CreateLayout()
{
	SetTitle(String("TakeGear"));

	GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String(), 0);
	{
		// ---------- toolbar row 1: flat icon buttons with tooltips
		GroupBegin(0, BFH_SCALEFIT, 0, 1, String(), 0);
		{
			GroupBorderSpace(6, 4, 4, 0);
			struct ToolDef
			{
				Int32 id; // 0 = separator
				const Char* tip;
			};
			static const ToolDef tools[] = {
				{ GID_BTN_ADDCLIP, "Add Clip for current Take (A)" },
				{ GID_BTN_SPLIT, "Split clip at playhead (S)" },
				{ GID_BTN_DUP, "Duplicate selected clip (D)" },
				{ GID_BTN_DELETE, "Delete selected clip (Del)" },
				{ 0, nullptr },
				{ GID_BTN_MARKER, "Add marker at playhead (M)" },
				{ GID_BTN_SETIN, "Set IN at playhead (I)" },
				{ GID_BTN_SETOUT, "Set OUT at playhead (O)" },
				{ GID_BTN_FOCUS, "Focus selected clip (F)" },
				{ 0, nullptr },
				{ GID_BTN_PREVIEW, "Preview Edit: loop IN..OUT and play (Space = play/stop)" },
				{ GID_BTN_RENDER, "Render Edit: prepare Render Settings + mark Takes" },
				{ GID_BTN_RENDERNOW, "Render Now: batch all clips via Render Queue" },
				{ GID_BTN_RANGE, "Write IN/OUT range into active Render Settings" },
				{ 0, nullptr },
				{ GID_BTN_AUDIO, "Load reference audio (WAV)" },
				{ GID_BTN_AUDIO_CLEAR, "Clear reference audio" },
				{ 0, nullptr },
				{ GID_BTN_ADDTRACK, "Add track" },
				{ GID_BTN_DELTRACK, "Delete current track" },
				{ GID_BTN_REFRESH, "Refresh all panels" },
			};
			for (const auto& td : tools)
			{
				if (!td.id)
				{
					AddSeparatorV(8, BFV_FIT);
					continue;
				}
				BaseContainer bc;
				bc.SetBool(BITMAPBUTTON_BUTTON, true);
				bc.SetString(BITMAPBUTTON_TOOLTIP, String(td.tip));
				BitmapButtonCustomGui* btn = (BitmapButtonCustomGui*)AddCustomGui(td.id, CUSTOMGUI_BITMAPBUTTON, String(), BFH_LEFT | BFV_CENTER, 26, 26, bc);
				if (btn)
				{
					BaseBitmap* bmp = icon::Build(td.id);
					if (bmp)
					{
						btn->SetImage(bmp, true);
						BaseBitmap::Free(bmp);
					}
				}
			}
		}
		GroupEnd();

		// (Auto Switch / Fallback / Snap / HUD live as chips in the sequencer
		// title bar now — no second toolbar row.)

		// ---------- tabs
		TabGroupBegin(GID_TABS, BFH_SCALEFIT | BFV_SCALEFIT, TAB_TABS);
		{
			// ====== TIMELINE TAB
			GroupBegin(GID_TAB_TIMELINE, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String("Timeline"), 0);
			{
				// weighted 2-column group: the divider between the takes panel and
				// the sequencer is user-draggable
				GroupBegin(GID_GRP_SPLIT, BFH_SCALEFIT | BFV_SCALEFIT, 2, 1, String(), BFV_GRIDGROUP_ALLOW_WEIGHTS);
				{
					// -- source takes panel (custom list)
					GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String(), 0);
					{
						C4DGadget* takesGadget = AddUserArea(GID_UA_TAKES, BFH_SCALEFIT | BFV_SCALEFIT, 70, 0);
						if (takesGadget)
							AttachUserArea(_takesUa, GID_UA_TAKES);
						AddButton(GID_BTN_NEWTAKE, BFH_SCALEFIT, 0, 0, String("New Take"));
					}
					GroupEnd();

					// -- timeline user area
					C4DGadget* uaGadget = AddUserArea(GID_UA_TIMELINE, BFH_SCALEFIT | BFV_SCALEFIT);
					if (uaGadget)
						AttachUserArea(_ua, GID_UA_TIMELINE);
				}
				GroupEnd();

				// initial splitter weights: ~16% takes panel / 84% sequencer
				BaseContainer wts;
				wts.SetInt32(GROUPWEIGHTS_PERCENT_W_CNT, 2);
				wts.SetFloat(GROUPWEIGHTS_PERCENT_W_VAL + 0, 16.0);
				wts.SetFloat(GROUPWEIGHTS_PERCENT_W_VAL + 1, 84.0);
				GroupWeightsLoad(GID_GRP_SPLIT, wts);

				// -- clip inspector row: controls are self-explanatory, no labels
				GroupBegin(GID_GRP_PROPS, BFH_SCALEFIT, 0, 1, String(), 0);
				{
					GroupBorderSpace(6, 3, 6, 4);
					AddColorField(GID_COLOR_CLIP, BFH_LEFT, 34, 13, DR_COLORFIELD_NO_BRIGHTNESS);
					AddEditText(GID_EDIT_CLIPNAME, BFH_LEFT, 150, 0);
					AddComboBox(GID_COMBO_TAKE, BFH_LEFT, 150, 0);
					AddComboBox(GID_COMBO_STATUS, BFH_LEFT, 96, 0);
					AddCheckbox(GID_CHK_CAMFOLLOW, BFH_LEFT, 0, 0, String("Cam"));
					AddSeparatorV(8, BFV_FIT);
					AddEditText(GID_EDIT_NOTE, BFH_SCALEFIT, 200, 0);
					AddStaticText(GID_TXT_CLIPINFO, BFH_RIGHT, 0, 0, String("              "), 0);
				}
				GroupEnd();
			}
			GroupEnd();

			// ====== SHEET TAB (custom drawn table)
			GroupBegin(GID_TAB_SHEET, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String("Sheet"), 0);
			{
				C4DGadget* sheetGadget = AddUserArea(GID_UA_SHEET, BFH_SCALEFIT | BFV_SCALEFIT);
				if (sheetGadget)
					AttachUserArea(_sheetUa, GID_UA_SHEET);
			}
			GroupEnd();

			// ====== HELP TAB (custom drawn page)
			GroupBegin(GID_TAB_HELP, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String("Help"), 0);
			{
				C4DGadget* helpGadget = AddUserArea(GID_UA_HELP, BFH_SCALEFIT | BFV_SCALEFIT);
				if (helpGadget)
					AttachUserArea(_helpUa, GID_UA_HELP);
				GroupBegin(0, BFH_SCALEFIT, 0, 1, String(), 0);
				{
					GroupBorderSpace(4, 2, 4, 4);
					AddButton(GID_BTN_DEBUG, BFH_LEFT, 0, 0, String("Write Debug Report"));
					AddStaticText(0, BFH_SCALEFIT, 0, 0, String("TakeGear ") + String(TC_VERSION_STRING), 0);
				}
				GroupEnd();
			}
			GroupEnd();
		}
		GroupEnd(); // tabs
	}
	GroupEnd();

	_takesUa.timeline = &_ua;
	_sheetUa.timeline = &_ua;
	return true;
}

Bool TCDialog::InitValues()
{
	// status combo entries are static
	FreeChildren(GID_COMBO_STATUS);
	for (Int32 i = 0; i < TC_STATUS_COUNT; ++i)
		AddChild(GID_COMBO_STATUS, i, String(TCStatusNames()[i]));

	SetTimer(125);
	RefreshAll();

	// restore the audio waveform cache after loading a document
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (m && m->audioPath.IsPopulated() && m->audioPeaks.empty())
		TCLoadAudioPeaks(*m);
	return true;
}

// ------------------------------------------------------------------ rebuilds

UInt32 TCDialog::ComputeTakesHash(BaseDocument* doc)
{
	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);
	UInt32 h = 2166136261u;
	TakeData* td = doc ? doc->GetTakeData() : nullptr;
	BaseTake* cur = td ? td->GetCurrentTake() : nullptr;
	for (auto& p : takes)
	{
		const String n = p.first->GetName();
		h = (h ^ (UInt32)n.GetHashCode()) * 16777619u;
		h = (h ^ (UInt32)p.second) * 16777619u;
		h = (h ^ (UInt32)(p.first == cur ? 1 : 0)) * 16777619u;
		h = (h ^ (UInt32)(p.first->IsChecked() ? 1 : 0)) * 16777619u;
	}
	return h;
}

void TCDialog::RebuildTakesPanel()
{
	BaseDocument* doc = GetActiveDocument();
	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);

	_takeNames.clear();
	for (Int32 i = 0; i < (Int32)takes.size(); ++i)
		_takeNames.push_back(takes[i].first->GetName());

	_takesUa.Rebuild();
	_takesUa.Redraw();

	// take combo in the props row
	_block = true;
	FreeChildren(GID_COMBO_TAKE);
	for (Int32 i = 0; i < (Int32)_takeNames.size(); ++i)
		AddChild(GID_COMBO_TAKE, i, _takeNames[i]);
	_block = false;

	// (current take name is shown in the sequencer title bar)
}

void TCDialog::RebuildSheet()
{
	_sheetUa.Rebuild();
	_sheetUa.Redraw();
}

void TCDialog::SyncProps()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	const TCClip* c = (m && _ua.selId != NOTOK) ? m->FindClip(_ua.selId) : nullptr;

	_block = true;
	const Bool on = (c != nullptr);
	Enable(GID_EDIT_CLIPNAME, on);
	Enable(GID_COMBO_TAKE, on);
	Enable(GID_COLOR_CLIP, on);
	Enable(GID_COMBO_STATUS, on);
	Enable(GID_CHK_CAMFOLLOW, on);
	Enable(GID_EDIT_NOTE, on);

	if (c)
	{
		SetString(GID_EDIT_CLIPNAME, c->name);
		Int32 takeIdx = NOTOK;
		for (Int32 i = 0; i < (Int32)_takeNames.size(); ++i)
			if (_takeNames[i] == c->takeName)
			{
				takeIdx = i;
				break;
			}
		if (takeIdx != NOTOK)
			SetInt32(GID_COMBO_TAKE, takeIdx);
		SetColorField(GID_COLOR_CLIP, c->color, 1.0, 1.0, 0);
		SetInt32(GID_COMBO_STATUS, c->status);
		SetBool(GID_CHK_CAMFOLLOW, c->camFollow);
		SetString(GID_EDIT_NOTE, c->note);
		SetString(GID_TXT_CLIPINFO, String("#") + IStr(c->id) + String("  [") + IStr(c->start) + String(" - ") + IStr(c->end - 1) + String("]  len ") + IStr(c->Length()));
	}
	else
	{
		SetString(GID_EDIT_CLIPNAME, String());
		SetString(GID_EDIT_NOTE, String());
		SetString(GID_TXT_CLIPINFO, String("no clip selected"));
	}
	_block = false;
}

void TCDialog::SyncSettings()
{
	// settings now live as chips inside the timeline title bar; the user area
	// reads the model directly, so a redraw is all that is needed
	_ua.Redraw();
}

void TCDialog::RefreshAll()
{
	BaseDocument* doc = GetActiveDocument();
	_takesHash = ComputeTakesHash(doc);
	RebuildTakesPanel();
	SyncSettings();
	SyncProps();
	RebuildSheet();
	_ua.Redraw();
}

// ------------------------------------------------------------------ messages

Bool TCDialog::CoreMessage(Int32 id, const BaseContainer& msg)
{
	switch (id)
	{
		case ID_TC_COMMAND: // a thumbnail finished rendering
			_ua.Redraw();
			break;
		case EVMSG_CHANGE:
		case EVMSG_DOCUMENTRECALCULATED:
		{
			// fires for EVERY frame during playback — keep this path cheap:
			// switch check is O(1) in steady state, redraw only on frame change,
			// takes-tree hashing throttled to twice a second
			BaseDocument* doc = GetActiveDocument();
			if (doc)
			{
				TCSwitchTakeForTime(doc);

				// while playing, a full panel repaint per frame competes with the
				// viewport on the GUI thread and causes stutter — cap the sequencer
				// repaint to ~7 fps during playback (instant outside of it)
				const Int32 frame = TCCurrentFrame(doc);
				if (frame != _lastFrame)
				{
					const Float64 nowUa = GeGetMilliSeconds();
					const Bool playing = CheckIsRunning(CHECKISRUNNING::ANIMATIONRUNNING);
					if (!playing || nowUa - _lastUaMs >= 150.0)
					{
						_lastFrame = frame;
						_lastUaMs = nowUa;
						_ua.Redraw();
					}
				}

				const Float64 now = GeGetMilliSeconds();
				if (now - _lastHashMs > 500.0)
				{
					_lastHashMs = now;
					const UInt32 h = ComputeTakesHash(doc);
					if (h != _takesHash)
					{
						_takesHash = h;
						RebuildTakesPanel();
						_ua.Redraw();
					}
				}
			}
			break;
		}
		default:
			break;
	}
	return GeDialog::CoreMessage(id, msg);
}

void TCDialog::Timer(const BaseContainer& msg)
{
	BaseDocument* doc = GetActiveDocument();
	if (!doc)
		return;

	const Int32 frame = TCCurrentFrame(doc);
	if (frame != _lastFrame)
	{
		TCSwitchTakeForTime(doc);
		const Float64 nowUa = GeGetMilliSeconds();
		const Bool playing = CheckIsRunning(CHECKISRUNNING::ANIMATIONRUNNING);
		if (!playing || nowUa - _lastUaMs >= 150.0)
		{
			_lastFrame = frame;
			_lastUaMs = nowUa;
			_ua.Redraw();
		}
	}

	// selection change: lightweight props sync only
	if (_ua.selId != _lastSel)
	{
		_lastSel = _ua.selId;
		SyncProps();
	}

	// structural edits: full refresh, but the sheet rebuild (render-status file
	// scans) is throttled — clip drags bump editCounter on every mouse move
	if (_ua.editCounter != _lastEdit)
	{
		const Float64 now = GeGetMilliSeconds();
		if (now - _lastSheetMs > 500.0)
		{
			_lastSheetMs = now;
			_lastEdit = _ua.editCounter;
			SyncProps();
			SyncSettings();
			RebuildSheet();
			_takesUa.Rebuild();
			_takesUa.Redraw();
		}
	}

	const Float64 now = GeGetMilliSeconds();
	if (now - _lastHashMs > 500.0)
	{
		_lastHashMs = now;
		const UInt32 h = ComputeTakesHash(doc);
		if (h != _takesHash)
		{
			_takesHash = h;
			RebuildTakesPanel();
			SyncProps();
		}
	}
}

// ------------------------------------------------------------------ commands

Bool TCDialog::Command(Int32 id, const BaseContainer& msg)
{
	if (_block)
		return true;

	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;

	// ---- toolbar
	switch (id)
	{
		case GID_BTN_ADDCLIP:
			_ua.AddClipFromCurrentTake();
			return true;
		case GID_BTN_SPLIT:
			_ua.SplitAtPlayhead();
			return true;
		case GID_BTN_DUP:
			_ua.DuplicateSelected();
			return true;
		case GID_BTN_DELETE:
			_ua.DeleteSelected();
			return true;
		case GID_BTN_MARKER:
			_ua.AddMarkerAtPlayhead();
			return true;
		case GID_BTN_SETIN:
			_ua.SetInAtPlayhead();
			return true;
		case GID_BTN_SETOUT:
			_ua.SetOutAtPlayhead();
			return true;
		case GID_BTN_FOCUS:
			_ua.ToggleFocusSelected();
			return true;
		case GID_BTN_PREVIEW:
			if (doc)
				TCPreviewEdit(doc);
			return true;
		case GID_BTN_STOP:
			if (doc)
				TCStopPlayback(doc);
			return true;
		case GID_BTN_RENDER:
			if (doc)
			{
				const Int32 n = TCRenderEdit(doc);
				RebuildSheet();
				MessageDialog(IStr(n) + String(" clip(s) prepared: per-clip Render Settings assigned, Takes marked.\n\nRender via: Render menu -> Render All Marked Takes."));
			}
			return true;
		case GID_BTN_RENDERNOW:
			if (doc)
			{
				String outDir;
				const Int32 n = TCRenderNow(doc, &outDir);
				RebuildSheet();
				if (n > 0)
				{
					if (QuestionDialog(IStr(n) + String(" clip job(s) added to the Render Queue.\nOutput: ") + outDir + String("\n\nStart rendering now?")))
						TCStartRenderQueue();
				}
				else
				{
					MessageDialog(String("No clips to render (check that clips exist and their takes are valid)."));
				}
			}
			return true;
		case GID_BTN_RANGE:
			if (doc)
				TCSetRenderRange(doc);
			return true;
		case GID_BTN_AUDIO:
			if (m)
			{
				Filename fn;
				if (fn.FileSelect(FILESELECTTYPE::ANYTHING, FILESELECT::LOAD, String("Load reference audio (PCM WAV)")))
				{
					TCAddUndo(doc);
					m->audioPath = fn.GetString();
					if (!TCLoadAudioPeaks(*m))
						MessageDialog(String("Could not read this file as PCM WAV (16/8 bit)."));
					TCSyncSoundTrack(doc); // audible during playback via a native sound track
					_ua.Redraw();
					EventAdd();
				}
			}
			return true;
		case GID_BTN_AUDIO_CLEAR:
			if (m)
			{
				TCAddUndo(doc);
				m->audioPath = String();
				m->audioPeaks.clear();
				m->audioSeconds = 0.0;
				TCSyncSoundTrack(doc); // removes the managed "TC Audio" null
				_ua.Redraw();
				EventAdd();
			}
			return true;
		case GID_BTN_ADDTRACK:
			_ua.AddTrack();
			return true;
		case GID_BTN_DELTRACK:
			_ua.DeleteCurrentTrack();
			return true;
		case GID_BTN_REFRESH:
			TCThumbClear(); // re-render clip thumbnails too
			RefreshAll();
			return true;
		case GID_BTN_NEWTAKE:
			if (doc && doc->GetTakeData())
			{
				doc->GetTakeData()->AddTake(String(), nullptr, nullptr);
				EventAdd();
				RebuildTakesPanel();
			}
			return true;
		case GID_BTN_DEBUG:
		{
			const String path = TCWriteDebugReport(doc);
			MessageDialog(String("Debug report written to:\n") + path);
			return true;
		}

		// ---- clip properties
		case GID_EDIT_CLIPNAME:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				if (c)
				{
					GetString(GID_EDIT_CLIPNAME, c->name);
					_ua.Redraw();
				}
			}
			return true;
		case GID_EDIT_NOTE:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				if (c)
				{
					GetString(GID_EDIT_NOTE, c->note);
					_ua.Redraw();
				}
			}
			return true;
		case GID_COMBO_TAKE:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				Int32 sel = 0;
				GetInt32(GID_COMBO_TAKE, sel);
				if (c && sel >= 0 && sel < (Int32)_takeNames.size())
				{
					TCAddUndo(doc);
					c->takeName = _takeNames[sel];
					TCSwitchTakeForTime(doc);
					_ua.Redraw();
					EventAdd();
				}
			}
			return true;
		case GID_COMBO_STATUS:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				Int32 sel = 0;
				GetInt32(GID_COMBO_STATUS, sel);
				if (c)
				{
					TCAddUndo(doc);
					c->status = sel;
					_ua.Redraw();
					RebuildSheet();
				}
			}
			return true;
		case GID_COLOR_CLIP:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				if (c)
				{
					Vector col;
					Float bright = 1.0;
					GetColorField(GID_COLOR_CLIP, col, bright);
					TCAddUndo(doc);
					c->color = col;
					_ua.Redraw();
					DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD);
				}
			}
			return true;
		case GID_CHK_CAMFOLLOW:
			if (m && _ua.selId != NOTOK)
			{
				TCClip* c = m->FindClip(_ua.selId);
				if (c)
				{
					TCAddUndo(doc);
					GetBool(GID_CHK_CAMFOLLOW, c->camFollow);
					_ua.Redraw();
				}
			}
			return true;
		default:
			break;
	}

	return true;
}

} // namespace tc
