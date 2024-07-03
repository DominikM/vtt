#include "main_frame.h"
#include "canvas.h"
#include "tabs_window.h"
#include "wx/event.h"
#include "wx/sizer.h"
#include <thread>

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

  printf("vulkan window pos %d %d\n", vulkan_window->GetPosition().x, vulkan_window->GetPosition().y);

  graphics_thread = std::thread(&VulkanWindow::run_graphics_loop, vulkan_window);
}
