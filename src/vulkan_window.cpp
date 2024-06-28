#include "vulkan_window.h"
#include "VkBootstrapDispatch.h"
#include "wx/event.h"
#include "wx/gtk/window.h"
#include <fstream>
#include <stdexcept>

#include <string>
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

    create_render_pass();
    create_graphics_pipeline();
    create_frame_buffers();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();
}

VulkanWindow::~VulkanWindow() {
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

void VulkanWindow::OnPaint(wxPaintEvent& event) {
}

void VulkanWindow::create_shaders() {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = 0;
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

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

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
    auto vert_code = read_file("build/shaders/shader.vert.spv");

    VkShaderModule vert_module = create_shader_module(vkb_device.device, vert_code);
    if (vert_module == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create shader module");
    }

    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info };

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
    pipeline_info.stageCount = 1;
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

    vkDestroyShaderModule(vkb_device.device, vert_module, nullptr);
}

void VulkanWindow::create_frame_buffers() {
    images = vkb_swapchain.get_images().value();
    image_views = vkb_swapchain.get_image_views().value();
    framebuffers.resize(image_views.size());

    for (size_t i = 0; i < image_views.size(); i++) {
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

