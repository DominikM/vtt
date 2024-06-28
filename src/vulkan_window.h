#include "VkBootstrap.h"
#include "wx/event.h"
#include <vector>
#include <vulkan/vulkan_core.h>
#include <wx/wx.h>

#include <vulkan/vulkan.h>

const int MAX_FRAMES_IN_FLIGHT = 2;

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
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> available_semaphores;
    std::vector<VkSemaphore> finished_semaphore;
    std::vector<VkFence> in_flight_fences;
    std::vector<VkFence> image_in_flight;
    size_t current_frame = 0;

    void create_shaders();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_frame_buffers();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();
  

    void OnPaint(wxPaintEvent& event);
  

    wxDECLARE_EVENT_TABLE();
};
