#include "tabs_window.h"
#include "chat_window.h"
#include "wx/gdicmn.h"

TabsWindow::TabsWindow(wxWindow *parent) : wxNotebook(parent, wxID_ANY) {
  AddPage(new ChatWindow(this), "chat");
}
