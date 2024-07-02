#include "vulkan_window.h"
#include "wx/dcclient.h"
#include "wx/event.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <string>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>
#include "VkBootstrap.h"
#include "wx/gdicmn.h"
#include <gdk/gdkwayland.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_core.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

void handle_registry(void* data, struct wl_registry* registry, uint32_t name,
			    const char* interface, uint32_t version) {
    VulkanWindow *vk = (VulkanWindow *) data;
    if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
	vk->subcompositor = (struct wl_subcompositor*) wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    }
}

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

    
    GtkWidget *gtk_widget = GetHandle();
    gtk_widget_realize(gtk_widget);
    GdkWindow *gdk_window = gtk_widget_get_window(gtk_widget);
    gdk_display = gtk_widget_get_display(gtk_widget);

    wl_surface = gdk_wayland_window_get_wl_surface(gdk_window);
    wl_display = gdk_wayland_display_get_wl_display(gdk_display);
    wl_registry *registry = wl_display_get_registry(wl_display);

    struct wl_registry_listener registry_listener = {
	.global = &handle_registry
    };

    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(wl_display);

    struct wl_compositor* compositor = gdk_wayland_display_get_wl_compositor(gdk_display);
    subsurface_surface = wl_compositor_create_surface(compositor);
    subsurface_subsurface = wl_subcompositor_get_subsurface(subcompositor, subsurface_surface, wl_surface);

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.display = wl_display;
    createInfo.surface = subsurface_surface;

    if (vkCreateWaylandSurfaceKHR(vkb_instance.instance, &createInfo, nullptr, &vk_surface) != VK_SUCCESS) {
	throw std::runtime_error("failed to create window surface!");
    }

    // VkXlibSurfaceCreateInfoKHR createInfo{};
    // createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    // createInfo.pNext = nullptr;
    // createInfo.flags = 0;
    // createInfo.dpy = gdk_x11_display_get_xdisplay(gdk_display);
    // createInfo.window = gdk_x11_window_get_xid(gdk_window);

    // if (vkCreateXlibSurfaceKHR(vkb_instance.instance, &createInfo, nullptr, &vk_surface) != VK_SUCCESS) {
    // 	throw std::runtime_error("blah");
    // }

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

    create_swapchain();
    create_render_pass();
    create_graphics_pipeline();
    create_frame_buffers();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();

    Bind(wxEVT_SIZE, &VulkanWindow::OnResize, this);
}

VulkanWindow::~VulkanWindow() {
    for (auto semaphore: available_semaphores) {
	vkDestroySemaphore(vkb_device.device, semaphore, nullptr);
    }
    for (auto semaphore: finished_semaphore) {
	vkDestroySemaphore(vkb_device.device, semaphore, nullptr);
    }
    vkDestroyCommandPool(vkb_device.device, command_pool, nullptr);
    for (VkFramebuffer fb: framebuffers) {
	vkDestroyFramebuffer(vkb_device.device, fb, nullptr);
    }
    vkDestroyPipeline(vkb_device.device, pipeline, nullptr);
    vkDestroyPipelineLayout(vkb_device.device, pipelineLayout, nullptr);
    vkDestroyRenderPass(vkb_device.device, renderPass, nullptr);
    vkb_swapchain.destroy_image_views(image_views);
    vkb::destroy_swapchain(vkb_swapchain);
    vkb::destroy_device(vkb_device);
    vkb::destroy_surface(vkb_instance, vk_surface);
    vkb::destroy_instance(vkb_instance);
}

void VulkanWindow::create_swapchain() {
    vkb::SwapchainBuilder swapchain_builder{ vkb_device };
    auto swap_ret = swapchain_builder
	.set_desired_extent(200, 200)
	.set_old_swapchain(vkb_swapchain)
	.build();
    if (!swap_ret){
	vkb_swapchain.swapchain = VK_NULL_HANDLE;
    }
    vkb::destroy_swapchain(vkb_swapchain);
    vkb_swapchain = swap_ret.value();
}

void VulkanWindow::OnPaint(wxPaintEvent& event) {
    printf("hello from OnPaint\n");
    vkWaitForFences(vkb_device.device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        vkb_device.device, vkb_swapchain.swapchain, UINT64_MAX, available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
	return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image.");
    }

    if (image_in_flight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(vkb_device.device, 1, &image_in_flight[image_index], VK_TRUE, UINT64_MAX);
    }
    
    image_in_flight[image_index] = in_flight_fences[current_frame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { available_semaphores[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = wait_semaphores;
    submitInfo.pWaitDstStageMask = wait_stages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffers[image_index];

    VkSemaphore signal_semaphores[] = { finished_semaphore[current_frame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signal_semaphores;

    vkResetFences(vkb_device.device, 1, &in_flight_fences[current_frame]);

    if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fences[current_frame]) != VK_SUCCESS) {
        return;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapChains[] = { vkb_swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapChains;

    present_info.pImageIndices = &image_index;
    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain();
	return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swapchain image");
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    printf("done with paint\n");
    //wl_surface_commit(wl_surface);
}

void VulkanWindow::recreate_swapchain() {
    vkDeviceWaitIdle(vkb_device.device);

    vkDestroyCommandPool(vkb_device.device, command_pool, nullptr);

    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(vkb_device.device, framebuffer, nullptr);
    }
    vkb_swapchain.destroy_image_views(image_views);

    create_swapchain();
    create_frame_buffers();
    create_command_pool();
    create_command_buffers();
    resized = false;
}

void VulkanWindow::create_render_pass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vkb_swapchain.image_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(vkb_device.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
	throw std::runtime_error("failed to create render pass!");
    }
}

std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

VkShaderModule create_shader_module(VkDevice& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE; // failed to create shader module
    }

    return shaderModule;
}

void VulkanWindow::create_graphics_pipeline() {
    auto vert_code = read_file("/home/dominik/projects/vtt/build/shaders/shader.vert.spv");
    auto frag_code = read_file("/home/dominik/projects/vtt/build/shaders/shader.frag.spv");

    VkShaderModule vert_module = create_shader_module(vkb_device.device, vert_code);
    VkShaderModule frag_module = create_shader_module(vkb_device.device, frag_code);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create shader module");
    }

    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info = {};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info, frag_stage_info };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) vkb_swapchain.extent.width;
    viewport.height = (float) vkb_swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = vkb_swapchain.extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &colorBlendAttachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(vkb_device.device, &pipeline_layout_info, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout");
    }

    std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_info.pDynamicStates = dynamic_states.data();

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_info;
    pipeline_info.layout = pipelineLayout;
    pipeline_info.renderPass = renderPass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(vkb_device.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipline");
    }

    vkDestroyShaderModule(vkb_device.device, frag_module, nullptr);
    vkDestroyShaderModule(vkb_device.device, vert_module, nullptr);
}

void VulkanWindow::create_frame_buffers() {
    images = vkb_swapchain.get_images().value();
    image_views = vkb_swapchain.get_image_views().value();
    framebuffers.resize(image_views.size());

    for (size_t i = 0; i < image_views.size(); i++) {
	printf("hello from framebuffer creation\n");
	VkImageView attachments[] = { image_views[i] };

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = renderPass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vkb_swapchain.extent.width;
        framebuffer_info.height = vkb_swapchain.extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(vkb_device.device, &framebuffer_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
	    throw std::runtime_error("can't create framebuffer");
        }
    }
}

void VulkanWindow::create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    if (vkCreateCommandPool(vkb_device.device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
	throw std::runtime_error("can't create command pool");
    }

    printf("created command pool\n");
}

void VulkanWindow::create_command_buffers() {
    command_buffers.resize(framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)command_buffers.size();

    if (vkAllocateCommandBuffers(vkb_device.device, &allocInfo, command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("can't allocate command buffers");
    }

    for (size_t i = 0; i < command_buffers.size(); i++) {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
	    throw std::runtime_error("can't begin command buffer");
        }

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = renderPass;
        render_pass_info.framebuffer = framebuffers[i];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = vkb_swapchain.extent;
        VkClearValue clearColor{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clearColor;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) vkb_swapchain.extent.width;
        viewport.height = (float) vkb_swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = vkb_swapchain.extent;

        vkCmdSetViewport(command_buffers[i], 0, 1, &viewport);
        vkCmdSetScissor(command_buffers[i], 0, 1, &scissor);

        vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffers[i]);

        if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }
    }
}

void VulkanWindow::create_sync_objects() {
    available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    finished_semaphore.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    image_in_flight.resize(vkb_swapchain.image_count, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkb_device.device, &semaphore_info, nullptr, &available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkb_device.device, &semaphore_info, nullptr, &finished_semaphore[i]) != VK_SUCCESS ||
            vkCreateFence(vkb_device.device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create sync objects");
        }
    }
}

void VulkanWindow::OnResize(wxSizeEvent& event) {
    resized = true;
    printf("hello from OnResize\n");
    wxSize size = GetSize();
    if (size.GetWidth() == 0 || size.GetHeight() == 0) {
        return;
    }
    recreate_swapchain();
    //wxRect refreshRect(size);
    //RefreshRect(refreshRect, false);
    printf("done with resize\n");
    resized = false;
    wl_surface_commit(wl_surface);
}
