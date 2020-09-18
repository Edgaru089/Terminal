
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include "SystemFrontend.hpp"
#include "Terminal.hpp"

#include "imgui/imgui.h"
#define imgui ImGui

using namespace std;
using namespace sf;

namespace fs = std::filesystem;

namespace {
	
	// Get a process name from its PID.
	// Source: http://stackoverflow.com/questions/15545341/process-name-from-its-pid-in-linux
	void get_process_name(const pid_t pid, char * name) {
		char procfile[BUFSIZ];
		sprintf(procfile, "/proc/%d/cmdline", pid);
		FILE* f = fopen(procfile, "r");
		if (f) {
			size_t size;
			size = fread(name, sizeof (char), sizeof (procfile), f);
			if (size > 0) {
				if ('\n' == name[size - 1])
					name[size - 1] = '\0';
			}
			fclose(f);
		}
	}

	string getProcessName(pid_t id) {
		char name[512];
		get_process_name(id, name);
		return string(name);
	}

	string getProcessWd(pid_t pid){
		char procfile[512],ans[512];
		sprintf(procfile,"/proc/%d/cwd",pid);
		ssize_t len = readlink(procfile,ans,sizeof(ans)-1);
		if (len>=0)
			return string(ans,len);
		else
			return string();
	}

}

void folderTreeGui(const fs::path& path, function<void()> onClick, function<void()> onDoubleClick){
	vector<fs::directory_entry> folders, files;
	try{
		for(auto& f : fs::directory_iterator(path))
			if(f.is_directory())
				folders.push_back(f);
			else
				files.push_back(f);

		sort(folders.begin(),folders.end());
		sort(files.begin(),files.end());
	}catch(fs::filesystem_error e){
		imgui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f,0.2f,0.2f,1.0f});
		imgui::TextUnformatted("ERROR:");
		imgui::PopStyleColor();
		imgui::SameLine();
		imgui::TextUnformatted(e.what());
	}

	for(auto& f:folders)
		if(imgui::TreeNodeEx(f.path().filename().u8string().c_str(),ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_SpanFullWidth)){
			folderTreeGui(f,onClick,onDoubleClick);
			imgui::TreePop();
		}

	for(auto& f:files)
		if(imgui::TreeNodeEx(f.path().filename().u8string().c_str(),ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_SpanFullWidth)){

			imgui::TreePop();
		}
}

void runImGui(Terminal* term, RenderWindow* win) {

	static bool fileSelectorOpen = false, demoOpen=false;

	imgui::SetNextWindowPos(ImVec2(0,-1),ImGuiCond_Always);
	if (imgui::BeginMainMenuBar()) {

		if (imgui::BeginMenu("View")) {

			imgui::MenuItem("File Selector     ", 0, &fileSelectorOpen, true);
			imgui::MenuItem("Demo Window", 0, &demoOpen, true);


			imgui::EndMenu();
		}

		imgui::TextUnformatted("|");

		if (term->renderAltScreen){
			if (imgui::SmallButton("ALT")){
				term->renderAltScreen = !term->renderAltScreen;
				term->invalidate();
			}
		} else {
			if (imgui::SmallButton("PRI")){
				term->renderAltScreen = !term->renderAltScreen;
				term->invalidate();
			}
		}
		if (imgui::IsItemHovered()){
			imgui::BeginTooltip();
			imgui::TextUnformatted("Primary / Alternative Buffer");
			imgui::EndTooltip();
		}

#ifdef SFML_SYSTEM_LINUX
		SystemFrontend* f=dynamic_cast<SystemFrontend*>(term->getFrontend());
		imgui::Text("Child%d(%s), FgGrp%d(%s,wd=%s)",f->getChild(),getProcessName(f->getChild()).c_str(),tcgetpgrp(f->getPtyFd()),getProcessName(tcgetpgrp(f->getPtyFd())).c_str(),getProcessWd(tcgetpgrp(f->getPtyFd())).c_str());
#endif

		imgui::EndMainMenuBar();
	}


	if (fileSelectorOpen) {
		if (imgui::Begin("File Selector", &fileSelectorOpen)) {
			SystemFrontend* f=dynamic_cast<SystemFrontend*>(term->getFrontend());
			static bool optionOpen=false;
			static fs::path folder(getProcessWd(tcgetpgrp(f->getPtyFd())));

			if (imgui::Button("Options..."))
				optionOpen=true;
			imgui::SameLine();
			if (imgui::Button("Reset FgGrpWd")){
#ifdef SFML_SYSTEM_LINUX
				folder=getProcessWd(tcgetpgrp(f->getPtyFd()));
#endif
			}
			imgui::TextUnformatted(((string)(folder)).c_str());
			imgui::Separator();

			folderTreeGui(folder,[](){},[](){});
		}
		imgui::End();

	}

	if (demoOpen){
		imgui::ShowDemoWindow(&demoOpen);
	}

}
