#include "main_frame.h"

#include "tabs_window.h"
#include "vulkan_window.h"

void MainFrame::OnQuit(wxCommandEvent& event) {
  Close();
}

MainFrame::MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
  wxMenu *helpMenu = new wxMenu();
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(helpMenu, wxT("&Help"));
    
  SetMenuBar(menuBar);

  wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
  
  VulkanWindow *vulkan_window = new VulkanWindow(this);
  mainSizer->Add(
      vulkan_window,
      wxSizerFlags(1).Expand());

  TabsWindow *tabs_window = new TabsWindow(this);
  mainSizer->Add(
      tabs_window,
      wxSizerFlags(0).Expand());

  SetSizerAndFit(mainSizer);

  vulkan_window->initialize();
}
