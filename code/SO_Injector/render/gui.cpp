//
// Created by Joel on 6/23/26.
//

#include "gui.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "menu/theme.h"
#include "menu/theme_manager.h"
#include "injector/config.h"

namespace render {

    void exampleGui(GLFWwindow* window) {
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoBackground
        );

        ImGui::Text("Linux ImGui App");

        if (ImGui::Button("Close")) {
            running = false;
        }

        ImGui::End();
    }

    void startRenderLoop(void (*drawFunction)(GLFWwindow*)) {
        glfwInit();

        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow* window =
            glfwCreateWindow(525, 480, "My App", nullptr, nullptr);


        glfwMakeContextCurrent(window);
        glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_TRUE);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        // Load user's default theme if set, otherwise apply built-in theme.
        std::string defaultTheme = config::get("default_theme");
        if (!defaultTheme.empty()) {
            if (!theme_manager::loadTheme(defaultTheme))
                theme::applyTheme();
        } else {
            theme::applyTheme();
        }

        ImGuiIO& io = ImGui::GetIO();

        while (!glfwWindowShouldClose(window) && running)
        {
            glfwPollEvents();

            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            io.DisplaySize = ImVec2((float)w, (float)h);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            drawFunction(window);

            ImGui::Render();

            glViewport(0, 0, w, h);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
