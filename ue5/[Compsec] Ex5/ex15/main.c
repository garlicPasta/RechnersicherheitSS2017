int main () {

	int authenticated = 0;
	char buffer[9];

	printf("Password:");

	scanf("%s", buffer);

	if (strcmp(buffer, "password") == 0)
		authenticated = 1;
	if (authenticated) {
		printf("Password OK\n");
		// do something else
	} 
	else 
		printf("Access denied\n");
	return 0;
}