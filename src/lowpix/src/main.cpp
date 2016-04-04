#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"
#include "lua.hpp"
#include <stdio.h>
#include <thread>
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#define CONF "lowpix.lua"

extern void LPE_Tick(void);

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

int main(int, char**)
{
	int window_w = 1280, window_h = 720;

	{
		lua_State* L = luaL_newstate();
		if (luaL_dofile(L, CONF) == 0)
		{
			if (lua_getglobal(L, "window_size") == LUA_TTABLE)
			{
				if (lua_getfield(L, -1, "width") == LUA_TNUMBER) window_w = (int)lua_tonumber(L, -1); lua_pop(L, 1);
				if (lua_getfield(L, -1, "height") == LUA_TNUMBER) window_h = (int)lua_tonumber(L, -1); lua_pop(L, 1);
			}
			lua_pop(L, 1);
			ImGui::LoadDock(L);
		}
		lua_close(L);
	}

    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(window_w, window_h, "lowpix", NULL, NULL);
    glfwMakeContextCurrent(window);
    gl3wInit();

    // Setup ImGui binding
    ImGui_ImplGlfwGL3_Init(window, true);
	glfwSwapInterval(0);

    ImVec4 clear_color = ImColor(114, 144, 154);

    // Main loop
	int monitorCount = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	double refreshPeriod = 1.0 / (monitorCount > 0 && monitors ? glfwGetVideoMode(monitors[0])->refreshRate : 60);
	if (refreshPeriod < 1.0/60) refreshPeriod = 1.0/60;
	double t0 = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();

		LPE_Tick();

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        glfwSwapBuffers(window);

		// delay
		double t1 = glfwGetTime();
		if (t1 - t0 >= refreshPeriod)
			t0 = t1;
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds((long long)(1000 * (refreshPeriod - (t1 - t0)))));
			t0 = glfwGetTime();
		}
    }

	{
		FILE* f = fopen(CONF, "wb");
		if (f)
		{
			int w, h;
			glfwGetWindowSize(window, &w, &h);
			fprintf(f, "window_size = { width = %d, height = %d }\n", w, h);
			ImGui::SaveDock(f);
			fclose(f);
		}
	}

    // Cleanup
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    return 0;
}

