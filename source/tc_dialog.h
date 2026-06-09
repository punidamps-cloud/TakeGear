// Take Control — main dialog.
#ifndef TC_DIALOG_H__
#define TC_DIALOG_H__

#include "tc_timeline.h"

namespace tc
{

// NLE-style source takes list (left panel)
class TCTakesArea : public GeUserArea
{
public:
	struct Row
	{
		String name;
		String cam; // directly assigned take camera (empty = inherited)
		Int32	 depth = 0;
		Bool	 checked = false;
		Bool	 hasKids = false;
		Bool	 isCurrent = false;
		Vector color = Vector(0.5);
	};
	std::vector<Row> rows;
	Int32 scrollY = 0;
	Int32 hoverRow = NOTOK;
	TCTimelineArea* timeline = nullptr;

	void Rebuild(); // refresh rows from the active document

	virtual Bool GetMinSize(Int32& w, Int32& h);
	virtual void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg);
	virtual Bool InputEvent(const BaseContainer& msg);
	virtual Int32 Message(const BaseContainer& msg, BaseContainer& result);

private:
	static const Int32 ROW_H = 20;
	static const Int32 HEAD_H = 24;
	Int32 RowAt(Int32 my) const;
	BaseTake* TakeByIndex(BaseDocument* doc, Int32 idx) const;
};

// NLE-style sheet table (custom drawn)
class TCSheetArea : public GeUserArea
{
public:
	struct Row
	{
		Int32	 clipId = NOTOK;
		Vector color = Vector(0.5);
		String name;
		String take;
		String cam;
		String range;
		String len;
		String render;
		String note;
		Int32	 status = 0;
		Bool	 missing = false; // take not found
	};
	std::vector<Row> rows;
	Int32 scrollY = 0;
	Int32 hoverRow = NOTOK;
	TCTimelineArea* timeline = nullptr;

	void Rebuild(); // refresh rows from the active document (includes render status scan)

	virtual Bool GetMinSize(Int32& w, Int32& h);
	virtual void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg);
	virtual Bool InputEvent(const BaseContainer& msg);
	virtual Int32 Message(const BaseContainer& msg, BaseContainer& result);

private:
	static const Int32 TITLE_H = 24;
	static const Int32 COLHEAD_H = 20;
	static const Int32 ROW_H = 24;
	Int32 RowAt(Int32 my) const;
};

// custom drawn help page (sections, key chips)
class TCHelpArea : public GeUserArea
{
public:
	Int32 scrollY = 0;
	virtual Bool GetMinSize(Int32& w, Int32& h);
	virtual void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg);
	virtual Bool InputEvent(const BaseContainer& msg);
};

class TCDialog : public GeDialog
{
public:
	virtual Bool CreateLayout();
	virtual Bool InitValues();
	virtual Bool Command(Int32 id, const BaseContainer& msg);
	virtual Bool CoreMessage(Int32 id, const BaseContainer& msg);
	virtual void Timer(const BaseContainer& msg);

private:
	TCTimelineArea _ua;
	TCTakesArea		 _takesUa;
	TCSheetArea		 _sheetUa;
	TCHelpArea		 _helpUa;

	std::vector<String> _takeNames;

	UInt32 _lastEdit = 0xFFFFFFFF;
	Int32	 _lastSel = -0x7FFFFFF;
	Int32	 _lastFrame = -0x7FFFFFF;
	UInt32 _takesHash = 0;
	Float64 _lastHashMs = 0.0;	// takes-tree hash is recomputed at most every 500ms
	Float64 _lastSheetMs = 0.0; // sheet rebuild (file scans!) at most every 500ms
	Float64 _lastUaMs = 0.0;		// timeline repaint cap during playback (150ms)
	Bool	 _block = false; // guard: ignore Command() while pushing values into gadgets

	void RebuildTakesPanel();
	void RebuildSheet();
	void SyncProps();
	void SyncSettings();
	UInt32 ComputeTakesHash(BaseDocument* doc);
	void RefreshAll();
};

} // namespace tc

#endif // TC_DIALOG_H__
