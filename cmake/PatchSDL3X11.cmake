# 固定版本 SDL3 的 X11 共享图像长度在窄整数中相乘；保留原有分配失败回退。
if(NOT DEFINED SDL3_SOURCE_DIR)
  message(FATAL_ERROR "SDL3_SOURCE_DIR is required")
endif()

set(QM_SDL_X11_FILE "${SDL3_SOURCE_DIR}/src/video/x11/SDL_x11toolkit.c")
file(READ "${QM_SDL_X11_FILE}" QM_SDL_X11_SOURCE)
set(QM_SDL_X11_ORIGINAL "data->shm_info.shmid = shmget(IPC_PRIVATE, data->image->bytes_per_line * data->image->height, IPC_CREAT | 0777);")
set(QM_SDL_X11_PATCHED [=[size_t image_size;
                /* QmClient: 校验共享图像长度，避免窄整数乘法溢出。 */
                if (data->image->bytes_per_line <= 0 || data->image->height <= 0 ||
                    !SDL_size_mul_check_overflow((size_t)data->image->bytes_per_line, (size_t)data->image->height, &image_size)) {
                    XDestroyImage(data->image);
                    data->image = NULL;
                    data->shm = false;
                    return;
                }
                data->shm_info.shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0777);]=])

string(FIND "${QM_SDL_X11_SOURCE}" "${QM_SDL_X11_PATCHED}" QM_SDL_X11_PATCH_POSITION)
if(NOT QM_SDL_X11_PATCH_POSITION EQUAL -1)
  return()
endif()
string(FIND "${QM_SDL_X11_SOURCE}" "${QM_SDL_X11_ORIGINAL}" QM_SDL_X11_ORIGINAL_POSITION)
if(QM_SDL_X11_ORIGINAL_POSITION EQUAL -1)
  message(FATAL_ERROR "unexpected SDL3 X11 source; review the pinned dependency patch")
endif()

string(REPLACE "${QM_SDL_X11_ORIGINAL}" "${QM_SDL_X11_PATCHED}" QM_SDL_X11_SOURCE "${QM_SDL_X11_SOURCE}")
file(WRITE "${QM_SDL_X11_FILE}" "${QM_SDL_X11_SOURCE}")
