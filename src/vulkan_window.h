#include "VkBootstrap.h"
#include "wx/event.h"
#include <vulkan/vulkan_core.h>
#include <wx/wx.h>

#include <vulkan/vulkan.h>

class VulkanWindow : public wxWindow {
 public:
  VulkanWindow(wxWindow *parent);
  ~VulkanWindow();
 private:
  vkb::Instance vkb_instance;
  vkb::Device vkb_device;
  vkb::Swapchain vkb_swapchain;
  VkSurfaceKHR vk_surface;
  VkQueue graphics_queue;
  VkQueue present_queue;

  void OnPaint(wxPaintEvent& event);

  wxDECLARE_EVENT_TABLE();
};
