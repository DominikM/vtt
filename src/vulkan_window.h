#include "VkBootstrap.h"
#include "wx/event.h"
#include <vector>
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
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> framebuffers;
  

    void create_shaders();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_frame_buffers();
  

    void OnPaint(wxPaintEvent& event);
  

    wxDECLARE_EVENT_TABLE();
};
