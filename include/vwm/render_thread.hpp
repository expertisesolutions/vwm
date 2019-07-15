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

namespace detail {

VkSampler render_thread_create_sampler (VkDevice device)
{
  using fastdraw::output::vulkan::from_result;
  using fastdraw::output::vulkan::vulkan_error_code;
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
  VkSampler texture_sampler;
  //CHRONO_COMPARE()
  auto r = from_result(vkCreateSampler(device, &samplerInfo, nullptr, &texture_sampler));
  if (r != vulkan_error_code::success)
    throw std::system_error(make_error_code(r));

  return texture_sampler;
}

VkSemaphore render_thread_create_semaphore (VkDevice device)
{
  using fastdraw::output::vulkan::from_result;
  using fastdraw::output::vulkan::vulkan_error_code;
  VkSemaphore semaphore;
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  auto r = from_result(vkCreateSemaphore (device, &semaphoreInfo, nullptr, &semaphore));
  if (r != vulkan_error_code::success)
    throw std::system_error(make_error_code(r));
  return semaphore;
}
  
}
  
template <typename Backend>
std::thread render_thread (ftk::ui::toplevel_window<Backend>* toplevel, bool& dirty, bool& exit, std::mutex& mutex, std::condition_variable& cond)
{
  std::thread thread
    ([toplevel, &dirty, &exit, &mutex, &cond]
     {
       VkSampler sampler = detail::render_thread_create_sampler (toplevel->window.voutput.device);
       auto const image_pipeline = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput);
       while (!exit)
       {
         std::cout << "render thread waiting to render (before lock)" << std::endl;
         std::unique_lock<std::mutex> l(mutex);
         std::cout << "render thread waiting to render" << std::endl;
         while (!dirty && !exit)
         {
           std::cout << "not dirty and not exit" << std::endl;
           cond.wait(l);
           std::cout << "condvar wake up" << std::endl;
         }
         if (exit) break;
         if (dirty == true)
           dirty = false;
         l.unlock();
                         
         std::cout << "drawing" << std::endl;
         using fastdraw::output::vulkan::from_result;
         using fastdraw::output::vulkan::vulkan_error_code;

         // first should create vertexbuffer for all, then record command buffer and then submitting
         std::cout << "number of images to draw " << toplevel->images.size() << std::endl;  
         auto images = toplevel->images;

         VkCommandPool commandPool = toplevel->window.voutput.command_pool;

         std::unique_ptr<VkCommandBuffer[]> commandBuffer{new VkCommandBuffer[images.size()]};
         VkCommandBufferAllocateInfo allocInfo = {};
         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         allocInfo.commandPool = commandPool;
         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         allocInfo.commandBufferCount = images.size();

         if (vkAllocateCommandBuffers(toplevel->window.voutput.device, &allocInfo, &commandBuffer[0]) != VK_SUCCESS) { 
           throw std::runtime_error("failed to allocate command buffers!");
         }
         //CHRONO_COMPARE()

         VkRenderPass renderPass;
         {
           VkAttachmentDescription colorAttachment = {};
           colorAttachment.format = toplevel->window.voutput.swapChainImageFormat;
           colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
           colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
           colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
           colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
           colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
           colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
           colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

           VkAttachmentReference colorAttachmentRef = {};
           colorAttachmentRef.attachment = 0;
           colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  
           VkSubpassDescription subpass = {};
           subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
           subpass.colorAttachmentCount = 1;
           subpass.pColorAttachments = &colorAttachmentRef;

           VkSubpassDependency dependency = {};
           dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
           dependency.dstSubpass = 0;
           dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
           dependency.srcAccessMask = 0;
           dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
           dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

           VkRenderPassCreateInfo renderPassInfo = {};
           renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
           renderPassInfo.attachmentCount = 1;
           renderPassInfo.pAttachments = &colorAttachment;
           renderPassInfo.subpassCount = 1;
           renderPassInfo.pSubpasses = &subpass;
           renderPassInfo.dependencyCount = 1;
           renderPassInfo.pDependencies = &dependency;
      
           if (vkCreateRenderPass(toplevel->window.voutput.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
             throw std::runtime_error("failed to create render pass!");
           }
         }
         
         VkSemaphore imageAvailable = detail::render_thread_create_semaphore (toplevel->window.voutput.device)
           , renderFinished = detail::render_thread_create_semaphore (toplevel->window.voutput.device);

         //CHRONO_COMPARE()
         uint32_t imageIndex;
         vkAcquireNextImageKHR(toplevel->window.voutput.device, toplevel->window.swapChain
                               , std::numeric_limits<uint64_t>::max(), imageAvailable
                               , /*toplevel->window.executionFinished*/nullptr, &imageIndex);
         //std::cout << "recording buffer" << std::endl;

         std::unique_ptr<VkDescriptorSet[]> descriptorSet {new VkDescriptorSet[images.size()]};
         {
           VkDescriptorPool descriptorPool;
           /*std::array<*/VkDescriptorPoolSize/*, 2>*/ poolSizes = {};
           // poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
           // poolSizes[0].descriptorCount = /*static_cast<uint32_t>(swapChainImages.size())*/1;
           poolSizes/*[1]*/.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
           poolSizes/*[1]*/.descriptorCount = images.size();

           VkDescriptorPoolCreateInfo poolInfo = {};
           poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
           poolInfo.poolSizeCount = 1/*static_cast<uint32_t>(poolSizes.size())*/;
           poolInfo.pPoolSizes = &poolSizes/*.data()*/;
           poolInfo.maxSets = images.size();

           //CHRONO_COMPARE()
           if (vkCreateDescriptorPool(toplevel->window.voutput.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
             throw std::runtime_error("failed to create descriptor pool!");
           }

           std::unique_ptr<VkDescriptorSetLayout[]> descriptor_set_layouts {new VkDescriptorSetLayout[images.size()]};
           for (VkDescriptorSetLayout* first = &descriptor_set_layouts[0]
                  , *last = first + images.size(); first != last; ++ first)
             *first = image_pipeline.descriptorSetLayout;
           
           VkDescriptorSetAllocateInfo allocInfo = {};
           allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
           allocInfo.descriptorPool = descriptorPool;
           allocInfo.descriptorSetCount = images.size();
           allocInfo.pSetLayouts = &descriptor_set_layouts[0];

           //CHRONO_COMPARE()
           if (vkAllocateDescriptorSets(toplevel->window.voutput.device, &allocInfo, &descriptorSet[0]) != VK_SUCCESS) {
             throw std::runtime_error("failed to allocate descriptor sets!");
           }
           //CHRONO_COMPARE()
         }

         VkBuffer vertexBuffer;
         const unsigned int vertex_data_size = (12 + 12 + 16) * sizeof(float);
           {
             auto whole_width = toplevel->window.voutput.swapChainExtent.width;
             auto whole_height = toplevel->window.voutput.swapChainExtent.height;
             // auto whole_width = output.swapChainExtent.width;
             // auto whole_height = output.swapChainExtent.height;

             auto scale = 2;

             //auto& image = images[0];
      
      //        for (int i = 0; i != 12; i += 2)
      //          {     
      //   std::cout << "x: " << vertices[i] << " y: " << vertices[i+1] << std::endl;
      // }

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
             std::tie(vertexBuffer, vertexBufferMemory) = create_vertex_buffer
               (toplevel->window.voutput.device, vertex_data_size * images.size(), toplevel->window.voutput.physical_device);

             //CHRONO_COMPARE()
             void* data;
             vkMapMemory(toplevel->window.voutput.device, vertexBufferMemory, 0, vertex_data_size * images.size(), 0, &data);

             //std::cout << "whole_width " << whole_width << " whole_height " << whole_height << std::endl;
             
             unsigned int i = 0;
             for (auto&& image : images)
             {
               auto x = image.x;
               auto y = image.y;
               const float vertices[] =
                 {     fastdraw::coordinates::ratio(x, whole_width/*/scale*/)/**2-1.0f*/              , fastdraw::coordinates::ratio(y, whole_height/*/scale*/)/**2-1.0f*/
                 , fastdraw::coordinates::ratio(x + image.width, whole_width/*/scale*/)/**2-1.0f*/, fastdraw::coordinates::ratio(y, whole_height/*/scale*/)/**2-1.0f*/
                 , fastdraw::coordinates::ratio(x + image.width, whole_width/*/scale*/)/**2-1.0f*/, fastdraw::coordinates::ratio(y + image.height, whole_height/*/scale*/)/**2-1.0f*/
                 , fastdraw::coordinates::ratio(x + image.width, whole_width/*/scale*/)/**2-1.0f*/, fastdraw::coordinates::ratio(y + image.height, whole_height/*/scale*/)/**2-1.0f*/
                 , fastdraw::coordinates::ratio(x, whole_width/*/scale*/)/**2-1.0f*/              , fastdraw::coordinates::ratio(y + image.height, whole_height/*/scale*/)/**2-1.0f*/
                 , fastdraw::coordinates::ratio(x, whole_width/*/scale*/)/**2-1.0f*/              , fastdraw::coordinates::ratio(y, whole_height/*/scale*/)/**2-1.0f*/
             };
               // const float vertices[]
               // {
               //      -1.0f,-1.0f
               //     , 1.0f,-1.0f
               //     , 1.0f, 1.0f
               //     , 1.0f, 1.0f
               //     ,-1.0f, 1.0f
               //     ,-1.0f,-1.0f
               // };
             const float coordinates[] =
             {     0.0f, 0.0f
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
             assert(vertex_data_size == sizeof(vertices) + sizeof(coordinates) + sizeof(transform_matrix));
               char* buffer = static_cast<char*>(data) + i * vertex_data_size;
               //CHRONO_COMPARE()
               std::memcpy(buffer, vertices, sizeof(vertices));
               std::memcpy(&buffer[sizeof(vertices)], coordinates, sizeof(coordinates));
               std::memcpy(&buffer[sizeof(vertices) + sizeof(coordinates)], transform_matrix, sizeof(transform_matrix));
               ++i;
             }

             //CHRONO_COMPARE()
             vkUnmapMemory(toplevel->window.voutput.device, vertexBufferMemory);
             //CHRONO_COMPARE()
           } // vertex buffer
         
         std::size_t descriptor_index = 0;
         for (auto&& image : images)
         {
           //CHRONO_START()
           using fastdraw::output::vulkan::from_result;
           using fastdraw::output::vulkan::vulkan_error_code;
           VkDescriptorImageInfo imageInfo = {};
           imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
           imageInfo.imageView = image.image_view;
           imageInfo.sampler = sampler;

           /*std::array<*/VkWriteDescriptorSet/*, 2>*/ descriptorWrites = {};
                             
           descriptorWrites/*[1]*/.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
           descriptorWrites/*[1]*/.dstSet = descriptorSet[descriptor_index]/*s[i]*/;
           descriptorWrites/*[1]*/.dstBinding = 1;
           descriptorWrites/*[1]*/.dstArrayElement = 0;
           descriptorWrites/*[1]*/.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
           descriptorWrites/*[1]*/.descriptorCount = 1;
           descriptorWrites/*[1]*/.pImageInfo = &imageInfo;

           //CHRONO_COMPARE()
           vkUpdateDescriptorSets(toplevel->window.voutput.device, 1/*static_cast<uint32_t>(descriptorWrites.size())*/
                                  , &descriptorWrites/*.data()*/, 0, nullptr);
           //CHRONO_COMPARE()
           ++descriptor_index;
         }

         descriptor_index = 0;
         for (auto&& image : images)
         {
           using fastdraw::output::vulkan::from_result;
           using fastdraw::output::vulkan::vulkan_error_code;

           VkCommandBufferBeginInfo beginInfo = {};
           beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
           if (vkBeginCommandBuffer(commandBuffer[descriptor_index], &beginInfo) != VK_SUCCESS) {
             throw std::runtime_error("failed to begin recording command buffer!");
           }

           VkRenderPassBeginInfo renderPassInfo = {};
           renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
           renderPassInfo.framebuffer = toplevel->window.swapChainFramebuffers[imageIndex];
           renderPassInfo.renderArea.offset = {image.x, image.y};
           auto width = image.x + image.width <= toplevel->window.voutput.swapChainExtent.width ? image.width : toplevel->window.voutput.swapChainExtent.width - image.x;
           auto height = image.y + image.height <= toplevel->window.voutput.swapChainExtent.height ? image.height : toplevel->window.voutput.swapChainExtent.height - image.y;
           renderPassInfo.renderArea.extent = {width/* + image.x*/, height/* + image.y*/};
           VkClearValue clear_value;
           // clearing for testing
           if (!descriptor_index)
           {
             renderPassInfo.renderPass = toplevel->window.voutput.renderpass;
             renderPassInfo.clearValueCount = 1;
             clear_value.color.uint32[0] = 0;
             clear_value.color.uint32[1] = 0;
             clear_value.color.uint32[2] = 0;
             clear_value.color.uint32[3] = 0;
             renderPassInfo.pClearValues = &clear_value;
           }
           else
             renderPassInfo.renderPass = renderPass;
         
           vkCmdBeginRenderPass(commandBuffer[descriptor_index], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

           vkCmdBindPipeline(commandBuffer[descriptor_index], VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline);

           std::vector<VkDeviceSize> offsets;
           std::vector<VkBuffer> buffers;

           offsets.push_back(vertex_data_size * descriptor_index);
           buffers.push_back(vertexBuffer);
           offsets.push_back(vertex_data_size * descriptor_index);
           buffers.push_back(vertexBuffer);

           vkCmdBindVertexBuffers(commandBuffer[descriptor_index], 0, 2, &buffers[0], &offsets[0]);
           
           vkCmdBindDescriptorSets(commandBuffer[descriptor_index], VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline_layout
                                   , 0, 1, &descriptorSet[descriptor_index] , 0, nullptr);
           
           vkCmdDraw(commandBuffer[descriptor_index], 6, 1, 0, 0);

           vkCmdEndRenderPass(commandBuffer[descriptor_index]);

           if (vkEndCommandBuffer(commandBuffer[descriptor_index]) != VK_SUCCESS) {
             throw std::runtime_error("failed to record command buffer!");
           }
           ++descriptor_index;
       } // for ?

         //vkDestroyRenderPass(toplevel->window.voutput.device, renderPass, nullptr);

       //CHRONO_COMPARE()
       {
         VkSubmitInfo submitInfo = {};
         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                             
         std::cout << "submit graphics" << std::endl;

         //std::array<VkCommandBuffer, 2> first_and_last{commandBuffer[0], commandBuffer[images.size()-1]};
         
         VkSemaphore waitSemaphores[] = {imageAvailable};
         VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
         submitInfo.waitSemaphoreCount = 1;
         submitInfo.pWaitSemaphores = waitSemaphores;
         submitInfo.pWaitDstStageMask = waitStages;
         submitInfo.commandBufferCount = images.size();
         submitInfo.pCommandBuffers = &commandBuffer[0];

         VkSemaphore signalSemaphores[] = {renderFinished};
         submitInfo.signalSemaphoreCount = 1;
         submitInfo.pSignalSemaphores = signalSemaphores;
         using fastdraw::output::vulkan::from_result;
         using fastdraw::output::vulkan::vulkan_error_code;

         ftk::ui::backend::vulkan_queues::lock_graphic_queue lock_queue(toplevel->window.queues);
         
         auto r = from_result(vkQueueSubmit(lock_queue.get_queue().queue_, 1, &submitInfo, toplevel->window.executionFinished));
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code (r));
       }
       //CHRONO_COMPARE_FORCE()

       std::cout << "graphic" << std::endl;
       if (vkWaitForFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished, VK_FALSE, -1) == VK_TIMEOUT)
       {
         std::cout << "Timeout waiting for fence" << std::endl;
         throw -1;
       }
       vkResetFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished);
                           
       std::cout << "submit presentation" << std::endl;
       //CHRONO_COMPARE()
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

         ftk::ui::backend::vulkan_queues::lock_presentation_queue lock_queue(toplevel->window.queues);
         
         using fastdraw::output::vulkan::from_result;
         using fastdraw::output::vulkan::vulkan_error_code;
         auto r = from_result (vkQueuePresentKHR(lock_queue.get_queue().queue_, &presentInfo));
         if (r != vulkan_error_code::success)
           throw std::system_error (make_error_code (r));
       }
       //CHRONO_COMPARE()
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

