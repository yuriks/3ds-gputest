#include <cstring>
#include <algorithm>

#include <3ds.h>

void fill_framebuffer(u8* fb, u32 color_mask) {
	for (size_t x = 0; x < 400; ++x) {
		u8 color = x * 256 / 400;

		u8 r = color & (color_mask >> 16);
		u8 g = color & (color_mask >> 8);
		u8 b = color & (color_mask);

		for (size_t y = 0; y < 240; ++y) {
			size_t off = (x * 240 + y) * 3;
			fb[off+0] = b;
			fb[off+1] = g;
			fb[off+2] = r;
		}
	}
}

void fill_square(u8* fb, unsigned int i, u8 color) {
	for (size_t x = i*16; x < i*16+16; ++x) {
		for (size_t y = 0; y < 16; ++y) {
			size_t off = (x * 240 + y) * 3;
			fb[off+0] = color;
			fb[off+1] = color;
			fb[off+2] = color;
		}
	}
}

int main() {
	gspInit();
	u8* gsp_shmem_addr = (u8*)0x10002000;
	GSPGPU_AcquireRight(nullptr, 0);

	Handle gsp_event = 0, gsp_shmem = 0;
	u8 gsp_thread_id;
	svcCreateEvent(&gsp_event, 0);
	GSPGPU_RegisterInterruptRelayQueue(nullptr, gsp_event, 1, &gsp_shmem, &gsp_thread_id);
	svcMapMemoryBlock(gsp_shmem, (uintptr_t)gsp_shmem_addr, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE), MEMPERM_DONTCARE);

	size_t fb_size = 400 * 240 * 3;

	u8* red_fb = (u8*)linearAlloc(fb_size);
	fill_framebuffer(red_fb, 0xFF0000);
	GSPGPU_FlushDataCache(nullptr, red_fb, fb_size);
	u8* green_fb = (u8*)linearAlloc(fb_size);
	fill_framebuffer(green_fb, 0x00FF00);
	GSPGPU_FlushDataCache(nullptr, green_fb, fb_size);
	u8* blue_fb = (u8*)linearAlloc(fb_size);
	fill_framebuffer(blue_fb, 0x0000FF);
	GSPGPU_FlushDataCache(nullptr, blue_fb, fb_size);

	u8* sub_fb = (u8*)linearAlloc(320*240*3);
	memset(sub_fb, 0, 320*240*3);

	GSP_FramebufferInfo framebuffer_info;
	framebuffer_info.framebuf0_vaddr = nullptr;
	framebuffer_info.framebuf1_vaddr = nullptr;
	framebuffer_info.framebuf_widthbytesize = 240 * 3;
	framebuffer_info.format = (1 << 8) | (1 << 6) | (0 << 5) | GSP_BGR8_OES;
	framebuffer_info.unk = 0;

	framebuffer_info.framebuf0_vaddr = (u32*)sub_fb;
	framebuffer_info.framebuf1_vaddr = (u32*)sub_fb;
	GSPGPU_SetBufferSwap(nullptr, GFX_BOTTOM, &framebuffer_info);

	gspInitEventHandler(gsp_event, gsp_shmem_addr, gsp_thread_id);
	gspWaitForVBlank();
	GSPGPU_SetLcdForceBlack(nullptr, 0);

	u32 frame = 0;

	int sequence = 0;
	int active_fb = 0;
	int do_toggle = 1;
	int method = 0;

	// Main loop
	while (aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break;
		if (kDown & KEY_A)
			method = (method + 1) % 2;
		if (kDown & KEY_B)
			do_toggle ^= 1;

		fill_square(sub_fb, 0, method ? 0xFF : 0x00);
		fill_square(sub_fb, 1, do_toggle ? 0xFF : 0x00);
		fill_square(sub_fb, 2, active_fb ? 0xFF : 0x00);
		GSPGPU_FlushDataCache(nullptr, sub_fb, 320*240*3);

		if (frame % 30 == 0) {
			sequence = (sequence + 1) % 3;

			switch (sequence) {
				case 0:
					framebuffer_info.framebuf0_vaddr = (u32*)red_fb;
					framebuffer_info.framebuf1_vaddr = (u32*)green_fb;
					break;
				case 1:
					framebuffer_info.framebuf0_vaddr = (u32*)green_fb;
					framebuffer_info.framebuf1_vaddr = (u32*)blue_fb;
					break;
				case 2:
					framebuffer_info.framebuf0_vaddr = (u32*)blue_fb;
					framebuffer_info.framebuf1_vaddr = (u32*)red_fb;
					break;
			}
			framebuffer_info.active_framebuf = active_fb;
			framebuffer_info.framebuf_dispselect = active_fb;
		}

		// Flush and swap framebuffers
		//gfxFlushBuffers();
		gspWaitForVBlank();

		if (frame % 30 == 0) {
			if (method == 0) {
				GSPGPU_SetBufferSwap(nullptr, GFX_TOP, &framebuffer_info);
			} else if (method == 1) {
				u8* framebuffer_info_header = gsp_shmem_addr + 0x200 + gsp_thread_id * 0x80;
				GSP_FramebufferInfo* fb_info = (GSP_FramebufferInfo*)&framebuffer_info_header[4];

				framebuffer_info_header[0] = active_fb;
				fb_info[active_fb] = framebuffer_info;
				framebuffer_info_header[1] = 1;
			}
			//gfxSwapBuffers();

			active_fb ^= do_toggle;
		}

		++frame;
	}

	gspExitEventHandler();

	linearFree(red_fb);
	linearFree(green_fb);
	linearFree(blue_fb);
	linearFree(sub_fb);

	svcUnmapMemoryBlock(gsp_shmem, (uintptr_t)gsp_shmem_addr);

	GSPGPU_UnregisterInterruptRelayQueue(nullptr);

	svcCloseHandle(gsp_shmem);
	svcCloseHandle(gsp_event);

	GSPGPU_ReleaseRight(nullptr);

	gspExit();

	return 0;
}
