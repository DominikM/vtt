#include "main_frame.h"
#include "canvas.h"
#include "tabs_window.h"
#include "wx/event.h"
#include "wx/sizer.h"
#include <cstddef>

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_MENU(wxID_EXIT, MainFrame::OnQuit)
END_EVENT_TABLE()

void MainFrame::OnQuit(wxCommandEvent& event) {
  Close();
}

MainFrame::MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
  SetDoubleBuffered(false);
  wxMenu *helpMenu = new wxMenu();
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(helpMenu, wxT("&Help"));
    
  SetMenuBar(menuBar);

  wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
  
  vulkan_window = new VulkanWindow(this);
  mainSizer->Add(
      vulkan_window,
      wxSizerFlags(1).Expand());

  TabsWindow *tabs_window = new TabsWindow(this);
  mainSizer->Add(
      tabs_window,
      wxSizerFlags(0).Expand());

  SetSizerAndFit(mainSizer);

  Bind(wxEVT_PAINT, &MainFrame::OnPaint, this);
}

void MainFrame::OnPaint(wxPaintEvent& event) {
  vulkan_window->OnPaint(event);
}
