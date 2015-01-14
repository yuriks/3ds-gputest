#include <cstring>
#include <algorithm>

#include <3ds.h>

void draw_bar(volatile u8* fb_top, volatile u8* fb_sub, int pos_y, u8 col) {
	const size_t stride = 240*3;

	fb_top += stride * 40;
	fb_top += pos_y * 3;
	fb_sub += pos_y * 3;

	const u8 a = 0x00;
	const u8 b = 0xFF;

	for (int x = 0; x < 320; ++x) {
#define WRITE(dst, i) dst[i+0] = a; dst[i+12] = b
		WRITE(fb_top, 0);
		WRITE(fb_sub, 0);
		WRITE(fb_top, 1);
		WRITE(fb_sub, 1);
		WRITE(fb_top, 2);
		WRITE(fb_sub, 2);

		fb_top += stride;
		fb_sub += stride;
	}
}

int main() {
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);

	u32 frame = 0;

	int pos_y = 0;

	{
		u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
		memset(fb, 0, 240*400*3);
		gfxFlushBuffers();
		gfxSwapBuffers();

		fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
		memset(fb, 0, 240*400*3);
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	u8* fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	u8* fb_sub = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	gfxSwapBuffers();

	// Main loop
	while (aptMainLoop()) {
		if (pos_y < 120) {
			gspWaitForVBlank0();
		} else {
			gspWaitForVBlank1();
		}

		u64 target = svcGetSystemTick() + 2234362;

		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		while (svcGetSystemTick() < target);
		draw_bar(fb_top, fb_sub, pos_y, 0x00);

		pos_y += 4;
		if (pos_y >= 240-4)
			pos_y = 0;

		// Flush and swap framebuffers
		//gfxFlushBuffers();
		//gfxSwapBuffers();
		++frame;
	}

	gfxExit();
	return 0;
}
