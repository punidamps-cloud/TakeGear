// Take Control — modal take/clip settings window implementation.
#include "tc_settings.h"

namespace tc
{

enum
{
	SD_TAKE_NAME = 1001,
	SD_TAKE_CAM,
	SD_TAKE_RD,
	SD_TAKE_MARK,
	SD_TAKE_CURRENT,

	SD_CLIP_NAME = 1101,
	SD_CLIP_TAKE,
	SD_CLIP_TRACK,
	SD_CLIP_START,
	SD_CLIP_END,
	SD_CLIP_LEN,
	SD_CLIP_COLOR,
	SD_CLIP_STATUS,
	SD_CLIP_CAMFOLLOW,
	SD_CLIP_RENDER,
	SD_CLIP_NOTE,

	SD_BTN_OK = 1900,
	SD_BTN_CANCEL
};

class TCSettingsDialog : public GeDialog
{
public:
	Int32	 clipId = NOTOK;
	String takeName;
	Bool	 applied = false;

	virtual Bool CreateLayout()
	{
		SetTitle((clipId != NOTOK) ? String("Clip / Take Settings") : String("Take Settings"));

		GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String(), 0);
		GroupBorderSpace(8, 8, 8, 8);

		// ------------------- take group
		GroupBegin(0, BFH_SCALEFIT, 2, 0, String("Take"), BFV_BORDERGROUP_FOLD_OPEN);
		GroupBorder(BORDER_GROUP_IN | BORDER_WITH_TITLE);
		GroupBorderSpace(6, 6, 6, 6);
		{
			AddStaticText(0, BFH_LEFT, 0, 0, String("Name"), 0);
			AddEditText(SD_TAKE_NAME, BFH_SCALEFIT, 220, 0);
			AddStaticText(0, BFH_LEFT, 0, 0, String("Camera"), 0);
			AddComboBox(SD_TAKE_CAM, BFH_SCALEFIT, 220, 0);
			AddStaticText(0, BFH_LEFT, 0, 0, String("Render Settings"), 0);
			AddComboBox(SD_TAKE_RD, BFH_SCALEFIT, 220, 0);
			AddStaticText(0, BFH_LEFT, 0, 0, String(""), 0);
			AddCheckbox(SD_TAKE_MARK, BFH_LEFT, 0, 0, String("Marked For Render"));
			AddStaticText(0, BFH_LEFT, 0, 0, String(""), 0);
			AddCheckbox(SD_TAKE_CURRENT, BFH_LEFT, 0, 0, String("Set As Current Take"));
		}
		GroupEnd();

		// ------------------- clip group
		if (clipId != NOTOK)
		{
			GroupBegin(0, BFH_SCALEFIT, 2, 0, String("Clip"), 0);
			GroupBorder(BORDER_GROUP_IN | BORDER_WITH_TITLE);
			GroupBorderSpace(6, 6, 6, 6);
			{
				AddStaticText(0, BFH_LEFT, 0, 0, String("Name"), 0);
				AddEditText(SD_CLIP_NAME, BFH_SCALEFIT, 220, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String("Take"), 0);
				AddComboBox(SD_CLIP_TAKE, BFH_SCALEFIT, 220, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String("Track"), 0);
				AddComboBox(SD_CLIP_TRACK, BFH_SCALEFIT, 220, 0);

				AddStaticText(0, BFH_LEFT, 0, 0, String("Start Frame"), 0);
				AddEditNumberArrows(SD_CLIP_START, BFH_LEFT, 100, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String("End Frame (incl.)"), 0);
				AddEditNumberArrows(SD_CLIP_END, BFH_LEFT, 100, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String("Length"), 0);
				AddEditNumberArrows(SD_CLIP_LEN, BFH_LEFT, 100, 0);

				AddStaticText(0, BFH_LEFT, 0, 0, String("Color"), 0);
				AddColorField(SD_CLIP_COLOR, BFH_LEFT, 60, 14, DR_COLORFIELD_NO_BRIGHTNESS);
				AddStaticText(0, BFH_LEFT, 0, 0, String("Status"), 0);
				AddComboBox(SD_CLIP_STATUS, BFH_LEFT, 140, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String("Render State"), 0);
				AddComboBox(SD_CLIP_RENDER, BFH_LEFT, 140, 0);
				AddStaticText(0, BFH_LEFT, 0, 0, String(""), 0);
				AddCheckbox(SD_CLIP_CAMFOLLOW, BFH_LEFT, 0, 0, String("Camera Follow (move cam keys with clip)"));

				AddStaticText(0, BFH_LEFT | BFV_TOP, 0, 0, String("Note"), 0);
				AddMultiLineEditText(SD_CLIP_NOTE, BFH_SCALEFIT, 220, 60, 0);
			}
			GroupEnd();
		}

		// ------------------- buttons
		GroupBegin(0, BFH_SCALEFIT, 2, 1, String(), 0);
		{
			AddButton(SD_BTN_OK, BFH_SCALEFIT, 0, 0, String("OK"));
			AddButton(SD_BTN_CANCEL, BFH_SCALEFIT, 0, 0, String("Cancel"));
		}
		GroupEnd();

		GroupEnd();
		return true;
	}

	virtual Bool InitValues()
	{
		BaseDocument* doc = GetActiveDocument();
		TCModel* m = doc ? TCGetModel(doc) : nullptr;
		TakeData* td = doc ? doc->GetTakeData() : nullptr;
		if (!doc || !td)
			return true;

		BaseTake* take = TCFindTakeByName(doc, takeName);
		TCCollectCameras(doc, _cams);
		TCCollectRenderData(doc, _rds);
		_takes.clear();
		{
			std::vector<std::pair<BaseTake*, Int32>> takes;
			TCCollectTakes(doc, takes);
			for (auto& p : takes)
				_takes.push_back(p.first->GetName());
		}

		// --- take widgets
		SetString(SD_TAKE_NAME, takeName);

		BaseObject* curCam = take ? take->GetCamera(td) : nullptr;
		FreeChildren(SD_TAKE_CAM);
		AddChild(SD_TAKE_CAM, 0, String("(inherit)"));
		Int32 camSel = 0;
		for (Int32 i = 0; i < (Int32)_cams.size(); ++i)
		{
			AddChild(SD_TAKE_CAM, i + 1, _cams[i]->GetName());
			if (_cams[i] == curCam)
				camSel = i + 1;
		}
		SetInt32(SD_TAKE_CAM, camSel);

		RenderData* curRd = take ? take->GetRenderData(td) : nullptr;
		FreeChildren(SD_TAKE_RD);
		AddChild(SD_TAKE_RD, 0, String("(inherit)"));
		Int32 rdSel = 0;
		for (Int32 i = 0; i < (Int32)_rds.size(); ++i)
		{
			AddChild(SD_TAKE_RD, i + 1, _rds[i]->GetName());
			if (_rds[i] == curRd)
				rdSel = i + 1;
		}
		SetInt32(SD_TAKE_RD, rdSel);

		SetBool(SD_TAKE_MARK, take ? take->IsChecked() : false);
		SetBool(SD_TAKE_CURRENT, take && td->GetCurrentTake() == take);

		// --- clip widgets
		const TCClip* c = (m && clipId != NOTOK) ? m->FindClip(clipId) : nullptr;
		if (c && m)
		{
			SetString(SD_CLIP_NAME, c->name);

			FreeChildren(SD_CLIP_TAKE);
			Int32 tSel = 0;
			for (Int32 i = 0; i < (Int32)_takes.size(); ++i)
			{
				AddChild(SD_CLIP_TAKE, i, _takes[i]);
				if (_takes[i] == c->takeName)
					tSel = i;
			}
			SetInt32(SD_CLIP_TAKE, tSel);

			FreeChildren(SD_CLIP_TRACK);
			for (Int32 i = 0; i < (Int32)m->tracks.size(); ++i)
				AddChild(SD_CLIP_TRACK, i, m->tracks[i].name);
			SetInt32(SD_CLIP_TRACK, c->track);

			_block = true;
			SetInt32(SD_CLIP_START, c->start, -1000000, 1000000);
			SetInt32(SD_CLIP_END, c->end - 1, -1000000, 1000000);
			SetInt32(SD_CLIP_LEN, c->Length(), 1, 1000000);
			_block = false;

			SetColorField(SD_CLIP_COLOR, c->color, 1.0, 1.0, 0);

			FreeChildren(SD_CLIP_STATUS);
			for (Int32 i = 0; i < TC_STATUS_COUNT; ++i)
				AddChild(SD_CLIP_STATUS, i, String(TCStatusNames()[i]));
			SetInt32(SD_CLIP_STATUS, c->status);

			FreeChildren(SD_CLIP_RENDER);
			AddChild(SD_CLIP_RENDER, 0, String("—"));
			AddChild(SD_CLIP_RENDER, 1, String("prepared"));
			AddChild(SD_CLIP_RENDER, 2, String("done"));
			SetInt32(SD_CLIP_RENDER, c->renderState);

			SetBool(SD_CLIP_CAMFOLLOW, c->camFollow);
			SetString(SD_CLIP_NOTE, c->note);
		}
		return true;
	}

	virtual Bool Command(Int32 id, const BaseContainer& msg)
	{
		if (_block)
			return true;
		switch (id)
		{
			// keep start / end / length consistent while editing
			case SD_CLIP_START:
			case SD_CLIP_LEN:
			{
				Int32 start = 0, len = 1;
				GetInt32(SD_CLIP_START, start);
				GetInt32(SD_CLIP_LEN, len);
				if (len < 1)
					len = 1;
				_block = true;
				SetInt32(SD_CLIP_END, start + len - 1, -1000000, 1000000);
				SetInt32(SD_CLIP_LEN, len, 1, 1000000);
				_block = false;
				return true;
			}
			case SD_CLIP_END:
			{
				Int32 start = 0, end = 0;
				GetInt32(SD_CLIP_START, start);
				GetInt32(SD_CLIP_END, end);
				if (end < start)
					end = start;
				_block = true;
				SetInt32(SD_CLIP_END, end, -1000000, 1000000);
				SetInt32(SD_CLIP_LEN, end - start + 1, 1, 1000000);
				_block = false;
				return true;
			}
			case SD_BTN_OK:
				Apply();
				applied = true;
				Close();
				return true;
			case SD_BTN_CANCEL:
				Close();
				return true;
			default:
				break;
		}
		return true;
	}

private:
	std::vector<BaseObject*> _cams;
	std::vector<RenderData*> _rds;
	std::vector<String> _takes;
	Bool _block = false;

	void Apply()
	{
		BaseDocument* doc = GetActiveDocument();
		TCModel* m = doc ? TCGetModel(doc) : nullptr;
		TakeData* td = doc ? doc->GetTakeData() : nullptr;
		if (!doc || !td || !m)
			return;

		TCAddUndo(doc);

		BaseTake* take = TCFindTakeByName(doc, takeName);
		String newName = takeName;

		// --- take settings
		if (take)
		{
			GetString(SD_TAKE_NAME, newName);
			if (newName.IsPopulated() && !(newName == takeName))
			{
				take->SetName(newName);
				// keep clip references valid after the rename
				for (auto& c : m->clips)
					if (c.takeName == takeName)
						c.takeName = newName;
			}
			else
			{
				newName = takeName;
			}

			Int32 camSel = 0;
			GetInt32(SD_TAKE_CAM, camSel);
			take->SetCamera(td, (camSel > 0 && camSel <= (Int32)_cams.size()) ? _cams[camSel - 1] : nullptr);

			Int32 rdSel = 0;
			GetInt32(SD_TAKE_RD, rdSel);
			take->SetRenderData(td, (rdSel > 0 && rdSel <= (Int32)_rds.size()) ? _rds[rdSel - 1] : nullptr);

			Bool mark = false;
			GetBool(SD_TAKE_MARK, mark);
			take->SetChecked(mark);

			Bool makeCur = false;
			GetBool(SD_TAKE_CURRENT, makeCur);
			if (makeCur && td->GetCurrentTake() != take)
				td->SetCurrentTake(take);
		}

		// --- clip settings
		TCClip* c = (clipId != NOTOK) ? m->FindClip(clipId) : nullptr;
		if (c)
		{
			GetString(SD_CLIP_NAME, c->name);

			Int32 tSel = 0;
			GetInt32(SD_CLIP_TAKE, tSel);
			if (tSel >= 0 && tSel < (Int32)_takes.size())
			{
				String sel = _takes[tSel];
				if (sel == takeName)
					sel = newName; // the take might just have been renamed
				c->takeName = sel;
			}

			Int32 track = 0;
			GetInt32(SD_CLIP_TRACK, track);
			if (track >= 0 && track < (Int32)m->tracks.size())
				c->track = track;

			Int32 start = 0, end = 0;
			GetInt32(SD_CLIP_START, start);
			GetInt32(SD_CLIP_END, end);
			if (end < start)
				end = start;
			c->start = start;
			c->end = end + 1; // model end is exclusive

			Vector col;
			Float bright = 1.0;
			GetColorField(SD_CLIP_COLOR, col, bright);
			c->color = col;

			GetInt32(SD_CLIP_STATUS, c->status);
			GetInt32(SD_CLIP_RENDER, c->renderState);
			GetBool(SD_CLIP_CAMFOLLOW, c->camFollow);
			GetString(SD_CLIP_NOTE, c->note);
		}

		TCSwitchTakeForTime(doc);
		EventAdd();
	}
};

Bool TCOpenSettings(Int32 clipId, const String& takeName)
{
	TCSettingsDialog dlg;
	dlg.clipId = clipId;
	dlg.takeName = takeName;
	dlg.Open(DLG_TYPE::MODAL_RESIZEABLE, 0, -1, -1, 460, 0);
	return dlg.applied;
}

} // namespace tc
