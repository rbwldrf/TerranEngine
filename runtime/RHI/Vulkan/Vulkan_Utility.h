/*
Copyright(c) 2016-2023 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

//= INCLUDES ========================
#include "../RHI_Implementation.h"
#include "../RHI_Descriptor.h"
#include "../RHI_Texture.h"
#include "../RHI_Device.h"
//===================================

namespace Spartan::vulkan_utility
{
    static VkDescriptorType to_vulkan_desscriptor_type(const RHI_Descriptor& descriptor)
    {
        if (descriptor.type == RHI_Descriptor_Type::Sampler)
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;

        if (descriptor.type == RHI_Descriptor_Type::Texture)
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

        if (descriptor.type == RHI_Descriptor_Type::TextureStorage)
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        if (descriptor.type == RHI_Descriptor_Type::StructuredBuffer)
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

        if (descriptor.type == RHI_Descriptor_Type::ConstantBuffer)
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

        SP_ASSERT_MSG(false, "Unhandled descriptor type");
        return VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    namespace timeline_semaphore
    {
        inline void create(void*& semaphore, const uint64_t intial_value = 0)
        {
            VkSemaphoreTypeCreateInfo timeline_info = {};
            timeline_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timeline_info.pNext         = nullptr;
            timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timeline_info.initialValue  = intial_value;
        
            VkSemaphoreCreateInfo semaphore_info = {};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = &timeline_info;
            semaphore_info.flags = 0;
        
            VkSemaphore* semaphore_vk = reinterpret_cast<VkSemaphore*>(&semaphore);
            SP_ASSERT_MSG(vkCreateSemaphore(RHI_Context::device, &semaphore_info, nullptr, semaphore_vk) == VK_SUCCESS, "Failed to create semaphore");
        }

        inline void destroy(void*& semaphore)
        {
            if (!semaphore)
                return;
        
            VkSemaphore semaphore_vk = static_cast<VkSemaphore>(semaphore);
            vkDestroySemaphore(RHI_Context::device, semaphore_vk, nullptr);
            semaphore = nullptr;
        }

        inline void wait(void*& semaphore, const uint64_t wait_value, uint64_t timeout = std::numeric_limits<uint64_t>::max())
        {
            SP_ASSERT_MSG(semaphore != nullptr, "Invalid semaphore");

            VkSemaphoreWaitInfo wait_info = {};
            wait_info.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.pNext          = nullptr;
            wait_info.flags          = 0;
            wait_info.semaphoreCount = 1;
            wait_info.pSemaphores    = reinterpret_cast<VkSemaphore*>(&semaphore);
            wait_info.pValues        = &wait_value;

            SP_ASSERT_MSG(vkWaitSemaphores(RHI_Context::device, &wait_info, timeout) == VK_SUCCESS, "Failed to wait for semaphore");
        }

        inline uint64_t get_counter_value(void*& semaphore)
        {
            if (!semaphore)
                return 0;

            uint64_t value;
            vkGetSemaphoreCounterValue(RHI_Context::device, static_cast<VkSemaphore>(semaphore), &value);
            return value;
        }
    }

    namespace image
    {
        static VkImageAspectFlags get_aspect_mask(const RHI_Texture* texture, const bool only_depth = false, const bool only_stencil = false)
        {
            VkImageAspectFlags aspect_mask = 0;

            if (texture->IsColorFormat())
            {
                aspect_mask |= VK_IMAGE_ASPECT_COLOR_BIT;
            }
            else
            {
                if (texture->IsDepthFormat() && !only_stencil)
                {
                    aspect_mask |= VK_IMAGE_ASPECT_DEPTH_BIT;
                }

                if (texture->IsStencilFormat() && !only_depth)
                {
                    aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            }

            return aspect_mask;
        }

        static VkPipelineStageFlags layout_to_access_mask(const VkImageLayout layout, const bool is_destination_mask)
        {
            VkPipelineStageFlags access_mask = 0;

            switch (layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                SP_ASSERT(!is_destination_mask && "The new layout used in a transition must not be VK_IMAGE_LAYOUT_UNDEFINED.");
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                SP_ASSERT(!is_destination_mask && "The new layout used in a transition must not be VK_IMAGE_LAYOUT_PREINITIALIZED.");
                access_mask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                access_mask = VK_ACCESS_2_NONE;
                break;

            // Transfer
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                access_mask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            // Color attachments
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            // Depth attachments
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
                access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
                access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                break;

            // Shader reads
            case VK_IMAGE_LAYOUT_GENERAL:
                access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                access_mask = VK_ACCESS_SHADER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
                access_mask = VK_ACCESS_SHADER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                access_mask = VK_ACCESS_SHADER_READ_BIT;
                break;

            default:
                SP_LOG_ERROR("Unexpected image layout");
                break;
            }

            return access_mask;
        }

        static VkPipelineStageFlags access_flags_to_pipeline_stage(VkAccessFlags access_flags)
        {
            VkPipelineStageFlags stages = 0;
            uint32_t enabled_graphics_stages = RHI_Device::GetEnabledGraphicsStages();

            while (access_flags != 0)
            {
                VkAccessFlagBits access_flag = static_cast<VkAccessFlagBits>(access_flags & (~(access_flags - 1)));
                SP_ASSERT(access_flag != 0 && (access_flag & (access_flag - 1)) == 0);
                access_flags &= ~access_flag;

                switch (access_flag)
                {
                case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                    break;

                case VK_ACCESS_INDEX_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                    break;

                case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                    break;

                case VK_ACCESS_UNIFORM_READ_BIT:
                    stages |= enabled_graphics_stages | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    break;

                case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;

                // Shader
                case VK_ACCESS_SHADER_READ_BIT:
                    stages |= enabled_graphics_stages | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    break;

                case VK_ACCESS_SHADER_WRITE_BIT:
                    stages |= enabled_graphics_stages | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    break;

                // Color attachments
                case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;

                case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
                    stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;

                // Depth stencil attachments
                case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;

                case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
                    stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    break;

                // Transfer
                case VK_ACCESS_TRANSFER_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;

                case VK_ACCESS_TRANSFER_WRITE_BIT:
                    stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;

                // Host
                case VK_ACCESS_HOST_READ_BIT:
                    stages |= VK_PIPELINE_STAGE_HOST_BIT;
                    break;

                case VK_ACCESS_HOST_WRITE_BIT:
                    stages |= VK_PIPELINE_STAGE_HOST_BIT;
                    break;
                }
            }
            return stages;
        }

        static void set_layout(void* cmd_buffer, void* image, const VkImageAspectFlags aspect_mask, const uint32_t mip_index, const uint32_t mip_range, const uint32_t array_length, const RHI_Image_Layout layout_old, const RHI_Image_Layout layout_new)
        {
            SP_ASSERT(cmd_buffer != nullptr);
            SP_ASSERT(image != nullptr);

            VkImageMemoryBarrier image_barrier            = {};
            image_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_barrier.pNext                           = nullptr;
            image_barrier.oldLayout                       = vulkan_image_layout[static_cast<VkImageLayout>(layout_old)];
            image_barrier.newLayout                       = vulkan_image_layout[static_cast<VkImageLayout>(layout_new)];
            image_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.image                           = static_cast<VkImage>(image);
            image_barrier.subresourceRange.aspectMask     = aspect_mask;
            image_barrier.subresourceRange.baseMipLevel   = mip_index;
            image_barrier.subresourceRange.levelCount     = mip_range;
            image_barrier.subresourceRange.baseArrayLayer = 0;
            image_barrier.subresourceRange.layerCount     = array_length;
            image_barrier.srcAccessMask                   = layout_to_access_mask(image_barrier.oldLayout, false);
            image_barrier.dstAccessMask                   = layout_to_access_mask(image_barrier.newLayout, true);

            VkPipelineStageFlags source_stage_mask = 0;
            {
                if (image_barrier.oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                {
                    source_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                }
                else if (image_barrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    source_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                }
                else
                {
                    source_stage_mask = access_flags_to_pipeline_stage(image_barrier.srcAccessMask);
                }
            }

            VkPipelineStageFlags destination_stage_mask = 0;
            {
                if (image_barrier.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                {
                    destination_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                }
                else
                {
                    destination_stage_mask = access_flags_to_pipeline_stage(image_barrier.dstAccessMask);
                }
            }

            vkCmdPipelineBarrier
            (
                static_cast<VkCommandBuffer>(cmd_buffer), // commandBuffer
                source_stage_mask,                        // srcStageMask
                destination_stage_mask,                   // dstStageMask
                0,                                        // dependencyFlags
                0,                                        // memoryBarrierCount
                nullptr,                                  // pMemoryBarriers
                0,                                        // bufferMemoryBarrierCount
                nullptr,                                  // pBufferMemoryBarriers
                1,                                        // imageMemoryBarrierCount
                &image_barrier                            // pImageMemoryBarriers
            );
        }

        static void set_layout(void* cmd_buffer, RHI_Texture* texture, const uint32_t mip_start, const uint32_t mip_range, const uint32_t array_length, const RHI_Image_Layout layout_old, const RHI_Image_Layout layout_new)
        {
            SP_ASSERT(cmd_buffer != nullptr);
            SP_ASSERT(texture != nullptr);

            set_layout(cmd_buffer, texture->GetRhiResource(), get_aspect_mask(texture), mip_start, mip_range, array_length, layout_old, layout_new);
        }

        namespace view
        {
            inline void create(
                void* image,
                void*& image_view,
                VkImageViewType type,
                const VkFormat format,
                const VkImageAspectFlags aspect_mask,
                const uint32_t array_index,
                const uint32_t array_length,
                const uint32_t mip_index,
                const uint32_t mip_count
            )
            {
                VkImageViewCreateInfo create_info           = {};
                create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                create_info.image                           = static_cast<VkImage>(image);
                create_info.viewType                        = type;
                create_info.format                          = format;
                create_info.subresourceRange.aspectMask     = aspect_mask;
                create_info.subresourceRange.baseMipLevel   = mip_index;
                create_info.subresourceRange.levelCount     = mip_count;
                create_info.subresourceRange.baseArrayLayer = array_index;
                create_info.subresourceRange.layerCount     = array_length;
                create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;

                SP_ASSERT_MSG(vkCreateImageView(RHI_Context::device, &create_info, nullptr, reinterpret_cast<VkImageView*>(&image_view)) == VK_SUCCESS, "Failed to create image view");
            }

            inline void create(
                void* image,
                void*& image_view,
                const RHI_Texture* texture,
                const ResourceType resource_type,
                const uint32_t array_index,
                const uint32_t array_length,
                const uint32_t mip_index,
                const uint32_t mip_count,
                const bool only_depth,
                const bool only_stencil
            )
            {
                VkImageViewType type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

                if (resource_type == ResourceType::Texture2d)
                {
                    type = VK_IMAGE_VIEW_TYPE_2D;
                }
                else if (resource_type == ResourceType::Texture2dArray)
                {
                    type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                }
                else if (resource_type == ResourceType::TextureCube)
                {
                    type = VK_IMAGE_VIEW_TYPE_CUBE;
                }

                create(image, image_view, type, vulkan_format[rhi_format_to_index(texture->GetFormat())], get_aspect_mask(texture, only_depth, only_stencil), array_index, array_length, mip_index, mip_count);
            }
        }
    }
}
