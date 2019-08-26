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
#include <ftk/ui/backend/vulkan_indirect_draw.hpp>

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
       auto last_time = std::chrono::high_resolution_clock::now();
       using fastdraw::output::vulkan::from_result;
       using fastdraw::output::vulkan::vulkan_error_code;
       VkSampler sampler = detail::render_thread_create_sampler (toplevel->window.voutput.device);
       auto const image_pipeline0 = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput
                                                                                    , 0);
       auto static const vkCmdPushDescriptorSetKHR_ptr
         = vkGetDeviceProcAddr (toplevel->window.voutput.device, "vkCmdPushDescriptorSetKHR");
       auto static const vkCmdPushDescriptorSetKHR
         = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkCmdPushDescriptorSetKHR_ptr);
       assert (vkCmdPushDescriptorSetKHR != nullptr);
       // auto const image_pipeline1 = fastdraw::output::vulkan::create_image_pipeline (toplevel->window.voutput
       //                                                                              , 1);
       auto const indirect_pipeline = ftk::ui::vulkan
         ::create_indirect_draw_buffer_filler_pipeline (toplevel->window.voutput);

       // auto static const compute_pipeline = ftk::ui::vulkan
       //   ::create_initialize_draw_buffer_pipeline (toplevel->window.voutput);
       // initialize indirect buffer
       {
         VkFence initialization_fence;
         VkFenceCreateInfo fenceInfo = {};
         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         auto r = from_result (vkCreateFence (toplevel->window.voutput.device, &fenceInfo, nullptr, &initialization_fence));
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code(r));

         VkCommandPool command_pool = toplevel->window.voutput.command_pool;
         VkCommandBuffer command_buffer;

         VkCommandBufferAllocateInfo allocInfo = {};
         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         allocInfo.commandPool = command_pool;
         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         allocInfo.commandBufferCount = 1;

         r = from_result (vkAllocateCommandBuffers(toplevel->window.voutput.device, &allocInfo
                                                          , &command_buffer));
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code (r));

         VkCommandBufferBeginInfo beginInfo = {};
         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
         beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
         r = from_result (vkBeginCommandBuffer(command_buffer, &beginInfo));
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code(r));

         // fill everything with 0's first
         vkCmdFillBuffer (command_buffer, toplevel->indirect_draw_buffer
                          , 0 /* offset */, (6 + 4096 + 4096) * sizeof(uint32_t)
                          * toplevel->indirect_draw_info_array_size
                          , 0);
         for (std::size_t i = 0; i != toplevel->indirect_draw_info_array_size; ++i)
         {
           // vertex count filling
           vkCmdFillBuffer (command_buffer, toplevel->indirect_draw_buffer
                            , (6 + 4096 + 4096) * sizeof(uint32_t) * i, sizeof (uint32_t), 6);
           // fill fg_zindex array
           vkCmdFillBuffer (command_buffer, toplevel->indirect_draw_buffer
                            , (6 + 4096 + 4096) * sizeof(uint32_t) * i
                            + (6 + 4096) * sizeof(uint32_t)
                            , sizeof (uint32_t) * 4096, 0xFFFFFFFF);
         }

         r = from_result (vkEndCommandBuffer(command_buffer));
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code(r));

         VkSubmitInfo submitInfo = {};
         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
         submitInfo.commandBufferCount = 1;
         submitInfo.pCommandBuffers = &command_buffer;

         {
           ftk::ui::backend::vulkan_queues::lock_graphic_queue lock_queue(toplevel->window.queues);
           r = from_result(vkQueueSubmit(lock_queue.get_queue().vkqueue, 1, &submitInfo, initialization_fence));
         }
         if (r != vulkan_error_code::success)
           throw std::system_error(make_error_code (r));

         if (vkWaitForFences (toplevel->window.voutput.device, 1, &initialization_fence, VK_FALSE, -1) == VK_TIMEOUT)
         {
           //std::cout << "Timeout waiting for fence" << std::endl;
           throw -1;
         }
         vkFreeCommandBuffers (toplevel->window.voutput.device, command_pool, 1, &command_buffer);
         vkDestroyFence (toplevel->window.voutput.device, initialization_fence, nullptr);
       }
       
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
         uint32_t imageIndex;
         VkSemaphore imageAvailable, renderFinished;
         imageAvailable = detail::render_thread_create_semaphore (toplevel->window.voutput.device);
         renderFinished = detail::render_thread_create_semaphore (toplevel->window.voutput.device);
         // std::cout << "render thread waiting to render (before lock)" << std::endl;
         std::unique_lock<std::mutex> l(mutex);
         // std::cout << "render thread waiting to render" << std::endl;
         imageIndex = -1;
         while ((!dirty || imageIndex == std::numeric_limits<uint32_t>::max()) && !exit)
         {
           // std::cout << "not dirty and not exit" << std::endl;
           if (imageIndex == std::numeric_limits<uint32_t>::max())
           {
             l.unlock();
             vkAcquireNextImageKHR(toplevel->window.voutput.device, toplevel->window.swapChain
                                   , std::numeric_limits<uint64_t>::max(), imageAvailable
                                   , /*toplevel->window.executionFinished*/nullptr, &imageIndex);
             std::cout << "Acquired image index " << imageIndex << std::endl;
             {
               auto now = std::chrono::high_resolution_clock::now();
               auto diff = now - last_time;
               last_time = now;
               std::cout << "Time between frames "
                         << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
                         << "ms" << std::endl;
             }
             l.lock();
           }
           if (!dirty && !exit)
             cond.wait(l);
           // std::cout << "condvar wake up" << std::endl;
         }
         if (exit) break;
         if (dirty == true)
           dirty = false;

         /**** acquire data ****/
         // std::cout << "drawing" << std::endl;

         // decltype (toplevel->images) images;

         auto framebuffer_damaged_regions = std::move (toplevel->framebuffers_damaged_regions[imageIndex]);
         assert (toplevel->framebuffers_damaged_regions[imageIndex].empty());

         if (framebuffer_damaged_regions.size() > 32)
         {
           auto first = std::next (framebuffer_damaged_regions.begin(), 32)
             , last = framebuffer_damaged_regions.end();
           for (;first != last; ++first)
           {
             toplevel->framebuffers_damaged_regions[imageIndex].push_back (std::move(*first));
           }
           framebuffer_damaged_regions.erase (first, last);
         }

         unsigned int i = 0;
         for (auto&& image : toplevel->images)
         {
           if (image.must_draw[imageIndex])
           {
             std::cout << "found drawable image " << &image << " image.must_draw[0] "
                       << image.must_draw[0] << " image.must_draw[1] " << image.must_draw[1] << std::endl;
             // images.push_back(image);
             image.must_draw[imageIndex] = false;
             image.framebuffers_regions[imageIndex] = {image.x, image.y, image.width, image.height};
             if (framebuffer_damaged_regions.size() < 32)
               framebuffer_damaged_regions.push_back
                 ({image.x, image.y, image.width, image.height});
             else
               toplevel->framebuffers_damaged_regions[imageIndex].push_back
                 ({image.x, image.y, image.width, image.height});
           }
           i++;
         }
         // std::cout << "number of images to draw " << images.size() << std::endl;

         //l.unlock();
                         
         // first should create vertexbuffer for all, then record command buffer and then submitting
         VkCommandPool commandPool = toplevel->window.voutput.command_pool;

         std::vector<VkCommandBuffer> damaged_command_buffers;

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


         std::cout << "recording " << framebuffer_damaged_regions.size() << " regions" << std::endl;
         // draw damage areas
         i = 0;
         for (auto&& region : framebuffer_damaged_regions)
         {
           auto damaged_command_buffer = damaged_command_buffers[i];

           auto x = region.x;
           auto y = region.y;
           auto width = static_cast<uint32_t>(region.width);
           auto height = static_cast<uint32_t>(region.height);

           std::cout << "damaged rect " << x << "x" << y << "-" << width << "x" << height << std::endl;
           
           VkRenderPassBeginInfo renderPassInfo = {};
           renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
           renderPassInfo.framebuffer = toplevel->window.swapChainFramebuffers[imageIndex];
           renderPassInfo.renderArea.offset = {x, y};
           {
             auto w = static_cast<uint32_t>(x) + width <= toplevel->window.voutput.swapChainExtent.width
               ? width : toplevel->window.voutput.swapChainExtent.width - static_cast<uint32_t>(x);
             auto h = static_cast<uint32_t>(y) + height <= toplevel->window.voutput.swapChainExtent.height
               ? height : toplevel->window.voutput.swapChainExtent.height - static_cast<uint32_t>(y);
             renderPassInfo.renderArea.extent = {w/* + image.x*/, h/* + image.y*/};

             std::cout << "rendering to " << renderPassInfo.renderArea.offset.x
                       << "x" << renderPassInfo.renderArea.offset.y
                       << " size " << renderPassInfo.renderArea.extent.width
                       << "x" << renderPassInfo.renderArea.extent.height << std::endl;
           }
           renderPassInfo.renderPass = toplevel->window.voutput.renderpass;

           VkCommandBufferBeginInfo beginInfo = {};
           beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
           beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
           auto r = from_result (vkBeginCommandBuffer(damaged_command_buffer, &beginInfo));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code(r));

           {
             VkDescriptorBufferInfo ssboInfo = {};
             ssboInfo.buffer = toplevel->image_ssbo_buffer;
             ssboInfo.range = VK_WHOLE_SIZE;

             // VkDescriptorBufferInfo ssboZIndexInfo = {};
             // ssboZIndexInfo.buffer = toplevel->image_zindex_ssbo_buffer;
             // ssboZIndexInfo.range = VK_WHOLE_SIZE;

             VkDescriptorBufferInfo indirect_draw_info = {};
             indirect_draw_info.buffer = toplevel->indirect_draw_buffer;
             indirect_draw_info.range = VK_WHOLE_SIZE;
             indirect_draw_info.offset = i*sizeof(typename ftk::ui::toplevel_window<Backend>::indirect_draw_info);

             std::cout << "offset of indirect draw info " << indirect_draw_info.offset << std::endl;
             
             VkWriteDescriptorSet descriptorWrites[2] = {};
                             
             descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
             descriptorWrites[0].dstSet = 0;
             descriptorWrites[0].dstBinding = 0;
             descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
             descriptorWrites[0].descriptorCount = 1;
             descriptorWrites[0].pBufferInfo = &ssboInfo;

             // descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
             // descriptorWrites[1].dstSet = 0;
             // descriptorWrites[1].dstBinding = 1;
             // descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
             // descriptorWrites[1].descriptorCount = 1;
             // descriptorWrites[1].pBufferInfo = nullptr;//&ssboZIndexInfo;

             descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
             descriptorWrites[1].dstSet = 0;
             descriptorWrites[1].dstBinding = 2;
             descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
             descriptorWrites[1].descriptorCount = 1;
             descriptorWrites[1].pBufferInfo = &indirect_draw_info;
             
             vkCmdBindDescriptorSets (damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS
                                      , indirect_pipeline.pipeline_layout
                                      , 0, 1, &toplevel->texture_descriptors.set
                                      , 0, 0);

             vkCmdBindDescriptorSets (damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS
                                      , indirect_pipeline.pipeline_layout
                                      , 1, 1, &toplevel->sampler_descriptors.set
                                      , 0, 0);

             vkCmdPushDescriptorSetKHR
               (damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS
                , indirect_pipeline.pipeline_layout
                , 2 /* from 1 */, sizeof(descriptorWrites)/sizeof(descriptorWrites[0]), &descriptorWrites[0]);

             vkCmdPushDescriptorSetKHR
               (damaged_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE
                , indirect_pipeline.pipeline_layout
                , 2 /* from 1 */, sizeof(descriptorWrites)/sizeof(descriptorWrites[0]), &descriptorWrites[0]);

             uint32_t image_size = toplevel->images.size();
             vkCmdPushConstants(damaged_command_buffer
                                , indirect_pipeline.pipeline_layout
                                , VK_SHADER_STAGE_VERTEX_BIT
                                | VK_SHADER_STAGE_COMPUTE_BIT
                                | VK_SHADER_STAGE_FRAGMENT_BIT
                                , 0, sizeof(uint32_t), &image_size);
           }

           vkCmdBeginRenderPass(damaged_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
           vkCmdBindPipeline(damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, indirect_pipeline.pipeline);
           VkRect2D scissor = renderPassInfo.renderArea;
           vkCmdSetScissor (damaged_command_buffer, 0, 1, &scissor);

             vkCmdDraw(damaged_command_buffer, 6, 1, 0, 0);

             VkMemoryBarrier memory_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
             memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
             memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

             vkCmdPipelineBarrier (damaged_command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                   , VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                                   | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                                   | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                                   | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT
                                   , 1, &memory_barrier, 0, /*&buffer_barrier*/nullptr, 0, nullptr);

             vkCmdEndRenderPass(damaged_command_buffer);
             vkCmdBeginRenderPass(damaged_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

             vkCmdBindPipeline(damaged_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline0.pipeline);
             vkCmdDrawIndirect (damaged_command_buffer, toplevel->indirect_draw_buffer
                                , sizeof(typename ftk::ui::toplevel_window<Backend>::indirect_draw_info)*i, 1, 0);

           vkCmdEndRenderPass(damaged_command_buffer);

           vkCmdFillBuffer (damaged_command_buffer, toplevel->indirect_draw_buffer
                            , sizeof(uint32_t) /* offset */, (5 + 4096) * sizeof(uint32_t), 0);

           vkCmdFillBuffer (damaged_command_buffer, toplevel->indirect_draw_buffer
                            , sizeof(uint32_t) * (6 + 4096) /* offset */, 4096 * sizeof(uint32_t), 0xFFFFFFFF);

           r = from_result (vkEndCommandBuffer(damaged_command_buffer));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code(r));

           ++i;
         }
         
         VkSubmitInfo submitInfo = {};
         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                             
         std::vector<VkCommandBuffer> buffers = damaged_command_buffers;

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

         auto queue_begin = std::chrono::high_resolution_clock::now();
         {
           ftk::ui::backend::vulkan_queues::lock_graphic_queue lock_queue(toplevel->window.queues);

           auto now = std::chrono::high_resolution_clock::now();
           auto diff = now - queue_begin;
           std::cout << "Time locking queue "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
                     << "ms" << std::endl;
         
           //std::cout << "submit graphics " << buffers.size() << std::endl;
           auto r = from_result(vkQueueSubmit(lock_queue.get_queue().vkqueue, 1, &submitInfo, executionFinished[imageIndex]));
           if (r != vulkan_error_code::success)
             throw std::system_error(make_error_code (r));

           auto now2 = std::chrono::high_resolution_clock::now();
           auto diff2 = now2 - now;
           std::cout << "Time submitting to queue "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(diff2).count()
                     << "ms" << std::endl;
         }

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

           {
             ftk::ui::backend::vulkan_queues::lock_presentation_queue lock_queue(toplevel->window.queues);
         
             using fastdraw::output::vulkan::from_result;
             using fastdraw::output::vulkan::vulkan_error_code;

             auto now = std::chrono::high_resolution_clock::now();
             
             auto r = from_result (vkQueuePresentKHR(lock_queue.get_queue().vkqueue, &presentInfo));
             if (r != vulkan_error_code::success)
               throw std::system_error (make_error_code (r));

             auto now2 = std::chrono::high_resolution_clock::now();
             auto diff = now2 - now;
             std::cout << "Time submitting presentation queue "
                       << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
                       << "ms" << std::endl;
             // r = from_result(vkQueueWaitIdle (lock_queue.get_queue().vkqueue));
             // if (r != vulkan_error_code::success)
             //   throw std::system_error (make_error_code (r));
           }
           
           {
             auto now = std::chrono::high_resolution_clock::now();
             auto diff = now - queue_begin;
             std::cout << "Time running drawing command "
                       << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
                       << "ms" << std::endl;
           }

           if (vkWaitForFences (toplevel->window.voutput.device, 1, &executionFinished[imageIndex], VK_FALSE, -1) == VK_TIMEOUT)
           {
             //std::cout << "Timeout waiting for fence" << std::endl;
             throw -1;
           }
           vkResetFences (toplevel->window.voutput.device, 1, &executionFinished[imageIndex]);
           vkFreeCommandBuffers (toplevel->window.voutput.device, commandPool, damaged_command_buffers.size()
                                 , &damaged_command_buffers[0]);

           /*
       {
           // indirect buffer
           auto data = toplevel->buffer_allocator.map (toplevel->indirect_draw_buffer);
           std::cout << "images " << toplevel->images.size() << std::endl;
           for (std::size_t ind = 0; ind != toplevel->indirect_draw_info_array_size; ++ind)
           {
             auto indirect_draw_info = static_cast<ftk::ui::toplevel_window<Backend>::indirect_draw_info*>(data)
               + ind;
             std::size_t fragment_length;
             std::cout << "== (" << ind << ") vertex count " << indirect_draw_info->indirect.vertexCount
                       << " instance count " << indirect_draw_info->indirect.instanceCount
                       << " first vertex " << indirect_draw_info->indirect.firstVertex
                       << " first_instance " << indirect_draw_info->indirect.firstInstance
                       << " image length " << indirect_draw_info->image_length
                       << " fragment data length " << (fragment_length = indirect_draw_info->fragment_data_length)
                       << std::endl;
             {
               std::size_t i = 0;
             for (; i != fragment_length; ++i)
             {
               std::cout << "(" << ind << ") draw zindex " << indirect_draw_info->fg_zindex[i] << std::endl;
             }
             std::cout << "(" << ind << ") NOT draw zindex " << indirect_draw_info->fg_zindex[i] << std::endl;
             }

             // indirect_draw_info->indirect.instanceCount = 0;
             // indirect_draw_info->fragment_data_length = 0;
             // std::memset (&indirect_draw_info->buffers_to_draw[0], 0
             //              , sizeof (indirect_draw_info->buffers_to_draw));
             // std::memset (&indirect_draw_info->fg_zindex[0], 255
             //              , sizeof (indirect_draw_info->fg_zindex));
             
           }
           toplevel->buffer_allocator.unmap (toplevel->image_ssbo_buffer);
           }*/
           
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

