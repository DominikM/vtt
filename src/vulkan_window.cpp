#include "vulkan_window.h"
#include "wx/event.h"
#include "wx/gtk/window.h"
#include <stdexcept>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#ifdef __LINUX__

#include <gtk-3.0/gtk/gtk.h>
#include <gtk-3.0/gdk/gdkwayland.h>
#include <gtk-3.0/gdk/gdkx.h>

#include "VkBootstrap.h"

#endif

wxBEGIN_EVENT_TABLE(VulkanWindow, wxWindow)
EVT_PAINT(VulkanWindow::OnPaint)
wxEND_EVENT_TABLE()

VulkanWindow::VulkanWindow(wxWindow *parent) : wxWindow(parent, wxID_ANY) {
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("Vulkan VTT Canvas")
	.request_validation_layers()
	.use_default_debug_messenger()
	.build();

    if (!inst_ret) {
	throw std::runtime_error("failed to initalize vulkan instance");
    }

    vkb_instance = inst_ret.value();
    
#ifdef __LINUX__
    GtkWidget *gtk_widget = GetHandle();
    gtk_widget_realize(gtk_widget);
    GdkWindow *gdk_window = gtk_widget_get_window(gtk_widget);
    GdkDisplay *gdk_display = gtk_widget_get_display(gtk_widget);

    auto wl_surface = gdk_wayland_window_get_wl_surface(gdk_window);
    auto wl_display = gdk_wayland_display_get_wl_display(gdk_display);

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.display = wl_display;
    createInfo.surface = wl_surface;

    if (vkCreateWaylandSurfaceKHR(vkb_instance.instance, &createInfo, nullptr, &vk_surface) != VK_SUCCESS) {
	throw std::runtime_error("failed to create window surface!");
    }
#endif

    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    auto phys_ret = selector.set_surface(vk_surface)
	.select ();
    if (!phys_ret) {
        throw std::runtime_error("Failed to select Vulkan Physical Device.");
    }

    vkb::DeviceBuilder device_builder{ phys_ret.value () };

    auto dev_ret = device_builder.build ();
    if (!dev_ret) {
	throw std::runtime_error("Failed to create Vulkan device.");
    }
    vkb_device = dev_ret.value ();

    auto graphics_queue_ret = vkb_device.get_queue (vkb::QueueType::graphics);
    if (!graphics_queue_ret) {
        throw std::runtime_error("Failed to create graphics queue.");
    }
    graphics_queue = graphics_queue_ret.value ();

    auto present_queue_ret = vkb_device.get_queue(vkb::QueueType::present);
    if (!present_queue_ret) {
        throw std::runtime_error("Failed to create present queue.");
    }
    present_queue = present_queue_ret.value();

    vkb::SwapchainBuilder swapchain_builder{ vkb_device };
    auto swap_ret = swapchain_builder.set_desired_extent(500, 500).build ();
    if (!swap_ret){
	throw std::runtime_error("Failed to create swapchain");
    }
    vkb_swapchain = swap_ret.value();
}

VulkanWindow::~VulkanWindow() {
    vkb::destroy_swapchain(vkb_swapchain);
    vkb::destroy_device(vkb_device);
    vkb::destroy_surface(vkb_instance, vk_surface);
    vkb::destroy_instance(vkb_instance);
}

void VulkanWindow::OnPaint(wxPaintEvent& event) {
    
}
