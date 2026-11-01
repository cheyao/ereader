#include <fstream>
#include <iostream>
#include <print>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

extern "C" {
#include "rawdraw_sf.h"
}

uint8_t map(uint8_t color) { return (color + 42) / 85 * 85; }

int main() {
	std::ifstream file("ttf/OpenSans-Regular.ttf", std::ios::binary);

	file.seekg(0, std::ios::end);
	std::streampos file_size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<std::uint8_t> font_data(file_size);
	file.read(reinterpret_cast<char*>(font_data.data()), file_size);

	stbtt_fontinfo font;
	stbtt_InitFont(&font, font_data.data(),
				   stbtt_GetFontOffsetForIndex(font_data.data(), 0));

	int index = 0;
	std::vector<int> offsets;

	printf("// Auto generated glyphs with font/main.cpp\n");
	printf("// OpenSans-Regular, font licenses applies\n\n");

	printf("#ifndef GLYPHS_H\n");
	printf("#define GLYPHS_H\n\n");

	printf("// clang-format off\n");

	printf("const uint8_t font_table[] = {\n    ");
	for (char c = '!'; c <= '~'; ++c) {
		offsets.emplace_back(index);

		int w, h, xoff, yoff;
		const auto bitmap = stbtt_GetCodepointBitmap(
				&font, 0, stbtt_ScaleForPixelHeight(&font, 28), c, &w, &h,
				&xoff, &yoff);

		int advance, lsb;
		stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);
		auto scale = stbtt_ScaleForPixelHeight(&font, 28);

		printf("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X,\n    ", w, h,
			   (uint8_t)xoff, (uint8_t)yoff, (int)lroundf(advance * scale),
			   (int)lroundf(advance * scale));

		index += 2;

		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; j += 4) {
				uint8_t out = 0;
				for (int num = 0; num < 4; ++num) {
					const uint8_t a = map(bitmap[i * w + j]) / 85;
					out = out << 2;
					out += a;
				}
				printf("0x%02X,", out);
				if (j + 4 < w) {
					printf(" ");
				}
				++index;
			}

			if (c != '~' || i != h - 1) {
				printf("\n    ");
			} else {
				printf("\n");
			}
		}

		if (c != '~') {
			printf("\n    ");
		}
	}
	printf("};\n\n");

	// Print offset table
	printf("const uint8_t offset_table[] = {\n    ");
	int i = 0;
	for (const auto offset : offsets) {
		printf("%d, ", offset);
		if (((++i) % 8 == 0)) {
			printf("\n    ");
		}
	}
	printf("\n};\n");

	printf("// clang-format on\n\n");
	printf("#endif // GLYPHS_H\n");
}

void HandleKey(int keycode, int bDown) {}
void HandleButton(int x, int y, int button, int bDown) {}
void HandleMotion(int x, int y, int mask) {}
int HandleDestroy() { return 0; }
