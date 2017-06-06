void doAcceot() {
	printf("Password Accepted\n");
	// do something else
}

void doDeny() {
	printf("Access denied\n");
}

int pwCheck() {
	char buffer[5];
	scanf("%s", buffer);
	return 0;
}

int main() {

	if (!pwCheck())
	{
		doDeny();
	}

	exit(0);
}
