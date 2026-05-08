int main() {
	int x = 0x45;

	switch(x) {
		case 0x12:
			return 1;
		case 0x24:
			return 2;
		case 0x45:
			return 3;
	}

	return 0;
}
