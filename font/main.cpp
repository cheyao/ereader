#include <fstream>
#include <iostream>
#include <print>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

extern "C" {
#include "rawdraw_sf.h"
}

// Quantize coverage to 4 levels, biased dark ("stem darkening"): at 24px most
// stems are only partly covered, so >= 50% coverage becomes full black and
// faint edge pixels are kept as light gray instead of being dropped.
uint8_t map(uint8_t color) {
	if (color >= 128) {
		return 255;
	}
	if (color >= 64) {
		return 170;
	}
	if (color >= 24) {
		return 85;
	}
	return 0;
}

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

	const int size = 24;
	const auto scale = stbtt_ScaleForPixelHeight(&font, size);

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	const float baseline_y = ascent * scale;

	printf("// Auto generated glyphs with font/main.cpp\n");
	printf("// OpenSans-Regular, font licenses applies\n\n");

	printf("#ifndef GLYPHS_H\n");
	printf("#define GLYPHS_H\n\n");

	printf("#define HEADER_LENGTH %d\n", 4);
	printf("#define LINE_HEIGHT %d\n\n",
		   (int)lroundf((ascent - descent + lineGap) * scale));
	int space_advance;
	stbtt_GetCodepointHMetrics(&font, ' ', &space_advance, 0);
	printf("#define SPACE_ADVANCE %d\n", (int)lroundf(space_advance * scale));

	printf("// clang-format off\n");

	printf("const uint8_t font_table[] = {\n    ");
	for (char ch = '!'; ch <= '~'; ++ch) {
        int c = ch;

		offsets.emplace_back(index);

        if (c == '-') {
            // Prefer em-dash for better formatting
            c = U'—';
        }

		int w, h, xoff, yoff;
		const auto bitmap = stbtt_GetCodepointBitmap(
				&font, 0, stbtt_ScaleForPixelHeight(&font, size), c, &w, &h,
				&xoff, &yoff);

		int advance;
		stbtt_GetCodepointHMetrics(&font, c, &advance, 0);

		uint8_t yoff_real = std::max(0, (int)lroundf(yoff + baseline_y));

		const int adv =
				std::max((int)lroundf(advance * scale), w + std::max(xoff, 0));
		printf("0x%02X, 0x%02X, 0x%02X, 0x%02X,\n    ", w, h + yoff_real,
			   (uint8_t)std::max(xoff, 0), adv);

		index += 4;

		for (int i = 0; i < w; ++i) {
			for (int j = 0; j < h + yoff_real; j += 8) {
				uint8_t data = 0;

				for (int num = 0; num < 8; ++num) {
					const int row = j + num - yoff_real;
					const uint8_t a = (row >= 0 && row < h)
											  ? map(bitmap[i + w * row]) / 85
											  : 0;
					data <<= 1;
					data += (a >> 0) & 1;
				}

				printf("0x%02X,", data);
				if (j + 8 < h + yoff_real) {
					printf(" ");
				}
				++index;
			}

			if (c != '~' || i != w - 1) {
				printf("\n    ");
			} else {
				printf("\n");
			}
		}

		printf("\n    ");

		for (int i = 0; i < w; ++i) {
			for (int j = 0; j < h + yoff_real; j += 8) {
				uint8_t data = 0;

				for (int num = 0; num < 8; ++num) {
					const int row = j + num - yoff_real;
					const uint8_t a = (row >= 0 && row < h)
											  ? map(bitmap[i + w * row]) / 85
											  : 0;
					data <<= 1;
					data += (a >> 1) & 1;
				}

				printf("0x%02X,", data);
				if (j + 8 < h + yoff_real) {
					printf(" ");
				}
				++index;
			}

			if (c != '~' || i != w - 1) {
				printf("\n    ");
			} else {
				printf("\n");
			}
		}

		if (c != '~') {
			printf("\n    ");
		}

		stbtt_FreeBitmap(bitmap, nullptr);
	}
	printf("};\n\n");

	// Print offset table
	printf("const uint32_t offset_table[] = {\n    ");
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
