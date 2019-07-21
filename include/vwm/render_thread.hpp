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
       using fastdraw::output::vulkan::from_result;
       using fastdraw::output::vulkan::vulkan_error_code;
       VkSampler sampler = detail::render_thread_create_sampler (toplevel->window.voutput.device);
       auto const image_pipeline = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput);

       VkFence executionFinished[2];
       VkFenceCreateInfo fenceInfo = {};
       fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
       auto r = from_result (vkCreateFence (toplevel->window.voutput.device, &fenceInfo, nullptr, &executionFinished[0]));
       if (r != vulkan_error_code::success)
         throw std::system_error(make_error_code(r));
       r = from_result(vkCreateFence (toplevel->window.voutput.device, &fenceInfo, nullptr, &executionFinished[1]));
       if (r != vulkan_error_code::success)
         throw std::system_error(make_error_code(r));
       
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

         /**** acquire data ****/
         l.unlock();
         uint32_t imageIndex;

         VkSemaphore imageAvailable, renderFinished;
         imageAvailable = detail::render_thread_create_semaphore (toplevel->window.voutput.device);
         renderFinished = detail::render_thread_create_semaphore (toplevel->window.voutput.device);

         vkAcquireNextImageKHR(toplevel->window.voutput.device, toplevel->window.swapChain
                               , std::numeric_limits<uint64_t>::max(), imageAvailable
                               , /*toplevel->window.executionFinished*/nullptr, &imageIndex);
         l.lock();
         std::cout << "drawing" << std::endl;

         decltype (toplevel->images) images;
         decltype (toplevel->buffer_cache) buffer_cache;

         unsigned int i = 0;
         for (auto&& image : toplevel->images)
         {
           if (image.must_draw)
           {
             std::cout << "found drawable image" << std::endl;
             images.push_back(image);
             buffer_cache.push_back (toplevel->buffer_cache[i]);
             image.must_draw = false;
             image.framebuffers_regions[imageIndex] = {image.x, image.y, image.width, image.height};
           }
           i++;
         }
         std::cout << "number of images to draw " << toplevel->images.size() << std::endl;

         //l.unlock();
                         
         // first should create vertexbuffer for all, then record command buffer and then submitting
         VkCommandPool commandPool = toplevel->window.voutput.command_pool;

         std::vector<VkCommandBuffer> damaged_command_buffers;

         auto framebuffer_damaged_regions = std::move (toplevel->framebuffers_damaged_regions[imageIndex]);
         assert (toplevel->framebuffers_damaged_regions[imageIndex].empty());

         damaged_command_buffers.resize (framebuffer_damaged_regions.size());
         
         VkCommandBufferAllocateInfo allocInfo = {};
         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         allocInfo.commandPool = commandPool;
         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         allocInfo.commandBufferCount = damaged_command_buffers.size();

         if (!damaged_command_buffers.empty())
         {
           auto r = from_result (vkAllocateCommandBuffers(toplevel->window.voutput.device, &allocInfo
                                                          , &damaged_command_buffers[0]));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code (r));
         }
           
         std::cout << "image index " << imageIndex << " damaged regions " << framebuffer_damaged_regions.size()
                   << std::endl;

         // draw damage areas
         i = 0;
         for (auto&& region : framebuffer_damaged_regions)
         {
           auto damaged_command_buffer = damaged_command_buffers[i];

           auto x = region.x;
           auto y = region.y;
           auto width = region.width;
           auto height = region.height;

           std::cout << "damaged rect " << x << "x" << y << "-" << width << "x" << height << std::endl;
           
           VkRenderPassBeginInfo renderPassInfo = {};
           renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
           renderPassInfo.framebuffer = toplevel->window.swapChainFramebuffers[imageIndex];
           renderPassInfo.renderArea.offset = {x, y};
           {
             auto w = x + width <= toplevel->window.voutput.swapChainExtent.width
               ? width : toplevel->window.voutput.swapChainExtent.width - x;
             auto h = y + height <= toplevel->window.voutput.swapChainExtent.height
               ? height : toplevel->window.voutput.swapChainExtent.height - y;
             renderPassInfo.renderArea.extent = {w/* + image.x*/, h/* + image.y*/};
           }
           // renderPassInfo.renderArea.offset = {0,0};
           // renderPassInfo.renderArea.extent = {toplevel->window.voutput.swapChainExtent.width
           //                                     , toplevel->window.voutput.swapChainExtent.height};
           renderPassInfo.renderPass = toplevel->window.voutput.renderpass;

           VkCommandBufferBeginInfo beginInfo = {};
           beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
           beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
           //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
           auto r = from_result (vkBeginCommandBuffer(damaged_command_buffer, &beginInfo));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code(r));

           vkCmdBeginRenderPass(damaged_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

           auto const static image_pipeline = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput);
           vkCmdBindPipeline(damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline);
           {
             std::vector<VkDeviceSize> offsets;
             std::vector<VkBuffer> buffers;

             offsets.push_back(0);
             buffers.push_back(toplevel->vbuffer.get_buffer());
             offsets.push_back(0);
             buffers.push_back(toplevel->vbuffer.get_buffer());
  
             vkCmdBindVertexBuffers(damaged_command_buffer, 0, 2, &buffers[0], &offsets[0]);

             VkDescriptorImageInfo imageInfo = {};
             imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
             imageInfo.imageView = toplevel->background.image_view;
             imageInfo.sampler = sampler;

             VkWriteDescriptorSet descriptorWrites = {};
                             
             descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
             descriptorWrites.dstSet = 0;
             descriptorWrites.dstBinding = 1;
             //descriptorWrites.dstArrayElement = 0;
             descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
             descriptorWrites.descriptorCount = 1;
             descriptorWrites.pImageInfo = &imageInfo;

             auto static const vkCmdPushDescriptorSetKHR
               = vkGetDeviceProcAddr (toplevel->window.voutput.device, "vkCmdPushDescriptorSetKHR");
             assert (vkCmdPushDescriptorSetKHR != nullptr);
             reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkCmdPushDescriptorSetKHR)
               (damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS
                , image_pipeline.pipeline_layout
                , 0, 1, &descriptorWrites);

             vkCmdDraw(damaged_command_buffer, 6, 1, 0, 0);

             vkCmdEndRenderPass(damaged_command_buffer);
           }

           r = from_result (vkEndCommandBuffer(damaged_command_buffer));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code(r));

           ++i;
         }
         
         VkSubmitInfo submitInfo = {};
         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                             
         std::vector<VkCommandBuffer> buffers = damaged_command_buffers;
         for (auto&& cache : buffer_cache)
         {
           buffers.push_back (cache.command_buffer[imageIndex]);
         }

         VkSemaphore waitSemaphores[] = {imageAvailable};
         VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
         submitInfo.waitSemaphoreCount = 1;
         submitInfo.pWaitSemaphores = waitSemaphores;
         submitInfo.pWaitDstStageMask = waitStages;
         submitInfo.commandBufferCount = buffers.size();
         submitInfo.pCommandBuffers = &buffers[0];

         VkSemaphore signalSemaphores[] = {renderFinished};
         submitInfo.signalSemaphoreCount = 1;
         submitInfo.pSignalSemaphores = signalSemaphores;
         using fastdraw::output::vulkan::from_result;
         using fastdraw::output::vulkan::vulkan_error_code;

         {
           ftk::ui::backend::vulkan_queues::lock_graphic_queue lock_queue(toplevel->window.queues);
         
           std::cout << "submit graphics " << buffers.size() << std::endl;
           auto r = from_result(vkQueueSubmit(lock_queue.get_queue().vkqueue, 1, &submitInfo, VK_NULL_HANDLE /*executionFinished[imageIndex]*/));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code (r));
         }

         std::cout << "graphic" << std::endl;

         // std::cout << "waiting on fence" << std::endl;
         // if (vkWaitForFences (toplevel->window.voutput.device, 1, &executionFinished[imageIndex], VK_FALSE, -1) == VK_TIMEOUT)
         // {
         //   std::cout << "Timeout waiting for fence" << std::endl;
         //   throw -1;
         // }
         // vkResetFences (toplevel->window.voutput.device, 1, &executionFinished[imageIndex]);

         // vkFreeCommandBuffers (toplevel->window.voutput.device, commandPool, damaged_command_buffers.size()
         //                       , &damaged_command_buffers[0]);
       
         std::cout << "waited for fence" << std::endl;
         //if (!buffer_cache.empty())
         {
           static unsigned int submissions = 0;
           std::cout << "submit presentation" << ++submissions <<  std::endl;
           //CHRONO_COMPARE()

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

           {
             ftk::ui::backend::vulkan_queues::lock_presentation_queue lock_queue(toplevel->window.queues);
         
             using fastdraw::output::vulkan::from_result;
             using fastdraw::output::vulkan::vulkan_error_code;

             auto r = from_result (vkQueuePresentKHR(lock_queue.get_queue().vkqueue, &presentInfo));
             if (r != vulkan_error_code::success)
               throw std::system_error (make_error_code (r));
           }
         }
       //CHRONO_COMPARE()
         
         //std::cout << "recording buffer" << std::endl;

       //   std::unique_ptr<VkDescriptorSet[]> descriptorSet {new VkDescriptorSet[images.size()]};
       //   {
       //     VkDescriptorPool descriptorPool;
       //     /*std::array<*/VkDescriptorPoolSize/*, 2>*/ poolSizes = {};
       //     // poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
       //     // poolSizes[0].descriptorCount = /*static_cast<uint32_t>(swapChainImages.size())*/1;
       //     poolSizes/*[1]*/.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
       //     poolSizes/*[1]*/.descriptorCount = images.size();

       //     VkDescriptorPoolCreateInfo poolInfo = {};
       //     poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
       //     poolInfo.poolSizeCount = 1/*static_cast<uint32_t>(poolSizes.size())*/;
       //     poolInfo.pPoolSizes = &poolSizes/*.data()*/;
       //     poolInfo.maxSets = images.size();

       //     //CHRONO_COMPARE()
       //     if (vkCreateDescriptorPool(toplevel->window.voutput.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
       //       throw std::runtime_error("failed to create descriptor pool!");
       //     }

       //     std::unique_ptr<VkDescriptorSetLayout[]> descriptor_set_layouts {new VkDescriptorSetLayout[images.size()]};
       //     for (VkDescriptorSetLayout* first = &descriptor_set_layouts[0]
       //            , *last = first + images.size(); first != last; ++ first)
       //       *first = image_pipeline.descriptorSetLayout;
           
       //     VkDescriptorSetAllocateInfo allocInfo = {};
       //     allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
       //     allocInfo.descriptorPool = descriptorPool;
       //     allocInfo.descriptorSetCount = images.size();
       //     allocInfo.pSetLayouts = &descriptor_set_layouts[0];

       //     //CHRONO_COMPARE()
       //     if (vkAllocateDescriptorSets(toplevel->window.voutput.device, &allocInfo, &descriptorSet[0]) != VK_SUCCESS) {
       //       throw std::runtime_error("failed to allocate descriptor sets!");
       //     }
       //     //CHRONO_COMPARE()
       //   }
         
       //   std::size_t descriptor_index = 0;
       //   for (auto&& image : images)
       //   {
       //     //CHRONO_START()
       //     using fastdraw::output::vulkan::from_result;
       //     using fastdraw::output::vulkan::vulkan_error_code;
       //     VkDescriptorImageInfo imageInfo = {};
       //     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
       //     imageInfo.imageView = image.image_view;
       //     imageInfo.sampler = sampler;

       //     /*std::array<*/VkWriteDescriptorSet/*, 2>*/ descriptorWrites = {};
                             
       //     descriptorWrites/*[1]*/.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
       //     descriptorWrites/*[1]*/.dstSet = descriptorSet[descriptor_index]/*s[i]*/;
       //     descriptorWrites/*[1]*/.dstBinding = 1;
       //     descriptorWrites/*[1]*/.dstArrayElement = 0;
       //     descriptorWrites/*[1]*/.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
       //     descriptorWrites/*[1]*/.descriptorCount = 1;
       //     descriptorWrites/*[1]*/.pImageInfo = &imageInfo;

       //     //CHRONO_COMPARE()
       //     vkUpdateDescriptorSets(toplevel->window.voutput.device, 1/*static_cast<uint32_t>(descriptorWrites.size())*/
       //                            , &descriptorWrites/*.data()*/, 0, nullptr);
       //     //CHRONO_COMPARE()
       //     ++descriptor_index;
       //   }

       //   descriptor_index = 0;
       //   for (auto&& image : images)
       //   {
       //     using fastdraw::output::vulkan::from_result;
       //     using fastdraw::output::vulkan::vulkan_error_code;

       //     VkCommandBufferBeginInfo beginInfo = {};
       //     beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
       //     if (vkBeginCommandBuffer(commandBuffer[descriptor_index], &beginInfo) != VK_SUCCESS) {
       //       throw std::runtime_error("failed to begin recording command buffer!");
       //     }

       //     VkRenderPassBeginInfo renderPassInfo = {};
       //     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
       //     renderPassInfo.framebuffer = toplevel->window.swapChainFramebuffers[imageIndex];
       //     renderPassInfo.renderArea.offset = {image.x, image.y};
       //     auto width = image.x + image.width <= toplevel->window.voutput.swapChainExtent.width ? image.width : toplevel->window.voutput.swapChainExtent.width - image.x;
       //     auto height = image.y + image.height <= toplevel->window.voutput.swapChainExtent.height ? image.height : toplevel->window.voutput.swapChainExtent.height - image.y;
       //     renderPassInfo.renderArea.extent = {width/* + image.x*/, height/* + image.y*/};
       //     VkClearValue clear_value;
       //     // clearing for testing
       //     if (!descriptor_index)
       //     {
       //       renderPassInfo.renderPass = toplevel->window.voutput.renderpass;
       //       renderPassInfo.clearValueCount = 1;
       //       clear_value.color.uint32[0] = 0;
       //       clear_value.color.uint32[1] = 0;
       //       clear_value.color.uint32[2] = 0;
       //       clear_value.color.uint32[3] = 0;
       //       renderPassInfo.pClearValues = &clear_value;
       //     }
       //     else
       //       renderPassInfo.renderPass = renderPass;
         
       //     vkCmdBeginRenderPass(commandBuffer[descriptor_index], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

       //     vkCmdBindPipeline(commandBuffer[descriptor_index], VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline);

       //     std::vector<VkDeviceSize> offsets;
       //     std::vector<VkBuffer> buffers;

       //     auto static const vertex_data_size = (12 + 12 + 16)*sizeof(float);

       //     if (toplevel->vbuffer.size())
       //     {
       //       auto vbuffer = toplevel->vbuffer.get_buffer();
           
       //       offsets.push_back(vertex_data_size * descriptor_index);
       //       buffers.push_back(vbuffer);
       //       offsets.push_back(vertex_data_size * descriptor_index);
       //       buffers.push_back(vbuffer);

       //       vkCmdBindVertexBuffers(commandBuffer[descriptor_index], 0, 2, &buffers[0], &offsets[0]);
       //     }
           
       //     vkCmdBindDescriptorSets(commandBuffer[descriptor_index], VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline.pipeline_layout
       //                             , 0, 1, &descriptorSet[descriptor_index] , 0, nullptr);
           
       //     vkCmdDraw(commandBuffer[descriptor_index], 6, 1, 0, 0);

       //     vkCmdEndRenderPass(commandBuffer[descriptor_index]);

       //     if (vkEndCommandBuffer(commandBuffer[descriptor_index]) != VK_SUCCESS) {
       //       throw std::runtime_error("failed to record command buffer!");
       //     }
       //     ++descriptor_index;
       // } // for ?

       //   //vkDestroyRenderPass(toplevel->window.voutput.device, renderPass, nullptr);

       // //CHRONO_COMPARE()
       // {
       //   VkSubmitInfo submitInfo = {};
       //   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                             
       //   std::cout << "submit graphics" << std::endl;

       //   //std::array<VkCommandBuffer, 2> first_and_last{commandBuffer[0], commandBuffer[images.size()-1]};
         
       //   VkSemaphore waitSemaphores[] = {imageAvailable};
       //   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
       //   submitInfo.waitSemaphoreCount = 1;
       //   submitInfo.pWaitSemaphores = waitSemaphores;
       //   submitInfo.pWaitDstStageMask = waitStages;
       //   submitInfo.commandBufferCount = images.size();
       //   submitInfo.pCommandBuffers = &commandBuffer[0];

       //   VkSemaphore signalSemaphores[] = {renderFinished};
       //   submitInfo.signalSemaphoreCount = 1;
       //   submitInfo.pSignalSemaphores = signalSemaphores;
       //   using fastdraw::output::vulkan::from_result;
       //   using fastdraw::output::vulkan::vulkan_error_code;

       //   ftk::ui::backend::vulkan_queues::lock_graphic_queue lock_queue(toplevel->window.queues);
         
       //   auto r = from_result(vkQueueSubmit(lock_queue.get_queue().vkqueue, 1, &submitInfo, toplevel->window.executionFinished));
       //   if (r != vulkan_error_code::success)
       //     throw std::system_error(make_error_code (r));
       // }
       //CHRONO_COMPARE_FORCE()

       // std::cout << "graphic" << std::endl;
       // if (vkWaitForFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished, VK_FALSE, -1) == VK_TIMEOUT)
       // {
       //   std::cout << "Timeout waiting for fence" << std::endl;
       //   throw -1;
       // }
       // vkResetFences (toplevel->window.voutput.device, 1, &toplevel->window.executionFinished);
                           
       // std::cout << "submit presentation" << std::endl;
       // //CHRONO_COMPARE()
       // {
       //   VkPresentInfoKHR presentInfo = {};
       //   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

       //   VkSemaphore waitSemaphores[] = {renderFinished};
         
       //   presentInfo.waitSemaphoreCount = 1;
       //   presentInfo.pWaitSemaphores = waitSemaphores;

       //   VkSwapchainKHR swapChains[] = {toplevel->window.swapChain};
       //   presentInfo.swapchainCount = 1;
       //   presentInfo.pSwapchains = swapChains;
       //   presentInfo.pImageIndices = &imageIndex;
       //   presentInfo.pResults = nullptr; // Optional

       //   ftk::ui::backend::vulkan_queues::lock_presentation_queue lock_queue(toplevel->window.queues);
         
       //   using fastdraw::output::vulkan::from_result;
       //   using fastdraw::output::vulkan::vulkan_error_code;

       //   auto r = from_result (vkQueuePresentKHR(lock_queue.get_queue().vkqueue, &presentInfo));
       //   if (r != vulkan_error_code::success)
       //     throw std::system_error (make_error_code (r));
       // }
       // //CHRONO_COMPARE()
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

inline std::function<void()> render_dirty (bool& dirty, std::unique_lock<std::mutex>& lock, std::condition_variable& cond)
{
  return [dirty = &dirty, &lock, cond = &cond]
         {
           *dirty = true;
           cond->notify_one();
           lock.unlock();
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

