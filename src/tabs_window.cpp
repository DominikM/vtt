#include "tabs_window.h"
#include "chat_window.h"

TabsWindow::TabsWindow(wxWindow *parent) : wxNotebook(parent, wxID_ANY, wxDefaultPosition, wxSize(400, 0)) {
  AddPage(new ChatWindow(this), "chat");
}
