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

//= INCLUDES =====================
#include "pch.h"
#include "../RHI_Implementation.h"
#include "../RHI_Device.h"
#include "../RHI_IndexBuffer.h"
#include "../RHI_CommandList.h"
#include "../Rendering/Renderer.h"
//================================

//= NAMESPACES =====
using namespace std;
//==================

namespace Spartan
{
    RHI_IndexBuffer::~RHI_IndexBuffer()
    {
        if (m_rhi_resource)
        {
            RHI_Device::AddToDeletionQueue(RHI_Resource_Type::Buffer, m_rhi_resource);
            m_rhi_resource = nullptr;
        }
    }

    void RHI_IndexBuffer::_create(const void* indices)
    {
        // Destroy previous buffer
        if (m_rhi_resource)
        {
            RHI_Device::AddToDeletionQueue(RHI_Resource_Type::Buffer, m_rhi_resource);
            m_rhi_resource = nullptr;
        }

        m_is_mappable = indices == nullptr;

        if (m_is_mappable)
        {
            // Define memory properties
            VkMemoryPropertyFlags flags  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // mappable

            // Create
            RHI_Device::CreateBuffer(m_rhi_resource, m_object_size_gpu, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, flags, nullptr, m_object_name.c_str());

            // Get mapped data pointer
            m_mapped_data = RHI_Device::GetMappedDataFromBuffer(m_rhi_resource);
        }
        else // The reason we use staging is because memory with VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT is not mappable but it's fast, we want that.
        {
            // Create staging/source buffer and copy the indices to it
            void* staging_buffer = nullptr;
            RHI_Device::CreateBuffer(staging_buffer, m_object_size_gpu, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indices, m_object_name.c_str());

            // Create destination buffer
            RHI_Device::CreateBuffer(m_rhi_resource, m_object_size_gpu, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr, m_object_name.c_str());

            // Copy staging buffer to destination buffer
            {
                // Create command buffer
                RHI_CommandList* cmd_list = RHI_Device::ImmediateBegin(RHI_Queue_Type::Copy);

                VkBuffer* buffer_vk         = reinterpret_cast<VkBuffer*>(&m_rhi_resource);
                VkBuffer* buffer_staging_vk = reinterpret_cast<VkBuffer*>(&staging_buffer);

                // Copy
                VkBufferCopy copy_region = {};
                copy_region.size = m_object_size_gpu;
                vkCmdCopyBuffer(static_cast<VkCommandBuffer>(cmd_list->GetRhiResource()), *buffer_staging_vk, *buffer_vk, 1, &copy_region);

                // Flush and free command buffer
                RHI_Device::ImmediateSubmit(cmd_list);

                // Destroy staging buffer
                RHI_Device::DestroyBuffer(staging_buffer);
            }
        }

        // Set debug name
        RHI_Device::SetResourceName(m_rhi_resource, RHI_Resource_Type::Buffer, m_object_name);
    }

    void* RHI_IndexBuffer::Map()
    {
        return m_mapped_data;
    }

    void RHI_IndexBuffer::Unmap()
    {
        // buffer is mapped on creation and unmapped during destruction
    }
}
