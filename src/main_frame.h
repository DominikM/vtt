#include <wx/wx.h>
#include <wx/splitter.h>

class MainFrame : public wxFrame {
 public:
  MainFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);

 private:
  DECLARE_EVENT_TABLE()
};
