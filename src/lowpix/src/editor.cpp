#include "lowpix.h"
#include "imgui.h"
#include "tinyfiledialogs.h"
#define DR_PATH_IMPLEMENTATION
#include "dr_path.h"

struct LPEPalNode
{
	LPEPalNode *next, *prev;
	struct LPPalette* pal;
	char* filename;
	bool need_save;
};
static struct LPE
{
	bool exit;
	bool need_save;
	int pal_c;
	LPEPalNode* pal_l;
	LPEPalNode* rem_paln_pending;
} lpe = { 0 };

enum LPEDialogResult
{
	LPE_DIALOG_WAIT,
	LPE_DIALOG_YES,
	LPE_DIALOG_NO,
	LPE_DIALOG_CANCEL,
};

static LPEPalNode* LPE_AddPalette(struct LPPalette* pal)
{
	LPEPalNode* n = (LPEPalNode*)lp_zalloc(sizeof(*lpe.pal_l)); n->pal = pal;
	if (lpe.pal_l) lpe.pal_l->prev = n;
	n->next = lpe.pal_l; lpe.pal_l = n;
	++lpe.pal_c; return n;
}
static void LPE_RemPalette(LPEPalNode* n)
{
	if (n->prev) n->prev->next = n->next; else lpe.pal_l = n->next;
	if (n->next) n->next->prev = n->prev;
	lp_alloc(n->pal, 0); lp_alloc(n, 0);
	--lpe.pal_c;
}
static void LPE_OpenPalette(const char* fn)
{
	if (struct LPPalette* pal = lp_pal_load(fn, 0, 0))
	{
		LPEPalNode* n = LPE_AddPalette(pal);
		n->filename = strdup(fn);
	}
}
static void LPE_Dialog_OpenPalette(void)
{	
	//"All (*.*)\0*.*\0Photoshop Palette (*.act)\0*.act\0BMP (*.bmp)\0*.bmp\0GIF (*.gif)\0*.gif\0GIMP Palette (*.gpl)\0*.gpl\0Microsoft Palette (*.pal)\0*.pal\0PCX (*.pcx)\0*.pcx\0PNG (*.png)\0*.png\0TGA (*.tga)\0*.tga\0"
	static const char* formats[] = { "*.act", "*.bin", "*.bmp", "*.gif", "*.gpl", "*.pal", "*.pcx", "*.png", "*.tga" };
	if (char* fns = const_cast<char*>(tinyfd_openFileDialog("Open Palette or Image File", "", sizeof(formats) / sizeof(*formats), formats, "Image or Palette Files", 1)))
		for (char* fn = strtok(fns, "|"); fn; fn = strtok(0, "|"))
			LPE_OpenPalette(fn);
}

static bool LPE_SavePalette(LPPalette* pal, const char* fn)
{
	return lp_pal_save(pal, fn, LP_PALETTEFORMAT_EXT) != 0;
}
static void LPE_Dialog_SavePalette(LPEPalNode* n)
{
	static const char* formats[] = { "*.act", "*.bin", "*.c", "*.gpl", "*.s" };
	if (const char* fn = tinyfd_saveFileDialog("Save Palette", "", sizeof(formats) / sizeof(*formats), formats, "Palette Files (*.act, *.bin, *.c, *.gpl, *.s)"))
	{
		if (LPE_SavePalette(n->pal, fn))
		{
			n->need_save = false;
			n->filename = strcpy((char*)lp_alloc(0, strlen(fn)+1), fn);
		}
	}
}

static LPEDialogResult LPE_Dialog_NeedSave(const char* name, const char* content)
{
	LPEDialogResult r = LPE_DIALOG_WAIT;
	if (ImGui::BeginPopupModal(name, 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImVec2 size(80, 0);
		ImGui::Text(content);
		if (ImGui::Button("YES", size)) { r = LPE_DIALOG_YES; ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (ImGui::Button("NO", size)) { r = LPE_DIALOG_NO; ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (ImGui::Button("CANCEL", size)) { r = LPE_DIALOG_CANCEL; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	return r;
}

void LPE_Tick(char* droppedFiles)
{
	//ImGui::ShowTestWindow(0);return;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	static const ImGuiStyle styledef;

	bool show_options = false;

	for (; *droppedFiles; droppedFiles += strlen(droppedFiles)+1) LPE_OpenPalette(droppedFiles);

	style.Colors[ImGuiCol_MenuBarBg] = lpe.need_save ? ImVec4(142.0f / 255.0f, 218.0f / 255.0f, 140.0f / 255.0f, styledef.Colors[ImGuiCol_MenuBarBg].w) : styledef.Colors[ImGuiCol_MenuBarBg];
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("FILE"))
		{
			if (ImGui::MenuItem("New")) { }
			if (ImGui::MenuItem("Open")) { LPE_Dialog_OpenPalette(); }
			if (ImGui::MenuItem("Save", "Ctrl+S", nullptr, lpe.need_save)) { }
			if (ImGui::MenuItem("Save As..", nullptr, nullptr, false)) { }
			ImGui::Separator();
			if (ImGui::MenuItem("Options")) show_options = true; // id scope doesn't allow us to open popup here
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { lpe.exit = true; }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("EDIT"))
		{
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (io.DisplaySize.y > 0)
	{
		auto pos = ImVec2(0, ImGui::GetFontSize() + style.FramePadding.y * 2);
		auto size = ImGui::GetIO().DisplaySize;
		size.y -= pos.y;
		ImGui::RootDock(pos, size);
	}
	lpe.need_save = false;

	LPEPalNode* rem_paln = 0;
	for (LPEPalNode* paln = lpe.pal_l; paln; paln = paln->next)
	{
		static const ImVec2 btn_sz = { 80, 0 };
		struct LPPalette* pal = paln->pal;
		char dock_name[256];
		snprintf(dock_name, sizeof(dock_name), "%s%s##%p", paln->filename ? drpath_file_name(paln->filename) : "<noname>", paln->need_save ? " *" : "", pal);
		dock_name[sizeof(dock_name)-1] = 0;
		if (ImGui::BeginDock(dock_name, false))
		{
			ImGui::PushID(pal);

			if (ImGui::Button("CLOSE", btn_sz)) rem_paln = paln;
			if (paln->filename)
			{
				ImGui::SameLine();
				if (ImGui::Button("SAVE", btn_sz)) { if (LPE_SavePalette(pal, paln->filename)) paln->need_save = false; }
			}
			ImGui::SameLine();
			if (ImGui::Button("SAVE AS", btn_sz)) { LPE_Dialog_SavePalette(paln); }

			if (ImGui::Button("CLONE", btn_sz)) LPE_AddPalette(lp_pal_clone(pal))->need_save = true;
			ImGui::SameLine();
			if (ImGui::Button("UNIQUE", btn_sz)) LPE_AddPalette(lp_pal_unique(pal))->need_save = true;

			uint32_t* c = pal->col;
			for (uint32_t i = 0; i < pal->col_count; ++i)
			{
				uint32_t col = c[i];
				ImVec4 colf = ImColor(col | 0xFF<<24).Value;
				ImGui::ColorButton(colf);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Entry %d [0x%X]\n#%02X%02X%02X (%.2f,%.2f,%.2f)", i, i, col&0xFF, (col>>8)&0xFF, (col>>16)&0xFF, colf.x, colf.y, colf.z);
				if ((i+1)%16 > 0) ImGui::SameLine();
			}

			ImGui::PopID();
		}
		ImGui::EndDock();
		if (paln->need_save) lpe.need_save = true;
	}
	if (rem_paln)
	{
		if (!rem_paln->need_save) LPE_RemPalette(rem_paln);
		else { lpe.rem_paln_pending = rem_paln; ImGui::OpenPopup("Save Palette ?"); }
	}

	if (lpe.rem_paln_pending) switch (LPE_Dialog_NeedSave("Save Palette ?", "Current palette is unsaved. Save it first ?"))
	{
	case LPE_DIALOG_NO:
		LPE_RemPalette(lpe.rem_paln_pending);
	case LPE_DIALOG_CANCEL:
		lpe.rem_paln_pending = 0;
		break;
	case LPE_DIALOG_YES:
		if (lpe.rem_paln_pending->filename) LPE_SavePalette(lpe.rem_paln_pending->pal, lpe.rem_paln_pending->filename);
		else LPE_Dialog_SavePalette(lpe.rem_paln_pending);
		lpe.rem_paln_pending = 0;
		break;
	}
}
