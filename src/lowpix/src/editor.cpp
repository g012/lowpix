#include "lowpix.h"
#include "imgui.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define NOC_FILE_DIALOG_WIN32
#elif defined(__APPLE_CC__) || defined(__APPLE__) || defined(__MACH__)
#define NOC_FILE_DIALOG_OSX
#else
#define NOC_FILE_DIALOG_GTK
#endif
#define NOC_FILE_DIALOG_IMPLEMENTATION
#include "noc_file_dialog.h"

static struct LPE
{
	bool exit;
	bool need_save;
	struct LPPalette* pal;
} lpe = { 0 };

static void LPE_OpenPalette(void)
{
	if (const char* fn = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "All\0*.*\0Photoshop Palette\0*.act\0BMP\0*.bmp\0GIF\0*.gif\0GIMP Palette\0*.gpl\0Microsoft Palette\0*.pal\0PCX\0*.pcx\0PNG\0*.png\0TGA\0*.tga\0", 0, 0))
	{
		if (struct LPPalette* pal = lp_pal_load(fn, 0, 0))
		{
			if (lpe.pal) lp_alloc(lpe.pal, 0);
			lpe.pal = pal;
		}
	}
}

void LPE_Tick(void)
{
	//ImGui::ShowTestWindow(0);return;

	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	static const ImGuiStyle styledef;

	bool show_options = false;

	style.Colors[ImGuiCol_MenuBarBg] = lpe.need_save ? ImVec4(142.0f / 255.0f, 218.0f / 255.0f, 140.0f / 255.0f, styledef.Colors[ImGuiCol_MenuBarBg].w) : styledef.Colors[ImGuiCol_MenuBarBg];
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("FILE"))
		{
			if (ImGui::MenuItem("New")) { }
			if (ImGui::MenuItem("Open")) { LPE_OpenPalette(); }
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

	if (ImGui::BeginDock("Palette"))
	{
		if (lpe.pal)
		{
			uint32_t* c = lpe.pal->col;
			for (uint32_t i = 0; i < lpe.pal->col_count; ++i)
			{
				ImGui::ColorButton(ImColor(c[i] | 0xFF<<24).Value, true, false);
				if ((i+1)%16 > 0) ImGui::SameLine();
			}
		}
	}
	ImGui::EndDock();
}
