#include "wx/event.h"
#include "vulkan_window.h"
#include <thread>
#include <wx/wx.h>
#include <wx/splitter.h>

class MainFrame : public wxFrame {
 public:
  MainFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnPaint(wxPaintEvent& event);
  std::thread graphics_thread;

 private:
};
