#include "lowpix.h"
#include "imgui.h"

static struct LPE
{
	bool exit;
	bool need_save;
} lpe = { 0 };

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
			if (ImGui::MenuItem("Open")) { }
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
}
