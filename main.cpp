// Dear ImGui: standalone example application for GLFW + OpenGL2, using legacy fixed pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_glfw_opengl2/ folder**
// See imgui_impl_glfw.cpp for details.

#include "mos6502/c_6502.h" // 6502 CPU emulation
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define FONT_SZ 28.0f // max size
#define FONT_SCALE 2  // default font is FONT_SZ/FONT_SCALE

#include "imgui/imgui.h"
#include "backend/imgui_impl_glfw.h"
#include "backend/imgui_impl_opengl2.h"
#include <stdio.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

const ImVec4 IMRED = ImVec4(1, 0, 0, 1); // RGBA
const ImVec4 IMGRN = ImVec4(0, 1, 0, 1);
const ImVec4 IMBLU = ImVec4(0, 0, 1, 1);
const ImVec4 IMCYN = ImVec4(0, 1, 1, 1);
const ImVec4 IMYLW = ImVec4(1, 1, 0, 1);
const ImVec4 IMMGN = ImVec4(1, 0, 1, 1);

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

cpu_6502 *cpu; // our cpu!

volatile bool cpu_running = false;
volatile bool cpu_stepping = true;
volatile unsigned long long cpu_time = 17000; // 17000 us
uint64_t total_cycles = 0;

volatile int done = 0;

static ImFont *HexWinFont;

bool show_mem_editor = true;
bool show_gui_settings = false;
bool show_help_window = true;

void CPURun();
void *CPUThread(void *);
void CodeEditor(bool *active);
void CPURegisters(float);
void GUISettings(bool *active);
void HelpWindow(bool *active);

#define DEFAULT_RST 0x8000
#define DEFAULT_NMI 0x0200
#define DEFAULT_IRQ 0x0300

ImVec4 clear_color = ImVec4(0, 0, 0, 1.00f);

int main(int, char **)
{
    // malloc CPU
    cpu = (cpu_6502 *)malloc(sizeof(cpu_6502));
    if (cpu == NULL)
    {
        perror("main: malloc: ");
        exit(-1);
    }
    // set vectors
    static word RESET_VEC = DEFAULT_RST; // default reset location
    static word NMI_VEC = DEFAULT_NMI;   // default NMI handler
    static word IRQ_VEC = DEFAULT_IRQ;   // default IRQ/BRK handler
    // update vectors
    cpu->mem[V_RESET] = RESET_VEC;
    cpu->mem[V_RESET + 1] = RESET_VEC >> 8;
    cpu->mem[V_NMI] = NMI_VEC;
    cpu->mem[V_NMI + 1] = NMI_VEC >> 8;
    cpu->mem[V_IRQ_BRK] = IRQ_VEC;
    cpu->mem[V_IRQ_BRK + 1] = IRQ_VEC >> 8;
    // fill out demo program
    cpu->mem[0x8000] = LDA_IMM;
    cpu->mem[0x8001] = 0x0;
    cpu->mem[0x8002] = NOP;
    cpu->mem[0x8003] = JMP_IND;
    cpu->mem[0x8004] = 0x00;
    cpu->mem[0x8005] = 0x90;
    cpu->mem[0x9000] = 0x00;
    cpu->mem[0x9001] = 0xa0;
    cpu->mem[0xa000] = ADC_IMM;
    cpu->mem[0xa001] = 0x09;
    cpu->mem[0xa002] = ADC_IMM;
    cpu->mem[0xa003] = 0x05;
    cpu->mem[0xa004] = JMP_ABS;
    cpu->mem[0xa005] = 0x02;
    cpu->mem[0xa006] = 0x80;
    // set up CPU thread
    pthread_t cpu_thread;
    if (pthread_create(&cpu_thread, NULL, &CPUThread, NULL) < 0)
    {
        perror("main: Could not set up CPU thread: ");
        exit(-2);
    }
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    GLFWwindow *window = glfwCreateWindow(1280, 720, "MOS6502 Assembly Programmer", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromFileTTF("imgui/font/Roboto-Medium.ttf", FONT_SZ);
    HexWinFont = io.Fonts->AddFontFromFileTTF("imgui/font/FiraCode-Regular.ttf", FONT_SZ);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        // if (show_cpu_internals)
        //     ImGui::CPURegisterWindow(&show_cpu_internals);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.

        // 3. Show another simple12window.
        if (show_mem_editor)
        {
            CodeEditor(&show_mem_editor);
        }

        if (show_gui_settings)
        {
            GUISettings(&show_gui_settings);
        }

        if (show_help_window)
        {
            HelpWindow(&show_help_window);
        }

        CPURun();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
        // you may need to backup/reset/restore other state, e.g. for current shader using the commented lines below.
        //GLint last_program;
        //glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        //glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        //glUseProgram(last_program);

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    done = 1;
    pthread_join(cpu_thread, NULL);
    free(cpu);
    return 0;
}

void CPURun()
{
    static word RESET_VEC = DEFAULT_RST; // default reset location
    static word NMI_VEC = DEFAULT_NMI;   // default NMI handler
    static word IRQ_VEC = DEFAULT_IRQ;   // default IRQ/BRK handler

    ImGui::Begin("MOS6502");
    static float font_scale = 1.0f / FONT_SCALE;
    static float usr_font_scale = 1.0f;
    ImGui::SetWindowFontScale(font_scale * usr_font_scale);
    float win_sz_x = (5 + 8 * 2) * font_scale * usr_font_scale * FONT_SZ;
    float win_sz_y = 22 * font_scale * usr_font_scale * (FONT_SZ + 6 / font_scale / usr_font_scale);
    ImGui::SetWindowSize(ImVec2(win_sz_x, win_sz_y));
    float __usr_font_scale = usr_font_scale;
    // ImGui::PushItemWidth(15 * font_scale * usr_font_scale * FONT_SZ);
    if (ImGui::InputFloat("Text Size", &__usr_font_scale, 0.1, 0.5, "%.1f"))
    {
        if (__usr_font_scale < 0.5)
            __usr_font_scale = 0.5;
    }
    static char cpustatus[128];
    snprintf(cpustatus, sizeof(cpustatus), "Status: %s", cpu_stepping ? "Stepping" : (cpu_running ? "Running" : "Paused"));
    ImGui::Text("%s", cpustatus);
    ImGui::SameLine();
    char tmp[25];
    ImGui::Text("Instruction Time: ");
    ImGui::SameLine();
    snprintf(tmp, sizeof(tmp), "%llu ms", cpu_time / 1000);
    ImGui::PushStyleColor(0, IMCYN);
    if (ImGui::SelectableInput("cputime", false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
    {
        unsigned long long num = strtoll(tmp, NULL, 10);
        if (num > 10 * 60 * 1000)
            num = 1 * 1000; // 1 second
        if (num == 0)
            num = 17;
        cpu_time = num * 1000;
    }
    ImGui::PopStyleColor();
    ImGui::Text("Total Cycles: %llu", total_cycles);
    ImGui::PushStyleColor(0, IMYLW);
    ImGui::Separator();
    ImGui::PopStyleColor();
    CPURegisters(font_scale * usr_font_scale);
    if (ImGui::Button("Reset CPU"))
    {
        cpu_running = false;
        cpu_stepping = true;
        total_cycles = 0;
        cpu_reset(cpu);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Memory"))
    {
        cpu_running = false;
        cpu_stepping = false;
        for (unsigned i = 0; i < MAX_MEM_SZ; i++)
            cpu->mem[i] = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Default"))
    {
        cpu_running = false;
        cpu_stepping = true;
        RESET_VEC = 0x8000; // default reset location
        NMI_VEC = 0x0200;   // default NMI handler
        IRQ_VEC = 0x0300;   // default IRQ/BRK handler
        cpu->mem[V_RESET] = RESET_VEC;
        cpu->mem[V_RESET + 1] = RESET_VEC >> 8;
        cpu->mem[V_NMI] = NMI_VEC;
        cpu->mem[V_NMI + 1] = NMI_VEC >> 8;
        cpu->mem[V_IRQ_BRK] = IRQ_VEC;
        cpu->mem[V_IRQ_BRK + 1] = IRQ_VEC >> 8;

        cpu->mem[0x8000] = LDA_IMM;
        cpu->mem[0x8001] = 0x0;
        cpu->mem[0x8002] = NOP;
        cpu->mem[0x8003] = JMP_IND;
        cpu->mem[0x8004] = 0x00;
        cpu->mem[0x8005] = 0x90;
        cpu->mem[0x9000] = 0x00;
        cpu->mem[0x9001] = 0xa0;
        cpu->mem[0xa000] = ADC_IMM;
        cpu->mem[0xa001] = 0x09;
        cpu->mem[0xa002] = ADC_IMM;
        cpu->mem[0xa003] = 0x05;
        cpu->mem[0xa004] = JMP_ABS;
        cpu->mem[0xa005] = 0x02;
        cpu->mem[0xa006] = 0x80;
    }
    if (ImGui::Button("Start"))
    {
        if (!cpu_running)
        {
            cpu_stepping = false;
            cpu_running = true;
        }
        else
        {
            cpu_stepping = true;
            cpu_running = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause"))
    {
        cpu_running = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step"))
    {
        cpu_stepping = true;
        cpu_running = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Test"))
    {
        cpu_stepping = true;
        cpu_running = false;
        FILE *fp = fopen("test/6502_functional_test.bin", "rb");
        if (fp != NULL)
        {
            // calculate size
            fseek(fp, 0, SEEK_END);
            ssize_t sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (sz != 0x10000)
            {
                printf("Binary file size: %ld bytes, which is not equal to %d bytes\n", sz, 0x10000);
            }
            else
            {
                ssize_t rdsz = fread(cpu->mem, 1, sz, fp);
                if (rdsz == sz)
                {
                    printf("Binary ROM read OK, setting RESET vector to 0x400\n");
                    RESET_VEC = 0x400;
                    NMI_VEC = cpu->mem[V_NMI];
                    NMI_VEC |= ((word)cpu->mem[V_NMI + 1]) << 8;
                    IRQ_VEC = cpu->mem[V_IRQ_BRK];
                    IRQ_VEC |= ((word)cpu->mem[V_IRQ_BRK + 1]) << 8;
                }
                else
                {
                    printf("Binary ROM read FAILED, read %ld bytes out of %ld bytes\n", rdsz, sz);
                }
            }
            fclose(fp);
        }
    }
    ImGui::Separator();
    ImGui::Columns(2, "vector_inputs", false);
    ImGui::Text("Reset Vector: ");
    ImGui::NextColumn();
    snprintf(tmp, sizeof(tmp), "0x%04X", RESET_VEC);
    ImGui::PushStyleColor(0, IMCYN);
    if (ImGui::SelectableInput("resetvec", false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
    {
        word num = strtoll(tmp, NULL, 16);
        if (num > MAX_MEM_SZ - 1)
            num = 0x8000; // 1 second
        if (num == 0)
            num = 0x400;
        RESET_VEC = num;
        cpu->mem[V_RESET] = RESET_VEC;
        cpu->mem[V_RESET + 1] = RESET_VEC >> 8;
    }
    ImGui::PopStyleColor();
    ImGui::NextColumn();
    ImGui::Text("NMI Vector: ");
    ImGui::NextColumn();
    snprintf(tmp, sizeof(tmp), "0x%04X", NMI_VEC);
    ImGui::PushStyleColor(0, IMCYN);
    if (ImGui::SelectableInput("nmivec", false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
    {
        word num = strtoll(tmp, NULL, 16);
        if (num > MAX_MEM_SZ - 1)
            num = 0x200; // 1 second
        if (num == 0)
            num = 0x200;
        NMI_VEC = num;
        cpu->mem[V_NMI] = NMI_VEC;
        cpu->mem[V_NMI + 1] = NMI_VEC >> 8;
    }
    ImGui::PopStyleColor();
    ImGui::NextColumn();
    ImGui::Text("IRQ Vector: ");
    ImGui::NextColumn();
    snprintf(tmp, sizeof(tmp), "0x%04X", IRQ_VEC);
    ImGui::PushStyleColor(0, IMCYN);
    if (ImGui::SelectableInput("irqvec", false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
    {
        word num = strtoll(tmp, NULL, 16);
        if (num > MAX_MEM_SZ - 1)
            num = 0x300; // 1 second
        if (num == 0)
            num = 0x300;
        IRQ_VEC = num;
        cpu->mem[V_IRQ_BRK] = IRQ_VEC;
        cpu->mem[V_IRQ_BRK + 1] = IRQ_VEC >> 8;
    }
    ImGui::PopStyleColor();
    ImGui::Columns(1);
    ImGui::PushStyleColor(0, IMYLW);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Checkbox("Show Memory Editor", &show_mem_editor);
    ImGui::Checkbox("Show Help Info", &show_help_window);
    ImGui::Checkbox("Show GUI Info", &show_gui_settings);
    ImGui::End();
    usr_font_scale = __usr_font_scale;
}

void *CPUThread(void *id)
{
    while (!done)
    {
        if (cpu_running)
        {
            cpu_exec(cpu);
            if (cpu_stepping)
                cpu_running = false;
            total_cycles++;
        }
        usleep(cpu_time / 2); // 60 Hz
        // other devices can read from memory here
        usleep(cpu_time / 2);
    }
    return NULL;
}

void CPURegisters(float font_scale)
{
    ImGui::Text("A: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("0x%02X", cpu->a);
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::Text("\t");
    ImGui::SameLine();

    ImGui::Text("Cycle: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("%s", CYCLE_NAME_6502[cpu->cycle]);
    ImGui::PopFont();

    ImGui::Text("X: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("0x%02X", cpu->x);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Text("\t");
    ImGui::SameLine();
    ImGui::Text("Y: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("0x%02X", cpu->y);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Text("PC: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("0x%04X", cpu->pc);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Text("\t");
    ImGui::SameLine();
    ImGui::Text("SP: ");
    ImGui::SameLine();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("0x01%02X", cpu->sp);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Columns(9);
    ImGui::SetColumnWidth(0, 4 * font_scale * FONT_SZ);
    for (int i = 1; i < 9; i++)
        ImGui::SetColumnWidth(i, 2 * font_scale * FONT_SZ);
    ImGui::Text("Flag");
    ImGui::NextColumn();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("N");
    ImGui::NextColumn();
    ImGui::Text("V");
    ImGui::NextColumn();
    ImGui::Text("-");
    ImGui::NextColumn();
    ImGui::Text("B");
    ImGui::NextColumn();
    ImGui::Text("D");
    ImGui::NextColumn();
    ImGui::Text("I");
    ImGui::NextColumn();
    ImGui::Text("Z");
    ImGui::NextColumn();
    ImGui::Text("C");
    ImGui::NextColumn();
    ImGui::PopFont();
    ImGui::Text("Value");
    ImGui::NextColumn();
    ImGui::PushFont(HexWinFont);
    ImGui::Text("%01X", cpu->n);
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->v);
    ImGui::NextColumn();
    ImGui::Text("-");
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->b);
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->d);
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->i);
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->z);
    ImGui::NextColumn();
    ImGui::Text("%01X", cpu->c);
    ImGui::NextColumn();
    ImGui::PopFont();
    ImGui::Columns(1);
    ImGui::Separator();
}

void CodeEditor(bool *active)
{
    ImGui::Begin("RAM Viewer and Editor", active);
    static float font_scale = 1.0f / FONT_SCALE;
    static float usr_font_scale = 1.0f;
    ImGui::SetWindowFontScale(font_scale * usr_font_scale);
    static int baddr = 0x8000;
    static int rc[] = {16, 16};
    float win_sz_x = (6 + rc[1] * 2) * font_scale * usr_font_scale * FONT_SZ;
    float win_sz_y = (5 + rc[0]) * font_scale * usr_font_scale * (FONT_SZ + 6 / font_scale / usr_font_scale);
    if (win_sz_x < (6 + 3 * 2) * font_scale * usr_font_scale * FONT_SZ)
        win_sz_x = (6 + 3 * 2) * font_scale * usr_font_scale * FONT_SZ;
    ImGui::SetWindowSize(ImVec2(win_sz_x, win_sz_y));
    float __usr_font_scale = usr_font_scale;
    // ImGui::PushItemWidth(15 * font_scale * usr_font_scale * FONT_SZ);
    if (ImGui::InputFloat("Text Size", &__usr_font_scale, 0.1, 0.5, "%.1f"))
    {
        if (__usr_font_scale < 0.5)
            __usr_font_scale = 0.5;
    }
    char buf[3];
    ImGui::Text("Rows: ");
    ImGui::SameLine();
    snprintf(buf, sizeof(buf), "%d", rc[0]);
    ImGui::PushItemWidth(2 * font_scale * usr_font_scale * FONT_SZ);
    if (ImGui::InputText("##Row", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        int tmp = strtol(buf, NULL, 10);
        if (tmp > 0 && tmp < 31)
            rc[0] = tmp;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("Cols: ");
    ImGui::SameLine();
    snprintf(buf, sizeof(buf), "%d", rc[1]);
    ImGui::PushItemWidth(2 * font_scale * usr_font_scale * FONT_SZ);
    if (ImGui::InputText("##Col", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        int tmp = strtol(buf, NULL, 10);
        if (tmp > 0 && tmp < 17)
        {
            rc[1] = tmp;
        }
    }
    ImGui::Columns(rc[1] + 1, "Offsets", false);
    ImGui::SetColumnWidth(0, 4 * font_scale * usr_font_scale * FONT_SZ);
    for (int i = 1; i < rc[1] + 1; i++)
        ImGui::SetColumnWidth(i, 2 * font_scale * usr_font_scale * FONT_SZ);

    for (int i = -1; i < rc[0]; i++)
    {
        if (i < 0) // print header
        {
            ImGui::Text("Base");
            ImGui::NextColumn();
            for (int j = 0; j < rc[1]; j++)
            {
                ImGui::Text("%02X", j);
                ImGui::NextColumn();
            }
            ImGui::PushFont(HexWinFont);
            ImGui::Separator();
            continue;
        }
        for (int j = -1; j < rc[1]; j++)
        {
            word pcaddr = cpu->pc; // : 0x0;
            word instraddr = cpu->instr_ptr; // : 0x0;
            if (j < 0) // address
            {
                if (i == 0) // selectable base address
                {
                    if (!cpu_running)
                    {
                        char tmp[10];
                        snprintf(tmp, sizeof(tmp), "0x%04X", baddr);
                        ImGui::PushStyleColor(0, IMCYN);
                        if (ImGui::SelectableInput("baddr", false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
                        {
                            unsigned short num = strtol(tmp, NULL, 16);
                            if (num + rc[0] * rc[1] > (int)MAX_MEM_SZ)
                                num = MAX_MEM_SZ - rc[0] * rc[1];
                            baddr = num;
                        }
                        float scr = 0.0f;
                        if ((scr = ImGui::GetIO().MouseWheel) != 0.0) // scroll detected!
                        {
                            // scroll max by 1 page
                            baddr &= 0xff00;
                            baddr += 0xff * (scr);
                            if (baddr + rc[0] * rc[1] > (int)MAX_MEM_SZ)
                                baddr = MAX_MEM_SZ - rc[0] * rc[1];
                            if (baddr < 0)
                                baddr = 0;
                        }
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        // calculate base address so that current PC falls under a "page"
                        word tmp_base = pcaddr & 0xff00; // max page
                        while (pcaddr > tmp_base + rc[0] * rc[1])
                            tmp_base += rc[0] + rc[1];
                        baddr = tmp_base;
                        // move to that base address
                        char tmp[10];
                        snprintf(tmp, sizeof(tmp), "0x%04X", baddr);
                        ImGui::SelectableInput("baddr", false, ImGuiSelectableFlags_Disabled, tmp, IM_ARRAYSIZE(tmp));
                    }
                    ImGui::NextColumn();
                }
                else
                {
                    ImGui::Text("0x%04X", baddr + rc[1] * i);
                    ImGui::NextColumn();
                }
            }
            else
            {
                char label[32];
                snprintf(label, 32, "mem_%d_%d", i, j);
                char tmp[10];
                int local_mem_idx = baddr + rc[1] * i + j;
                snprintf(tmp, sizeof(tmp), "%02X", cpu->mem[local_mem_idx]);
                bool colorpushed = false;
                if (local_mem_idx == pcaddr)
                {
                    ImGui::PushStyleColor(0, IMGRN);
                    colorpushed = true;
                }
                else if (local_mem_idx == instraddr)
                {
                    ImGui::PushStyleColor(0, IMYLW);
                    colorpushed = true;
                }
                if (!cpu_running)
                {
                    if (ImGui::SelectableInput(label, false, ImGuiSelectableFlags_None, tmp, IM_ARRAYSIZE(tmp)))
                    {
                        unsigned short num = strtol(tmp, NULL, 16);
                        if (num > 0xff)
                            num = 0;
                        cpu->mem[local_mem_idx] = num;
                    }
                }
                else
                {
                    ImGui::Text("%s", tmp);
                    // ImGui::SelectableInput(label, false, ImGuiSelectableFlags_Disabled, tmp, IM_ARRAYSIZE(tmp));
                }
                if (colorpushed)
                    ImGui::PopStyleColor();
                ImGui::NextColumn();
            }
        }
    }
    ImGui::Columns(1);
    ImGui::PopFont();
    ImGui::End();
    usr_font_scale = __usr_font_scale;
}

void GUISettings(bool *active)
{
    ImGui::Begin("GUI Settings", active);
    static float font_scale = 1.0f / FONT_SCALE;
    ImGui::SetWindowFontScale(font_scale);                               // Create a window called "Hello, world!" and append into it.
    ImGui::ColorEdit3("Change Background Color", (float *)&clear_color); // Edit 3 floats representing a color
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void HelpWindow(bool *active)
{
    ImGui::Begin("Information", active);
    static float font_scale = 1.0f / FONT_SCALE;
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetWindowSize(ImVec2(80 * font_scale * FONT_SZ, 24 * font_scale * FONT_SZ));
    ImGui::Text("MOS6502 Emulator");
    ImGui::Separator();
    ImGui::Text("Reset CPU: Load current value of reset vector (default: 0x8000) to program counter (PC), clear all registers, and set the CPU into stepping mode.");
    ImGui::End();
}