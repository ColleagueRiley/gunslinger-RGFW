
/*================================================================
    * Copyright: 2020 John Jackson 
    * File: gs_platform_impl.h
    All Rights Reserved
=================================================================*/

#ifndef GS_PLATFORM_IMPL_H
#define GS_PLATFORM_IMPL_H

/*=================================
// Default Platform Implemenattion
=================================*/

// Define default platform implementation if certain platforms are enabled
#if (!defined GS_PLATFORM_IMPL_NO_DEFAULT)
    #define GS_PLATFORM_IMPL_DEFAULT
#endif

/*=============================
// Default Impl
=============================*/

#ifdef GS_PLATFORM_IMPL_DEFAULT

#if !(defined GS_PLATFORM_WIN)
    #include <sys/stat.h>
    #include <dirent.h>
    #include <dlfcn.h>  // dlopen, RTLD_LAZY, dlsym
#else
	#include "../external/dirent/dirent.h"
    #include <direct.h>
#endif

/*== Platform Window ==*/

GS_API_DECL gs_platform_t* 
gs_platform_create()
{
    // Construct new platform interface
    gs_platform_t* platform = gs_malloc_init(gs_platform_t);

    // Initialize windows
    platform->windows = gs_slot_array_new(gs_platform_window_t);

    // Set up video mode (for now, just do opengl)
    platform->settings.video.driver = GS_PLATFORM_VIDEO_DRIVER_TYPE_OPENGL;

    return platform;
}

GS_API_DECL void 
gs_platform_destroy(gs_platform_t* platform)
{
    if (platform == NULL) return;

    // Free all resources
    gs_slot_array_free(platform->windows);

    // Free platform
    gs_free(platform);
    platform = NULL;
}

GS_API_DECL uint32_t 
gs_platform_window_create(const gs_platform_window_desc_t* desc)
{ 
    gs_assert(gs_instance() != NULL);
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t win = gs_platform_window_create_internal(desc); 

    // Insert and return handle
    return (gs_slot_array_insert(platform->windows, win));
} 

GS_API_DECL uint32_t 
gs_platform_main_window()
{
    // Should be the first element of the slot array...Great assumption to make.
    return 0;
}

/*== Platform Time ==*/

GS_API_DECL const 
gs_platform_time_t* gs_platform_time()
{
    return &gs_subsystem(platform)->time;
}

GS_API_DECL float 
gs_platform_delta_time()
{
    return gs_platform_time()->delta;
}

GS_API_DECL float 
gs_platform_frame_time()
{
    return gs_platform_time()->frame;
}

/*== Platform UUID ==*/

GS_API_DECL struct gs_uuid_t 
gs_platform_uuid_generate()
{
    gs_uuid_t uuid;

    srand(clock());
    char guid[40];
    int32_t t = 0;
    const char* sz_temp = "xxxxxxxxxxxx4xxxyxxxxxxxxxxxxxxx";
    const char* sz_hex = "0123456789abcdef-";
    int32_t n_len = (int32_t)strlen(sz_temp);

    for (t=0; t < n_len + 1; t++)
    {
        int32_t r = rand () % 16;
        char c = ' ';   

        switch (sz_temp[t])
        {
            case 'x' : { c = sz_hex [r]; } break;
            case 'y' : { c = sz_hex [(r & 0x03) | 0x08]; } break;
            case '-' : { c = '-'; } break;
            case '4' : { c = '4'; } break;
        }

        guid[t] = (t < n_len) ? c : 0x00;
    }

    // Convert to uuid bytes from string
    const char* hex_string = guid, *pos = hex_string;

     /* WARNING: no sanitization or error-checking whatsoever */
    for (size_t count = 0; count < 16; count++) 
    {
        sscanf(pos, "%2hhx", &uuid.bytes[count]);
        pos += 2;
    }

    return uuid;
}

// Mutable temp buffer 'tmp_buffer'
GS_API_DECL void 
gs_platform_uuid_to_string(char* tmp_buffer, const gs_uuid_t* uuid)
{
    gs_snprintf 
    (
        tmp_buffer, 
        32,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        uuid->bytes[0],
        uuid->bytes[1],
        uuid->bytes[2],
        uuid->bytes[3],
        uuid->bytes[4],
        uuid->bytes[5],
        uuid->bytes[6],
        uuid->bytes[7],
        uuid->bytes[8],
        uuid->bytes[9],
        uuid->bytes[10],
        uuid->bytes[11],
        uuid->bytes[12],
        uuid->bytes[13],
        uuid->bytes[14],
        uuid->bytes[15]
    );
}

uint32_t gs_platform_uuid_hash(const gs_uuid_t* uuid)
{
    char temp_buffer[] = gs_uuid_temp_str_buffer();
    gs_platform_uuid_to_string(temp_buffer, uuid);
    return (gs_hash_str(temp_buffer));
}

#define __gs_input()\
    (&gs_subsystem(platform)->input)

/*=== Platform Input ===*/

GS_API_DECL gs_platform_input_t* gs_platform_input()
{
    return &gs_subsystem(platform)->input;
}

void gs_platform_update_input(gs_platform_input_t* input)
{
    // Update all input and mouse keys from previous frame
    // Previous key presses
    gs_for_range_i(GS_KEYCODE_COUNT) {
        input->prev_key_map[i] = input->key_map[i];
    }

    // Previous mouse button presses
    gs_for_range_i(GS_MOUSE_BUTTON_CODE_COUNT) {
        input->mouse.prev_button_map[i] = input->mouse.button_map[i];
    }

    input->mouse.wheel = gs_v2s(0.0f);
    input->mouse.delta = gs_v2s(0.f);
    input->mouse.moved_this_frame = false;

    // Update all touch deltas
    for (uint32_t i = 0; i < GS_PLATFORM_MAX_TOUCH; ++i) {
        input->touch.points[i].delta = gs_v2s(0.f);
        input->touch.points[i].down = input->touch.points[i].pressed;
    }
}


/*

    RGFW NOTE:

    RGFW should probably be using the normal event loop and be put into here


*/

void gs_platform_poll_all_events()
{
    gs_platform_t* platform = gs_subsystem(platform);
   
   platform->input.mouse.delta.x = 0;
   platform->input.mouse.delta.y = 0;

    // Iterate through events, don't consume
    gs_platform_event_t evt = gs_default_val();
    while (gs_platform_poll_events(&evt, false))
    {
        switch (evt.type)
        {
            case GS_PLATFORM_EVENT_MOUSE:
            {
                switch (evt.mouse.action)
                {
                    case GS_PLATFORM_MOUSE_MOVE:
                    {
                        // If locked, then movement amount will be applied to delta, 
                        // otherwise set position
                        if (gs_platform_mouse_locked()) {
                            platform->input.mouse.delta = gs_vec2_add(platform->input.mouse.delta, evt.mouse.move);
                        } else {
                            platform->input.mouse.delta = gs_vec2_sub(evt.mouse.move, platform->input.mouse.position);
                            platform->input.mouse.position = evt.mouse.move;
                        }
                    } break;

                    case GS_PLATFORM_MOUSE_WHEEL:
                    {
                        platform->input.mouse.wheel = evt.mouse.wheel;
                    } break;

                    case GS_PLATFORM_MOUSE_BUTTON_PRESSED:
                    {
                        gs_platform_press_mouse_button(evt.mouse.button);
                    } break;

                    case GS_PLATFORM_MOUSE_BUTTON_RELEASED:
                    {
                        gs_platform_release_mouse_button(evt.mouse.button);
                    } break;

                    case GS_PLATFORM_MOUSE_BUTTON_DOWN:
                    {
                        gs_platform_press_mouse_button(evt.mouse.button);
                    } break;

                    case GS_PLATFORM_MOUSE_ENTER:
                    {
                        // If there are user callbacks, could trigger them here
                    } break;

                    case GS_PLATFORM_MOUSE_LEAVE:
                    {
                        // If there are user callbacks, could trigger them here
                    } break;
                }

            } break;

            case GS_PLATFORM_EVENT_KEY:
            {
                switch (evt.key.action) 
                {
                    case GS_PLATFORM_KEY_PRESSED:
                    {
                        gs_platform_press_key(evt.key.keycode);
                    } break;

                    case GS_PLATFORM_KEY_DOWN:
                    {
                        gs_platform_press_key(evt.key.keycode);
                    } break;

                    case GS_PLATFORM_KEY_RELEASED:
                    {
                        gs_platform_release_key(evt.key.keycode);
                    } break;
                }

            } break;

            case GS_PLATFORM_EVENT_WINDOW:
            {
                switch (evt.window.action)
                {
                    default: break;
                }

            } break;

            case GS_PLATFORM_EVENT_TOUCH:
            {
                gs_platform_point_event_data_t* point = &evt.touch.point;

                switch (evt.touch.action)
                {
                    case GS_PLATFORM_TOUCH_DOWN:
                    {
                        uintptr_t id = point->id;
                        gs_vec2 *pos = &point->position;
                        gs_vec2 *p = &platform->input.touch.points[id].position;
                        gs_vec2 *d = &platform->input.touch.points[id].delta;
                        gs_platform_press_touch(id);
                        *p = *pos;
                        gs_subsystem(platform)->input.touch.size++;
                    } break;

                    case GS_PLATFORM_TOUCH_UP:
                    {
                        uintptr_t id = point->id;
                        gs_println("Releasing ID: %zu", id);
                        gs_platform_release_touch(id);
                        gs_subsystem(platform)->input.touch.size--;
                    } break;

                    case GS_PLATFORM_TOUCH_MOVE:
                    {
                        uintptr_t id = point->id;
                        gs_vec2* pos = &point->position;
                        gs_vec2* p = &platform->input.touch.points[id].position;
                        gs_vec2* d = &platform->input.touch.points[id].delta;
                        gs_platform_press_touch(id);  // Not sure if this is causing issues...
                        *d = gs_vec2_sub(*pos, *p);
                        *p = *pos;
                    } break;

                    case GS_PLATFORM_TOUCH_CANCEL:
                    {
                        uintptr_t id = point->id;
                        gs_platform_release_touch(id);
                        gs_subsystem(platform)->input.touch.size--;
                    } break;
                }
            } break;

            default: break;
        } 
    }
}

void gs_platform_update(gs_platform_t* platform)
{
    // Update platform input from previous frame        
    gs_platform_update_input(&platform->input);

    // Process input for this frame (user dependent update)
    gs_platform_process_input(&platform->input);

    // Poll all events
    gs_platform_poll_all_events();

    gs_platform_update_internal(platform);
}

bool gs_platform_poll_events(gs_platform_event_t* evt, bool32_t consume)
{
    gs_platform_t* platform = gs_subsystem(platform);

    if (!evt) return false;
    if (gs_dyn_array_empty(platform->events)) return false;
    if (evt->idx >= gs_dyn_array_size(platform->events)) return false;

    if (consume) {
        // Back event
        *evt = gs_dyn_array_back(platform->events);
        // Pop back
        gs_dyn_array_pop(platform->events);
    }
    else {
        uint32_t idx = evt->idx;
        *evt = platform->events[idx++]; 
        evt->idx = idx;
    }

    return true;
}

void gs_platform_add_event(gs_platform_event_t* evt)
{
    gs_platform_t* platform = gs_subsystem(platform);
    if (!evt) return;
    gs_dyn_array_push(platform->events, *evt);
}

bool gs_platform_was_key_down(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    return (input->prev_key_map[code]);
}

bool gs_platform_key_down(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    return (input->key_map[code]);
}

bool gs_platform_key_pressed(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    return (gs_platform_key_down(code) && !gs_platform_was_key_down(code));
}

bool gs_platform_key_released(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    return (gs_platform_was_key_down(code) && !gs_platform_key_down(code));
}

bool gs_platform_touch_down(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        return input->touch.points[idx].pressed;
    }
    return false;
}

bool gs_platform_touch_pressed(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        return (gs_platform_was_touch_down(idx) && !gs_platform_touch_down(idx));
    }
    return false;
}

bool gs_platform_touch_released(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        return (gs_platform_was_touch_down(idx) && !gs_platform_touch_down(idx));
    }
    return false;
}

bool gs_platform_was_mouse_down(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    return (input->mouse.prev_button_map[code]);
}

void gs_platform_press_mouse_button(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    if ((u32)code < (u32)GS_MOUSE_BUTTON_CODE_COUNT) 
    {
        input->mouse.button_map[code] = true;
    }
}

void gs_platform_release_mouse_button(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    if ((u32)code < (u32)GS_MOUSE_BUTTON_CODE_COUNT) 
    {
        input->mouse.button_map[code] = false;
    }
}

bool gs_platform_mouse_down(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    return (input->mouse.button_map[code]);
}

bool gs_platform_mouse_pressed(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    if (gs_platform_mouse_down(code) && !gs_platform_was_mouse_down(code))
    {
        return true;
    }
    return false;
}

bool gs_platform_mouse_released(gs_platform_mouse_button_code code)
{
    gs_platform_input_t* input = __gs_input();
    return (gs_platform_was_mouse_down(code) && !gs_platform_mouse_down(code));
}

bool gs_platform_mouse_moved()
{
    gs_platform_input_t* input = __gs_input();
    return (input->mouse.delta.x != 0.f || input->mouse.delta.y != 0.f);
}

void gs_platform_mouse_delta(float* x, float* y)
{
    gs_platform_input_t* input = __gs_input();
    *x = input->mouse.delta.x;
    *y = input->mouse.delta.y;
}

gs_vec2 gs_platform_mouse_deltav()
{
    gs_platform_input_t* input = __gs_input();
    gs_vec2 delta = gs_default_val();
    gs_platform_mouse_delta(&delta.x, &delta.y);
    return delta;
}

gs_vec2 gs_platform_mouse_positionv()
{
    gs_platform_input_t* input = __gs_input();

    return gs_v2( 
        input->mouse.position.x, 
        input->mouse.position.y
    );
}

void gs_platform_mouse_position(int32_t* x, int32_t* y)
{
    gs_platform_input_t* input = __gs_input();
    *x = (int32_t)input->mouse.position.x;
    *y = (int32_t)input->mouse.position.y;
}

void gs_platform_mouse_wheel(f32* x, f32* y)
{
    gs_platform_input_t* input = __gs_input();
    *x = input->mouse.wheel.x;
    *y = input->mouse.wheel.y;  
}

GS_API_DECL gs_vec2 gs_platform_mouse_wheelv()
{
    gs_vec2 wheel = gs_default_val();
    gs_platform_mouse_wheel(&wheel.x, &wheel.y);
    return wheel;
}

bool gs_platform_mouse_locked()
{
    return (__gs_input())->mouse.locked;
}

void gs_platform_touch_delta(uint32_t idx, float* x, float* y)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        *x = input->touch.points[idx].delta.x;
        *y = input->touch.points[idx].delta.y;
    }
}

gs_vec2 gs_platform_touch_deltav(uint32_t idx)
{
    gs_vec2 delta = gs_v2s(0.f);
    gs_platform_touch_delta(idx, &delta.x, &delta.y);
    return delta;
}

void gs_platform_touch_position(uint32_t idx, float* x, float* y)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        *x = input->touch.points[idx].position.x;
        *y = input->touch.points[idx].position.y;
    }
}

gs_vec2 gs_platform_touch_positionv(uint32_t idx)
{
    gs_vec2 p = gs_default_val();
    gs_platform_touch_position(idx, &p.x, &p.y);
    return p;
}

void gs_platform_press_touch(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        input->touch.points[idx].pressed = true;
    }
}

void gs_platform_release_touch(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        gs_println("releasing: %zu", idx);
        input->touch.points[idx].pressed = false;
    }
}

bool gs_platform_was_touch_down(uint32_t idx)
{
    gs_platform_input_t* input = __gs_input();
    if (idx < GS_PLATFORM_MAX_TOUCH) {
        return input->touch.points[idx].down;
    }
    return false;
}

void gs_platform_press_key(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    if (code < GS_KEYCODE_COUNT) {
        input->key_map[code] = true;
    }
}

void gs_platform_release_key(gs_platform_keycode code)
{
    gs_platform_input_t* input = __gs_input();
    if (code < GS_KEYCODE_COUNT) {
        input->key_map[code] = false;
    }
}

// Platform File IO
char* gs_platform_read_file_contents_default_impl(const char* file_path, const char* mode, size_t* sz)
{
    const char* path = file_path;

    #ifdef GS_PLATFORM_ANDROID
        const char* internal_data_path = gs_app()->android.internal_data_path;
        gs_snprintfc(tmp_path, 1024, "%s/%s", internal_data_path, file_path);
        path = tmp_path;
    #endif

    char* buffer = 0;
    FILE* fp = fopen(path, mode);
    size_t read_sz = 0;
    if (fp)
    {
        read_sz = gs_platform_file_size_in_bytes(file_path);
        buffer = (char*)gs_malloc(read_sz + 1);
        if (buffer) {
           size_t _r = fread(buffer, 1, read_sz, fp);
        }
        buffer[read_sz] = '\0';
        fclose(fp);
        if (sz) *sz = read_sz;
    }

    return buffer;
}

gs_result gs_platform_write_file_contents_default_impl(const char* file_path, const char* mode, void* data, size_t sz)
{
    const char* path = file_path;

    #ifdef GS_PLATFORM_ANDROID
        const char* internal_data_path = gs_app()->android.internal_data_path;
        gs_snprintfc(tmp_path, 1024, "%s/%s", internal_data_path, file_path);
        path = tmp_path;
    #endif

    FILE* fp = fopen(path, mode);
    if (fp) 
    {
        size_t ret = fwrite(data, sizeof(uint8_t), sz, fp);
        if (ret == sz)
        {
            fclose(fp);
            return GS_RESULT_SUCCESS;
        }
        fclose(fp);
    }
    return GS_RESULT_FAILURE;
}

GS_API_DECL bool gs_platform_dir_exists_default_impl(const char* dir_path)
{
	DIR* dir = opendir(dir_path);
	if (dir)
	{
		closedir(dir);
		return true;
	}
	return false;
}

GS_API_DECL int32_t gs_platform_mkdir_default_impl(const char* dir_path, int32_t opt)
{ 
    #ifdef __MINGW32__
        return mkdir(dir_path);
    #else
        return mkdir(dir_path, opt);
    #endif
}

bool gs_platform_file_exists_default_impl(const char* file_path)
{
    const char* path = file_path;

    #ifdef GS_PLATFORM_ANDROID
        const char* internal_data_path = gs_app()->android.internal_data_path;
        gs_snprintfc(tmp_path, 1024, "%s/%s", internal_data_path, file_path);
        path = tmp_path;
    #endif

    FILE* fp = fopen(path, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

int32_t gs_platform_file_size_in_bytes_default_impl(const char* file_path)
{
    #ifdef GS_PLATFORM_WIN

        HANDLE hFile = CreateFile(file_path, GENERIC_READ, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile==INVALID_HANDLE_VALUE)
            return -1; // error condition, could call GetLastError to find out more

        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size))
        {
            CloseHandle(hFile);
            return -1; // error condition, could call GetLastError to find out more
        }

        CloseHandle(hFile);
        return gs_util_safe_truncate_u64(size.QuadPart);

    #elif (defined GS_PLATFORM_ANDROID)

        const char* internal_data_path = gs_app()->android.internal_data_path;
        gs_snprintfc(tmp_path, 1024, "%s/%s", internal_data_path, file_path);
        struct stat st;
        stat(tmp_path, &st);
        return (int32_t)st.st_size;

    #else

        struct stat st;
        stat(file_path, &st);
        return (int32_t)st.st_size; 

    #endif
}

void gs_platform_file_extension_default_impl(char* buffer, size_t buffer_sz, const char* file_path)
{
    gs_util_get_file_extension(buffer, buffer_sz, file_path);
}

GS_API_DECL int32_t    
gs_platform_file_delete_default_impl(const char* file_path)
{
    #if (defined GS_PLATFORM_WIN)

        // Non-zero if successful
        return DeleteFile(file_path); 

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)

        // Returns 0 if successful
        return !remove(file_path);

    #endif

    return 0;
}

GS_API_DECL int32_t    
gs_platform_file_copy_default_impl(const char* src_path, const char* dst_path)
{
    #if (defined GS_PLATFORM_WIN)

        return CopyFile(src_path, dst_path, false);

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)

        FILE* file_w = NULL;
        FILE* file_r = NULL;
        char buffer[2048] = gs_default_val();

        if ((file_w = fopen(src_path, "wb")) == NULL) {
            return 0;
        }
        if ((file_r = fopen(dst_path, "rb")) == NULL) {
            return 0;
        }

        // Read file in 2kb chunks to write to location
        int32_t len = 0;
        while ((len = fread(buffer, sizeof(buffer), 1, file_r)) > 0) {
            fwrite(buffer, len, 1, file_w);
        }

        // Close both files
        fclose(file_r);
        fclose(file_w);

    #endif

    return 0;
}

GS_API_DECL int32_t    
gs_platform_file_compare_time(uint64_t time_a, uint64_t time_b)
{ 
    return time_a < time_b ? -1 : time_a == time_b ? 0 : 1; 
}

GS_API_DECL gs_platform_file_stats_t 
gs_platform_file_stats(const char* file_path)
{
    gs_platform_file_stats_t stats = gs_default_val();

    #if (defined GS_PLATFORM_WIN) 
        
        WIN32_FILE_ATTRIBUTE_DATA data = gs_default_val();
        FILETIME ftime = gs_default_val();
        FILETIME ctime = gs_default_val();
        FILETIME atime = gs_default_val();
        if (GetFileAttributesExA(file_path, GetFileExInfoStandard, &data))
        {
            ftime = data.ftLastWriteTime;
            ctime = data.ftCreationTime;
            atime = data.ftLastAccessTime;
        }

        stats.modified_time = *((uint64_t*)&ftime);
        stats.access_time = *((uint64_t*)&atime);
        stats.creation_time = *((uint64_t*)&ctime);

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)
        struct stat attr = gs_default_val();
        stat(file_path, &attr); 
        stats.modified_time = *((uint64_t*)&attr.st_mtime);

    #endif

    return stats;
}

GS_API_DECL void*      
gs_platform_library_load_default_impl(const char* lib_path)
{
    #if (defined GS_PLATFORM_WIN) 

        return (void*)LoadLibraryA(lib_path);

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)

        return (void*)dlopen(lib_path, RTLD_LAZY);

    #endif

    return NULL;
}

GS_API_DECL void       
gs_platform_library_unload_default_impl(void* lib)
{
    if (!lib) return;

    #if (defined GS_PLATFORM_WIN) 
        
        FreeLibrary((HMODULE)lib);

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)

        dlclose(lib);

    #endif
}

GS_API_DECL void*      
gs_platform_library_proc_address_default_impl(void* lib, const char* func)
{
    if (!lib) return NULL;

    #if (defined GS_PLATFORM_WIN) 

        return (void*)GetProcAddress((HMODULE)lib, func);

    #elif (defined GS_PLATFORM_LINUX || defined GS_PLATFORM_APPLE || defined GS_PLATFORM_ANDROID)

        return (void*)dlsym(lib, func);

    #endif

    return NULL;
}

#undef GS_PLATFORM_IMPL_DEFAULT
#endif // GS_PLATFORM_IMPL_DEFAULT

/*======================
// RGFW Implemenation
======================*/

#ifdef GS_PLATFORM_IMPL_RGFW

#define GLAD_IMPL
#include "../external/glad/glad_impl.h"

#define RGFW_IMPLEMENTATION
#define b8 u8
#include "../external/RGFW/RGFW.h"
#undef b8

#if (defined GS_PLATFORM_APPLE || defined GS_PLATFORM_LINUX)

    #include <sched.h>

#endif

// Forward Decls.
void __rgfw_key_callback(RGFW_window* window, u32 keycode, char keyName[16], u8 lockState, u8 pressed);
void __rgfw_char_callback(RGFW_window* window, uint32_t codepoint);
void __rgfw_mouse_button_callback(RGFW_window* win, u8 button, double scroll, u8 pressed);
void __rgfw_mouse_cursor_position_callback(RGFW_window* win, RGFW_point point);
void __rgfw_mouse_scroll_wheel_callback(RGFW_window* window, f64 xoffset, f64 yoffset);
void __rgfw_mouse_cursor_enter_callback(RGFW_window* win, RGFW_point point, u8 status);
void __rgfw_frame_buffer_size_callback(RGFW_window* window, s32 width, s32 height);
void __rgfw_drop_callback(RGFW_window* window);

/*
#define __rgfw_window_from_handle(platform, handle)\
    ((RGFW_window*)(gs_slot_array_get((platform)->windows, (handle))))
*/

/*== Platform Init / Shutdown == */

void gs_platform_init(gs_platform_t* pf)
{
    gs_assert(pf);

    gs_println("Initializing RGFW");

    u32 win_args = 0;

    switch (pf->settings.video.driver)
    {
        case GS_PLATFORM_VIDEO_DRIVER_TYPE_OPENGL:
        {
            #if (defined GS_PLATFORM_APPLE)
                RGFW_setGLVersion(RGFW_GL_CORE, 4, 1);
            #else
                win_args |= RGFW_SCALE_TO_MONITOR;
                if (pf->settings.video.graphics.debug)
                {
                    RGFW_setGLVersion(RGFW_GL_CORE, 4, 3);
                }
            #endif
            // glfwSwapInterval(platform->settings.video.vsync_enabled);
            // glfwSwapInterval(0);
        } break;

        default:
        {
            // Default to no output at all.
            gs_println("Video format not supported.");
            gs_assert(false);
        } break;
    }

    // Construct cursors
    pf->cursors[(u32)GS_PLATFORM_CURSOR_ARROW]      = (void*)RGFW_MOUSE_ARROW;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_IBEAM]      = (void*)RGFW_MOUSE_IBEAM;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_SIZE_NW_SE] = (void*)RGFW_MOUSE_CROSSHAIR;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_SIZE_NE_SW] = (void*)RGFW_MOUSE_CROSSHAIR;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_SIZE_NS]    = (void*)RGFW_MOUSE_RESIZE_NS;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_SIZE_WE]    = (void*)RGFW_MOUSE_RESIZE_EW;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_SIZE_ALL]   = (void*)RGFW_MOUSE_CROSSHAIR;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_HAND]       = (void*)RGFW_MOUSE_POINTING_HAND;
    pf->cursors[(u32)GS_PLATFORM_CURSOR_NO]         = (void*)RGFW_MOUSE_ARROW;
}

GS_API_DECL void gs_platform_update_internal(gs_platform_t* platform)
{ 
    gs_platform_input_t* input = &platform->input; 

    // Platform time
    platform->time.elapsed = (RGFW_getTime() * 1000.0);

    // Update all window/framebuffer state
    for (
        gs_slot_array_iter it = gs_slot_array_iter_new(platform->windows); 
        gs_slot_array_iter_valid(platform->windows, it); 
        gs_slot_array_iter_advance(platform->windows, it)
    )
    {
        // Cache all necessary window information 
		int32_t wx = 0, wy = 0, fx = 0, fy = 0, wpx = 0, wpy = 0;
        gs_platform_window_t* win = gs_slot_array_getp(platform->windows, it); 
        fx = wx = ((RGFW_window*)win->hndl)->r.w; 
        fy = wy = ((RGFW_window*)win->hndl)->r.h;
        wpx = ((RGFW_window*)win->hndl)->r.x; 
        wpy = ((RGFW_window*)win->hndl)->r.y;
		win->window_size = gs_v2( (float)wx, (float)wy);
		win->window_position = gs_v2( (float)wpx, (float)wpy);
		win->framebuffer_size = gs_v2( (float)fx, (float)fy);
    }
}

void gs_platform_shutdown(gs_platform_t* pf)
{
    // Free all windows in glfw
    // TODO(john): Figure out crash with glfwDestroyWindow && glfwTerminate
    for
    (
        gs_slot_array_iter it = 0;
        gs_slot_array_iter_valid(pf->windows, it);
        gs_slot_array_iter_advance(pf->windows, it)
    )
    {
        // RGFW_window* win =  __rgfw_window_from_handle(pf, it);
        // glfwDestroyWindow(win);
    }

    // glfwTerminate();
}

uint32_t gs_platform_key_to_codepoint(gs_platform_keycode key)
{
    uint32_t code = 0;
    switch (key)
    {
        default:
        case GS_KEYCODE_COUNT:
        case GS_KEYCODE_INVALID:          code = 0; break;
        case GS_KEYCODE_SPACE:            code = RGFW_Space; break;
        case GS_KEYCODE_APOSTROPHE:       code = 39; break;
        case GS_KEYCODE_COMMA:            code = 44; break;
        case GS_KEYCODE_MINUS:            code = 45; break;
        case GS_KEYCODE_PERIOD:           code = 46; break;
        case GS_KEYCODE_SLASH:            code = 47; break;
        case GS_KEYCODE_0:                code = 48; break;
        case GS_KEYCODE_1:                code = 49; break;
        case GS_KEYCODE_2:                code = 50; break;
        case GS_KEYCODE_3:                code = 51; break;
        case GS_KEYCODE_4:                code = 52; break;
        case GS_KEYCODE_5:                code = 53; break;
        case GS_KEYCODE_6:                code = 54; break;
        case GS_KEYCODE_7:                code = 55; break;
        case GS_KEYCODE_8:                code = 56; break;
        case GS_KEYCODE_9:                code = 57; break;
        case GS_KEYCODE_SEMICOLON:        code = 59; break;  /* ; */
        case GS_KEYCODE_EQUAL:            code = 61; break;  /* code = */
        case GS_KEYCODE_A:                code = 65; break;
        case GS_KEYCODE_B:                code = 66; break;
        case GS_KEYCODE_C:                code = 67; break;
        case GS_KEYCODE_D:                code = 68; break;
        case GS_KEYCODE_E:                code = 69; break;
        case GS_KEYCODE_F:                code = 70; break;
        case GS_KEYCODE_G:                code = 71; break;
        case GS_KEYCODE_H:                code = 72; break;
        case GS_KEYCODE_I:                code = 73; break;
        case GS_KEYCODE_J:                code = 74; break;
        case GS_KEYCODE_K:                code = 75; break;
        case GS_KEYCODE_L:                code = 76; break;
        case GS_KEYCODE_M:                code = 77; break;
        case GS_KEYCODE_N:                code = 78; break;
        case GS_KEYCODE_O:                code = 79; break;
        case GS_KEYCODE_P:                code = 80; break;
        case GS_KEYCODE_Q:                code = 81; break;
        case GS_KEYCODE_R:                code = 82; break;
        case GS_KEYCODE_S:                code = 83; break;
        case GS_KEYCODE_T:                code = 84; break;
        case GS_KEYCODE_U:                code = 85; break;
        case GS_KEYCODE_V:                code = 86; break;
        case GS_KEYCODE_W:                code = 87; break;
        case GS_KEYCODE_X:                code = 88; break;
        case GS_KEYCODE_Y:                code = 89; break;
        case GS_KEYCODE_Z:                code = 90; break;
        case GS_KEYCODE_LEFT_BRACKET:     code = 91; break;  /* [ */
        case GS_KEYCODE_BACKSLASH:        code = 92; break;  /* \ */
        case GS_KEYCODE_RIGHT_BRACKET:    code = 93; break;  /* ] */
        case GS_KEYCODE_GRAVE_ACCENT:     code = 96; break;  /* ` */
        case GS_KEYCODE_WORLD_1:          code = 161; break; /* non-US #1 */
        case GS_KEYCODE_WORLD_2:          code = 162; break; /* non-US #2 */
        case GS_KEYCODE_ESC:              code = 256; break;
        case GS_KEYCODE_ENTER:            code = 257; break;
        case GS_KEYCODE_TAB:              code = 258; break;
        case GS_KEYCODE_BACKSPACE:        code = 259; break;
        case GS_KEYCODE_INSERT:           code = 260; break;
        case GS_KEYCODE_DELETE:           code = RGFW_Delete; break;
        case GS_KEYCODE_RIGHT:            code = 262; break;
        case GS_KEYCODE_LEFT:             code = 263; break;
        case GS_KEYCODE_DOWN:             code = 264; break;
        case GS_KEYCODE_UP:               code = 265; break;
        case GS_KEYCODE_PAGE_UP:          code = 266; break;
        case GS_KEYCODE_PAGE_DOWN:        code = 267; break;
        case GS_KEYCODE_HOME:             code = 268; break;
        case GS_KEYCODE_END:              code = 269; break;
        case GS_KEYCODE_CAPS_LOCK:        code = 280; break;
        case GS_KEYCODE_SCROLL_LOCK:      code = 281; break;
        case GS_KEYCODE_NUM_LOCK:         code = 282; break;
        case GS_KEYCODE_PRINT_SCREEN:     code = 283; break;
        case GS_KEYCODE_PAUSE:            code = 284; break;
        case GS_KEYCODE_F1:               code = 290; break;
        case GS_KEYCODE_F2:               code = 291; break;
        case GS_KEYCODE_F3:               code = 292; break;
        case GS_KEYCODE_F4:               code = 293; break;
        case GS_KEYCODE_F5:               code = 294; break;
        case GS_KEYCODE_F6:               code = 295; break;
        case GS_KEYCODE_F7:               code = 296; break;
        case GS_KEYCODE_F8:               code = 297; break;
        case GS_KEYCODE_F9:               code = 298; break;
        case GS_KEYCODE_F10:              code = 299; break;
        case GS_KEYCODE_F11:              code = 300; break;
        case GS_KEYCODE_F12:              code = 301; break;
        case GS_KEYCODE_F13:              code = 302; break;
        case GS_KEYCODE_F14:              code = 303; break;
        case GS_KEYCODE_F15:              code = 304; break;
        case GS_KEYCODE_F16:              code = 305; break;
        case GS_KEYCODE_F17:              code = 306; break;
        case GS_KEYCODE_F18:              code = 307; break;
        case GS_KEYCODE_F19:              code = 308; break;
        case GS_KEYCODE_F20:              code = 309; break;
        case GS_KEYCODE_F21:              code = 310; break;
        case GS_KEYCODE_F22:              code = 311; break;
        case GS_KEYCODE_F23:              code = 312; break;
        case GS_KEYCODE_F24:              code = 313; break;
        case GS_KEYCODE_F25:              code = 314; break;
        case GS_KEYCODE_KP_0:             code = 320; break;
        case GS_KEYCODE_KP_1:             code = 321; break;
        case GS_KEYCODE_KP_2:             code = 322; break;
        case GS_KEYCODE_KP_3:             code = 323; break;
        case GS_KEYCODE_KP_4:             code = 324; break;
        case GS_KEYCODE_KP_5:             code = 325; break;
        case GS_KEYCODE_KP_6:             code = 326; break;
        case GS_KEYCODE_KP_7:             code = 327; break;
        case GS_KEYCODE_KP_8:             code = 328; break;
        case GS_KEYCODE_KP_9:             code = 329; break;
        case GS_KEYCODE_KP_DECIMAL:       code = 330; break;
        case GS_KEYCODE_KP_DIVIDE:        code = 331; break;
        case GS_KEYCODE_KP_MULTIPLY:      code = 332; break;
        case GS_KEYCODE_KP_SUBTRACT:      code = 333; break;
        case GS_KEYCODE_KP_ADD:           code = 334; break;
        case GS_KEYCODE_KP_ENTER:         code = 335; break;
        case GS_KEYCODE_KP_EQUAL:         code = 336; break;
        case GS_KEYCODE_LEFT_SHIFT:       code = 340; break;
        case GS_KEYCODE_LEFT_CONTROL:     code = 341; break;
        case GS_KEYCODE_LEFT_ALT:         code = 342; break;
        case GS_KEYCODE_LEFT_SUPER:       code = 343; break;
        case GS_KEYCODE_RIGHT_SHIFT:      code = 344; break;
        case GS_KEYCODE_RIGHT_CONTROL:    code = 345; break;
        case GS_KEYCODE_RIGHT_ALT:        code = 346; break;
        case GS_KEYCODE_RIGHT_SUPER:      code = 347; break;
        case GS_KEYCODE_MENU:             code = 348; break;
    }
    return code;
}

gs_platform_keycode rgfw_key_to_gs_keycode(u32 code);

gs_platform_keycode gs_platform_codepoint_to_key(uint32_t code)
{
    gs_platform_keycode key = GS_KEYCODE_INVALID;
    return rgfw_key_to_gs_keycode(code);
}

/*=== RGFW Callbacks ===*/

gs_platform_keycode rgfw_key_to_gs_keycode(u32 code)
{
    switch (code)
    {
        case RGFW_a:                return GS_KEYCODE_A; break;
        case RGFW_b:                return GS_KEYCODE_B; break;
        case RGFW_c:                return GS_KEYCODE_C; break;
        case RGFW_d:                return GS_KEYCODE_D; break;
        case RGFW_e:                return GS_KEYCODE_E; break;
        case RGFW_f:                return GS_KEYCODE_F; break;
        case RGFW_g:                return GS_KEYCODE_G; break;
        case RGFW_h:                return GS_KEYCODE_H; break;
        case RGFW_i:                return GS_KEYCODE_I; break;
        case RGFW_j:                return GS_KEYCODE_J; break;
        case RGFW_k:                return GS_KEYCODE_K; break;
        case RGFW_l:               return GS_KEYCODE_L; break;
        case RGFW_m:                return GS_KEYCODE_M; break;
        case RGFW_n:                return GS_KEYCODE_N; break;
        case RGFW_o:                return GS_KEYCODE_O; break;
        case RGFW_p:                return GS_KEYCODE_P; break;
        case RGFW_q:                return GS_KEYCODE_Q; break;
        case RGFW_r:                return GS_KEYCODE_R; break;
        case RGFW_s:                return GS_KEYCODE_S; break;
        case RGFW_t:                return GS_KEYCODE_T; break;
        case RGFW_u:                return GS_KEYCODE_U; break;
        case RGFW_v:                return GS_KEYCODE_V; break;
        case RGFW_w:                return GS_KEYCODE_W; break;
        case RGFW_x:                return GS_KEYCODE_X; break;
        case RGFW_y:                return GS_KEYCODE_Y; break;
        case RGFW_z:                return GS_KEYCODE_Z; break;
        case RGFW_ShiftL:       return GS_KEYCODE_LEFT_SHIFT; break;
        case RGFW_ShiftR:      return GS_KEYCODE_RIGHT_SHIFT; break;
        case RGFW_AltL:         return GS_KEYCODE_LEFT_ALT; break;
        case RGFW_AltR:        return GS_KEYCODE_RIGHT_ALT; break;
        case RGFW_ControlL:     return GS_KEYCODE_LEFT_CONTROL; break;
        case RGFW_ControlR:    return GS_KEYCODE_RIGHT_CONTROL; break;
        case RGFW_BackSpace:        return GS_KEYCODE_BACKSPACE; break;
        case RGFW_BackSlash:        return GS_KEYCODE_BACKSLASH; break;
        case RGFW_Slash:            return GS_KEYCODE_SLASH; break;
        case RGFW_Backtick:     return GS_KEYCODE_GRAVE_ACCENT; break;
        case RGFW_Comma:            return GS_KEYCODE_COMMA; break;
        case RGFW_Period:           return GS_KEYCODE_PERIOD; break;
        case RGFW_Escape:           return GS_KEYCODE_ESC; break; 
        case RGFW_Space:            return GS_KEYCODE_SPACE; break;
        case RGFW_Left:             return GS_KEYCODE_LEFT; break;
        case RGFW_Up:               return GS_KEYCODE_UP; break;
        case RGFW_Right:            return GS_KEYCODE_RIGHT; break;
        case RGFW_Down:             return GS_KEYCODE_DOWN; break;
        case RGFW_0:                return GS_KEYCODE_0; break;
        case RGFW_1:                return GS_KEYCODE_1; break;
        case RGFW_2:                return GS_KEYCODE_2; break;
        case RGFW_3:                return GS_KEYCODE_3; break;
        case RGFW_4:                return GS_KEYCODE_4; break;
        case RGFW_5:                return GS_KEYCODE_5; break;
        case RGFW_6:                return GS_KEYCODE_6; break;
        case RGFW_7:                return GS_KEYCODE_7; break;
        case RGFW_8:                return GS_KEYCODE_8; break;
        case RGFW_9:                return GS_KEYCODE_9; break;
        case RGFW_KP_0:             return GS_KEYCODE_KP_0; break;
        case RGFW_KP_1:             return GS_KEYCODE_KP_1; break;
        case RGFW_KP_2:             return GS_KEYCODE_KP_2; break;
        case RGFW_KP_3:             return GS_KEYCODE_KP_3; break;
        case RGFW_KP_4:             return GS_KEYCODE_KP_4; break;
        case RGFW_KP_5:             return GS_KEYCODE_KP_5; break;
        case RGFW_KP_6:             return GS_KEYCODE_KP_6; break;
        case RGFW_KP_7:             return GS_KEYCODE_KP_7; break;
        case RGFW_KP_8:             return GS_KEYCODE_KP_8; break;
        case RGFW_KP_9:             return GS_KEYCODE_KP_9; break;
        case RGFW_CapsLock:        return GS_KEYCODE_CAPS_LOCK; break;
        case RGFW_Delete:           return GS_KEYCODE_DELETE; break;
        case RGFW_End:              return GS_KEYCODE_END; break;
        case RGFW_F1:               return GS_KEYCODE_F1; break;
        case RGFW_F2:               return GS_KEYCODE_F2; break;
        case RGFW_F3:               return GS_KEYCODE_F3; break;
        case RGFW_F4:               return GS_KEYCODE_F4; break;
        case RGFW_F5:               return GS_KEYCODE_F5; break;
        case RGFW_F6:               return GS_KEYCODE_F6; break;
        case RGFW_F7:               return GS_KEYCODE_F7; break;
        case RGFW_F8:               return GS_KEYCODE_F8; break;
        case RGFW_F9:               return GS_KEYCODE_F9; break;
        case RGFW_F10:              return GS_KEYCODE_F10; break;
        case RGFW_F11:              return GS_KEYCODE_F11; break;
        case RGFW_F12:              return GS_KEYCODE_F12; break;
        case RGFW_Home:             return GS_KEYCODE_HOME; break;
        case RGFW_Equals:            return GS_KEYCODE_EQUAL; break;
        case RGFW_Minus:            return GS_KEYCODE_MINUS; break;
        case RGFW_Bracket:     return GS_KEYCODE_LEFT_BRACKET; break;
        case RGFW_CloseBracket:    return GS_KEYCODE_RIGHT_BRACKET; break;
        case RGFW_Semicolon:        return GS_KEYCODE_SEMICOLON; break;
        case RGFW_Return:            return GS_KEYCODE_ENTER; break;
        case RGFW_Insert:           return GS_KEYCODE_INSERT; break;
        case RGFW_PageUp:          return GS_KEYCODE_PAGE_UP; break;
        case RGFW_PageDown:        return GS_KEYCODE_PAGE_DOWN; break;
        case RGFW_Numlock:         return GS_KEYCODE_NUM_LOCK; break;
        case RGFW_Tab:              return GS_KEYCODE_TAB; break;
        case RGFW_Multiply:      return GS_KEYCODE_KP_MULTIPLY; break;
        case RGFW_KP_Slash:        return GS_KEYCODE_KP_DIVIDE; break;
        case RGFW_KP_Minus:      return GS_KEYCODE_KP_SUBTRACT; break;
        case RGFW_KP_Return:         return GS_KEYCODE_KP_ENTER; break;
        case RGFW_KP_Period:       return GS_KEYCODE_KP_DECIMAL; break;
        default:                        return GS_KEYCODE_COUNT; break;
    }

    // Shouldn't reach here
    return GS_KEYCODE_COUNT;
}

gs_platform_mouse_button_code __rgfw_button_to_gs_mouse_button(s32 code)
{
    switch (code)
    {
        case RGFW_mouseLeft:    return GS_MOUSE_LBUTTON; break;
        case RGFW_mouseRight:   return GS_MOUSE_RBUTTON; break;
        case RGFW_mouseMiddle: return GS_MOUSE_MBUTTON; break;
    }   

    // Shouldn't reach here
    return GS_MOUSE_BUTTON_CODE_COUNT;
}

void __rgfw_char_callback(RGFW_window* window, uint32_t codepoint)
{
    // Grab platform instance from engine
    gs_platform_t* platform = gs_subsystem(platform); 

    gs_platform_event_t evt = gs_default_val();
    evt.type = GS_PLATFORM_EVENT_TEXT;
    evt.text.codepoint = codepoint;

    // Add action
    gs_platform_add_event(&evt);
}

void __rgfw_key_callback(RGFW_window* window, u32 keycode, char keyName[16], u8 lockState, u8 pressed) 
{
    // Grab platform instance from engine
    gs_platform_t* platform = gs_subsystem(platform);

    // Get keycode from key
    gs_platform_keycode key = rgfw_key_to_gs_keycode(keycode);

    // Push back event into platform events
    gs_platform_event_t evt = gs_default_val();
    evt.type = GS_PLATFORM_EVENT_KEY;
    evt.key.codepoint = keycode;
    evt.key.keycode = key;
    evt.key.modifier = (gs_platform_key_modifier_type)lockState;

    switch (pressed)
    {
        // Released
        case 0: {
            gs_platform_release_key(key);
            evt.key.action = GS_PLATFORM_KEY_RELEASED;
        } break;

        // Pressed
        case 1: {
            gs_platform_press_key(key);
            evt.key.action = GS_PLATFORM_KEY_PRESSED;
        } break;

        default: {
        } break;
    }

    // Add action
    gs_platform_add_event(&evt);
}

void __rgfw_mouse_button_callback(RGFW_window* win, u8 codepoint, double scroll, u8 pressed)
{
    if (codepoint >= RGFW_mouseScrollUp)
        __rgfw_mouse_scroll_wheel_callback(win, 0, scroll);
    
    // Grab platform instance from engine
    gs_platform_t* platform = gs_subsystem(platform);

    // Get mouse code from key
    gs_platform_mouse_button_code button = __rgfw_button_to_gs_mouse_button(button);

    // Push back event into platform events
    gs_platform_event_t evt = gs_default_val();
    evt.type = GS_PLATFORM_EVENT_MOUSE;
    evt.mouse.codepoint = codepoint;
    evt.mouse.button = button;

    switch (pressed)
    {
        // Released
        case 0:
        {
            gs_platform_release_mouse_button(button);
            evt.mouse.action = GS_PLATFORM_MOUSE_BUTTON_RELEASED;
        } break;

        // Pressed
        case 1:
        {
            gs_platform_press_mouse_button(button);
            evt.mouse.action = GS_PLATFORM_MOUSE_BUTTON_PRESSED;
        } break;
    }

    // Add action
    gs_platform_add_event(&evt);
}

void __rgfw_mouse_cursor_position_callback(RGFW_window* win, RGFW_point point)
{
    gs_platform_t* platform = gs_subsystem(platform);
    // platform->input.mouse.position = gs_v2((f32)x, (f32)y);
    // platform->input.mouse.moved_this_frame = true;

    gs_platform_event_t gs_evt = gs_default_val();
    gs_evt.type = GS_PLATFORM_EVENT_MOUSE;
    gs_evt.mouse.action = GS_PLATFORM_MOUSE_MOVE;

    // gs_println("pos: <%.2f, %.2f>, old: <%.2f, %.2f>", x, y, platform->input.mouse.position.x, platform->input.mouse.position.y);

    // gs_evt.mouse.move = gs_v2((f32)x, (f32)y);

    // Calculate mouse move based on whether locked or not
    if (gs_platform_mouse_locked()) {
        gs_evt.mouse.move.x = point.x - platform->input.mouse.position.x;
        gs_evt.mouse.move.y = point.y - platform->input.mouse.position.y;
        platform->input.mouse.position.x = point.x;
        platform->input.mouse.position.y = point.y;
    } else {
        gs_evt.mouse.move = gs_v2((f32)point.x, (f32)point.y);
    }

    // Push back event into platform events
    gs_platform_add_event(&gs_evt);
}

void __rgfw_mouse_scroll_wheel_callback(RGFW_window* window, f64 x, f64 y)
{
    gs_platform_t* platform = gs_subsystem(platform);
    platform->input.mouse.wheel = gs_v2((f32)x, (f32)y);

    // Push back event into platform events
    gs_platform_event_t gs_evt = gs_default_val();
    gs_evt.type = GS_PLATFORM_EVENT_MOUSE;
    gs_evt.mouse.action = GS_PLATFORM_MOUSE_WHEEL;
    gs_evt.mouse.wheel = gs_v2((f32)x, (f32)y);
    gs_platform_add_event(&gs_evt);
}

// Gets called when mouse enters or leaves frame of window
void __rgfw_mouse_cursor_enter_callback(RGFW_window* win, RGFW_point point, u8 status)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_event_t gs_evt = gs_default_val();
    gs_evt.type = GS_PLATFORM_EVENT_MOUSE;
    gs_evt.mouse.action = status ? GS_PLATFORM_MOUSE_ENTER : GS_PLATFORM_MOUSE_LEAVE;
    gs_platform_add_event(&gs_evt);
}

void __rgfw_frame_buffer_size_callback(RGFW_window* window, s32 width, s32 height)
{
    // Nothing for now
}

/*== Platform Input == */

void gs_platform_process_input(gs_platform_input_t* input)
{
    // not sure why it's designed this way and I don't feel like figuring out how this is supposed to work
    // this (the window abstraction system gunslinger has) shouldn't be as messy/complicated as it is
    gs_platform_t* platform = gs_subsystem(platform);
    RGFW_window* win = platform->windows->data->hndl;
    RGFW_window_checkEvent(win);
}

/*== Platform Util == */
#include <unistd.h>

void  gs_platform_sleep(float ms)
{
    #if (defined GS_PLATFORM_WIN)

            timeBeginPeriod(1);
            Sleep((uint64_t)ms);
            timeEndPeriod(1);

    #elif (defined GS_PLATFORM_APPLE)

            usleep(ms * 1000.f); // unistd.h
    #else
	    if (ms < 0.f) {
		return;
	    }

	    struct timespec ts = gs_default_val();
	    int32_t res = 0;
	    ts.tv_sec = ms / 1000.f;
	    ts.tv_nsec = ((uint64_t)ms % 1000) * 1000000;
	    do {
		res = nanosleep(&ts, &ts);
	    } while (res);

            // usleep(ms * 1000.f); // unistd.h
    #endif
}

GS_API_DECL double 
gs_platform_elapsed_time()
{ 
    gs_platform_t* platform = gs_subsystem(platform);
    return platform->time.elapsed;
}

/*== Platform Video == */

GS_API_DECL void
gs_platform_enable_vsync(int32_t enabled)
{
    gs_platform_t* platform = gs_subsystem(platform); 
    RGFW_window_swapInterval(platform->windows->data->hndl, enabled ? 1 : 0);
}

/*== OpenGL debug callback == */
void GLAPIENTRY __gs_platform_gl_debug(GLenum source, GLenum type, GLuint id, GLenum severity,
                            GLsizei len, const GLchar* msg, const void* user)
{
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        gs_println("GL: %s", msg);
    }
}

/*== Platform Window == */

GS_API_DECL gs_platform_window_t 
gs_platform_window_create_internal(const gs_platform_window_desc_t* desc)
{
    gs_platform_window_t win = gs_default_val();

    if (!desc) 
    {
        // Log warning
        gs_log_warning("Window descriptor is null.");
        return win;
    }

    // Grab window hints from application desc
    u32 window_hints = desc->flags;
    bool visible = ~window_hints & GS_WINDOW_FLAGS_INVISIBLE;
    
    // Set whether or not the screen is resizable
    u32 hints;
    if (GS_WINDOW_FLAGS_NO_RESIZE == GS_WINDOW_FLAGS_NO_RESIZE)
        hints |= RGFW_NO_RESIZE;

    RGFW_window* window = NULL;

    #define CONSTRUCT_WINDOW(W, H, T, M, I)\
    do {\
        window = RGFW_createWindow(T, RGFW_RECT(0, 0, W, H), hints);\
        win.hndl = window;\
    } while (0)

    if (visible) {
        // Set multi-samples
        if (desc->num_samples) {
            RGFW_setGLSamples(desc->num_samples); 
        }
        else {
            RGFW_setGLSamples(0);
        }

        // Get monitor if fullscreen
        RGFW_monitor monitor;
        if ((window_hints & GS_WINDOW_FLAGS_FULLSCREEN) == GS_WINDOW_FLAGS_FULLSCREEN) {
            int monitor_count;
            RGFW_monitor* monitors = RGFW_getMonitors();
            if (desc->monitor_index < 6) {
                monitor = monitors[desc->monitor_index];
            }
        } 
        CONSTRUCT_WINDOW(desc->width, desc->height, desc->title, monitor, NULL);

        // Callbacks for window
        RGFW_window_makeCurrent(window);
        RGFW_setKeyCallback(__rgfw_key_callback);
        RGFW_setMouseButtonCallback(__rgfw_mouse_button_callback);
        RGFW_setMouseNotifyCallBack(__rgfw_mouse_cursor_enter_callback);
        RGFW_setMousePosCallback(__rgfw_mouse_cursor_position_callback);

        // Cache all necessary window information 
        int32_t wx = 0, wy = 0, fx = 0, fy = 0, wpx = 0, wpy = 0;
        fx = wx = ((RGFW_window*)win.hndl)->r.w; 
        fy = wy = ((RGFW_window*)win.hndl)->r.h;
        wpx = ((RGFW_window*)win.hndl)->r.x; 
        wpy = ((RGFW_window*)win.hndl)->r.y;

        win.window_size = gs_v2((float)wx, (float)wy);
        win.window_position = gs_v2((float)wpx, (float)wpy);
        win.framebuffer_size = gs_v2((float)fx, (float)fy);
    }
    else { 
        void* mwin = gs_platform_raw_window_handle(gs_platform_main_window());
        CONSTRUCT_WINDOW(1, 1, desc->title, 0, mwin);
    }

    if (window == NULL) {
        gs_log_error("Failed to create window.");
        return win;
    }

    // Need to make sure this is ONLY done once.
    if (gs_slot_array_empty(gs_subsystem(platform)->windows)) {
        if (!gladLoadGLLoader((GLADloadproc)RGFW_getProcAddress)) {
            gs_log_warning("Failed to initialize RGFW.");
            return win;
        }

        switch (gs_subsystem(platform)->settings.video.driver) {
            case GS_PLATFORM_VIDEO_DRIVER_TYPE_OPENGL: {
                gs_log_info("OpenGL Version: %s", glGetString(GL_VERSION));
                if (gs_subsystem(platform)->settings.video.graphics.debug) {
                    glDebugMessageCallback(__gs_platform_gl_debug, NULL);
                }
            } break;
            default: break;
        }
    }

    return win;
}



gs_dropped_files_callback_t gs_dropped_files_callback_func= NULL;
gs_window_close_callback_t gs_window_close_callback_func = NULL;
gs_framebuffer_resize_callback_t gs_framebuffer_resize_callback_func = NULL;
gs_character_callback_t gs_character_callback_func = NULL;



void RGFW_gs_keyfunc(RGFW_window* win, u32 keycode, char keyName[16], u8 lockState, u8 pressed) {
    if (pressed == RGFW_FALSE)
        return;
    
    gs_character_callback_func(win, RGFW_keyCodeToCharAuto(keycode, lockState));
}
void RGFW_gs_windowquitfunc(RGFW_window* win) {
    gs_window_close_callback_func(win);
}
void RGFW_gs_dndfunc(RGFW_window* win, char droppedFiles[RGFW_MAX_DROPS][RGFW_MAX_PATH], u32 droppedFilesCount) {
    gs_dropped_files_callback_func(win, droppedFilesCount, (const char**)droppedFiles);
}
void RGFW_gs_windowresizefunc(RGFW_window* win, RGFW_rect r) {
    gs_framebuffer_resize_callback_func(win, r.w, r.h);
}


// Platform callbacks
GS_API_DECL void 
gs_platform_set_dropped_files_callback(uint32_t handle, gs_dropped_files_callback_t cb)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    gs_dropped_files_callback_func = cb;

    RGFW_setDndCallback(RGFW_gs_dndfunc);
}

GS_API_DECL void 
gs_platform_set_window_close_callback(uint32_t handle, gs_window_close_callback_t cb)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    gs_window_close_callback_func = cb;
    
    RGFW_setWindowQuitCallback(RGFW_gs_windowquitfunc);
}

GS_API_DECL void 
gs_platform_set_character_callback(uint32_t handle, gs_character_callback_t cb)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    gs_character_callback_func = cb;
    RGFW_setKeyCallback(RGFW_gs_keyfunc);
}

GS_API_DECL void 
gs_platform_set_framebuffer_resize_callback(uint32_t handle, gs_framebuffer_resize_callback_t cb)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    gs_framebuffer_resize_callback_func = cb;
    RGFW_setWindowResizeCallback(RGFW_gs_windowresizefunc);
}

GS_API_DECL void 
gs_platform_mouse_set_position(uint32_t handle, float x, float y)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    RGFW_window_moveMouse((RGFW_window*)win->hndl, RGFW_POINT((i32)x, (i32)y));
}

GS_API_DECL void* 
gs_platform_raw_window_handle(uint32_t handle)
{
    // Grab instance of platform from engine
    gs_platform_t* platform = gs_subsystem(platform);

    // Grab window from handle
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    return (void*)win->hndl;
}

GS_API_DECL void 
gs_platform_window_swap_buffer(uint32_t handle)
{
    // Grab instance of platform from engine
    gs_platform_t* platform = gs_subsystem(platform);

    // Grab window from handle
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, handle);
    RGFW_window_swapBuffers((RGFW_window*)win->hndl);
}

GS_API_DECL void     
gs_platform_window_make_current(uint32_t hndl)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(platform->windows, hndl);
    RGFW_window_makeCurrent((RGFW_window*)win->hndl);
}

GS_API_DECL void     
gs_platform_window_make_current_raw(void* win)
{
    RGFW_window_makeCurrent(win);
}

GS_API_DECL gs_vec2 
gs_platform_window_sizev(uint32_t handle)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return window->window_size;
}

GS_API_DECL void 
gs_platform_window_size(uint32_t handle, uint32_t* w, uint32_t* h)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    *w = (int32_t)window->window_size.x;
    *h = (int32_t)window->window_size.y;
}

uint32_t gs_platform_window_width(uint32_t handle)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return (uint32_t)window->window_size.x;
}

uint32_t gs_platform_window_height(uint32_t handle)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return (uint32_t)window->window_size.y;
}

bool32_t gs_platform_window_fullscreen(uint32_t handle)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return RGFW_window_isFullscreen(window->hndl);
}

void gs_platform_window_position(uint32_t handle, uint32_t* x, uint32_t* y)
{ 
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    *x = (uint32_t)window->window_position.x;
    *y = (uint32_t)window->window_position.y;
}

gs_vec2 gs_platform_window_positionv(uint32_t handle)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return window->window_position;
}

void gs_platform_set_window_size(uint32_t handle, uint32_t w, uint32_t h)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_window_resize((RGFW_window*)window->hndl, RGFW_AREA((uint32_t)w, (uint32_t)h));
}

void gs_platform_set_window_sizev(uint32_t handle, gs_vec2 v)
{
    gs_platform_window_t* window = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_window_resize((RGFW_window*)window->hndl, RGFW_AREA((uint32_t)v.x, (uint32_t)v.y));
}

void gs_platform_set_window_fullscreen(uint32_t handle, bool32_t fullscreen)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_monitor monitor;

    int32_t x, y, w, h;
    x = ((RGFW_window*)win->hndl)->r.w; 
    y = ((RGFW_window*)win->hndl)->r.h;
    w = ((RGFW_window*)win->hndl)->r.x; 
    h = ((RGFW_window*)win->hndl)->r.y;

    if (fullscreen)
    {
        uint32_t monitor_index = gs_instance()->ctx.app.window.monitor_index;
        int monitor_count;
        RGFW_monitor* monitors = RGFW_getMonitors();
        if (monitor_index < 6)
        {
            monitor = monitors[monitor_index];
        }
    }

    //rgfwSetWindowMonitor((RGFW_window*)win->hndl, monitor, x, y, w, h, RGFW_DONT_CARE);
}

void gs_platform_set_window_position(uint32_t handle, uint32_t x, uint32_t y)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_window_move((RGFW_window*)win->hndl, RGFW_POINT((int32_t)x, (int32_t)y));
}

void gs_platform_set_window_positionv(uint32_t handle, gs_vec2 v)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_window_move((RGFW_window*)win->hndl, RGFW_POINT((int32_t)v.x, (int32_t)v.y));
}

void gs_platform_framebuffer_size(uint32_t handle, uint32_t* w, uint32_t* h)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    *w = (uint32_t)win->framebuffer_size.x;
    *h = (uint32_t)win->framebuffer_size.y;
}

gs_vec2 gs_platform_framebuffer_sizev(uint32_t handle)
{
    uint32_t w = 0, h = 0;
    gs_platform_framebuffer_size(handle, &w, &h);
    return gs_v2((float)w, (float)h);
}

uint32_t gs_platform_framebuffer_width(uint32_t handle)
{
    uint32_t w = 0, h = 0;
    gs_platform_framebuffer_size(handle, &w, &h);
    return w;
}

uint32_t gs_platform_framebuffer_height(uint32_t handle)
{
    uint32_t w = 0, h = 0;
    gs_platform_framebuffer_size(handle, &w, &h);
    return h;
}

GS_API_DECL gs_vec2 gs_platform_monitor_sizev(uint32_t id)
{
    gs_vec2 ms = gs_v2s(0.f);
    int32_t width, height, xpos, ypos;
    int32_t count;
    RGFW_monitor monitor;
    gs_platform_t* platform = gs_subsystem(platform);

    RGFW_monitor* monitors = RGFW_getMonitors();
    if (count && id < 6) { 
        monitor = monitors[id];
    } 
    else {
        monitor = RGFW_getPrimaryMonitor();
    } 

    monitor.rect.x = (float)width;
    monitor.rect.y = (float)height;
    return ms;
}

GS_API_DECL void gs_platform_window_set_clipboard(uint32_t handle, const char* str)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    RGFW_writeClipboard(str, strlen(str));
}
GS_API_DECL const char* gs_platform_window_get_clipboard(uint32_t handle)
{
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    return RGFW_readClipboard(NULL);
}

void gs_platform_set_cursor(uint32_t handle, gs_platform_cursor cursor)
{
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);
    u8 cp = (u8)platform->cursors[(u32)cursor]; 
    RGFW_window_setMouseStandard(win->hndl, cp);
}

void gs_platform_lock_mouse(uint32_t handle, bool32_t lock)
{
    __gs_input()->mouse.locked = lock;
    gs_platform_t* platform = gs_subsystem(platform);
    gs_platform_window_t* win = gs_slot_array_getp(gs_subsystem(platform)->windows, handle);

    RGFW_window_showMouse((RGFW_window*)win->hndl, !lock);
    // Not sure if I want to support this or not
    // if (glfwRawMouseMotionSupported()) {
    //     glfwSetInputMode(win, RGFW_RAW_MOUSE_MOTION, lock ? RGFW_TRUE : RGFW_FALSE);
    // }
}

/* Main entry point for platform*/
#ifndef GS_NO_HIJACK_MAIN

    int32_t main(int32_t argv, char** argc)
    {
        gs_t* inst = gs_create(gs_main(argv, argc));
        while (gs_app()->is_running) {
            gs_frame();
        }
        // Free engine
        gs_free(inst);
        return 0;
    }

#endif // GS_NO_HIJACK_MAIN

#undef GS_PLATFORM_IMPL_RGFW
#endif // GS_PLATFORM_IMPL_RGFW

#endif // __GS_PLATFORM_IMPL_H__
























