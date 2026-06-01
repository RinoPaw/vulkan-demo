#include "vk_common.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

struct Vec3 { float x, y, z; };

static Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static Vec3 normalize(Vec3 v) {
    float len = std::sqrt(dot(v, v));
    return {v.x / len, v.y / len, v.z / len};
}

struct Mat4 { float m[16]{}; };

static Mat4 identity() {
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

static Mat4 rotateY(float radians) {
    Mat4 r = identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(sub(center, eye));
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 r = identity();

    // column-major matrix
    r.m[0]  = s.x;
    r.m[1]  = u.x;
    r.m[2]  = -f.x;
    r.m[3]  = 0.0f;

    r.m[4]  = s.y;
    r.m[5]  = u.y;
    r.m[6]  = -f.y;
    r.m[7]  = 0.0f;

    r.m[8]  = s.z;
    r.m[9]  = u.z;
    r.m[10] = -f.z;
    r.m[11] = 0.0f;

    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    r.m[15] = 1.0f;

    return r;
}

static Mat4 perspective(float fovyRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 r{};
    float f = 1.0f / std::tan(fovyRadians / 2.0f);
    r.m[0] = f / aspect;
    r.m[5] = -f;
    r.m[10] = farPlane / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return r;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        bool typeOk = typeFilter & (1u << i);
        bool propsOk = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeOk && propsOk) return i;
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

void createBuffer(VulkanBase& vk, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vk.device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(vk.device, buffer, &req);
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(vk.physicalDevice, req.memoryTypeBits, properties);
    VK_CHECK(vkAllocateMemory(vk.device, &alloc, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(vk.device, buffer, memory, 0));
}

void createDepthImage(VulkanBase& vk, uint32_t width, uint32_t height, VkFormat format, VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateImage(vk.device, &imageInfo, nullptr, &image));

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(vk.device, image, &req);
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(vk.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(vk.device, &alloc, nullptr, &memory));
    VK_CHECK(vkBindImageMemory(vk.device, image, memory, 0));
}

int main() {
    const uint32_t WIDTH = 512;
    const uint32_t HEIGHT = 512;
    const VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

    VulkanBase vk;
    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkFence fence{};
    VkImage depthImage{};
    VkDeviceMemory depthMemory{};
    VkImageView depthView{};
    VkRenderPass renderPass{};
    VkFramebuffer framebuffer{};
    VkShaderModule vertShader{};
    VkShaderModule fragShader{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};
    VkBuffer vertexBuffer{};
    VkDeviceMemory vertexMemory{};
    VkBuffer indexBuffer{};
    VkDeviceMemory indexMemory{};
    VkBuffer readbackBuffer{};
    VkDeviceMemory readbackMemory{};

    try {
        vk.init();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vk.graphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &commandPool));

        VkCommandBufferAllocateInfo cmdAlloc{};
        cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAlloc.commandPool = commandPool;
        cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAlloc.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(vk.device, &cmdAlloc, &commandBuffer));

        std::vector<float> vertices = {
            -1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
            -1, -1,  1,   1, -1,  1,   1,  1,  1,  -1,  1,  1,
        };

        std::vector<uint16_t> indices = {
            0, 1, 2,  2, 3, 0,
            4, 6, 5,  6, 4, 7,
            0, 4, 5,  5, 1, 0,
            3, 2, 6,  6, 7, 3,
            1, 5, 6,  6, 2, 1,
            0, 3, 7,  7, 4, 0,
        };

        createBuffer(vk, sizeof(float) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexMemory);
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(vk.device, vertexMemory, 0, VK_WHOLE_SIZE, 0, &mapped));
        std::memcpy(mapped, vertices.data(), sizeof(float) * vertices.size());
        vkUnmapMemory(vk.device, vertexMemory);

        createBuffer(vk, sizeof(uint16_t) * indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexMemory);
        VK_CHECK(vkMapMemory(vk.device, indexMemory, 0, VK_WHOLE_SIZE, 0, &mapped));
        std::memcpy(mapped, indices.data(), sizeof(uint16_t) * indices.size());
        vkUnmapMemory(vk.device, indexMemory);

        createDepthImage(vk, WIDTH, HEIGHT, DEPTH_FORMAT, depthImage, depthMemory);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = DEPTH_FORMAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(vk.device, &viewInfo, nullptr, &depthView));

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = DEPTH_FORMAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 0;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 0;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[0].srcAccessMask = 0;
        deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = deps;
        VK_CHECK(vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &renderPass));

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &depthView;
        framebufferInfo.width = WIDTH;
        framebufferInfo.height = HEIGHT;
        framebufferInfo.layers = 1;
        VK_CHECK(vkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &framebuffer));

        auto vertCode = readFile("shaders/depth.vert.spv");
        auto fragCode = readFile("shaders/depth.frag.spv");
        vertShader = createShaderModule(vk.device, vertCode);
        fragShader = createShaderModule(vk.device, fragCode);

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(float) * 16;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        VK_CHECK(vkCreatePipelineLayout(vk.device, &layoutInfo, nullptr, &pipelineLayout));

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShader;
        vertStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(float) * 3;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription attr{};
        attr.location = 0;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = &attr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(WIDTH);
        viewport.height = static_cast<float>(HEIGHT);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {WIDTH, HEIGHT};
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.depthClampEnable = VK_FALSE;
        raster.rasterizerDiscardEnable = VK_FALSE;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.depthBiasEnable = VK_FALSE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.sampleShadingEnable = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable = VK_FALSE;
        colorBlend.attachmentCount = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        VK_CHECK(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

        createBuffer(vk, WIDTH * HEIGHT * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, readbackBuffer, readbackMemory);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        VkClearValue clear{};
        clear.depthStencil.depth = 1.0f;
        clear.depthStencil.stencil = 0;
        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = renderPass;
        rpBegin.framebuffer = framebuffer;
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = {WIDTH, HEIGHT};
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clear;

        vkCmdBeginRenderPass(commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        Mat4 model = rotateY(0.55f);
        Mat4 view = lookAt({2.5f, 2.0f, 2.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        Mat4 proj = perspective(45.0f * 3.1415926f / 180.0f, 1.0f, 0.1f, 10.0f);
        Mat4 mvp = multiply(proj, multiply(view, model));

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, mvp.m);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        VkBufferImageCopy copy{};
        copy.bufferOffset = 0;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset = {0, 0, 0};
        copy.imageExtent = {WIDTH, HEIGHT, 1};
        vkCmdCopyImageToBuffer(commandBuffer, depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, 1, &copy);

        VkBufferMemoryBarrier readBarrier{};
        readBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        readBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        readBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        readBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        readBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        readBarrier.buffer = readbackBuffer;
        readBarrier.offset = 0;
        readBarrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &readBarrier, 0, nullptr);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(vk.device, &fenceInfo, nullptr, &fence));

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        VK_CHECK(vkQueueSubmit(vk.graphicsQueue, 1, &submit, fence));
        VK_CHECK(vkWaitForFences(vk.device, 1, &fence, VK_TRUE, UINT64_MAX));

        float* depthData = nullptr;
        VK_CHECK(vkMapMemory(vk.device, readbackMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&depthData)));
	std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3);

float minDepth = 1.0f;
float maxDepth = 0.0f;

for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
        float d = depthData[y * WIDTH + x];
        d = std::clamp(d, 0.0f, 1.0f);

        minDepth = std::min(minDepth, d);
        maxDepth = std::max(maxDepth, d);
    }
}

std::cout << "Depth range: " << minDepth << " ... " << maxDepth << "\n";

float range = maxDepth - minDepth;
if (range < 0.000001f) {
    range = 1.0f;
}

for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
        float d = depthData[y * WIDTH + x];
        d = std::clamp(d, 0.0f, 1.0f);

        float normalized = (maxDepth - d) / range;
        unsigned char v = static_cast<unsigned char>(normalized * 255.0f);

        size_t i = (y * WIDTH + x) * 3;
        pixels[i + 0] = v;
        pixels[i + 1] = v;
        pixels[i + 2] = v;
    }
}

vkUnmapMemory(vk.device, readbackMemory);

int ok = stbi_write_png(
    "cube_depth.png",
    WIDTH,
    HEIGHT,
    3,
    pixels.data(),
    WIDTH * 3
);

if (!ok) {
    throw std::runtime_error("Failed to write cube_depth.png");
}
        std::cout << "Saved cube_depth.png\n";

        vkDestroyFence(vk.device, fence, nullptr);
        vkDestroyBuffer(vk.device, readbackBuffer, nullptr);
        vkFreeMemory(vk.device, readbackMemory, nullptr);
        vkDestroyPipeline(vk.device, pipeline, nullptr);
        vkDestroyPipelineLayout(vk.device, pipelineLayout, nullptr);
        vkDestroyShaderModule(vk.device, fragShader, nullptr);
        vkDestroyShaderModule(vk.device, vertShader, nullptr);
        vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
        vkDestroyRenderPass(vk.device, renderPass, nullptr);
        vkDestroyImageView(vk.device, depthView, nullptr);
        vkDestroyImage(vk.device, depthImage, nullptr);
        vkFreeMemory(vk.device, depthMemory, nullptr);
        vkDestroyBuffer(vk.device, indexBuffer, nullptr);
        vkFreeMemory(vk.device, indexMemory, nullptr);
        vkDestroyBuffer(vk.device, vertexBuffer, nullptr);
        vkFreeMemory(vk.device, vertexMemory, nullptr);
        vkDestroyCommandPool(vk.device, commandPool, nullptr);
        vk.cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
