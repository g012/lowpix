#include "lowpix.h"
#include "imgui.h"
#include "tinyfiledialogs.h"
#define DR_PATH_IMPLEMENTATION
#include "dr_path.h"

struct LPEPalNode { LPEPalNode *next, *prev; struct LPPalette* pal; char* filename; };
static struct LPE
{
	bool exit;
	bool need_save;
	int pal_c;
	struct LPEPalNode* pal_l;
} lpe = { 0 };

static LPEPalNode* LPE_AddPalette(struct LPPalette* pal)
{
	LPEPalNode* n = (LPEPalNode*)lp_zalloc(sizeof(*lpe.pal_l)); n->pal = pal;
	if (lpe.pal_l) lpe.pal_l->prev = n;
	n->next = lpe.pal_l; lpe.pal_l = n;
	++lpe.pal_c; return n;
}
static void LPE_RemPalette(LPEPalNode* n)
{
	if (n->prev) n->prev->next = n->next; if (n->next) n->next->prev = n->prev;
	if (n == lpe.pal_l) lpe.pal_l = 0;
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
	static const char* formats[] = { "*.act", "*.bmp", "*.gif", "*.gpl", "*.pal", "*.pcx", "*.png", "*.tga" };
	if (char* fns = const_cast<char*>(tinyfd_openFileDialog("Open Palette or Image File", "", sizeof(formats) / sizeof(*formats), formats, "Image or Palette Files", 1)))
	{
		for (char* fn = strtok(fns, "|"); fn; fn = strtok(0, "|"))
			LPE_OpenPalette(fn);
	}
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

	for (LPEPalNode* paln = lpe.pal_l; paln; paln = paln->next)
	{
		struct LPPalette* pal = paln->pal;
		char dock_name[256];
		snprintf(dock_name, sizeof(dock_name), "%s##%p", drpath_file_name(paln->filename), pal);
		dock_name[sizeof(dock_name)-1] = 0;
		if (ImGui::BeginDock(dock_name, false))
		{
			ImGui::PushID(pal);
			uint32_t* c = pal->col;
			for (uint32_t i = 0; i < pal->col_count; ++i)
			{
				uint32_t col = c[i];
				ImVec4 colf = ImColor(col).Value;
				ImGui::ColorButton(colf, false, true, false);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Entry %d [0x%X]\n#%02X%02X%02X (%.2f,%.2f,%.2f)", i, i, col&0xFF, (col>>8)&0xFF, (col>>16)&0xFF, colf.x, colf.y, colf.z);
				if ((i+1)%16 > 0) ImGui::SameLine();
			}
			ImGui::PopID();
		}
		ImGui::EndDock();
	}
}
