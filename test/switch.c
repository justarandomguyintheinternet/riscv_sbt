int getValue() __attribute__((noinline));

int getValue() {
	volatile int x = 3;
	return x;
}

int main() {
	int x = getValue();

	switch(x) {
		case 0: return 1;
		case 1: return 2;
		case 2: return 3;
		case 3: return 4;
		case 4: return 5;
		case 5: return 6;
	}

	return 0;
}