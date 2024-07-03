#include "main_frame.h"

#include "tabs_window.h"
#include "vulkan_window.h"

#include <wx/aui/aui.h>
#include <wx/aui/framemanager.h>

void MainFrame::OnQuit(wxCommandEvent& event) {
  Close();
}

MainFrame::MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
  wxMenu *helpMenu = new wxMenu();
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(helpMenu, wxT("&Help"));
    
  SetMenuBar(menuBar);

  wxAuiManager *manager = new wxAuiManager(this);
  
  VulkanWindow *vulkan_window = new VulkanWindow(this);
  manager->AddPane(vulkan_window, wxCENTER);

  wxAuiPaneInfo tabs_window_pi;
  tabs_window_pi.MinSize(wxSize(400, 400)).Direction(wxRight);
  TabsWindow *tabs_window = new TabsWindow(this);
  manager->AddPane(tabs_window, tabs_window_pi);

  manager->Update();

  vulkan_window->initialize();
}
