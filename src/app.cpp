#include "app.h"
#include "main_frame.h"

DECLARE_APP(App)
IMPLEMENT_APP(App)

bool App::OnInit()
{
  MainFrame *frame = new MainFrame(wxT("vtt"));
  frame->Show(true);
  return true;
}
