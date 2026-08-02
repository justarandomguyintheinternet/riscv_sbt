// Force jump table creation to test jump target identification / fallback

#include <stdio.h>

int test_jump_table(int input, int a) {
	int result = a;

	// no break to avoid ret instructions causing new basic blocks to begin, to have a true indirect jump to a unknown address
	switch (input) {
		case 0: result ^= 0x1111;
		case 1: result += 0x2222;
		case 2: result ^= 0x3333;
		case 3: result -= 0x4444;
		case 4: result ^= 0x5555;
		case 5: result += 0x6666;
		case 6: result ^= 0x7777;
		case 7: result -= 0x8888;
		case 8: result ^= 0x9999;
		case 9: result += 0xAAAA;
	}

	return result;
}

int main(int argc, char **argv) {
	printf("Result: %d\n", test_jump_table(argc, 10));
	return 0;
}