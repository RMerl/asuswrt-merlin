#include <string.h>
#include <stdio.h>

class Q
{
	int v;
	protected:
		int qx;
		int qy;
	int w;
};

class B
{
	int k;
	public:
		int bx;
		int by;
};

class A
{
	int u;

	public:
		A()
		{
		};
		int x;
		char buffer[10];
	
	protected:
		int y;
		B b;
	
	private:
		float z;
};

class C : public A
{
	public:
		C()
		{
		};
		int zzzz;
	private:
		int ssss;
};

int main()
{
	A a;
	C c;
	Q q;
	strcpy( a.buffer, "test" );
	printf ( "%.10s\n", a.buffer );
	return 0;
}
