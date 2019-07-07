///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_RENDER_THREAD_HPP
#define VWM_RENDER_THREAD_HPP

#include <ftk/ui/toplevel_window.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>

namespace vwm {

template <typename Backend>
std::thread render_thread (ftk::ui::toplevel_window<Backend>* toplevel, bool& dirty, bool& exit, std::mutex& mutex, std::condition_variable& cond)
{
  std::thread thread
    ([toplevel, &dirty, &exit, &mutex, &cond]
     {
       while (!exit)
       {
         std::cout << "render thread waiting to render (before lock)" << std::endl;
         std::unique_lock<std::mutex> l(mutex);
         std::cout << "render thread waiting to render" << std::endl;
         while (!dirty && !exit)
         {
           cond.wait(l);
         }
         if (exit) break;
         if (dirty == true)
           dirty = false;
         l.unlock();
                         
         std::cout << "drawing" << std::endl;

         auto image_pipeline = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput);
         VkSamplerCreateInfo samplerInfo = {};
         samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
         samplerInfo.magFilter = VK_FILTER_LINEAR;
         samplerInfo.minFilter = VK_FILTER_LINEAR;

         samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
         samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
         samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

         samplerInfo.anisotropyEnable = VK_FALSE;
         samplerInfo.maxAnisotropy = 0;

         samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

         samplerInfo.unnormalizedCoordinates = VK_FALSE;

         samplerInfo.compareEnable = VK_FALSE;
         samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

         samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
         samplerInfo.mipLodBias = 0.0f;
         samplerInfo.minLod = 0.0f;
         samplerInfo.maxLod = 0.0f;
         
         std::cout << __FILE__ ":" << __LINE__ << std::endl;
         VkSampler textureSampler;
         CHRONO_START()
         if (vkCreateSampler(toplevel->window.voutput.device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
           throw std::runtime_error("failed to create texture sampler!");
         }
         CHRONO_COMPARE()

         for (auto&& image : toplevel->images)
                         {
                           using fastdraw::output::vulkan::from_result;
                           using fastdraw::output::vulkan::vulkan_error_code;
                           
                           CHRONO_START ()
                           // record buffer
                           VkCommandPool commandPool;

                           VkCommandPoolCreateInfo poolInfo = {};
                           poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                           poolInfo.queueFamilyIndex = toplevel->window.graphicsFamilyIndex;
                           poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

                           if (vkCreateCommandPool(toplevel->window.voutput.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                             throw std::runtime_error("failed to create command pool!");
                           }
                           CHRONO_COMPARE()

                           VkCommandBuffer commandBuffer;
                           VkCommandBufferAllocateInfo allocInfo = {};
                           allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                           allocInfo.commandPool = commandPool;
                           allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                           allocInfo.commandBufferCount = 1;

                           if (vkAllocateCommandBuffers(toplevel->window.voutput.device, &allocInfo, &commandBuffer) != VK_SUCCESS) { 
                             throw std::runtime_error("failed to allocate command buffers!");
                           }
                           CHRONO_COMPARE()

                             std::cout << "acquiring" << std::endl;
                           // if (vkWaitForFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished, VK_FALSE, -1) == VK_TIMEOUT)
                           // {
                           //   std::cout << "Timeout waiting for fence" << std::endl;
                           //   throw -1;
                           // }
                           // vkResetFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished);
                           // submit

                           VkSemaphore imageAvailable, renderFinished;
                           {
                             VkSemaphoreCreateInfo semaphoreInfo = {};
                             semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

                             auto r = from_result(vkCreateSemaphore (toplevel->window.voutput.device
                                                                     , &semaphoreInfo
                                                                     , nullptr, &imageAvailable));
                             if (r != vulkan_error_code::success)
                               throw std::system_error (make_error_code (r));

                             r = from_result(vkCreateSemaphore (toplevel->window.voutput.device
                                                                , &semaphoreInfo
                                                                , nullptr, &renderFinished));
                             if (r != vulkan_error_code::success)
                               throw std::system_error (make_error_code (r));
                           }
                           CHRONO_COMPARE()
                           uint32_t imageIndex;
                           vkAcquireNextImageKHR(toplevel->window.voutput.device, toplevel->window.swapChain
                                                 , std::numeric_limits<uint64_t>::max(), imageAvailable
                                                 , /*toplevel->window.executionFinished*/nullptr, &imageIndex);
                           CHRONO_COMPARE()

                           std::cout << "recording buffer" << std::endl;
                           CHRONO_COMPARE()

                           VkCommandBufferBeginInfo beginInfo = {};
                           beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                           if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                             throw std::runtime_error("failed to begin recording command buffer!");
                           }

                           VkRenderPassBeginInfo renderPassInfo = {};
                           renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                           renderPassInfo.renderPass = toplevel->window.voutput.renderpass;
                           renderPassInfo.framebuffer = toplevel->window.swapChainFramebuffers[imageIndex];
                           renderPassInfo.renderArea.offset = {0, 0};
                           renderPassInfo.renderArea.extent = toplevel->window.voutput.swapChainExtent;

                           vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                           //fastdraw::output::vulkan::draw (draw_info, commandBuffer);
                           {
                             
                             // VkDescriptorSetAllocateInfo allocInfo = {};
                             // allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                             // allocInfo.descriptorPool = descriptorPool;
                             // allocInfo.descriptorSetCount = 1;
                             // allocInfo.pSetLayouts = /*layouts.data()*/&descriptorSetLayout;

                             // CHRONO_COMPARE()
                             // r = from_result(vkAllocateDescriptorSets(output.device, &allocInfo, &descriptorSet));
                             // if (r != vulkan_error_code::success)
                             //   throw std::system_error(make_error_code(r));
                             // CHRONO_COMPARE()

                             VkDescriptorImageInfo imageInfo = {};
                             imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                             imageInfo.imageView = image.image_view;
                             imageInfo.sampler = textureSampler;

                             /*std::array<*/VkWriteDescriptorSet/*, 2>*/ descriptorWrites = {};
                             
                             descriptorWrites/*[1]*/.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                             descriptorWrites/*[1]*/.dstSet = *image_pipeline.descriptorSet/*s[i]*/;
                             descriptorWrites/*[1]*/.dstBinding = 1;
                             descriptorWrites/*[1]*/.dstArrayElement = 0;
                             descriptorWrites/*[1]*/.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                             descriptorWrites/*[1]*/.descriptorCount = 1;
                             descriptorWrites/*[1]*/.pImageInfo = &imageInfo;

                             CHRONO_COMPARE()
                             vkUpdateDescriptorSets(toplevel->window.voutput.device, 1/*static_cast<uint32_t>(descriptorWrites.size())*/, &descriptorWrites/*.data()*/, 0, nullptr);
                             CHRONO_COMPARE()

    VkBuffer vertexBuffer;
    {
      auto whole_width = toplevel->window.voutput.swapChainExtent.height;
      auto whole_height = toplevel->window.voutput.swapChainExtent.width;
      // auto whole_width = output.swapChainExtent.width;
      // auto whole_height = output.swapChainExtent.height;

      auto scale = 2;
      
      const float vertices[] =
        {     fastdraw::coordinates::ratio(image.x, whole_width)              , fastdraw::coordinates::ratio(image.y, whole_height)
            , fastdraw::coordinates::ratio(image.x + image.width*scale, whole_width), fastdraw::coordinates::ratio(image.y, whole_height)              
            , fastdraw::coordinates::ratio(image.x + image.width*scale, whole_width), fastdraw::coordinates::ratio(image.y + image.height*scale, whole_height)
            , fastdraw::coordinates::ratio(image.x + image.width*scale, whole_width), fastdraw::coordinates::ratio(image.y + image.height*scale, whole_height)
            , fastdraw::coordinates::ratio(image.x, whole_width)              , fastdraw::coordinates::ratio(image.y + image.height*scale, whole_height)
            , fastdraw::coordinates::ratio(image.x, whole_width)              , fastdraw::coordinates::ratio(image.y, whole_height)
        };
      const float coordinates[] =
        {   0.0f, 0.0f
            , 1.0f, 0.0f
            , 1.0f, 1.0f
            , 1.0f, 1.0f
            , 0.0f, 1.0f
            , 0.0f, 0.0f
        };
      const float transform_matrix[] =
        {
          /* 90 degrees */
          //   0.0f, -1.0f, 0.0f, 0.0f
          // , 1.0f,  0.0f, 0.0f, 0.0f
          // , 0.0f,  0.0f, 1.0f, 0.0f
          // , 0.0f,  0.0f, 0.0f, 1.0f
          /* normal */
            1.0f, 0.0f, 0.0f, 0.0f
          , 0.0f, 1.0f, 0.0f, 0.0f
          , 0.0f, 0.0f, 1.0f, 0.0f
          , 0.0f, 0.0f, 0.0f, 1.0f
        };

      for (int i = 0; i != 12; i += 2)
      {        
        std::cout << "x: " << vertices[i] << " y: " << vertices[i+1] << std::endl;
      }

      auto findMemoryType =
        [] (uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice) -> uint32_t
        { 
          VkPhysicalDeviceMemoryProperties memProperties;
          vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

          for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
              return i;
            }
          }
  
          throw std::runtime_error("failed to find suitable memory type!");
        };


      auto create_vertex_buffer =
        [findMemoryType] (VkDevice device, std::size_t size, VkPhysicalDevice physicalDevice) -> std::pair<VkBuffer, VkDeviceMemory>
        {
         VkBuffer vertexBuffer;
         VkDeviceMemory vertexBufferMemory;

         VkBufferCreateInfo bufferInfo = {};
         bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
         bufferInfo.size = /*sizeof(vertices[0]) * vertices.size()*/size;
         bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
         bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

         if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
           throw std::runtime_error("failed to create vertex buffer!");
         }

         VkMemoryRequirements memRequirements;
         vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

         VkMemoryAllocateInfo allocInfo = {};
         allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
         allocInfo.allocationSize = memRequirements.size;
         allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, physicalDevice);

         if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
           throw std::runtime_error("failed to allocate vertex buffer memory!");
         }
         vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

         return {vertexBuffer, vertexBufferMemory};
        };

      
      VkDeviceMemory vertexBufferMemory;
      std::tie(vertexBuffer, vertexBufferMemory) = create_vertex_buffer (toplevel->window.voutput.device, sizeof(vertices) + sizeof(coordinates)
                                                                         + sizeof(transform_matrix)
                                                                         , toplevel->window.voutput.physical_device);

  CHRONO_COMPARE()
      void* data;
      vkMapMemory(toplevel->window.voutput.device, vertexBufferMemory, 0, sizeof(vertices) + sizeof(coordinates) + sizeof(transform_matrix), 0, &data);
    
  CHRONO_COMPARE()
      std::memcpy(data, vertices, sizeof(vertices));
      std::memcpy(&static_cast<char*>(data)[sizeof(vertices)], coordinates, sizeof(coordinates));
      std::memcpy(&static_cast<char*>(data)[sizeof(vertices) + sizeof(coordinates)], transform_matrix, sizeof(transform_matrix));

  CHRONO_COMPARE()
      vkUnmapMemory(toplevel->window.voutput.device, vertexBufferMemory);
  CHRONO_COMPARE()

                             }
                             std::vector<VkDeviceSize> offsets;
                             std::vector<VkBuffer> buffers;
                             // for (auto&& buffer_info : info.vertex_buffers)
                             // {
                               offsets.push_back(0);
                               buffers.push_back(vertexBuffer);
                               offsets.push_back(0);
                               buffers.push_back(vertexBuffer);
                             // }
                             vkCmdBindVertexBuffers(commandBuffer, 0, 2, &buffers[0], &offsets[0]);

                             assert (!!image_pipeline.descriptorSet);
                             {
                               vkCmdBindDescriptorSets(
                                                       commandBuffer,
                                                       VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                       /*info.descriptorSetLayout*/image_pipeline.pipeline_layout,
                                                       0, // firstSet
                                                       1, // descriptorSetCount
                                                       &*image_pipeline.descriptorSet,
                                                       0, // dynamicOffsetCount
                                                       nullptr); // pDynamicOffsets  
                             }              
                             vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline);
                             vkCmdDraw(commandBuffer, 6, 1, 0, 0);
                             
                           }
        
                           vkCmdEndRenderPass(commandBuffer);

                           if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                             throw std::runtime_error("failed to record command buffer!");
                           }

                           CHRONO_COMPARE()
                           {
                             VkSubmitInfo submitInfo = {};
                             submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

                             std::cout << "submit graphics" << std::endl;
      
                             VkSemaphore waitSemaphores[] = {imageAvailable};
                             VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                             submitInfo.waitSemaphoreCount = 1;
                             submitInfo.pWaitSemaphores = waitSemaphores;
                             submitInfo.pWaitDstStageMask = waitStages;
                             submitInfo.commandBufferCount = 1;
                             submitInfo.pCommandBuffers = &commandBuffer;

                             VkSemaphore signalSemaphores[] = {renderFinished};
                             submitInfo.signalSemaphoreCount = 1;
                             submitInfo.pSignalSemaphores = signalSemaphores;
                             using fastdraw::output::vulkan::from_result;
                             using fastdraw::output::vulkan::vulkan_error_code;
                             auto r = from_result(vkQueueSubmit(toplevel->window.voutput.graphics_queue, 1, &submitInfo, toplevel->window.executionFinished));
                             if (r != vulkan_error_code::success)
                               throw std::system_error(make_error_code (r));
                           }
                           CHRONO_COMPARE_FORCE()

                           std::cout << "graphic" << std::endl;
                           if (vkWaitForFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished, VK_FALSE, -1) == VK_TIMEOUT)
                           {
                             std::cout << "Timeout waiting for fence" << std::endl;
                             throw -1;
                           }
                           vkResetFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished);
                           
                           std::cout << "submit presentation" << std::endl;
                           CHRONO_COMPARE()
                           {
                             VkPresentInfoKHR presentInfo = {};
                             presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

                             VkSemaphore waitSemaphores[] = {renderFinished};

                             presentInfo.waitSemaphoreCount = 1;
                             presentInfo.pWaitSemaphores = waitSemaphores;

                             VkSwapchainKHR swapChains[] = {toplevel->window.swapChain};
                             presentInfo.swapchainCount = 1;
                             presentInfo.pSwapchains = swapChains;
                             presentInfo.pImageIndices = &imageIndex;
                             presentInfo.pResults = nullptr; // Optional

                             using fastdraw::output::vulkan::from_result;
                             using fastdraw::output::vulkan::vulkan_error_code;
                             auto r = from_result (vkQueuePresentKHR(toplevel->window.voutput.present_queue, &presentInfo));
                             if (r != vulkan_error_code::success)
                               throw std::system_error (make_error_code (r));
                           }
                           CHRONO_COMPARE()
                           
                         }
                       }
                       std::cout << "Exiting render thread" << std::endl;
                     });
  return thread;
}

inline std::function<void()> render_dirty (bool& dirty, std::mutex& mutex, std::condition_variable& cond)
{
  return [dirty = &dirty, mutex = &mutex, cond = &cond]
         {
           std::unique_lock<std::mutex> l(*mutex);
           *dirty = true;
           cond->notify_one();
         };
}

inline std::function<void()> render_exit (bool& exit, std::mutex& mutex, std::condition_variable& cond)
{
  return [exit = &exit, mutex = &mutex, cond = &cond]
         {
           std::unique_lock<std::mutex> l(*mutex);
           *exit = true;
           cond->notify_one();
         };
}
  
}

#endif

