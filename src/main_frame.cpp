#include "main_frame.h"
#include "canvas.h"
#include "tabs_window.h"
#include "vulkan_window.h"
#include "wx/sizer.h"

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_MENU(wxID_EXIT, MainFrame::OnQuit)
END_EVENT_TABLE()

void MainFrame::OnQuit(wxCommandEvent& event) {
  Close();
}

MainFrame::MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
  wxMenu *helpMenu = new wxMenu();
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(helpMenu, wxT("&Help"));
    
  SetMenuBar(menuBar);

  wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
  
  VulkanWindow *canvas = new VulkanWindow(this);
  mainSizer->Add(
      canvas,
      wxSizerFlags(1).Expand());

  TabsWindow *tabs_window = new TabsWindow(this);
  mainSizer->Add(
      tabs_window,
      wxSizerFlags(0).Expand());

  SetSizerAndFit(mainSizer);
}
