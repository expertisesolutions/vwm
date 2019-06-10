#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libkms.h>

#include <cassert>
#include <iostream>
#include <cstring>

int drm_init()
{
    //int fd = drmOpen("rcar-du", nullptr);
    // int fd = open("/dev/dri/card0", O_RDWR);
    // assert (!!fd);

    // drmModeResPtr resources;

    // resources = drmModeGetResources (fd);
    // assert (!!resources);

    // drmModeConnectorPtr connector;
    // for(int i=0; i < resources->count_connectors; ++i)
    // { 
    //   connector = drmModeGetConnector(fd, resources->connectors[i]);
    //   if(connector != NULL)
    //   {
    //     fprintf(stderr, "connector %d found\n", connector->connector_id);
    //     if(connector->connection == DRM_MODE_CONNECTED
    //        && connector->count_modes > 0)
    //     {
    //       auto connector_id = connector->connector_id;
    
    //       std::cout << "resources crtc_id " << resources->crtcs[0] << " connector " << connector_id
    //                 << " connectors " << resources->count_connectors
    //                 << std::endl;
          
    //       std::cout << "Found connector connected" << std::endl;
    //       std::cout << "physical size of connector WxH: " << connector->mmWidth << "x" << connector->mmHeight << std::endl;
    //       break;
    //     }
    //     drmModeFreeConnector(connector);
    //   }
    // }

    // std::cout << "found proper connector" << std::endl;

    // drmModeModeInfo mode;
    // uint32_t width;
    // uint32_t height;

    // assert (!!connector);
    // mode = connector->modes[0];
    // width = mode.hdisplay;
    // height = mode.vdisplay;

    // std::cout << "w " << width << " height " << height << std::endl;

    // drmModeEncoderPtr encoder = nullptr;
    // encoder = drmModeGetEncoder(fd, connector->encoder_id);
    //   // for(i=0; i < resources->count_encoders; ++i)
    //   // { 
    //   //   if(encoder != NULL)
    //   //   {
    //   //     fprintf(stderr, "encoder %d found\n", encoder->encoder_id);
    //   //     if(encoder->encoder_id == connector->encoder_id)
    //   //       break;
    //   //     drmModeFreeEncoder(encoder);
    //   //   }
    //   //   else
    //   //     fprintf(stderr, "get a null encoder pointer\n");
    //   // }
    // assert (!!encoder);

    // std::cout << "found encoder" << std::endl;
      
    // struct kms_driver *kms;
    // if (kms_create(fd, &kms) != 0)
    //   throw -1;

    // struct kms_bo* kms_bo;
      
    // unsigned bo_attribs[] = { 
    //                          KMS_WIDTH,   mode.hdisplay,
    //                          KMS_HEIGHT,  mode.vdisplay,
    //                          KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
    //                          KMS_TERMINATE_PROP_LIST
    // };
    // unsigned pitch;
    // kms_bo_create(kms, bo_attribs, &kms_bo);
    // kms_bo_get_prop(kms_bo, KMS_PITCH, &pitch);
    
    // void* data;
    // kms_bo_map (kms_bo, &data);

    // std::memset (data, 0xFF, pitch * height);

    // kms_bo_unmap (kms_bo);

    // //orig_crtc = drmModeGetCrtc(fd, encoder->crtc_id);

    // int bo_handle;
    // kms_bo_get_prop(kms_bo, KMS_HANDLE, static_cast<unsigned*>(static_cast<void*>(&bo_handle)));

    // uint32_t fb_id;
      
    // drmModeAddFB(fd, width, height, 24, 32, pitch, bo_handle, &fb_id);
    // drmModeSetCrtc(  
    //                fd, encoder->crtc_id, fb_id, 
    //                0, 0,     /* x, y */ 
    //                &connector->connector_id, 
    //                1,         /* element count of the connectors array above*/
    //                &mode);

}
