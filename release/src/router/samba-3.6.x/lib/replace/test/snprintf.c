void foo(const char *format, ...)
{
	va_list ap;
	int len;
	char buf[20];
	long long l = 1234567890;
	l *= 100;

	va_start(ap, format);
	len = vsnprintf(buf, 0, format, ap);
	va_end(ap);
	if (len != 5) exit(1);

	va_start(ap, format);
	len = vsnprintf(0, 0, format, ap);
	va_end(ap);
	if (len != 5) exit(2);

	if (snprintf(buf, 3, "hello") != 5 || strcmp(buf, "he") != 0) exit(3);

	if (snprintf(buf, 20, "%lld", l) != 12 || strcmp(buf, "123456789000") != 0) exit(4);
	if (snprintf(buf, 20, "%zu", 123456789) != 9 || strcmp(buf, "123456789") != 0) exit(5);
	if (snprintf(buf, 20, "%2\$d %1\$d", 3, 4) != 3 || strcmp(buf, "4 3") != 0) exit(6);
	if (snprintf(buf, 20, "%s", 0) < 3) exit(7);

	printf("1");
	exit(0);
}
main() { foo("hello"); }
