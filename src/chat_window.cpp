#include "chat_window.h"
#include "wx/gdicmn.h"
#include "wx/gtk/textctrl.h"
#include "wx/gtk/textentry.h"
#include "wx/scrolwin.h"
#include "wx/sizer.h"
#include "wx/stringimpl.h"
#include <wx/textctrl.h>

ChatWindow::ChatWindow(wxWindow *parent) : wxWindow(parent, wxID_ANY) {
  wxScrolledWindow *chat_log = new wxScrolledWindow(this);
  wxTextCtrl *text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(0, 200));

  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  sizer->Add(
      chat_log,
      wxSizerFlags(1).Expand());
  sizer->Add(
      text_ctrl,
      wxSizerFlags(0).Expand());

  SetSizerAndFit(sizer);
}
